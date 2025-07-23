#include "bike_adapter.h"
#include <chrono>
#include <functional>
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace rj = rapidjson;

BikeAdapter::BikeAdapter(const Config& config) : config(config) {
    if (config.debug) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug logging enabled.");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Info logging enabled.");
    }

    // Initialize DDS client.
    dds_ = new Dds("BikeAdapter", config.domain_id, on_message_dds);
    dds_->provision_publisher("generation/objects");
    dds_->provision_publisher("generation/sensors");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Build sensor information using RapidJSON.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 3, alloc);
    sensorDoc.AddMember("sensorType", 3, alloc);
    sensorDoc.AddMember("shadowingApplies", false, alloc);
    rj::StringBuffer sensorBuffer;
    rj::Writer<rj::StringBuffer> sensorWriter(sensorBuffer);
    sensorDoc.Accept(sensorWriter);
    std::string sensorInfoStr = sensorBuffer.GetString();
    dds_->publish("generation/sensors", sensorInfoStr);
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

void BikeAdapter::run() {
    spdlog::info("Bike Adapter started running...");

    // Publish sensor information every 5 seconds.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 3, alloc);
    sensorDoc.AddMember("sensorType", 3, alloc);
    sensorDoc.AddMember("shadowingApplies", false, alloc);
    rj::StringBuffer sensorBuffer;
    rj::Writer<rj::StringBuffer> sensorWriter(sensorBuffer);
    sensorDoc.Accept(sensorWriter);
    std::string sensorInfoStr = sensorBuffer.GetString();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        dds_->publish("generation/sensors", sensorInfoStr);
        spdlog::info("Sensor information published: {}", sensorInfoStr);
    }
}

void BikeAdapter::on_message_mqtt(const std::string& topic, const std::string& message) {
    spdlog::debug("Received MQTT message on topic: {}", topic);
    std::string parsed_message = parseMessage(message);
    spdlog::debug("Parsed message: {}", parsed_message);
    dds_->publish("generation/objects", parsed_message);
}

std::string BikeAdapter::parseMessage(const std::string& input) {

    auto t1 = std::chrono::high_resolution_clock::now();

    rj::Document doc;
    doc.Parse(input.c_str());
    if (doc.HasParseError()) {
        spdlog::error("Parse error in MQTT message: {}", input);
        return "";
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    if(doc.HasMember("detections") && doc["detections"].IsArray()) {

        // Build output JSON document.
        rj::Document outDoc;
        outDoc.SetObject();
        rj::Document::AllocatorType& allocOut = outDoc.GetAllocator();

        rj::Value objects(rj::kArrayType);

        for (rj::SizeType i = 0; i < doc["detections"].Size(); i++) {
            const rj::Value& rj_obj = doc["detections"][i];
            Object obj;
            obj.objectID = (rj_obj.HasMember("object_id") && rj_obj["object_id"].IsInt()) ? rj_obj["object_id"].GetInt() : -1;
            if (obj.objectID == -1) { spdlog::error("Mandatory (Object ID) not present in message: {}", input); return ""; }
            obj.sensorID = 3; // Hardcoded for Monovideo from bike
            obj.timestamp = (rj_obj.HasMember("timestamp") && rj_obj["timestamp"].IsDouble()) ? rj_obj["timestamp"].GetDouble() : 0.0;
            obj.classification = (rj_obj.HasMember("class") && rj_obj["class"].IsInt()) ? rj_obj["class"].GetInt() : 0;
            obj.confidence = (rj_obj.HasMember("confidence") && rj_obj["confidence"].IsInt()) ? rj_obj["confidence"].GetInt() : 0;
            obj.latitude = (rj_obj.HasMember("lat") && rj_obj["lat"].IsFloat()) ? rj_obj["lat"].GetFloat() : 0.0f;
            if (obj.latitude == 0.0f) { spdlog::error("Mandatory (Latitude) not present in message: {}", input); return ""; }
            obj.longitude = (rj_obj.HasMember("lon") && rj_obj["lon"].IsFloat()) ? rj_obj["lon"].GetFloat() : 0.0f;
            if (obj.longitude == 0.0f) { spdlog::error("Mandatory (Longitude) not present in message: {}", input); return ""; }
            
            rj::Value objVal(rj::kObjectType);
            objVal.AddMember("objectID", obj.objectID, allocOut);
            objVal.AddMember("sensorID", obj.sensorID, allocOut);
            objVal.AddMember("timestamp", obj.timestamp, allocOut);
            objVal.AddMember("classification", obj.classification, allocOut);
            objVal.AddMember("confidence", obj.confidence, allocOut);
            objVal.AddMember("latitude", obj.latitude, allocOut);
            objVal.AddMember("longitude", obj.longitude, allocOut);
    
            objects.PushBack(objVal, allocOut);
        }

        outDoc.AddMember("objects", objects, allocOut);

        auto t3 = std::chrono::high_resolution_clock::now();
        
        rj::StringBuffer buffer;
        rj::Writer<rj::StringBuffer> writer(buffer);
        outDoc.Accept(writer);
        std::string output = buffer.GetString();

        auto t4 = std::chrono::high_resolution_clock::now();

        spdlog::debug("Parse time: {}, Extract & Build time: {}, Serialize time: {}, Total time: {}", 
            std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count(),
            std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count(),
            std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count(),
            std::chrono::duration_cast<std::chrono::microseconds>(t4 - t1).count());

        return output;
    } else {
        spdlog::warn("No objects array found in message: {}", input);
        return "";
    }
}

std::string BikeAdapter::getRandomNumberString() {
    int uniqueVar;
    std::default_random_engine generator(static_cast<unsigned int>(
        std::hash<std::size_t>{}(std::time(0)) ^ reinterpret_cast<std::size_t>(&uniqueVar)));
    std::uniform_int_distribution<int> distribution(0, 10000);
    return std::to_string(distribution(generator));
}