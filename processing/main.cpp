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
string dds_pub_full_topic;

//MQTT variables
MqttWrapper* mqtt_server;
data_mqtt_server data_mqtt;
string mqtt_pub_topic;
string mqtt_pub_full_topic;
string mqtt_sub_topic;
bool mqtt_enable_subscribe;
bool mqtt_enable_publish;

//global variables
string processed_cpm;
int debug = 1;
int repeatIntervalSec = 0;

//TIME LOGS
bool write_logs = false;
long cam_seq_number = 0;
long cpm_seq_number = 0;
long cam_on_message_time = 0;
long cpm_on_message_time = 0;
map<long , long> cam_time_map;
map<long , long> cpm_time_map;

void readConfigFile(const string& path){
    INIReader reader (path);

    //DDS
    dds_enable_publish = reader.GetBoolean("dds-processing", "enable_publisher", true);
    dds_enable_subscribe = reader.GetBoolean("dds-processing", "enable_subscriber", true);
    dds_pub_topic = reader.Get("general", "topic_objects_publish", "objects");
    dds_pub_full_topic = reader.Get("general", "topic_objects_full_publish", "objects/full");
    repeatIntervalSec = reader.GetInteger("general", "idRepeatInterval", 3600);
    spdlog::info("Repeat interval timestamp: {}", repeatIntervalSec);
    string dds_sub_topic = reader.Get("general", "topic_cpm_subscribe", "vanetza/out/cpm,vanetza/out/cam_full,vanetza/out/cam");
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
    mqtt_pub_topic = reader.Get("mqtt-processing", "topic_objects_publish", "objects");
    mqtt_pub_full_topic = reader.Get("mqtt-processing", "topic_objects_full_publish", "objects/full");
    spdlog::info("Publishing MQTT to topic {}", mqtt_pub_topic);
    mqtt_sub_topic = reader.Get("general", "topic_cpm_subscribe", "vanetza/in/cpm");

    debug = reader.GetInteger("general", "debug", 1);
}

data_mqtt_server getMqttData(const string& path, bool mqtt_enable_subscribe){
    data_mqtt_server data_mqtt;
    INIReader reader (path);

    string host = reader.Get("mqtt-processing", "host", "mosquitto");
    int port = reader.GetInteger("mqtt", "port", 1883);

    data_mqtt.address = "tcp://" + host + ":" + to_string(port);
    data_mqtt.client_id = "cpm-processing";
    data_mqtt.publish_topic = mqtt_pub_topic;
    if (mqtt_enable_subscribe) {
        string sub_topic = mqtt_sub_topic;
        vector<string> topics;
        topics.push_back(sub_topic);
        data_mqtt.subscription_topic = topics;
    }

    return data_mqtt;
}

string json_to_string(Document& json){
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    json.Accept(writer);
    return buffer.GetString();
}

void dds_handler(string topic, const string& response){

    auto on_message_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // spdlog::info("DDS Received message {}", response);
    Document cpm;

    string cpmJson;
    string fullCpmJson;

    if(topic == "vanetza/out/cpm"){
        spdlog::info("Received CPM {}", response);

        cpm.Parse(response.c_str());

        int senderId = cpm["stationID"].GetInt();
        int receiverId = cpm["receiverID"].GetInt();
        int receiverType = cpm["receiverType"].GetInt();

        Document innerCpm;
        innerCpm.CopyFrom(cpm["fields"]["payload"], innerCpm.GetAllocator());

        cpmJson = simplify_cpm(senderId, receiverId, receiverType, repeatIntervalSec, innerCpm);

        fullCpmJson = process_cpm(senderId, receiverId, receiverType, repeatIntervalSec, innerCpm);

    } else if (topic == "vanetza/time/cpm") {
        spdlog::info("Received CPM {}", response);

        cpm.Parse(response.c_str());

        int senderId = cpm["stationID"].GetInt();
        int receiverId = cpm["receiverID"].GetInt();
        int receiverType = cpm["receiverType"].GetInt();

        Document innerCpm;  
        innerCpm.CopyFrom(cpm["fields"]["cpm"], innerCpm.GetAllocator());

        cpmJson = simplify_cpm(senderId, receiverId, receiverType, repeatIntervalSec, innerCpm);

        fullCpmJson = process_cpm(senderId, receiverId, receiverType, repeatIntervalSec, innerCpm);

    } else if(topic == "cps-v2/in/cpm" || topic == "vanetza/in/cpm"){
        spdlog::info("Received CPM {}", response);
        cpm.Parse(response.c_str());

        cpmJson = simplify_cpm(domain_id, domain_id, 15, repeatIntervalSec, cpm);
        fullCpmJson = process_cpm(domain_id, domain_id, 15, repeatIntervalSec, cpm);
    } else if (topic == "vanetza/out/cam_full"){
        cam_on_message_time = on_message_time;
        return;
    }

    if (cpmJson.empty()){
        return;
    }

    spdlog::info("Processed message: {}", cpmJson);

    if(dds_enable_publish){
        server->publish(dds_pub_topic, cpmJson);
        // spdlog::info("Published message to DDS topic {} ", dds_pub_topic);

        server->publish(dds_pub_full_topic, fullCpmJson);
        spdlog::info("Published message to DDS topic objects/full");
    }

    if(mqtt_enable_publish){
        try{
            mqtt_server->publish(mqtt_pub_topic, cpmJson);
            spdlog::info("Published message to MQTT topic {} on host {} ", mqtt_pub_topic, data_mqtt.address);
            mqtt_server->publish(mqtt_pub_full_topic, fullCpmJson);
            spdlog::info("Published message to MQTT topic objects/full on host {} ", data_mqtt.address);
        } catch (const mqtt::exception& exc) {
            spdlog::error("Error publishing to MQTT: ", exc.what());
        }
        
    }

}


void mqtt_handler(std::string topic, std::string message) {

    spdlog::info("MQTT Received message {}", message);

}

void setup_dds(){
    server = new Dds("Processing", domain_id, dds_handler);
    cout << "Domain ID: " << domain_id << endl;
    cout << "Provisioning publisher for " << dds_pub_topic << endl;
    server->provision_publisher(dds_pub_topic);
    server->provision_publisher(dds_pub_full_topic);
    cout << "Provisioned publisher for " << dds_pub_topic << endl;
    for (auto& dds_sub_topic : dds_sub_topics){
        server->subscribe(dds_sub_topic);
        cout << "Subscribed to " << dds_sub_topic << endl;
    }
}

void writeMapToFile(map<long, long> &time_map, string filename) {
    ofstream file;
    file.open(filename);
    for (auto const &pair : time_map) {
        file << pair.first << ": " << pair.second << endl;
    }
    file.close();
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

    spdlog::info("Setting up DDS...");
    if(dds_enable_publish || dds_enable_subscribe){
        setup_dds();
    }

    if(mqtt_enable_publish || mqtt_enable_subscribe){
        spdlog::info("Setting up MQTT...");
        data_mqtt = getMqttData("/config.ini", mqtt_enable_subscribe);
        mqtt_server = new MqttWrapper(data_mqtt, mqtt_handler);
        while(!mqtt_server->is_connected()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    }

    while (1){
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}

