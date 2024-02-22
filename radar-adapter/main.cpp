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
std::mutex lock_mutex;
Dds* dds_;

mqtt_server readConfigFile(const std::string& path)
{
    mqtt_server mqttInfo;

    INIReader reader (path);

    std::string host = reader.Get("mqtt", "host", "localhost");
    std::cout << "Host: " << host << std::endl;
    int port = reader.GetInteger("mqtt", "port", 1883);

    mqttInfo.address = "tcp://" + host + ":" + std::to_string(port);
    std::cout << "Address: " << mqttInfo.address << std::endl;
    mqttInfo.client_id = reader.Get("mqtt", "client_id", "client");
    mqttInfo.subscription_topic = reader.Get("mqtt", "topic_subscribe", "hello");

    std::cout << "Subscription topic: " << mqttInfo.subscription_topic << std::endl;
    mqttInfo.qos= reader.GetInteger("mqtt", "qos", 1);
    mqttInfo.n_retry_attempts= reader.GetInteger("mqtt", "n_retry_attempts", 5);

    return mqttInfo;
}


void on_message_dds(std::string topic, std::string message) {
    std::cout << "Message: " << message << " RECEIVED FROM TOPIC" << topic << std::endl;
    if (topic == "to/adapters") {
        std::cout << "Received request for adapters" << std::endl;
        string reply = prepare_reply(message, &lock_mutex, &objects_to_send, &last_sent);
//        std::cout << "reply: " << reply << std::endl;
        dds_->publish("from/adapters", reply);
        spdlog::info("Reply to {} sent.\n", message);
        // clear "objects_to_send"
        std::lock_guard guard(lock_mutex);
        objects_to_send.clear();
    }
}

void on_message_mqtt(std::string topic, std::string message) {
//    std::cout << "Message: " << message << " RECEIVED." << std::endl;

    radarMqttObject obj = json_to_struct(message);
    bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);

    if (is_newInfo) {
        // save to objects to send objects
        std::lock_guard guard(lock_mutex);
        objects_to_send[obj.objectID] = obj;
    }
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
