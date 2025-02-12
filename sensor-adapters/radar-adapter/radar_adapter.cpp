#include "radar_adapter.h"
#include <chrono>
#include <functional>

using json = nlohmann::json;

RadarAdapter::RadarAdapter(const Config& config) : config(config) {
    if (config.debug == 1) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug logging enabled.");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Info logging enabled.");
    }

    // DDS
    dds_ = new Dds("RadarAdapter", config.domain_id, on_message_dds);
    dds_->provision_publisher("cps/objects");
    dds_->provision_publisher("cps/sensors");
    
    //dds_->publish("cps/sensors", "hello there");

    // MQTT
    data_mqtt_server mqttInfo;
    mqttInfo.address = "tcp://" + config.mqtt_host + ":" + std::to_string(config.mqtt_port);
    mqttInfo.client_id = config.mqtt_client_id + "-" + std::to_string(config.domain_id) + getRandomNumberString();
    mqttInfo.subscription_topic.push_back(config.mqtt_topic);

    mqtt_wrapper = new MqttWrapper(mqttInfo, [this](const std::string& topic, const std::string& message) {
        this->on_message_mqtt(topic, message);
    });

    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void RadarAdapter::run() {
    spdlog::info("Radar Adapter started running...");
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void RadarAdapter::on_message_mqtt(const std::string& topic, const std::string& message) {
    spdlog::debug("Received MQTT message on topic: {}", topic);
    std::string parsed_message = parseMessage(message);
    spdlog::debug("Parsed message: {}", parsed_message);
    dds_->publish("cps/objects", parsed_message);
}

std::string RadarAdapter::parseMessage(const std::string& input) {


    Object obj;
    try {
        json j = json::parse(input);

        obj.objectID = j.value("objectID", -1);
        obj.sensorID = 1;  // Hardcoded sensor ID for Radar
        obj.timestamp = j.value("timestamp", 0.0);
        obj.classification = j.value("classification", 0);
        obj.confidence = j.value("confidence", 0);
        obj.speed = j.value("speed", 0.0f);
        obj.heading = j.value("heading", 0.0f);
        obj.latitude = j.value("latitude", 0.0f);
        obj.longitude = j.value("longitude", 0.0f);
        obj.size_x = j.value("length", 0.0f);

        // Safe handling for acceleration
        auto acc_value = j.value("acceleration", json(nullptr));
        if (acc_value.is_number()) obj.acceleration = acc_value.get<float>();
        else obj.acceleration = 0.0f;

    } catch (const nlohmann::json::type_error& e) {
        spdlog::error("Type error: {} | Input: {}", e.what(), input);
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("Parse error: {} | Input: {}", e.what(), input);
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error: {} | Input: {}", e.what(), input);
    }

    // Convert Object struct to JSON
    json output = {
        {"objectID", obj.objectID},
        {"sensorID", obj.sensorID},
        {"timestamp", obj.timestamp},
        {"classification", obj.classification},
        {"confidence", obj.confidence},
        {"speed", obj.speed},
        {"heading", obj.heading},
        {"acceleration", obj.acceleration},
        {"latitude", obj.latitude},
        {"longitude", obj.longitude},
        {"size_x", obj.size_x}
    };

    return output.dump();
}

std::string RadarAdapter::getRandomNumberString() {
    int uniqueVar;
    std::default_random_engine generator(static_cast<unsigned int>(
        std::hash<std::size_t>{}(std::time(0)) ^ reinterpret_cast<std::size_t>(&uniqueVar)));
    std::uniform_int_distribution<int> distribution(0, 10000);
    return std::to_string(distribution(generator));
}