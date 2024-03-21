#include <map>
#include <iostream>
#include <cstring>
#include <string>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <thread>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <mutex>
#include <condition_variable>

//json
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/prettywriter.h>

//DDS
#include "fastdds/dds.hpp"
#include "fastdds/MQTTMessagePubSubTypes.h"

//MQTT
#include "mqttwrapper.h"

//config
#include "config_reader.hpp"

//cpm builder
#include "cpm_builder.hpp"

using namespace std;
using namespace boost::asio;
using namespace rapidjson;

//DDS variables
Dds* server;
int domain_id = 0;

//MQTT variables
data_mqtt_server data_mqtt;
bool mqtt_enable_publish = false;

//global variables initialization
int exptected_responses = 0;
int received_responses = 0;
std::chrono::milliseconds request_deadline(0);
std::chrono::milliseconds request_interval(0);
std::chrono::milliseconds add_sensor_interval(0); 
std::chrono::milliseconds last_request(0);
std::chrono::milliseconds last_sensor(0);
std::chrono::milliseconds max_interval(0);
string sub_adapter_topic = "";
// string pub_adapter_topic = "";
string pub_cpm_topic = "";
unsigned long int timestamp_milliseconds = 0;
unsigned long int reply_instance = 0;
int maxObjectAge = 0;
int stationType = 0;
vector<Document> sensorInfo = vector<Document>();

//CAM variables initialization
float cam_latitude = 40.63028;
float cam_longitude =  -8.65423;
float cam_altitude = 63.8;
int cam_alitude_conf = 9;
float cam_heading = 9.1;

vector<Document> received_objects = vector<Document>();
map<int, Document> objects_map = map<int, Document>();

//thread variables
std::mutex mtx;
std::condition_variable cv;

//Read config file
void readConfigFile(const string& path){
    INIReader reader (path);
    sub_adapter_topic = reader.Get("dds", "topic_adapter_subscribe", "from/adapters");
    // pub_adapter_topic = reader.Get("dds", "topic_adapter_publish", "to/adapters");
    pub_cpm_topic = reader.Get("general", "topic_cpm_publish", "cps-v2/in/cpm");
    domain_id = reader.GetInteger("dds", "domain_id", 0);
    add_sensor_interval = std::chrono::milliseconds(reader.GetInteger("dds", "add_sensor_interval", 1000));
    max_interval = std::chrono::milliseconds(reader.GetInteger("dds", "max_interval", 1000));
    maxObjectAge = reader.GetInteger("general", "clean_object_interval", 15000);
    mqtt_enable_publish = reader.GetBoolean("mqtt", "enable_publish", false);
    stationType = reader.GetInteger("general", "stationType", 15);
}

//Read MQTT data from config file
data_mqtt_server readMqttData(const string& path){
    data_mqtt_server data;

    INIReader reader (path);

    string host = reader.Get("mqtt", "host", "atcll-p35-apu.nap.av.it.pt");
    int port = reader.GetInteger("mqtt", "port", 1883);

    data.address = "tcp://" + host + ":" + to_string(port);
    data.client_id = "cpm-generation";
    data.publish_topic = reader.Get("mqtt", "topic_cpm_publish", "vanetza/in/cpm");

    string sub_topic = "vanetza/own/cam";
    vector<string> topics;
    topics.push_back(sub_topic);
    data.subscription_topic = topics;

    return data;
}

