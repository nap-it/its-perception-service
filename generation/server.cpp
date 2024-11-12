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
#include <fstream>

//json
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/prettywriter.h>

//DDS
// #include "fastdds/dds.hpp"
#include "fastdds-cpp-wrapper/dds.hpp"

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
int domain_id = 227;

//MQTT variables
data_mqtt_server data_mqtt;
bool mqtt_enable_publish = false;

//CAM variables
float cam_latitude = 40.63028;
float cam_longitude =  -8.65423;
float cam_altitude = 0;
int cam_alitude_conf = 0;
float cam_heading = 0;

//globals
int exptected_responses = 0;
int received_responses = 0;
std::chrono::milliseconds request_deadline(0);
std::chrono::milliseconds request_interval(0);
std::chrono::milliseconds add_sensor_interval(0); 
std::chrono::milliseconds last_request(0);
std::chrono::milliseconds last_sensor(0);
std::chrono::milliseconds max_interval(0);
string sub_adapter_topic = "";
string pub_adapter_topic = "";
string pub_cpm_topic = "";
unsigned long int timestamp_milliseconds = 0;
unsigned long int request_instance = 0;
unsigned long int reply_instance = 0;
int maxObjectAge = 0;
int stationType = 0;
vector<Document> sensorInfo = vector<Document>();
int debug = 0;

//TIME LOGS
bool logs_written = false;
int sequenceNumber = 0;
long on_message_time = 0;
long publish_time = 0;
int request_iteration = 0;
map<int,long> on_message_map;
map<int,long> publish_map;
map<int,long> request_map;



vector<Document> received_objects = vector<Document>();

//thread variables
std::mutex mtx;
std::condition_variable cv;

void readConfigFile(const string& path){
    INIReader reader (path);

    if (reader.ParseError() < 0) {
        spdlog::error("Can't load config file");
        return;
    }

    sub_adapter_topic = reader.Get("cpm-generation", "subscribe_adapter_topic", "from/adapters");
    spdlog::info("[GENERATION - CONFIG] Sub Adapter Topic: {}", sub_adapter_topic);
    
    pub_adapter_topic = reader.Get("cpm-generation", "publish_adapter_topic", "to/adapters");
    spdlog::info("[GENERATION - CONFIG] Pub Adapter Topic: {}", pub_adapter_topic);

    pub_cpm_topic = reader.Get("cpm-generation", "publish_cpm_topic", "vanetza/in/cpm");
    spdlog::info("[GENERATION - CONFIG] Pub CPM Topic: {}", pub_cpm_topic);

    domain_id = reader.GetInteger("cpm-generation", "domain_id", 0);
    spdlog::info("[GENERATION - CONFIG] Domain ID: {}", domain_id);

    request_deadline = std::chrono::milliseconds(reader.GetInteger("cpm-generation", "request_deadline", 50));
    spdlog::info("[GENERATION - CONFIG] Request Deadline: {}", request_deadline.count());

    request_interval = std::chrono::milliseconds(reader.GetInteger("cpm-generation", "request_interval", 100));
    spdlog::info("[GENERATION - CONFIG] Request Interval: {}", request_interval.count());

    add_sensor_interval = std::chrono::milliseconds(reader.GetInteger("cpm-generation", "add_sensor_interval", 1000));
    spdlog::info("[GENERATION - CONFIG] Add Sensor Interval: {}", add_sensor_interval.count());

    max_interval = std::chrono::milliseconds(reader.GetInteger("cpm-generation", "max_interval", 1000));
    spdlog::info("[GENERATION - CONFIG] Max Interval: {}", max_interval.count());

    maxObjectAge = reader.GetInteger("cpm-generation", "clean_object_interval", 15000);
    spdlog::info("[GENERATION - CONFIG] Clean Object Interval: {}", maxObjectAge);

    mqtt_enable_publish = reader.GetBoolean("cpm-generation", "mqtt_enable_publish", false);
    spdlog::info("[GENERATION - CONFIG] MQTT Enable Publish: {}", mqtt_enable_publish);

    exptected_responses = reader.GetInteger("cpm-generation", "expected_responses", 1);
    spdlog::info("[GENERATION - CONFIG] Expected Responses: {}", exptected_responses);

    stationType = reader.GetInteger("cpm-generation", "station_type", 5);
    spdlog::info("[GENERATION - CONFIG] Station Type: {}", stationType);
    
    debug = reader.GetInteger("cpm-generation", "debug", 1);
    spdlog::info("[GENERATION - CONFIG] Debug: {}", debug);

    cam_latitude = reader.GetReal("cpm-generation", "latitude", 40.63028);
    spdlog::info("[GENERATION - CONFIG] Cam Latitude: {}", cam_latitude);

    cam_longitude = reader.GetReal("cpm-generation", "longitude", -8.65423);
    spdlog::info("[GENERATION - CONFIG] Cam Longitude: {}", cam_longitude);
}

