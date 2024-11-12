#include <map>
#include <chrono>
#include <boost/asio.hpp>
#include <thread>
#include <list>
#include <random>
#include <functional>
#include "fastdds-cpp-wrapper/dds.hpp"
#include "mqttwrapper.h"
#include "config_reader.h"
#include "camera_data_management.h"

using namespace std;

// Global variables
map<int, cameraMqttObject> objects_to_send;      // shared between threads
map<int, cameraMqttObject> last_sent;            // save data of the last time the object was included in a CPM
map<int, string> serialized_objects_to_send;    // shared between threads
stringstream str_objects_to_send; 
int n_objects_to_send = 0;                    // shared between threads
std::mutex lock_mutex;
const long int time2004ms = 1072915200000;
int debug = 0;
unsigned long int last_clean = 0;

//Configuration variables
string mqtt_host = "";
int mqtt_port = 0;
string mqtt_topic = "";
string mqtt_client_id = "";
int clean_last_sent_interval = 0;
int max_object_age = 0;

//DDS
Dds* dds_;
int domain_id = 0;


string getRandomNumberString() {
    int uniqueVar;
    std::size_t uniqueId = reinterpret_cast<std::size_t>(&uniqueVar);
    std::time_t currentTime_randomGenerator = std::time(0);
    std::size_t seed = std::hash<std::size_t>{}(currentTime_randomGenerator) ^ uniqueId;
    std::default_random_engine generator(static_cast<unsigned int>(seed));
    std::uniform_int_distribution<int> distribution(0, 10000); // Define range
    int random_number = distribution(generator);       // Random value 
    string random_number_string = std::to_string(random_number);
    return random_number_string;
}


data_mqtt_server readConfigFile(const std::string& path) {
    data_mqtt_server mqttInfo;
    INIReader reader (path);

    if (reader.ParseError() < 0) {
        spdlog::error("Can't load 'config.ini'");
        return mqttInfo;
    }

    // Read configuration

    mqtt_host = reader.Get("camera-adapter", "mqtt_host", "");
    spdlog::info("[CAMERA-CONFIG] Host: {}", mqtt_host);

    mqtt_port = reader.GetInteger("camera-adapter", "mqtt_port", 0);
    spdlog::info("[CAMERA-CONFIG] Port: {}", mqtt_port);

    mqtt_topic = reader.Get("camera-adapter", "mqtt_topic", "");
    spdlog::info("[CAMERA-CONFIG] Topic: {}", mqtt_topic);

    mqtt_client_id = reader.Get("camera-adapter", "mqtt_client_id", "");
    spdlog::info("[CAMERA-CONFIG] Client ID: {}", mqtt_client_id);

    clean_last_sent_interval = reader.GetInteger("camera-adapter", "clean_last_sent_interval", 0);
    spdlog::info("[CAMERA-CONFIG] Clean last sent interval: {}", clean_last_sent_interval);

    max_object_age = reader.GetInteger("camera-adapter", "max_object_age", 0);
    spdlog::info("[CAMERA-CONFIG] Max object age: {}", max_object_age);

    domain_id = reader.GetInteger("camera-adapter", "domain_id", 0);
    spdlog::info("[CAMERA-CONFIG] Domain ID: {}", domain_id);

    debug = reader.GetInteger("camera-adapter", "debug", 0);
    spdlog::info("[CAMERA-CONFIG] Debug: {}", debug);

    mqttInfo.address = "tcp://" + mqtt_host + ":" + std::to_string(mqtt_port);
    spdlog::info("Address: {}", mqttInfo.address);
    
    string rnd = getRandomNumberString();
    mqttInfo.client_id = mqtt_client_id + "-" + to_string(domain_id) + rnd;
    spdlog::info("Client ID: {}", mqttInfo.client_id);
    
    vector<string> topics;
    topics.push_back(mqtt_topic);
    mqttInfo.subscription_topic = topics;

    return mqttInfo;
}

int clean_last_sent(std::map<int, cameraMqttObject> * last_sent_dict, unsigned long int current_time) {
    int n_erased = 0;
    for(auto it = last_sent_dict->begin(); it != last_sent_dict->end(); ) {
        if (current_time - it->second.timestamp > max_object_age) {
            it = last_sent_dict->erase(it);
            spdlog::debug("[LAST] Erased object with id: {}", it->first);
            n_erased++;
        } else {
            ++it;
        }
    }
    return n_erased;
}

int clean_to_send(std::map<int, cameraMqttObject> * objects_to_send, unsigned long int current_time) {
    int n_erased = 0;
    for(auto it = objects_to_send->begin(); it != objects_to_send->end(); ) {
        if (current_time - it->second.timestamp > 2000) {
            it = objects_to_send->erase(it);
            spdlog::debug("[TO SEND] Erased object with id: {}", it->first);
            n_erased++;
        } else {
            ++it;
        }
    }
    return n_erased;
}

