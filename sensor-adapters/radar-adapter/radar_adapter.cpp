#include "radar_adapter.h"
#include <chrono>
#include <functional>

using json = nlohmann::json;

RadarAdapter::RadarAdapter(const Config& config) : config(config) {
    if (config.debug) {
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
    
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Publish sensor information
    SensorInfo sensorInfo = {
        1, 11, false, 75, 20, 3061, 0, 0, 0
    };
    string sensorInfoStr = to_json(sensorInfo).dump();
    dds_->publish("cps/sensors", sensorInfoStr);
    spdlog::info("Sensor information published {}", sensorInfoStr);

    // MQTT
    data_mqtt_server mqttInfo;
    mqttInfo.address = "tcp://" + config.mqtt_host + ":" + std::to_string(config.mqtt_port);
    mqttInfo.client_id = config.mqtt_client_id + "-" + std::to_string(config.domain_id) + getRandomNumberString();
    mqttInfo.subscription_topic.push_back(config.mqtt_topic);

    mqtt_wrapper = new MqttWrapper(mqttInfo, [this](const std::string& topic, const std::string& message) {
        this->on_message_mqtt(topic, message);
    });

    int max_retries = 10;
    int retries = 0;
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        spdlog::info("Waiting for MQTT connection, retrying...");
        if (retries++ > max_retries) {
            spdlog::error("Failed to connect to MQTT server, exiting...");
            exit(1);
        }
    }
}

void RadarAdapter::run() {
    spdlog::info("Radar Adapter started running...");

    // Publish sensor information
    SensorInfo sensorInfo = {
        1, 11, false, 75, 20, 3061, 0, 0, 0
    };
    string sensorInfoStr = to_json(sensorInfo).dump();
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        dds_->publish("cps/sensors", sensorInfoStr);
        spdlog::info("Sensor information published {}", sensorInfoStr);

        //Check if MQTT is still connected
        if (!mqtt_wrapper->is_connected()) {
            spdlog::error("MQTT connection lost, reconnecting...");
        }
    }
}

void RadarAdapter::on_message_mqtt(const std::string& topic, const std::string& message) {
    spdlog::debug("Received MQTT message on topic: {}", topic);
    std::string parsed_message = parseMessage(message);
    spdlog::debug("Parsed message: {}", parsed_message);
    dds_->publish("cps/objects", parsed_message);
}

std::string RadarAdapter::parseMessage(const std::string& input) {

    auto t1 = std::chrono::high_resolution_clock::now();
    Object obj;
    try {
        json j = json::parse(input);
        auto t2 = std::chrono::high_resolution_clock::now();

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

        auto t3 = std::chrono::high_resolution_clock::now();

        spdlog::debug("Parsing time: {} | Extracting time: {}", 
            std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count(),
            std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count());

    } catch (const nlohmann::json::type_error& e) {
        spdlog::error("Type error: {} | Input: {}", e.what(), input);
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("Parse error: {} | Input: {}", e.what(), input);
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error: {} | Input: {}", e.what(), input);
    }

    auto t4 = std::chrono::high_resolution_clock::now();
    // Convert Object struct to JSON
    json object = {
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

    json output = {
        {"objects", {object}}
    };

    auto t5 = std::chrono::high_resolution_clock::now();

    spdlog::debug("JSON Building time: {}", 
        std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count());

    return output.dump();
}

std::string RadarAdapter::getRandomNumberString() {
    int uniqueVar;
    std::default_random_engine generator(static_cast<unsigned int>(
        std::hash<std::size_t>{}(std::time(0)) ^ reinterpret_cast<std::size_t>(&uniqueVar)));
    std::uniform_int_distribution<int> distribution(0, 10000);
    return std::to_string(distribution(generator));
}