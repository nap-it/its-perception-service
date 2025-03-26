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
#include <fstream>

//json
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/prettywriter.h>

//DDS
#include "fastdds-cpp-wrapper/dds.hpp"

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

//MQTT variables
MqttWrapper* mqtt_server;
data_mqtt_server data_mqtt;
mqtt::client* remote_mqtt_client = nullptr;
data_mqtt_server remote_data_mqtt;

//global variables
string processed_cpm;

//Configuration variables
bool debug;
int domain_id;
int station_type;
string subscribe_cpm_topic; 
string dds_publish_objects_topic;
string dds_publish_objects_full_topic;
bool local_mqtt_enable_publisher;
string local_mqtt_host;
int local_mqtt_port;
string local_mqtt_publish_objects_topic;
string local_mqtt_publish_objects_full_topic;
bool remote_mqtt_enable_publisher;
string remote_mqtt_host;
int remote_mqtt_port;
string remote_mqtt_username;
string remote_mqtt_password;
string remote_mqtt_publish_objects_topic;
string remote_mqtt_publish_objects_full_topic;
int repeat_id_interval;

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
    if (reader.ParseError() < 0) {
        spdlog::error("Can't load {}", path);
        exit(1);
    }

    debug = reader.GetInteger("cpm-processing", "debug", 1);
    spdlog::info("[PROCESSING - CONFIG] Debug: {}", debug);

    domain_id = reader.GetInteger("cpm-processing", "domain_id", 0);
    spdlog::info("[PROCESSING - CONFIG] Domain ID: {}", domain_id);

    station_type = reader.GetInteger("cpm-processing", "station_type", 15);
    spdlog::info("[PROCESSING - CONFIG] Station Type: {}", station_type);

    subscribe_cpm_topic = reader.Get("cpm-processing", "subscribe_cpm_topic", "vanetza/out/cpm");
    spdlog::info("[PROCESSING - CONFIG] Subscribe CPM topic: {}", subscribe_cpm_topic);

    dds_publish_objects_topic = reader.Get("cpm-processing", "dds_publish_objects_topic", "");
    spdlog::info("[PROCESSING - CONFIG] DDS Publish Objects topic: {}", dds_publish_objects_topic);

    dds_publish_objects_full_topic = reader.Get("cpm-processing", "dds_publish_objects_full_topic", "");
    spdlog::info("[PROCESSING - CONFIG] DDS Publish Objects Full topic: {}", dds_publish_objects_full_topic);

    local_mqtt_enable_publisher = reader.GetBoolean("cpm-processing", "local_mqtt_enable_publisher", false);
    spdlog::info("[PROCESSING - CONFIG] Local MQTT Enable Publisher: {}", local_mqtt_enable_publisher);

    local_mqtt_host = reader.Get("cpm-processing", "local_mqtt_host", "mosquitto");
    spdlog::info("[PROCESSING - CONFIG] Local MQTT Host: {}", local_mqtt_host);

    local_mqtt_port = reader.GetInteger("cpm-processing", "local_mqtt_port", 1883);
    spdlog::info("[PROCESSING - CONFIG] Local MQTT Port: {}", local_mqtt_port);

    local_mqtt_publish_objects_topic = reader.Get("cpm-processing", "local_mqtt_publish_objects_topic", "");
    spdlog::info("[PROCESSING - CONFIG] Local MQTT Publish Objects topic: {}", local_mqtt_publish_objects_topic);

    local_mqtt_publish_objects_full_topic = reader.Get("cpm-processing", "local_mqtt_publish_objects_full_topic", "");
    spdlog::info("[PROCESSING - CONFIG] Local MQTT Publish Objects Full topic: {}", local_mqtt_publish_objects_full_topic);

    remote_mqtt_enable_publisher = reader.GetBoolean("cpm-processing", "remote_mqtt_enable_publisher", false);
    spdlog::info("[PROCESSING - CONFIG] Remote MQTT Enable Publisher: {}", remote_mqtt_enable_publisher);

    remote_mqtt_host = reader.Get("cpm-processing", "remote_mqtt_host", "mosquitto");
    spdlog::info("[PROCESSING - CONFIG] Remote MQTT Host: {}", remote_mqtt_host);

    remote_mqtt_port = reader.GetInteger("cpm-processing", "remote_mqtt_port", 1883);
    spdlog::info("[PROCESSING - CONFIG] Remote MQTT Port: {}", remote_mqtt_port);

    remote_mqtt_username = reader.Get("cpm-processing", "remote_mqtt_username", "");
    spdlog::info("[PROCESSING - CONFIG] Remote MQTT Username: {}", remote_mqtt_username);

    remote_mqtt_password = reader.Get("cpm-processing", "remote_mqtt_password", "");
    spdlog::info("[PROCESSING - CONFIG] Remote MQTT Password: {}", remote_mqtt_password);

    remote_mqtt_publish_objects_topic = reader.Get("cpm-processing", "remote_mqtt_publish_objects_topic", "");
    spdlog::info("[PROCESSING - CONFIG] Remote MQTT Publish Objects topic: {}", remote_mqtt_publish_objects_topic);

    remote_mqtt_publish_objects_full_topic = reader.Get("cpm-processing", "remote_mqtt_publish_objects_full_topic", "");
    spdlog::info("[PROCESSING - CONFIG] Remote MQTT Publish Objects Full topic: {}", remote_mqtt_publish_objects_full_topic);

    repeat_id_interval = reader.GetInteger("cpm-processing", "repeat_id_interval", 1);
    spdlog::info("[PROCESSING - CONFIG] Repeat Interval Sec: {}", repeat_id_interval);

}