data_mqtt_server readMqttData(const string& path){
    data_mqtt_server data;

    INIReader reader (path);

    string host = reader.Get("cpm-generation", "mqtt_host", "localhost");
    int port = reader.GetInteger("cpm-generation", "mqtt_port", 1883);

    data.address = "tcp://" + host + ":" + to_string(port);
    data.client_id = "cpm-generation";
    data.publish_topic = reader.Get("cpm-generation", "mqtt_publish_cpm_topic", "vanetza/in/cpm");

    string sub_topic1 = "vanetza/own/cam";
    string sub_topic2 = "vanetza/time/cam_full";
    vector<string> topics;
    topics.push_back(sub_topic1);
    topics.push_back(sub_topic2);
    data.subscription_topic = topics;

    return data;
}

string documentToString(const Document& value) {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

void printJsonVector(const vector<Document>& objects){
    // Print vector of objects
    cout << "-------------- Printing Vector<Document> --------------" << endl;
    for (const auto& obj : objects) {
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        obj.Accept(writer);
        cout << buffer.GetString() << endl;   
    }
}

void adapter_handler(const string& response){
    auto start_handle_adapter = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    reply_instance = start_handle_adapter;

    unsigned long int total = reply_instance - request_instance;

    // spdlog::debug("Adapter response: {}", response);

    Document doc;
    doc.Parse(response.c_str());

    if (doc.HasMember("sensorType") && doc["sensorType"].IsInt()) {
        sensorInfo.push_back(move(doc));
        printJsonVector(sensorInfo);
        return;
    }
    
    
    // Check requestID is the same as the one sent
    if (doc.HasMember("requestID") && doc["requestID"].IsUint64()) {
        unsigned long int receivedRequestID = doc["requestID"].GetUint64();
        if (receivedRequestID != timestamp_milliseconds) {
            // cout << "Invalid requestID (expected: " << timestamp_milliseconds << ", received: " << receivedRequestID << ")" << endl;
            spdlog::error("Invalid requestID (expected: {}, received: {})", timestamp_milliseconds, receivedRequestID);
            return;
        }
    } else {
        // cout << "No requestID on response" << endl;
        spdlog::error("No requestID on response");
        return;
    }

    // Check sequenceNumber
    if (doc.HasMember("sequenceNumber") && doc["sequenceNumber"].IsInt()) {
        sequenceNumber = doc["sequenceNumber"].GetInt();
        spdlog::debug("Sequence Number: {}", sequenceNumber);
        // on_message_map[sequenceNumber] = on_message_time;
    } 

    // Extract objects data
    if (doc.HasMember("objects") && doc["objects"].IsArray()) {
        const Value& objs = doc["objects"];

        for (auto& obj : objs.GetArray()) {
            Document copyDoc;
            copyDoc.CopyFrom(obj, copyDoc.GetAllocator());
            std::lock_guard<std::mutex> lock(mtx);
            received_objects.push_back(move(copyDoc));
            // cout << "Received object" << endl;
        }
    } else {
        // cout << "No objects on response" << endl;
        spdlog::error("No objects on response");
    }

    // Increase received responses
    std::lock_guard<std::mutex> lock(mtx);
    received_responses++;
    cv.notify_all();

    auto end_handle_adapter = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // spdlog::info("Time to handle adapter response: {}", (end_handle_adapter - start_handle_adapter));

}

void inCam_handler(const string& response){
    Document doc;
    doc.Parse(response.c_str());
    
    if (doc.HasMember("latitude") && doc["latitude"].IsFloat()) {
        cam_latitude = doc["latitude"].GetFloat();
    }

    if (doc.HasMember("longitude") && doc["longitude"].IsFloat()) {
        cam_longitude = doc["longitude"].GetFloat();
    }
}

void inCamFull_handler(const string& response){
    Document doc;
    doc.Parse(response.c_str());
    
    if (doc.HasMember("camParameters")) {
        Value& camParameters = doc["camParameters"];
        if (camParameters.HasMember("basicContainer")) {
            Value& basicContainer = camParameters["basicContainer"];
            if (basicContainer.HasMember("referencePosition")) {
                Value& referencePosition = basicContainer["referencePosition"];
                if (referencePosition.HasMember("latitude") && referencePosition["latitude"].IsFloat()) {
                    cam_latitude = referencePosition["latitude"].GetFloat();
                }
                if (referencePosition.HasMember("longitude") && referencePosition["longitude"].IsFloat()) {
                    cam_longitude = referencePosition["longitude"].GetFloat();
                }
            }
        }
    }
}


void handle_response(string topic, const string& response){

    //topic handler
    if(topic == "from/adapters") {
        on_message_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        adapter_handler(response);
    } else if (topic == "vanetza/in/cam") {
        spdlog::debug("Received CAM from vanetza/in/cam");
        inCam_handler(response);
    } else if (topic == "vanetza/in/cam_full") {
        spdlog::debug("Received CAM_FULL from vanetza/in/cam_full: {}", response);
        inCamFull_handler(response);
    } else {
        cout << "Invalid topic" << endl;
        return;
    }
}

void on_message_mqtt(std::string topic, std::string message) {
    // spdlog::debug("Received CAM");
    if(topic == "vanetza/own/cam"){
        cout << "Received own/cam_full" << endl;
        // ownCam_handler(message);
    } else if (topic == "vanetza/time/cam_full") {
        cout << "Received time/cam_full" << endl;
        // timeCamFull_handler(message);
    }
    
}

void setup_dds(){
    server = new Dds("Generation", domain_id, handle_response);
    cout << "Domain ID: " << domain_id << endl;
    server->provision_publisher(pub_adapter_topic);
    server->provision_publisher(pub_cpm_topic);
    server->subscribe(sub_adapter_topic);
    server->subscribe("vanetza/in/cam");
    server->subscribe("vanetza/in/cam_full");
}

void writeMapToFile(map<int, long> &time_map, string filename) {
    ofstream file;
    file.open(filename);
    for (auto const &pair : time_map) {
        file << pair.first << ": " << pair.second << endl;
    }
    file.close();
}

int main() {
    
    cout << "Starting server..." << endl;
    readConfigFile("/config.ini");
    if(debug) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }
    cout << "Setting up MQTT..." << endl;
    data_mqtt = readMqttData("/config.ini");
    MqttWrapper* mqtt_server;
    if(mqtt_enable_publish){
        mqtt_server = new MqttWrapper(data_mqtt, on_message_mqtt);
        while (!mqtt_server->is_connected()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        cout << "MQTT setup completed" << endl;
    }
    cout << "Setting up DDS..." << endl;
    setup_dds();
    cout << "DDS setup completed" << endl;
    // vector<Document> sensorInfo = initSensorInformation();
    last_request = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - request_interval;
    last_sensor = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - add_sensor_interval;
    bool add_sensor_data = true;
    vector<string> cpmList = vector<string>();

    
    while(1) {
        try {
            auto current_request = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

            //Check if it is time to request data
            if (current_request - last_request >= request_interval) {
                spdlog::debug("-------------------- New request {} --------------------", request_iteration);

                last_request = current_request;

                timestamp_milliseconds = current_request.count() - 1072915200000;

                // string request = getRequestData(timestamp_milliseconds);

                auto starting_time_before_request = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                received_responses = 0;

                //Request data from adapters
                server->publish("to/adapters", "{\"requestID\":" + to_string(timestamp_milliseconds) + ",\"numberObjects\":" + to_string(request_iteration) + "}");
                spdlog::debug("Sent request to {} with ID {} and numberObjects {}", "to/adapters", timestamp_milliseconds, request_iteration);
                // auto ending_time_after_publish = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                // request_map[request_iteration] = ending_time_after_publish;
                

                
                // request_instance = starting_time_before_request;

                // spdlog::debug("Time to publish request: {} us with ID {} and numberObjects {}", (ending_time_after_publish - starting_time_before_request), timestamp_milliseconds, request_iteration);

                // request_iteration++;
                
                std::unique_lock<std::mutex> lk(mtx);
                if (!cv.wait_for(lk, request_deadline, []{return received_responses >= exptected_responses;})) {
                    spdlog::warn("Request deadline reached or received responses: {}", received_responses);
                }  
                //Check if it is time to add sensor information
                if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - last_sensor > add_sensor_interval) {
                    last_sensor = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                    add_sensor_data = true;
                } else {
                    add_sensor_data = false;
                }

                auto starting_time_before_processing = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                //Received objects processing
                vector<string> cpmList = generateCPM(received_objects, cam_latitude, cam_longitude, cam_altitude, cam_alitude_conf, cam_heading, add_sensor_data, sensorInfo, stationType, mtx, sequenceNumber);
                
                auto ending_time_after_processing = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                // spdlog::debug("FULL time to generate CPM: {}", (ending_time_after_processing - starting_time_before_processing));


                for (const auto& cpm_str : cpmList) {

                    auto starting_time_before_publish_dds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                    //Publish CPM to DDS
                    server->publish(pub_cpm_topic, cpm_str);

                    auto ending_time_after_publish_dds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                    publish_time = ending_time_after_publish_dds;
                    // publish_map[sequenceNumber] = publish_time;

                    // spdlog::debug("Time to publish to DDS on topic {}: {}", pub_cpm_topic, (ending_time_after_publish_dds - starting_time_before_publish_dds));

                    spdlog::debug("[{}] CPM Published: {}", sequenceNumber, cpm_str);

                    //Publish CPM to MQTT

                    // auto starting_time_before_publish_mqtt = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    // if (mqtt_enable_publish) { 
                    //     mqtt_server->publish(data_mqtt.publish_topic, cpm_str);
                    //     auto ending_time_after_publish_mqtt = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    //     spdlog::debug("Time to publish to MQTT: {}", (ending_time_after_publish_mqtt - starting_time_before_publish_mqtt));
                    // }
                }

                //Clean and reset everything
                received_objects.clear();
                cpmList.clear();
                cleanOldObjectsIDs(maxObjectAge);

                auto ending_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                spdlog::info("Total time to publish CPM: {} with ID {}", (ending_time - starting_time_before_request), timestamp_milliseconds);

                // if((sequenceNumber >= 950) && !logs_written) {
                //     writeMapToFile(on_message_map, "/times/times_6.txt");
                //     writeMapToFile(publish_map, "/times/times_7.txt");
                //     writeMapToFile(request_map, "/times/times_3.txt");
                //     logs_written = true;
                // }

                //Sleep for the rest of the interval until next request to avoid busy waiting
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                if (now - current_request < request_interval) {
                    std::this_thread::sleep_for(request_interval - (now - current_request) - std::chrono::milliseconds(5));
                }

            }

        } catch(...) {
            raise(SIGTERM);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
