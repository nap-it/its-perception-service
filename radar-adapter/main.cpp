#include <map>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <thread>
#include "fastdds/dds.hpp"
#include "mqtt.h"
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
Dds* dds_;
const long int time2004ms = 1072915200000;

mqtt_server readConfigFile(const std::string& path)
{
    mqtt_server mqttInfo;

    INIReader reader (path);

    std::string host = reader.Get("mqtt", "host", "atcll-p35-jetson.nap.av.it.pt");
    std::cout << "Host: " << host << std::endl;
    long port = reader.GetInteger("mqtt", "port", 1883);

    mqttInfo.address = "tcp://" + host + ":" + std::to_string(port);
    std::cout << "Address: " << mqttInfo.address << std::endl;
    string client = reader.Get("mqtt", "client_id", "client") + "-radar";
    cout << "Client: " << client << endl;
    mqttInfo.client_id = client;
    mqttInfo.subscription_topic = reader.Get("mqtt", "radar_topic", "jetson/radar-plus");

    std::cout << "Subscription topic: " << mqttInfo.subscription_topic << std::endl;
    mqttInfo.qos= reader.GetInteger("mqtt", "qos", 1);
    mqttInfo.n_retry_attempts= reader.GetInteger("mqtt", "n_retry_attempts", 5);

    return mqttInfo;
}

void clean_last_sent(std::map<int, radarMqttObject> * last_sent_dict, unsigned long int current_time) {
    for(auto it = last_sent_dict->begin(); it != last_sent_dict->end(); ) {
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
        spdlog::info("getReply: {} microseconds", end_getReply - start_getReply);

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
        
        // clear "objects_to_send"
        std::lock_guard guard(lock_mutex);
        objects_to_send.clear();
        serialized_objects_to_send.clear();
        str_objects_to_send.str("");
        n_objects_to_send = 0;

        // clean "last_sent"
        unsigned long int now = static_cast<unsigned long int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time2004ms);
        clean_last_sent(&last_sent, now);
    }
}

void on_message_mqtt(std::string topic, std::string message) {
    // std::cout << "Message: " << message << " RECEIVED." << std::endl;
    auto start_jsonToStruct = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    radarMqttObject obj = json_to_struct(message);
    auto end_jsonToStruct = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // spdlog::info("jsonToStruct: {} microseconds", end_jsonToStruct - start_jsonToStruct);


    auto start_serializedObj = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    string serialized_obj = struct_to_string(obj);
    auto end_serializedObj = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // spdlog::info("serializedObj: {} microseconds", end_serializedObj - start_serializedObj);

    auto start_calcIsNewInfo = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);
    auto end_calcIsNewInfo = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // spdlog::info("calcIsNewInfo({}): {} microseconds", is_newInfo, end_calcIsNewInfo - start_calcIsNewInfo);

    if (is_newInfo) {
        // save to objects to send objects
        auto start_saveToSend = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::lock_guard guard(lock_mutex);
        objects_to_send[obj.objectID] = obj;
        serialized_objects_to_send[obj.objectID] = serialized_obj;

        str_objects_to_send << serialized_obj << ",";
        n_objects_to_send++;

        auto end_saveToSend = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // spdlog::info("saveToSend: {} microseconds", end_saveToSend - start_saveToSend);
    
    }
}

int main() {

    str_objects_to_send.str("");

    // Read config file
    mqtt_server mqttServerInfo = readConfigFile("/config.ini");

    MqttWrapper * mqtt_wrapper = new MqttWrapper(mqttServerInfo, on_message_mqtt);

    // Wait for the connection to be established before further actions
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // DDS
    dds_ = new Dds("RadarAdapter", 0, on_message_dds);
    dds_->provision_publisher("from/adapters");
    dds_->subscribe("to/adapters");


    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    /* Disconnect from the MQTT server */
    mqtt_wrapper->disconnect();


    return 0;
}