data_mqtt_server getMqttData(const string& path){
    data_mqtt_server data_mqtt;
    INIReader reader (path);

    if (reader.ParseError() < 0) {
        spdlog::error("Can't load {}", path);
        exit(1);
    }

    data_mqtt.address = "tcp://" + local_mqtt_host + ":" + to_string(local_mqtt_port);
    spdlog::info("LOCAL MQTT address: {}", data_mqtt.address);
    data_mqtt.client_id = "cpm-processing" + to_string(domain_id);
    data_mqtt.publish_topic = local_mqtt_publish_objects_topic;

    return data_mqtt;
}

data_mqtt_server getRemoteMqttData(const string& path){
    data_mqtt_server data_mqtt;
    INIReader reader (path);

    if (reader.ParseError() < 0) {
        spdlog::error("Can't load {}", path);
        exit(1);
    }


    string address = "tcp://" + remote_mqtt_host + ":" + to_string(remote_mqtt_port);
    spdlog::info("REMOTE MQTT address: {}", address);
    remote_data_mqtt.address = address;
    remote_data_mqtt.client_id = "cpm-processing-remote" + to_string(domain_id);

    remote_data_mqtt.username = remote_mqtt_username;
    remote_data_mqtt.password = remote_mqtt_password;

    
    remote_data_mqtt.publish_topic = remote_mqtt_publish_objects_topic;
    spdlog::info("REMOTE MQTT username: {}", remote_data_mqtt.username);
    spdlog::info("REMOTE MQTT password: {}", remote_data_mqtt.password);

    return remote_data_mqtt;
}

string json_to_string(Document& json){
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    json.Accept(writer);
    return buffer.GetString();
}

void writeOnMessageMapToFile(const std::string& filePath, const std::map<unsigned long int, int64_t>& processingTimeMap) {
    // Open the file in append mode (std::ios::app). If the file does not exist, it will be created.
    std::ofstream outFile(filePath, std::ios::app);

    // Check if the file opened successfully
    if (!outFile) {
        std::cerr << "Error: Could not open the file at " << filePath << std::endl;
        return;
    }

    // Write each key-value pair from the map to the file in the format <key>:<value>
    for (const auto& pair : processingTimeMap) {
        outFile << pair.first << ":" << pair.second << std::endl;
    }

    // Close the file after writing
    outFile.close();
}

