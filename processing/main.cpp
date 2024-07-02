#include <map>
#include <iostream>
#include <cstring>
#include <string>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <thread>
#include <signal.h>
#include <random>
#include <functional>
//json
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/prettywriter.h>

//DDS
#include "fastdds-cpp-wrapper/dds.hpp"
#include "fastdds-cpp-wrapper/JSONMessagePubSubTypes.h"

//MQTT
#include "mqttwrapper.h"
#include "mqtt/client.h"

//Config reader
#include "config_reader.hpp"

#include "cpm-processing.h"

using namespace std;
using namespace boost::asio;
using namespace rapidjson;

//thread variables
std::mutex mtx;
std::condition_variable cv;

//DDS variables
Dds* server;
string dds_pub_topic;
vector<string> dds_sub_topics = {};
bool dds_enable_publish;
bool dds_enable_subscribe;
int domain_id = 0;

//MQTT variables
MqttWrapper* mqtt_server;
data_mqtt_server data_mqtt;

mqtt::client* remote_mqtt_client = nullptr;
data_mqtt_server remote_data_mqtt;
string remote_pub_topic = "";

string mqtt_pub_topic;
string mqtt_sub_topic;
bool mqtt_enable_subscribe;
bool mqtt_enable_publish;
bool enable_remote_mqtt = false;

//global variables
string processed_cpm;
int debug = 0;

class Callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener {
    void on_failure(const mqtt::token& tok) override {
        std::cout << "Connection attempt failed" << std::endl;
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        exit(1);
    }

    void on_success(const mqtt::token& tok) override {
        std::cout << "Operation succeeded" << std::endl;
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        auto top = tok.get_topics();
        if (top && !top->empty())
            std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
    }

    void connected(const std::string& cause) override {
        std::cout << "\nConnection success" << std::endl;
    }

