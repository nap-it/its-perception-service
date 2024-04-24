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
vector<string> dds_sub_topics; 
bool dds_enable_publish;
bool dds_enable_subscribe;
int domain_id = 0;

//MQTT variables
MqttWrapper* mqtt_server;
data_mqtt_server data_mqtt;
string mqtt_pub_topic;
string mqtt_sub_topic;
bool mqtt_enable_subscribe;
bool mqtt_enable_publish;

//global variables
string processed_cpm;
int debug = 0;

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

    debug = reader.GetInteger("general", "debug", 1);
}

data_mqtt_server getMqttData(const string& path, bool mqtt_enable_subscribe){
    data_mqtt_server data_mqtt;
    INIReader reader (path);

    string host = reader.Get("mqtt-processing", "host", "localhost");
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

}


void mqtt_handler(std::string topic, std::string message) {

    spdlog::info("MQTT Received message {}", message);

}


void setup_dds(){
    server = new Dds("Processing", domain_id, dds_handler);
    server->provision_publisher(dds_pub_topic);
    for (auto& dds_sub_topic : dds_sub_topics){
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
        // string temp = R"({"generationDeltaTime":637329278612,"cpmParameters":{"managementContainer":{"referenceTime":637329278612,"referencePosition":{"latitude":40.630279541015625,"longitude":-8.654230117797852,"altitude":{"altitudeValue":63.79999923706055,"altitudeConfidence":9},"positionConfidenceEllipse":{"semiMajorConfidence":4095,"semiMinorConfidence":4095,"semiMajorOrientation":0.0}}},"wrappedCpmContainer":[{"containerId":1,"containerData":{"orientationAngle":9.100000381469727}},{"containerId":5,"containerData":{"numberOfPerceivedObjects":1,"perceivedObjects":[{"objectID":2,"sensorIDList":[2],"measurementDeltaTime":129,"objectPerceptionQuality":82,"position":{"xCoordinate":{"value":18.83220100402832,"confidence":1},"yCoordinate":{"value":1.2725249528884888,"confidence":1}},"xSpeed":{"value":16383.0,"confidence":1},"ySpeed":{"value":16383.0,"confidence":1},"xAcceleration":{"longitudinalAccelerationValue":161.0,"longitudinalAccelerationConfidence":102},"yAcceleration":{"lateralAccelerationValue":161.0,"lateralAccelerationConfidence":102},"classification":[{"objectClass":{"vehicleSubClass":5},"confidence":101}]}]}}]}})";

        // processed_cpm = process_cpm(temp);

        // spdlog::info("Processed message {}", processed_cpm);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}

