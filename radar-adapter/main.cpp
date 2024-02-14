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

/** Global Mqtt variable */
MqttWrapper * mqtt_wrapper;

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
    std::cout << "Message: " << message << " RECEIVED." << std::endl;
    if (topic == "to/adapters") {
        std::cout << "Received request for adapters" << std::endl;
    }
}

void on_message_mqtt(std::string topic, std::string message) {
    std::cout << "Message: " << message << " RECEIVED." << std::endl;
    radarMqttObject obj = json_to_struct(message);
}

int main() {
    // Read config file
    mqtt_server mqttServerInfo = readConfigFile("/config.ini");

    mqtt_wrapper = new MqttWrapper(mqttServerInfo, on_message_mqtt);

    // Wait for the connection to be established before further actions
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // DDS
    Dds* dds = new Dds("RadarAdapter", 0, on_message_dds);
    dds->subscribe("to/adapters");

    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    /* Disconnect from the MQTT server */
    mqtt_wrapper->disconnect();


    return 0;
}