    void connection_lost(const std::string& cause) override {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::cout << "Message arrived" << std::endl;
        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {
        std::cout << "Delivery complete for token: [" << (token ? token->get_message_id() : -1) << "]" << std::endl;
    }
};


void readConfigFile(const string& path){
    INIReader reader (path);

    //DDS
    dds_enable_publish = reader.GetBoolean("dds-processing", "enable_publisher", true);
    dds_enable_subscribe = reader.GetBoolean("dds-processing", "enable_subscriber", true);
    dds_pub_topic = reader.Get("general", "topic_objects_publish", "objects");
    string dds_sub_topic = reader.Get("general", "topic_cpm_subscribe", "cps-v2/out/cpm,cps-v2/in/cpm");
    domain_id = reader.GetInteger("dds", "domain_id", 0);

    // Split the string by comma to get individual topics
    std::string topic;
    std::istringstream iss(dds_sub_topic);
    while (std::getline(iss, topic, ',')) {
        //remove whitespace
        topic.erase(std::remove(topic.begin(), topic.end(), ' '), topic.end());
        dds_sub_topics.push_back(topic);
        spdlog::info("Subscription topic {}", topic);
    }

    //MQTT
    mqtt_enable_publish = reader.GetBoolean("mqtt-processing", "enable_publisher", true);
    mqtt_enable_subscribe = reader.GetBoolean("mqtt-processing", "enable_subscriber", false);
    mqtt_pub_topic = reader.Get("general", "topic_objects_publish", "objects");
    mqtt_sub_topic = reader.Get("general", "topic_cpm_subscribe", "vanetza/in/cpm");
    enable_remote_mqtt = reader.GetBoolean("mqtt-processing", "enable_remote_mqtt", false);

    debug = reader.GetInteger("general", "debug", 1);
}


string getRandomNumberString() {
    // Use the address of a local variable as a unique identifier
    int uniqueVar;
    std::size_t uniqueId = reinterpret_cast<std::size_t>(&uniqueVar);

    // Get the current time
    std::time_t currentTime_randomGenerator = std::time(0);
    // Combine the current time and the unique identifier using a hash function
    std::size_t seed = std::hash<std::size_t>{}(currentTime_randomGenerator) ^ uniqueId;

    // Create a random number engine and seed it with the combined seed
    std::default_random_engine generator(static_cast<unsigned int>(seed));
    std::uniform_int_distribution<int> distribution(0, 10000); // Define range

    // Create random interval at the beginning
    int random_number = distribution(generator);       // Random value 

    // Convert the random number to a string
    string random_number_string = std::to_string(random_number);

    return random_number_string;
}


int getDeviceID(const string& path){
    INIReader reader (path);
    int id = reader.GetInteger("general", "id", 0);
    return id;
}


data_mqtt_server getMqttData(const string& path, bool mqtt_enable_subscribe){
    data_mqtt_server data_mqtt;
    INIReader reader (path);

    string host = reader.Get("mqtt-processing", "host", "localhost");
    int port = reader.GetInteger("mqtt", "port", 1883);
    data_mqtt.address = "tcp://" + host + ":" + to_string(port);
    string rnd = getRandomNumberString();
    data_mqtt.client_id = "cpm-processing-" + to_string(domain_id) + "-" + rnd;
    data_mqtt.publish_topic = mqtt_pub_topic;
    if (mqtt_enable_subscribe) {
        string sub_topic = mqtt_sub_topic;
        vector<string> topics;
        topics.push_back(sub_topic);
        data_mqtt.subscription_topic = topics;
    }

    return data_mqtt;
}


data_mqtt_server getRemoteMqttData(const string& path){
    data_mqtt_server data_mqtt;
    INIReader reader (path);

    string host = reader.Get("mqtt-processing", "remote_host", "localhost");
    int port = reader.GetInteger("mqtt-processing", "remote_port", 1884);
    string address = "tcp://" + host + ":" + to_string(port);
    spdlog::debug("Remote MQTT server address: {}", address);
    remote_data_mqtt.address = address;
    string rnd = getRandomNumberString();
    remote_data_mqtt.client_id = "cpm-processing-remote-" + to_string(domain_id) + "-" + rnd;
    string username = reader.Get("mqtt-processing", "remote_username", "username").c_str();
    spdlog::debug("Remote MQTT username: {}", username);
    string password = reader.Get("mqtt-processing", "remote_password", "password").c_str();
    spdlog::debug("Remote MQTT password: {}", password);
    remote_data_mqtt.username = username;
    remote_data_mqtt.password = password;

    // remote_data_mqtt.address = "tcp://atcll-services.nap.av.it.pt:1884";
    // remote_data_mqtt.client_id = "cpm-processing-227";
    // remote_data_mqtt.username = "atcll-services";
    // remote_data_mqtt.password = "bWAoaB&6kQ#pjE9@Dv2opvjDiyxKsw";

    //
    int stationType = reader.GetInteger("general", "stationType", 1);
    int id = getDeviceID("/services/info.ini");
    if(stationType == 15){
        remote_pub_topic = "p" + to_string(id) + "/objects";
    } else {
        remote_pub_topic = "obu" + to_string(id) + "/objects";
    }
    remote_data_mqtt.publish_topic = remote_pub_topic;

    string sub_topic = mqtt_sub_topic;
    vector<string> topics;
    topics.push_back(sub_topic);
    data_mqtt.subscription_topic = topics;

    return remote_data_mqtt;
}


string json_to_string(Document& json){
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    json.Accept(writer);
    return buffer.GetString();
}


void dds_handler(string topic, const string& response){

    // spdlog::info("DDS Received message {}", response);
    Document cpm;

    string cpmJson;

    if(topic == "cps-v2/out/cpm"){
        cpm.Parse(response.c_str());
        Document innerCpm;
        innerCpm.CopyFrom(cpm["fields"]["cpm"], innerCpm.GetAllocator());
        cpmJson = process_cpm(innerCpm);
    } else if(topic == "cps-v2/in/cpm"){
        cpm.Parse(response.c_str());
        cpmJson = process_cpm(cpm);
    } else {
        spdlog::error("Unknown topic {}", topic);
    }

    if (cpmJson.empty()){
        return;
    }

    spdlog::info("Processed message: {}", cpmJson);

    if(dds_enable_publish){
        server->publish(dds_pub_topic, cpmJson);
        spdlog::info("Published message to DDS topic {} ", dds_pub_topic);
    }

    if(mqtt_enable_publish){
        try{
            mqtt_server->publish(mqtt_pub_topic, cpmJson);
            spdlog::info("Published message to MQTT topic {} on host {} ", mqtt_pub_topic, data_mqtt.address);
        } catch (const mqtt::exception& exc) {
            spdlog::error("Error publishing to MQTT: ", exc.what());
        }   
    }

    if(enable_remote_mqtt){
        try{
            remote_mqtt_client->publish(remote_pub_topic, cpmJson.data(), cpmJson.length(), 0,false);
            spdlog::info("Published message to remote MQTT topic {} on host {} ", remote_data_mqtt.publish_topic, remote_data_mqtt.address);
        } catch (const mqtt::exception& exc) {
            spdlog::error("Error publishing to remote MQTT: ", exc.what());
        }   
    }

}


void mqtt_handler(std::string topic, std::string message) {

    spdlog::info("MQTT Received message {}", message);

}


void setup_dds(){
    server = new Dds("Processing", domain_id, dds_handler);
    cout << "Provisioning publisher for " << dds_pub_topic << endl;
    server->provision_publisher(dds_pub_topic);
    cout << "Provisioned publisher for " << dds_pub_topic << endl;
    for (auto& dds_sub_topic : dds_sub_topics){
        cout << "Subscribing to " << dds_sub_topic << endl;
        server->subscribe(dds_sub_topic);
        cout << "Subscribed to " << dds_sub_topic << endl;
    }
}


int main() {
    spdlog::info("Starting server...");
    readConfigFile("/config.ini");

    if(debug) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    if(dds_enable_subscribe == mqtt_enable_subscribe){
        spdlog::error("DDS and MQTT cannot be both enabled or disabled");
        return 1;
    }

    if(mqtt_enable_publish || mqtt_enable_subscribe){
        spdlog::info("Setting up local MQTT...");
        data_mqtt = getMqttData("/config.ini", mqtt_enable_subscribe);
        mqtt_server = new MqttWrapper(data_mqtt, mqtt_handler);
        while(!mqtt_server->is_connected()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    if(enable_remote_mqtt){
        spdlog::info("Setting up remote MQTT...");
        remote_data_mqtt = getRemoteMqttData("/config.ini");

        remote_mqtt_client = new mqtt::client(remote_data_mqtt.address, remote_data_mqtt.client_id);
        Callback cb;
        remote_mqtt_client->set_callback(cb);

        mqtt::connect_options connOpts;
        connOpts.set_clean_session(true);
        connOpts.set_user_name(remote_data_mqtt.username);
        connOpts.set_password(remote_data_mqtt.password);

        string TOPIC = "atcll-services/objects";

        try {
            spdlog::info("Connecting to the remote MQTT server...");
            remote_mqtt_client->connect(connOpts);
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            spdlog::error("Unable to connect to remote MQTT server, check the configuration file or the server status, exiting...");
            return -1;
        }
        spdlog::info("Connected to remote MQTT server");
    }

    spdlog::info("Setting up DDS...");
    if(dds_enable_publish || dds_enable_subscribe){
        setup_dds();
    }

    // if(enable_remote_mqtt){
    //     spdlog::info("Setting up remote MQTT...");
    //     remote_data_mqtt = getRemoteMqttData("/config.ini");
    //     spdlog::info("Remote MQTT server address: {}", remote_data_mqtt.address);
    //     spdlog::info("Remote MQTT client ID: {}", remote_data_mqtt.client_id);
    //     spdlog::info("Remote MQTT username: {}", remote_data_mqtt.username);
    //     spdlog::info("Remote MQTT password: {}", remote_data_mqtt.password);
    //     remote_mqtt_server = new MqttWrapper(remote_data_mqtt, mqtt_handler, true);
    //     while(!remote_mqtt_server->is_connected()){
    //         std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //     }
    // }

    while (1){
        // string temp = R"({"generationDeltaTime":637329278612,"cpmParameters":{"managementContainer":{"referenceTime":637329278612,"referencePosition":{"latitude":40.630279541015625,"longitude":-8.654230117797852,"altitude":{"altitudeValue":63.79999923706055,"altitudeConfidence":9},"positionConfidenceEllipse":{"semiMajorConfidence":4095,"semiMinorConfidence":4095,"semiMajorOrientation":0.0}}},"wrappedCpmContainer":[{"containerId":1,"containerData":{"orientationAngle":9.100000381469727}},{"containerId":5,"containerData":{"numberOfPerceivedObjects":1,"perceivedObjects":[{"objectID":2,"sensorIDList":[2],"measurementDeltaTime":129,"objectPerceptionQuality":82,"position":{"xCoordinate":{"value":18.83220100402832,"confidence":1},"yCoordinate":{"value":1.2725249528884888,"confidence":1}},"xSpeed":{"value":16383.0,"confidence":1},"ySpeed":{"value":16383.0,"confidence":1},"xAcceleration":{"longitudinalAccelerationValue":161.0,"longitudinalAccelerationConfidence":102},"yAcceleration":{"lateralAccelerationValue":161.0,"lateralAccelerationConfidence":102},"classification":[{"objectClass":{"vehicleSubClass":5},"confidence":101}]}]}}]}})";

        // processed_cpm = process_cpm(temp);

        // spdlog::info("Processed message {}", processed_cpm);
        // mqtt_server->publish(mqtt_pub_topic, "Local message");
        // string payload = "{\"hello\": \"world +" + to_string(rand()) + "\"}";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}

