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

//DDS
#include "fastdds/DDSSubscriber.hpp"
#include "fastdds/DDSPublisher.hpp"
#include "fastdds/MQTTMessagePubSubTypes.h"
#include "dds-sub.h"

//MQTT
#include "mqtt.h"
#include "mqtt/async_client.h"

//Config reader
#include "config_reader.hpp"

using namespace std;
using namespace boost::asio;

//global variables
bool ddsPubEnabled = false;
bool mqttPubEnabled = false;

//DDS Publisher variables
string publisher_dds_topic;
TypeSupport* typeSupport;
DDSPublisher<MQTTMessage, MQTTMessagePubSubType>* dds_publisher;
MQTTMessagePubSubType mqttMessagePubSubType;

void init_dds_publisher() {
    cout << publisher_dds_topic << endl;
    typeSupport = new TypeSupport(&mqttMessagePubSubType);
    dds_publisher = new DDSPublisher<MQTTMessage, MQTTMessagePubSubType>(typeSupport);
    dds_publisher->init("TestPublisher", 0, publisher_dds_topic, "MQTTMessage", TOPIC_QOS_DEFAULT);
}

//MQTT Publisher variables
string publisher_mqtt_topic;
string publisher_mqtt_host;
string publisher_mqtt_port;
mqtt::async_client *mqtt_pub_client;

void init_mqtt_publisher() {
    mqtt_pub_client = new mqtt::async_client(publisher_mqtt_host, "publisher");
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(false);
    connOpts.set_automatic_reconnect(true);
    connOpts.set_keep_alive_interval(20);
    
    try {
        spdlog::info("Publisher connecting to the MQTT server...");
        mqtt_pub_client->connect(connOpts);
    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Error: {}", exc.what());
        return;
    }
    spdlog::info( "Publisher connected to MQTT");
    
}

void publishProcessedCpm(string cpm_objects) {
    if (ddsPubEnabled) {
        MQTTMessage* mqttMessage = new MQTTMessage();
        mqttMessage->uuid(1);
        mqttMessage->topic(publisher_dds_topic);
        mqttMessage->message(cpm_objects);
        mqttMessage->datetime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        dds_publisher->publish(mqttMessage);
        spdlog::info("Published message to DDS");

    }
    
    if (mqttPubEnabled) {
        try
        {
            mqtt_pub_client->publish(publisher_mqtt_topic, cpm_objects.c_str(), cpm_objects.size());

        } catch (const mqtt::exception& exc) {
            std::cerr << "Error publishing message: " << exc.what() << std::endl;
            throw; // Rethrow the exception or handle it as needed
        }
        spdlog::info("Published message to MQTT");
    }
}

void run_dds(string sub_topic) {

    ddsSetPublishProcessedCpm(publishProcessedCpm);

    SubListener* listener_;
    TypeSupport* typeSupport;
    MQTTMessagePubSubType mqttMessagePubSubType;
    
    //DDS setup
    listener_ = new SubListener();
    typeSupport = new TypeSupport(&mqttMessagePubSubType);
    DDSSubscriber* subscriber = new DDSSubscriber(listener_, typeSupport);
    subscriber->init("TestSubscriber", 0, sub_topic, "MQTTMessage", TOPIC_QOS_DEFAULT);

    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void run_mqtt(){
    //MQTT setup
    server mqttServer = readConfigFile("./config.ini");
    mqtt::async_client client(mqttServer.address, mqttServer.client_id);
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(false);
    connOpts.set_automatic_reconnect(true);

    callback cb(client, connOpts, mqttServer);
    client.set_callback(cb);

    mqttSetPublishProcessedCpm(publishProcessedCpm);

    // Start the connection.
    // When completed, the callback will subscribe to topic.
    try {
        spdlog::info("Subscriber connecting to the MQTT server...");
        client.connect(connOpts, nullptr, cb);
    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Error: {}", exc.what());
        return;
    }

    spdlog::info("Subscriber connected to MQTT");

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
}

int main() {

    //Read config file
    INIReader reader("./config.ini");

    //Reading configuration parameters 
    bool ddsSubEnabled = reader.GetBoolean("dds", "enable_subscriber", false);
    bool mqttSubEnabled = reader.GetBoolean("mqtt", "enable_subscriber", false);
    ddsPubEnabled = reader.GetBoolean("dds", "enable_publisher", false);
    mqttPubEnabled = reader.GetBoolean("mqtt", "enable_publisher", false);

    if(ddsPubEnabled){
        publisher_dds_topic = reader.Get("dds", "topic_publish", "apu/objects");
        init_dds_publisher();
        cout << "DDS Publisher enabled" << endl;
    }
    if (mqttPubEnabled) {
        publisher_mqtt_topic = reader.Get("mqtt", "topic_publish", "apu/objects");
        publisher_mqtt_host = reader.Get("mqtt", "host", "localhost");
        publisher_mqtt_port = reader.Get("mqtt", "port", "1883");
        init_mqtt_publisher();
        cout << "MQTT Publisher enabled" << endl;
        
    } 

    //Abort if both are disabled or enabled
    if (ddsSubEnabled == mqttSubEnabled) {
        spdlog::error("DDS and MQTT cannot be both enabled or disabled");
        return 1;
    }

    //Run DDS or MQTT client
    if (ddsSubEnabled) {
        cout << "Running DDS" << endl;
        string sub_topic = reader.Get("dds", "topic_subscribe", "vanetza/out/cpm");
        run_dds(sub_topic);
    } else {
        cout << "Running MQTT" << endl;
        run_mqtt();
    }

    return 0;
}

