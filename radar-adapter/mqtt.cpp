#include "mqtt.h"


mqtt_server readConfigFile(std::string path)
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

void loop_forever()
{
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    };
}

/* Executed function when a message arrived to the subscribed topic */
void callback::message_arrived(mqtt::const_message_ptr msg) {
    spdlog::info("Message arrived from topic {}", msg->get_topic());
    spdlog::info("Message: {}", msg->to_string());
    radarMqttObject obj = json_to_struct(msg->to_string());

}

/* The topic subscription  is made when the connection to the MQTT server was successful */
void callback::connected(const std::string &cause) {
    spdlog::info("Connection success");
    spdlog::info("Subscribing to topic {}", mqtt_server_info.subscription_topic);

    cli_.subscribe(mqtt_server_info.subscription_topic, mqtt_server_info.qos, nullptr, subListener_);

}

void callback::reconnect() {
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    try {
        cli_.connect(connOpts_, nullptr, *this);
    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Error: ", exc.what());
        exit(1);
    }
}

void callback::on_success(const mqtt::token &tok) {}

void callback::on_failure(const mqtt::token &tok) {
    spdlog::warn("Easy padding in numbers like {:08d}", 12);
}

void callback::connection_lost(const std::string &cause) {
    spdlog::warn("Connection lost");
    if (!cause.empty())
        spdlog::warn("Connection lost caused by {}", cause);

    spdlog::info("Reconnecting...");
    n_retry_ = 0;
    reconnect();

}

void callback::delivery_complete(mqtt::delivery_token_ptr token) {

    spdlog::info("Delivery complete");

}

void action_listener::on_failure(const mqtt::token &tok) {

    if (tok.get_message_id() != 0){
        spdlog::warn("{} failure for token: {}", name_, tok.get_message_id());
    }
}

void action_listener::on_success(const mqtt::token &tok) {
    if (tok.get_message_id() != 0){
        spdlog::info("{} success for token: {}", name_, tok.get_message_id());
    }

    auto top = tok.get_topics();
    if (top && !top->empty()){
        spdlog::info("\ttoken topic: {}", (*top)[0]);
    }

}
