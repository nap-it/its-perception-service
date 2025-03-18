#include "radar_adapter.h"
#include <chrono>
#include <functional>
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace rj = rapidjson;

RadarAdapter::RadarAdapter(const Config& config) : config(config) {
    if (config.debug) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug logging enabled.");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Info logging enabled.");
    }

    // Initialize DDS client.
    dds_ = new Dds("RadarAdapter", config.domain_id, on_message_dds);
    dds_->provision_publisher("cps/objects");
    dds_->provision_publisher("cps/sensors");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Build sensor information using RapidJSON.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 1, alloc);
    sensorDoc.AddMember("sensorType", 1, alloc);
    sensorDoc.AddMember("shadowingApplies", false, alloc);
    // sensorDoc.AddMember("semiMajorRangeLength", 75, alloc);
    // sensorDoc.AddMember("semiMinorRangeLength", 20, alloc);
    // sensorDoc.AddMember("semiMajorRangeOrientation", 3061, alloc);
    // sensorDoc.AddMember("range", 100, alloc);
    // sensorDoc.AddMember("stationaryHorizontalOpeningAngleStart", 3601, alloc);
    // sensorDoc.AddMember("stationaryHorizontalOpeningAngleEnd", 3601, alloc);
    rj::StringBuffer sensorBuffer;
    rj::Writer<rj::StringBuffer> sensorWriter(sensorBuffer);
    sensorDoc.Accept(sensorWriter);
    std::string sensorInfoStr = sensorBuffer.GetString();
    dds_->publish("cps/sensors", sensorInfoStr);
    spdlog::info("Sensor information published: {}", sensorInfoStr);

    // MQTT configuration.
    data_mqtt_server mqttInfo;
    mqttInfo.address = "tcp://" + config.mqtt_host + ":" + std::to_string(config.mqtt_port);
    mqttInfo.client_id = config.mqtt_client_id + "-" + std::to_string(config.domain_id) + getRandomNumberString();
    mqttInfo.subscription_topic.push_back(config.mqtt_topic);

    mqtt_wrapper = new MqttWrapper(mqttInfo, [this](const std::string& topic, const std::string& message) {
        this->on_message_mqtt(topic, message);
    });

    while(!mqtt_wrapper->is_connected()){
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        spdlog::info("Waiting for MQTT connection, retrying...");
    }
    spdlog::info("[Locator] MQTT client connected to broker {} on topic {}", mqttInfo.address, config.mqtt_topic);
}