void dds_handler(string topic, const string& response){

    auto on_message_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // spdlog::info("DDS Received message {}", response);
    Document cpm;

    string cpmJson;
    string fullCpmJson;
    unsigned long cpmTimestamp = -1;

    if(topic == "vanetza/out/cpm"){
        spdlog::info("Received out CPM {}", response);
        cpm.Parse(response.c_str());

        if(cpm.HasMember("fields") && cpm["fields"].HasMember("payload") && cpm["fields"]["payload"].HasMember("managementContainer") && cpm["fields"]["payload"]["managementContainer"].HasMember("referenceTime")){
            cpmTimestamp = cpm["fields"]["payload"]["managementContainer"]["referenceTime"].GetInt64();
        } else {
            spdlog::error("CPM does not have referenceTime");
        }

        int senderId = cpm["stationID"].GetInt();
        int receiverId = cpm["receiverID"].GetInt();
        int receiverType = cpm["receiverType"].GetInt();

        if(cpm.HasMember("fields") && cpm["fields"].HasMember("payload")){
            Document innerCpm;
            innerCpm.CopyFrom(cpm["fields"]["payload"], innerCpm.GetAllocator());

            if (!dds_publish_objects_topic.empty() || !local_mqtt_publish_objects_topic.empty() || !remote_mqtt_publish_objects_topic.empty()){
                cpmJson = simplify_cpm(senderId, receiverId, receiverType, repeat_id_interval, innerCpm);
            } else {
                spdlog::warn("No simple processing");
            }

            if (!dds_publish_objects_full_topic.empty() || !local_mqtt_publish_objects_full_topic.empty() || !remote_mqtt_publish_objects_full_topic.empty()){
                fullCpmJson = process_cpm(senderId, receiverId, receiverType, repeat_id_interval, innerCpm);
            } else {
                spdlog::warn("No full processing");
            }
        } else {
            spdlog::error("CPM message does not have fields/payload");
            return;
        }

    } else if (topic == "vanetza/time/cpm") {
        spdlog::info("Received time CPM {}", response);

        cpm.Parse(response.c_str());

        int senderId = cpm["stationID"].GetInt();
        int receiverId = cpm["receiverID"].GetInt();
        int receiverType = cpm["receiverType"].GetInt();
        cpmTimestamp = cpm["fields"]["cpm"]["managementContainer"]["referenceTime"].GetInt64();

        Document innerCpm;  
        innerCpm.CopyFrom(cpm["fields"]["cpm"], innerCpm.GetAllocator());


        if (!dds_publish_objects_topic.empty() || !local_mqtt_publish_objects_topic.empty() || !remote_mqtt_publish_objects_topic.empty()){
            cpmJson = simplify_cpm(senderId, receiverId, receiverType, repeat_id_interval, innerCpm);
        } else {
            spdlog::warn("No simple processing");
        }

        if (!dds_publish_objects_full_topic.empty() || !local_mqtt_publish_objects_full_topic.empty() || !remote_mqtt_publish_objects_full_topic.empty()){
            fullCpmJson = process_cpm(senderId, receiverId, receiverType, repeat_id_interval, innerCpm);
        } else {
            spdlog::warn("No full processing");
        }

    } else if(topic == "cps-v2/in/cpm" || topic == "vanetza/in/cpm"){
        spdlog::info("Received in CPM {}", response);
        cpm.Parse(response.c_str());
        cpmTimestamp = cpm["managementContainer"]["referenceTime"].GetInt64();

        if (!dds_publish_objects_topic.empty() || !local_mqtt_publish_objects_topic.empty() || !remote_mqtt_publish_objects_topic.empty()){
            cpmJson = simplify_cpm(domain_id, domain_id, station_type, repeat_id_interval, cpm);
        } else {
            spdlog::warn("No simple processing");
        }
        
        if (!dds_publish_objects_full_topic.empty() || !local_mqtt_publish_objects_full_topic.empty() || !remote_mqtt_publish_objects_full_topic.empty()){
            fullCpmJson = process_cpm(domain_id, domain_id, station_type, repeat_id_interval, cpm);
        } else {
            spdlog::warn("No full processing");
        }

    } else {
        spdlog::error("Unknown topic {}", topic);
        return;
    }

    if (fullCpmJson.empty()){
        spdlog::info("No objects to publish");
        return;
    }

    spdlog::info("Processed message: {}", fullCpmJson);

    if(!dds_publish_objects_topic.empty()){
        server->publish(dds_publish_objects_topic, cpmJson);
        spdlog::info("Published objects to DDS topic {} ", dds_publish_objects_topic);
    }
    if(!dds_publish_objects_full_topic.empty()){
        server->publish(dds_publish_objects_full_topic, fullCpmJson);
        spdlog::info("Published objects to DDS topic {} ", dds_publish_objects_full_topic);
    }

    if(local_mqtt_enable_publisher){
        try{
            if (!local_mqtt_publish_objects_topic.empty()){
                mqtt_server->publish(local_mqtt_publish_objects_topic, cpmJson);
                spdlog::info("Published objects to LOCAL MQTT topic {} on host {} ", local_mqtt_publish_objects_topic, data_mqtt.address);
            }

            if (!local_mqtt_publish_objects_full_topic.empty()){
                mqtt_server->publish(local_mqtt_publish_objects_full_topic, fullCpmJson);
                spdlog::info("Published objects to LOCALMQTT topic {} on host {} ", local_mqtt_publish_objects_full_topic, data_mqtt.address);
            }

        } catch (const mqtt::exception& exc) {
            spdlog::error("Error publishing to MQTT: ", exc.what());
        }
        
    }

    if(remote_mqtt_enable_publisher){
        try{
            if (!remote_mqtt_publish_objects_topic.empty()){
                remote_mqtt_client->publish(remote_mqtt_publish_objects_topic, cpmJson.data(), cpmJson.length(), 0,false);
                spdlog::info("Published objects to REMOTE MQTT topic {} on host {} ", remote_mqtt_publish_objects_topic, remote_data_mqtt.address);
            }

            if (!remote_mqtt_publish_objects_full_topic.empty()){
                remote_mqtt_client->publish(remote_mqtt_publish_objects_full_topic, fullCpmJson.data(), fullCpmJson.length(), 0,false);
                spdlog::info("Published objects to REMOTE MQTT topic {} on host {} ", remote_mqtt_publish_objects_full_topic, remote_data_mqtt.address);
            }

        } catch (const mqtt::exception& exc) {
            spdlog::error("Error publishing to MQTT: ", exc.what());
        }
    }

}