//Debbuging function
string documentToString(const Document& value) {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

//Debbuging function
void printJsonVector(const vector<Document>& objects){
    for (const auto& obj : objects) {
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        obj.Accept(writer);
        cout << buffer.GetString() << endl;   
    }
}

//Handle message from adapters
void adapter_handler(const string& response){
    Document doc;
    doc.Parse(response.c_str());

    spdlog::info("Received response from adapter: {}", response);

    //Check if response is sensor data
    if (doc.HasMember("sensorType") && doc["sensorType"].IsInt()) {
        sensorInfo.push_back(move(doc));
        return;
    } else {
        std::unique_lock<std::mutex> lk(mtx);
        objects_map.insert_or_assign(doc["objID"].GetInt(), move(doc));
        cv.notify_all();
    }
}

//Handle own CAM
void ownCam_handler(const string& response){

    Document doc;
    doc.Parse(response.c_str());
    
    if (!doc.HasMember("longitude") || !doc["longitude"].IsFloat() || !doc.HasMember("latitude") || !doc["latitude"].IsFloat()) {
        cout << "Invalid CAM" << endl;
        return;
    }
    cam_longitude = doc["longitude"].GetFloat();
    cam_latitude = doc["latitude"].GetFloat();

    if (doc.HasMember("altitude") && doc["altitude"].IsFloat()) {
        cam_altitude = doc["altitude"].GetFloat();
    }

    if (doc.HasMember("altitudeConf") && doc["altitudeConf"].IsInt()) {
        cam_alitude_conf = doc["altitudeConf"].GetInt();
    }

    if (doc.HasMember("heading") && doc["heading"].IsFloat()) {
        cam_heading = doc["heading"].GetFloat();
    }

}

//Handle DDS responses
void handle_response(string topic, const string& response){

    //topic handler
    if(topic == "from/adapters") {
        adapter_handler(response);
    } else {
        cout << "Invalid topic" << endl;
        return;
    }
}

//Handle MQTT messages
void on_message_mqtt(std::string topic, std::string message) {
    if(topic == "vanetza/own/cam"){
        ownCam_handler(message);
    }   
    
}

//Setup DDS
void setup_dds(){
    server = new Dds("Generation", domain_id, handle_response);
    // server->provision_publisher(pub_adapter_topic);
    server->provision_publisher(pub_cpm_topic);
    server->subscribe(sub_adapter_topic);
    spdlog::info("DDS domain id: {}, subscribe topic: {}, publish topic: {}", domain_id, sub_adapter_topic, pub_cpm_topic);
}


int main() {
    cout << "Starting server..." << endl;
    readConfigFile("/config.ini");

    cout << "Setting up MQTT..." << endl;
    data_mqtt = readMqttData("/config.ini");
    MqttWrapper* mqtt_server = new MqttWrapper(data_mqtt, on_message_mqtt);
    while (!mqtt_server->is_connected()){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    cout << "Setting up DDS..." << endl;
    setup_dds();
    
    last_request = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - request_interval;
    last_sensor = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - add_sensor_interval;
    bool add_sensor_data = true;

    while(1) {
        try {
            auto current_request = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

            //Check if it is time to request data
            if (current_request - last_request >= request_interval) {
                spdlog::info("----------------------- New trigger -----------------------");
                last_request = current_request;
                timestamp_milliseconds = current_request.count() - 1072915200000;

                //Time logging
                auto starting_time_before_request = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                //Check if it is time to add sensor information
                if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - last_sensor > add_sensor_interval) {
                    last_sensor = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                    add_sensor_data = true;
                } else {
                    add_sensor_data = false;
                }

                //Time logging
                auto starting_time_before_processing = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                //Received objects processing
                vector<string> cpmList = generateCPM(objects_map, cam_latitude, cam_longitude, cam_altitude, cam_alitude_conf, cam_heading, add_sensor_data, sensorInfo, stationType, mtx);
                
                //Time logging
                auto ending_time_after_processing = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                spdlog::info("FULL time to generate CPM: {}", (ending_time_after_processing - starting_time_before_processing));

                //Publish CPMs
                for (const auto& cpm_str : cpmList) {

                    //Time logging
                    auto starting_time_before_publish_dds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                   
                    //Publish CPM to DDS
                    server->publish(pub_cpm_topic, cpm_str);

                    //Time logging
                    auto ending_time_after_publish_dds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    spdlog::info("Time to publish to DDS: {}", (ending_time_after_publish_dds - starting_time_before_publish_dds));

                    // spdlog::info("CPM: {}", cpm_str);

                    //Publish CPM to MQTT
                    auto starting_time_before_publish_mqtt = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    if (mqtt_enable_publish) { 
                        mqtt_server->publish(data_mqtt.publish_topic, cpm_str);
                        auto ending_time_after_publish_mqtt = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                        spdlog::info("Time to publish to MQTT: {}", (ending_time_after_publish_mqtt - starting_time_before_publish_mqtt));
                    }
                }

                //Clear objects map with lock (thread safe)
                {
                    std::unique_lock<std::mutex> lk(mtx);
                    objects_map.clear();
                }

                //Clean unused IDs
                cleanOldObjectsIDs(maxObjectAge);

                //Time logging
                auto ending_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                spdlog::info("Total time: {}", (ending_time - starting_time_before_request));

                //Sleep for the rest of the interval until next request to avoid busy waiting
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                if (now - current_request < request_interval) {
                    std::this_thread::sleep_for(abs(request_interval - (now - current_request)) - std::chrono::milliseconds(5));
                }
            }

        } catch(...) {
            raise(SIGTERM);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
