#include <map>
#include <chrono>
#include <boost/asio.hpp>
#include <thread>
#include <list>
#include "fastdds/dds.hpp"
#include "mqtt.h"
#include "config_reader.h"
#include "camera_data_management.h"

using namespace std;

// Global variables
map<int, cameraMqttObject> objects_to_send;      // shared between threads
map<int, cameraMqttObject> last_sent;            // save data of the last time the object was included in a CPM
map<int, string> serialized_objects_to_send;    // shared between threads
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
    string client = reader.Get("mqtt", "client_id", "client") + "-camera";
    mqttInfo.client_id = client;
    mqttInfo.subscription_topic = reader.Get("mqtt", "camera_topic", "jetson/camera/tracking/objects");

    std::cout << "Subscription topic: " << mqttInfo.subscription_topic << std::endl;
    mqttInfo.qos= reader.GetInteger("mqtt", "qos", 1);
    mqttInfo.n_retry_attempts= reader.GetInteger("mqtt", "n_retry_attempts", 5);

    return mqttInfo;
}

void clean_last_sent(std::map<int, cameraMqttObject> * last_sent_dict, unsigned long int current_time) {
    for(auto it = last_sent_dict->begin(); it != last_sent_dict->end(); ) {
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

        auto end_getReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        spdlog::info("getReply: {} microseconds", end_getReply - start_getReply);
        
        auto start_publish = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        dds_->publish("from/adapters", reply2);
        auto end_publish = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        spdlog::info("publish: {} microseconds", end_publish - start_publish);
        
        // clear "objects_to_send"
        std::lock_guard guard(lock_mutex);
        objects_to_send.clear();
        serialized_objects_to_send.clear();

        unsigned long int now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time2004ms;
        clean_last_sent(&last_sent, now);
    }
}

void on_message_mqtt(std::string topic, std::string message) {
//    std::cout << "Message: " << message << " RECEIVED." << std::endl;

    std::list<cameraMqttObject> camera_objects = parse_json(message);

    std::list<string> serialized_list = structs_to_string(camera_objects);

    for(int i = 0; i < serialized_list.size(); i++) {
        cameraMqttObject obj = camera_objects.front();
        bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);

        if (is_newInfo) {
            // save to objects to send objects
            std::lock_guard guard(lock_mutex);
            objects_to_send[obj.objectID] = obj;
            serialized_objects_to_send[obj.objectID] = serialized_list.front();
        }

        camera_objects.pop_front();
        serialized_list.pop_front();
    }

    // for(auto obj : camera_objects) {
    //     bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);

    //     if (is_newInfo) {
    //         // save to objects to send objects
    //         std::lock_guard guard(lock_mutex);
    //         objects_to_send[obj.objectID] = obj;
    //     }
    // }
}

int main() {

    // Read config file
    mqtt_server mqttServerInfo = readConfigFile("/config.ini");

    MqttWrapper * mqtt_wrapper = new MqttWrapper(mqttServerInfo, on_message_mqtt);

    // Wait for the connection to be established before further actions
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // DDS
    dds_ = new Dds("CameraAdapter", 0, on_message_dds);
    dds_->provision_publisher("from/adapters");
    dds_->subscribe("to/adapters");

    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    /* Disconnect from the MQTT server */
    mqtt_wrapper->disconnect();


    return 0;
}