void mqtt_handler(std::string topic, std::string message) {

    spdlog::info("MQTT Received message {}", message);

}

void setup_dds(){
    server = new Dds("CPM-Processing", domain_id, dds_handler);
    server->provision_publisher(dds_publish_objects_full_topic);
    server->provision_publisher(dds_publish_objects_topic);
    for (auto& dds_sub_topic : {subscribe_cpm_topic}){
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

    if(local_mqtt_enable_publisher){
        spdlog::info("Setting up local MQTT...");
        data_mqtt = getMqttData("/config.ini");
        mqtt_server = new MqttWrapper(data_mqtt, mqtt_handler);
        while(!mqtt_server->is_connected()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        spdlog::info("Connected to local MQTT server");
        
    }

    if(remote_mqtt_enable_publisher){
        spdlog::info("Setting up remote MQTT...");
        remote_data_mqtt = getRemoteMqttData("/config.ini");

        remote_mqtt_client = new mqtt::client(remote_data_mqtt.address, remote_data_mqtt.client_id);
        Callback cb;
        remote_mqtt_client->set_callback(cb);

        mqtt::connect_options connOpts;
        connOpts.set_clean_session(true);
        connOpts.set_user_name(remote_data_mqtt.username);
        connOpts.set_password(remote_data_mqtt.password);

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
    setup_dds();
    spdlog::info("DDS setup complete");

    while (1){
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}

