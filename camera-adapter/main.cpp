#include <map>
#include <chrono>
#include <boost/asio.hpp>
#include <thread>
#include <list>
#include "fastdds-cpp-wrapper/dds.hpp"
#include "mqtt.h"
#include "config_reader.h"
#include "camera_data_management.h"
#include <cstdlib>
#include <time.h>

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

//DDS
Dds* dds_;
int domain_id = 0;


mqtt_server readConfigFile(const std::string& path)
{
    mqtt_server mqttInfo;

    INIReader reader (path);

    std::string host = reader.Get("mqtt", "host", "atcll-p35-jetson.nap.av.it.pt");
    std::cout << "Host: " << host << std::endl;
    long port = reader.GetInteger("mqtt", "port", 1883);

    mqttInfo.address = "tcp://" + host + ":" + std::to_string(port);
    std::cout << "Address: " << mqttInfo.address << std::endl;

    // Set random client id
    srand(time(0));
    int randomNum = rand();

    string client = reader.Get("mqtt", "client_id", "client") + "-camera-" + std::to_string(randomNum);
    mqttInfo.client_id = client;
    mqttInfo.subscription_topic = reader.Get("mqtt", "camera_topic", "jetson/camera/tracking/objects");

    std::cout << "Subscription topic: " << mqttInfo.subscription_topic << std::endl;
    mqttInfo.qos= reader.GetInteger("mqtt", "qos", 1);
    mqttInfo.n_retry_attempts= reader.GetInteger("mqtt", "n_retry_attempts", 5);

    domain_id = reader.GetInteger("dds", "domain_id", 0);

    debug = reader.GetInteger("general", "debug", 0);

    return mqttInfo;
}

void clean_last_sent(std::map<int, cameraMqttObject> * last_sent_dict, unsigned long int current_time) {
    // cout << "Cleaning last_sent_dict..." << endl;
    for(auto it = last_sent_dict->begin(); it != last_sent_dict->end(); ) {
        // cout << "id: " << it->first << " timestamp: " << it->second.timestamp << endl;
        if (current_time - it->second.timestamp > 15000) {
            it = last_sent_dict->erase(it);
        } else {
            ++it;
        }
    }
}

void on_message_dds(std::string topic, std::string message) {
    // std::cout << "Message: " << message << " RECEIVED FROM TOPIC " << topic << std::endl;
    if (topic == "to/adapters") {
        // auto start_prepareReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        // string reply = prepare_reply(message, &lock_mutex, &objects_to_send, &last_sent);

        // auto end_prepareReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        // spdlog::info("prepareReply: {} microseconds", end_prepareReply - start_prepareReply);

        auto start_getReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        string reply2 = get_reply(message, &lock_mutex, &serialized_objects_to_send);

        if(reply2.empty()) {
            return;
        }

        auto end_getReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        spdlog::debug("getReply: {} microseconds", end_getReply - start_getReply);

        //third reply

        // auto start_reply3 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // rapidjson::Document requestJson;
        // requestJson.Parse(message.c_str());

        // stringstream reply3;
        // reply3  << "{\"requestID\":" << requestJson["requestID"].GetUint64() << ",\"numberObjects\":" << requestJson["numberObjects"].GetInt() << ",\"objects\":[";

        // //remove last comma from str_objects_to_send
        // string str_objects_to_send_str = str_objects_to_send.str();
        
        // if (str_objects_to_send_str.back() == ',') str_objects_to_send_str.pop_back();
        // reply3 << str_objects_to_send_str << "]}";

        // auto end_reply3 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // spdlog::info("reply3: {} microseconds", end_reply3 - start_reply3);
        
        auto start_publish = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        dds_->publish("from/adapters", reply2);
        auto end_publish = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        spdlog::info("publish {} objects: {} microseconds", n_objects_to_send, end_publish - start_publish);

        //update last_sent
        std::lock_guard guard(lock_mutex);
        for(auto const& [key, value] : objects_to_send) {
            last_sent.insert_or_assign(key, value);
        }
        
        // clear "objects_to_send"
        objects_to_send.clear();
        serialized_objects_to_send.clear();
        n_objects_to_send = 0;

        //clean last_sent old data
        // unsigned long int now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time2004ms;
        // clean_last_sent(&last_sent, now);
    }
}

void on_message_mqtt(std::string topic, std::string message) {
//    std::cout << "Message: " << message << " RECEIVED." << std::endl;

    std::list<cameraMqttObject> camera_objects = parse_json(message);

    std::list<string> serialized_list = structs_to_string(camera_objects);

    for(int i = 0; i < serialized_list.size(); i++) {
        cameraMqttObject obj = camera_objects.front();

        //ceck if obj is in objects_to_send
        if (objects_to_send.find(obj.objectID) != objects_to_send.end()) {
            spdlog::debug("Object {} already in objects_to_send", obj.objectID);
            //update object in objects_to_send
            std::lock_guard guard(lock_mutex);
            objects_to_send[obj.objectID] = obj;
            serialized_objects_to_send[obj.objectID] = serialized_list.front();
            camera_objects.pop_front();
            serialized_list.pop_front();
            continue;
        }

        bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);

        if (is_newInfo) {
            // save to objects to send objects
            spdlog::debug("Object {} is new", obj.objectID);
            std::lock_guard guard(lock_mutex);
            objects_to_send[obj.objectID] = obj;


            auto first = serialized_list.front();
            serialized_objects_to_send[obj.objectID] = first;
            // last_sent.insert_or_assign(obj.objectID, obj);

            // str_objects_to_send << first << ",";
            n_objects_to_send++;

        }

        camera_objects.pop_front();
        serialized_list.pop_front();
    }
}

int main() {
    // Read config file
    mqtt_server mqttServerInfo = readConfigFile("/config.ini");
    if(debug) {
        spdlog::set_level(spdlog::level::debug); // Set global log level to debug
    } else {
        spdlog::set_level(spdlog::level::info); // Set global log level to info
    }

    // DDS
    cout << "Setting up DDS..." << endl;
    dds_ = new Dds("RadarAdapter", domain_id, on_message_dds);
    dds_->provision_publisher("from/adapters");
    dds_->subscribe("to/adapters");
    //wait half a second for the subscriber to be ready
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cout << "Domain ID: " << domain_id << ", publishing to 'from/adapters', subscribing to 'to/adapters'" << endl;
    cout << "DDS set up" << endl;

    cout << "Sending sensor data to DDS..."<< endl;

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

    cout << "Sensor data sent to DDS" << endl;

    cout << "Setting up MQTT..." << endl;

    MqttWrapper * mqtt_wrapper = new MqttWrapper(mqttServerInfo, on_message_mqtt);

    // Wait for the connection to be established before further actions
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    /* Disconnect from the MQTT server */
    mqtt_wrapper->disconnect();


    return 0;
}