void RadarAdapter::run() {
    spdlog::info("Radar Adapter started running...");

    // Publish sensor information every 5 seconds.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 1, alloc);
    sensorDoc.AddMember("sensorType", 1, alloc);
    sensorDoc.AddMember("shadowingApplies", false, alloc);
    // sensorDoc.AddMember("semiMajorRangeLength", 75, alloc);
    // sensorDoc.AddMember("semiMinorRangeLength", 20, alloc);
    // sensorDoc.AddMember("semiMajorRangeOrientation", 3061, alloc);
    // sensorDoc.AddMember("range", 100, alloc);
    // sensorDoc.AddMember("stationaryHorizontalOpeningAngleStart", 3601, alloc);
    // sensorDoc.AddMember("stationaryHorizontalOpeningAngleEnd", 3601, alloc);
    rj::StringBuffer sensorBuffer;
    rj::Writer<rj::StringBuffer> sensorWriter(sensorBuffer);
    sensorDoc.Accept(sensorWriter);
    std::string sensorInfoStr = sensorBuffer.GetString();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        dds_->publish("cps/sensors", sensorInfoStr);
        spdlog::info("Sensor information published: {}", sensorInfoStr);
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

    rj::Document doc;
    doc.Parse(input.c_str());
    if (doc.HasParseError()) {
        spdlog::error("Parse error in MQTT message: {}", input);
        return "";
    }
    
    auto t2 = std::chrono::high_resolution_clock::now();

    Object obj;
    obj.objectID = (doc.HasMember("objectID") && doc["objectID"].IsInt()) ? doc["objectID"].GetInt() : -1;
    if (obj.objectID == -1) { spdlog::error("Mandatory (Object ID) not present in message: {}", input); return ""; }
    obj.sensorID = 1; // Hardcoded for Radar.
    obj.timestamp = (doc.HasMember("timestamp") && doc["timestamp"].IsNumber()) ? doc["timestamp"].GetDouble() : 0.0;
    if (obj.timestamp == 0.0) { spdlog::error("Mandatory (Timestamp) not present in message: {}", input); return ""; }
    obj.classification = (doc.HasMember("classification") && doc["classification"].IsInt()) ? doc["classification"].GetInt() : 0;
    obj.confidence = (doc.HasMember("confidence") && doc["confidence"].IsInt()) ? doc["confidence"].GetInt() : 0;
    obj.speed = (doc.HasMember("speed") && doc["speed"].IsNumber()) ? static_cast<float>(doc["speed"].GetDouble()) : 0.0f;
    obj.heading = (doc.HasMember("heading") && doc["heading"].IsNumber()) ? static_cast<float>(doc["heading"].GetDouble()) : 0.0f;
    obj.acceleration = (doc.HasMember("acceleration") && doc["acceleration"].IsNumber()) ? static_cast<float>(doc["acceleration"].GetDouble()) : 0.0f;
    obj.latitude = (doc.HasMember("latitude") && doc["latitude"].IsNumber()) ? static_cast<float>(doc["latitude"].GetDouble()) : 0.0f;
    if (obj.latitude == 0.0f) { spdlog::error("Mandatory (Latitude) not present in message: {}", input); return ""; }
    obj.longitude = (doc.HasMember("longitude") && doc["longitude"].IsNumber()) ? static_cast<float>(doc["longitude"].GetDouble()) : 0.0f;
    if (obj.longitude == 0.0f) { spdlog::error("Mandatory (Longitude) not present in message: {}", input); return ""; }
    obj.size_x = (doc.HasMember("length") && doc["length"].IsNumber()) ? static_cast<float>(doc["length"].GetDouble()) : 0.0f;
    
    auto t3 = std::chrono::high_resolution_clock::now();

    // Build output JSON document.
    rj::Document outDoc;
    outDoc.SetObject();
    rj::Document::AllocatorType& allocOut = outDoc.GetAllocator();
    
    rj::Value objects(rj::kArrayType);
    rj::Value objVal(rj::kObjectType);
    objVal.AddMember("objectID", obj.objectID, allocOut);
    objVal.AddMember("sensorID", obj.sensorID, allocOut);
    objVal.AddMember("timestamp", obj.timestamp, allocOut);
    objVal.AddMember("classification", obj.classification, allocOut);
    objVal.AddMember("confidence", obj.confidence, allocOut);
    objVal.AddMember("speed", obj.speed, allocOut);
    objVal.AddMember("heading", obj.heading, allocOut);
    objVal.AddMember("acceleration", obj.acceleration, allocOut);
    objVal.AddMember("latitude", obj.latitude, allocOut);
    objVal.AddMember("longitude", obj.longitude, allocOut);
    objVal.AddMember("size_x", obj.size_x, allocOut);
    
    objects.PushBack(objVal, allocOut);
    outDoc.AddMember("objects", objects, allocOut);

    auto t4 = std::chrono::high_resolution_clock::now();
    
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    outDoc.Accept(writer);
    std::string output = buffer.GetString();

    auto t5 = std::chrono::high_resolution_clock::now();

    spdlog::debug("Parse time: {}, Extract time: {}, Build time: {}, Serialize time: {}, Total time: {}", 
        std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(t5 - t1).count());

    return output;
}

std::string RadarAdapter::getRandomNumberString() {
    int uniqueVar;
    std::default_random_engine generator(static_cast<unsigned int>(
        std::hash<std::size_t>{}(std::time(0)) ^ reinterpret_cast<std::size_t>(&uniqueVar)));
    std::uniform_int_distribution<int> distribution(0, 10000);
    return std::to_string(distribution(generator));
}