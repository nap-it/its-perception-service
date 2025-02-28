#include "mqttwrapper.h"

action_listener::action_listener(std::string name) : name_(std::move(name)) {}

void action_listener::on_failure(const mqtt::token& tok) {
    if (tok.get_message_id() != 0) {
        spdlog::warn("{} failure for token: {}", name_, tok.get_message_id());
    }
}

void action_listener::on_success(const mqtt::token& tok) {
    if (tok.get_message_id() != 0) {
        spdlog::info("{} success for token: {}", name_, tok.get_message_id());
    }

    auto top = tok.get_topics();
    if (top && !top->empty()) {
        spdlog::info("\ttoken topic: {}", (*top)[0]);
    }
}

action_listener::~action_listener() {
}

MqttWrapper::MqttWrapper(data_mqtt_server data, std::function<void(std::string, std::string)> on_message_received)
        : client_(data.address, data.client_id), listener_("YourListenerName"), connOpts_(), data_server(std::move(data)){

    this->connOpts_.set_clean_session(false);
    this->connOpts_.set_automatic_reconnect(true);
    this->connOpts_.set_keep_alive_interval(20);

    this->client_.set_callback(*this);

    this->on_message_function = on_message_received;

    // Start the connection
    try {
        spdlog::info("Connecting to the MQTT server...");
        this->client_.connect(this->connOpts_, nullptr, *this);
    } catch (const mqtt::exception& exc) {
        spdlog::error("Unable to connect to MQTT server: ", exc.what());
        exit(1);
    }

    std::thread mqtt_th(&MqttWrapper::mqtt_thread, this);
    mqtt_th.detach();

}

MqttWrapper::MqttWrapper(data_mqtt_server data)
        : client_(data.address, data.client_id), listener_("YourListenerName"), connOpts_(), data_server(std::move(data)){

    this->connOpts_.set_clean_session(false);
    this->connOpts_.set_automatic_reconnect(true);
    this->connOpts_.set_keep_alive_interval(20);

    this->client_.set_callback(*this);

    // Start the connection
    try {
        spdlog::info("Connecting to the MQTT server...");
        this->client_.connect(this->connOpts_, nullptr, *this);
    } catch (const mqtt::exception& exc) {
        spdlog::error("Unable to connect to MQTT server: ", exc.what());
        exit(1);
    }

    std::thread mqtt_th(&MqttWrapper::mqtt_thread, this);
    mqtt_th.detach();

}

MqttWrapper::~MqttWrapper() {
}

void MqttWrapper::connect() {
    try {
        this->client_.connect(this->connOpts_, nullptr, *this);
    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Error: ", exc.what());
        exit(1);
    }

}

void MqttWrapper::on_success(const mqtt::token &tok) { }

void MqttWrapper::on_failure(const mqtt::token &tok) {
    spdlog::warn("MqttWrapper on_failure for token: {}", tok.get_message_id());
}

void MqttWrapper::connection_lost(const std::string &cause) {
    spdlog::warn("Connection lost");
    if (!cause.empty())
        spdlog::warn("Connection lost caused by {}", cause);

    spdlog::info("Reconnecting...");

    reconnect();
}

void MqttWrapper::connected(const std::string &cause) {
    spdlog::info("Connection success");

    // Subscribe to the list of topic passed in the constructor
    for (int i=0; i<this->data_server.subscription_topic.size(); i++) {
        this->subscribe(data_server.subscription_topic.at(i));
        spdlog::info("Subscribing to topic {}", this->data_server.subscription_topic.at(i));
    }

}

bool MqttWrapper::is_connected(){
    return this->client_.is_connected();
}

void MqttWrapper::reconnect() {
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    this->connect();
}

void MqttWrapper::subscribe(const std::string topic) {
    this->client_.subscribe(topic, 0, nullptr, this->listener_);
}


void MqttWrapper::message_arrived(mqtt::const_message_ptr msg) {
    on_message_function(msg->get_topic(), msg->to_string());

}

void MqttWrapper::delivery_complete(mqtt::delivery_token_ptr token) {
    spdlog::info("Delivery complete");

}

void MqttWrapper::publish(const std::string &topic, const std::string &message) {
    try {
        this->client_.publish(topic, message);
    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Error: ", exc.what());
    }
}

void MqttWrapper::disconnect(){
    try {
        spdlog::info("Disconnecting from the MQTT server...");
        this->client_.disconnect()->wait();
        spdlog::info("Disconnected");
    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Error: ", exc.what());
        exit(1);
    }

}

void  MqttWrapper::mqtt_thread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(100));
    }
}
