#include <map>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <thread>
#include "fastdds-cpp-wrapper/dds.hpp"
#include "mqttwrapper.h"
#include "config_reader.h"
#include "radar_data_management.h"

using namespace std;

// Global variables
map<int, radarMqttObject> objects_to_send;      // shared between threads
map<int, radarMqttObject> last_sent;            // save data of the last time the object was included in a CPM
map<int, string> serialized_objects_to_send;    // shared between threads
stringstream str_objects_to_send;                     // shared between threads
int n_objects_to_send = 0;                    // shared between threads
std::mutex lock_mutex;
const long int time2004ms = 1072915200000;
int debug = 0;

//DDS
Dds* dds_;
int domain_id = 0;


data_mqtt_server readConfigFile(const std::string& path)
{
    data_mqtt_server mqttInfo;

    INIReader reader (path);

    std::string host = reader.Get("mqtt-radar", "host", "atcll-p35-jetson.nap.av.it.pt");
    std::cout << "Host: " << host << std::endl;
    long port = reader.GetInteger("mqtt-radar", "port", 1883);

    mqttInfo.address = "tcp://" + host + ":" + std::to_string(port);
    std::cout << "Address: " << mqttInfo.address << std::endl;
    string client = reader.Get("mqtt-radar", "client_id", "mqtt-adapter-radar");
    cout << "Client: " << client << endl;
    mqttInfo.client_id = client;
    string sub_topic = reader.Get("mqtt-radar", "radar_topic", "jetson/radar-plus");
    vector<string> topics;
    topics.push_back(sub_topic);
    mqttInfo.subscription_topic = topics;

    domain_id = reader.GetInteger("dds", "domain_id", 0);

    debug = reader.GetInteger("general", "debug", 1);

    return mqttInfo;
}

void clean_last_sent(std::map<int, radarMqttObject> * last_sent_dict, unsigned long int current_time) {
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
    // std::cout << "Message: " << message << " RECEIVED FROM TOPIC" << topic << std::endl;
    auto arrived_request = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - 1072915200000;
    // cout << "arrived request: " << arrived_request << endl;
    if (topic == "to/adapters") {
        // auto start_prepareReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        // string reply = prepare_reply(message, &lock_mutex, &objects_to_send, &last_sent);
        // auto end_prepareReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        
        // spdlog::info("prepareReply: {} microseconds", end_prepareReply - start_prepareReply);


        auto start_getReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        string reply2 = get_reply(message, &lock_mutex, &serialized_objects_to_send);

        auto end_getReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        spdlog::debug("getReply: {}, in {} microseconds", reply2, end_getReply - start_getReply);

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
        std::lock_guard<std::mutex> guard(lock_mutex);
        for(auto const& [key, value] : objects_to_send) {
            last_sent.insert_or_assign(key, value);
        }
        
        // clear "objects_to_send"
        objects_to_send.clear();
        serialized_objects_to_send.clear();
        n_objects_to_send = 0;

        // clean "last_sent"
        // unsigned long int now = static_cast<unsigned long int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time2004ms);
        // clean_last_sent(&last_sent, now);
    }
}

void on_message_mqtt(std::string topic, std::string message) {
    // std::cout << "Message: " << message << " RECEIVED." << std::endl;
    auto start_jsonToStruct = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    radarMqttObject obj = json_to_struct(message);
    auto end_jsonToStruct = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    //error in json_to_struct
    if (obj.objectID == -101) {
        return;
    }

    
    string serialized_obj = struct_to_string(obj);

    //check if object is in objects_to_send
    {
        std::lock_guard<std::mutex> guard(lock_mutex);
        if (objects_to_send.find(obj.objectID) != objects_to_send.end()) {
            // spdlog::debug("Object {} already in objects_to_send", obj.objectID);
            //update object in objects_to_send
            objects_to_send[obj.objectID] = obj;
            serialized_objects_to_send[obj.objectID] = serialized_obj;
            return;
        }
    }

    bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, &objects_to_send, obj);

    if (is_newInfo) {
        // save to objects to send objects
        // spdlog::debug("Object {} is new", obj.objectID);
        std::lock_guard<std::mutex> guard(lock_mutex);
        objects_to_send[obj.objectID] = obj;
        serialized_objects_to_send[obj.objectID] = serialized_obj;

        // str_objects_to_send << serialized_obj << ",";
        n_objects_to_send++;    
    }
}

int main() {
    // Read config file
    data_mqtt_server mqttServerInfo = readConfigFile("/config.ini");
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

    sensorInfo.AddMember("sensorID", 1, allocator);
    sensorInfo.AddMember("sensorType", 11, allocator);
    sensorInfo.AddMember("shadowingApplies", false, allocator);

    rapidjson::Value perceptionRegionShapeObj(rapidjson::kObjectType);  
    perceptionRegionShapeObj.AddMember("semiMajorRangeLength", 75, allocator);
    perceptionRegionShapeObj.AddMember("semiMinorRangeLength", 20, allocator);
    perceptionRegionShapeObj.AddMember("semiMajorRangeOrientation", 3061, allocator);
    perceptionRegionShapeObj.AddMember("range", 0, allocator);
    perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleStart", 0, allocator);
    perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleEnd", 0, allocator);

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