void on_message_dds(std::string topic, std::string message) {
    
    spdlog::info("Message arrived: {}", message);

    if (topic == "to/adapters") {

        // generate reply based on request and objects_to_send
        string reply = get_reply(message, &lock_mutex, &serialized_objects_to_send);
        
        auto start_publish = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        dds_->publish("from/adapters", reply);

        spdlog::info("Replied with {} objects", n_objects_to_send);
        
        //update last_sent
        std::lock_guard<std::mutex> guard(lock_mutex);
        for(auto const& [key, value] : objects_to_send) {
            last_sent.insert_or_assign(key, value);
        }
        
        // clear "objects_to_send"
        objects_to_send.clear();
        serialized_objects_to_send.clear();
        n_objects_to_send = 0;
    }
}

void on_message_mqtt(std::string topic, std::string message) {

    spdlog::debug("[MQTT] Message arrived: {}", message);

    std::list<cameraMqttObject> camera_objects = parse_json(message);
    spdlog::debug("Parsed {} objects", camera_objects.size());

    std::list<string> serialized_list = structs_to_string(camera_objects);
    spdlog::debug("Serialized {} objects", serialized_list.size());
    
    int n_objects = camera_objects.size();
    for(int i = 0; i < n_objects; i++) {
        cameraMqttObject obj = camera_objects.front();

        //Check if object is in objects_to_send

        if (objects_to_send.find(obj.objectID) != objects_to_send.end()) {
            spdlog::debug("Object {} already in objects_to_send, updating info...", obj.objectID);
            //update object in objects_to_send
            std::lock_guard<std::mutex> guard(lock_mutex);
            objects_to_send[obj.objectID] = obj;
            serialized_objects_to_send[obj.objectID] = serialized_list.front();

            camera_objects.pop_front();
            serialized_list.pop_front();
            continue;
        }

        bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);

        if (is_newInfo) {
            spdlog::debug("Object attributes are new, adding to objects_to_send");
            std::lock_guard<std::mutex> guard(lock_mutex);
            objects_to_send[obj.objectID] = obj;


            auto first = serialized_list.front();
            serialized_objects_to_send[obj.objectID] = first;
            n_objects_to_send++;
        } else {
            spdlog::debug("Object has no new information, not adding to objects_to_send");
        }

        camera_objects.pop_front();
        serialized_list.pop_front();
    }
}

int main() {
    // Read config file
    data_mqtt_server mqttServerInfo = readConfigFile("/config.ini");
    if(debug) spdlog::set_level(spdlog::level::debug); // Set global log level to debug
    else spdlog::set_level(spdlog::level::info); // Set global log level to info

    // DDS
    spdlog::info("Setting up DDS...");
    dds_ = new Dds("CameraAdapter", domain_id, on_message_dds);
    dds_->provision_publisher("from/adapters");
    dds_->subscribe("to/adapters");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    spdlog::info("DDS set up, publishing to 'from/adapters', subscribing to 'to/adapters' with domain ID: {}", domain_id);


    // Send sensor data to DDS
    rapidjson::Document sensorInfo;
    sensorInfo.SetObject();

    rapidjson::Document::AllocatorType& allocator = sensorInfo.GetAllocator();

    sensorInfo.AddMember("sensorID", 2, allocator);
    sensorInfo.AddMember("sensorType", 12, allocator);
    sensorInfo.AddMember("shadowingApplies", false, allocator);

    rapidjson::Value perceptionRegionShapeObj(rapidjson::kObjectType);  
    perceptionRegionShapeObj.AddMember("semiMajorRangeLength", 0, allocator);
    perceptionRegionShapeObj.AddMember("semiMinorRangeLength", 0, allocator);
    perceptionRegionShapeObj.AddMember("semiMajorRangeOrientation", 0, allocator);
    perceptionRegionShapeObj.AddMember("range", 10, allocator);
    perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleStart", 3601, allocator);
    perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleEnd", 3601, allocator);

    sensorInfo.AddMember("perceptionRegionShape", perceptionRegionShapeObj, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    sensorInfo.Accept(writer);
    dds_->publish("from/adapters", buffer.GetString());
    spdlog::info("Published sensor data to 'from/adapters'");

    // MQTT
    spdlog::info("Setting up MQTT...");
    MqttWrapper * mqtt_wrapper = new MqttWrapper(mqttServerInfo, on_message_mqtt);

    // Wait for the connection to be established before further actions
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::info("Connected to MQTT server");

    while (1) {

        // clean "last_sent"
        unsigned long int now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time2004ms;
        if (now - last_clean > clean_last_sent_interval) {
            spdlog::info("Cleaning last_sent that has size: {}", last_sent.size());
            last_clean = now;
            int n_erased = clean_last_sent(&last_sent, now);
            spdlog::info("Cleaned {} objects from last_sent", n_erased);

        }

        // Clean objects_to_send that are too old to be sent to DDS
        spdlog::info("Cleaning objects_to_send that has size: {}", objects_to_send.size());
        int n_erased = clean_to_send(&objects_to_send, now);
        spdlog::info("Cleaned {} objects from objects_to_send", n_erased);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    mqtt_wrapper->disconnect();
    return 0;
}
