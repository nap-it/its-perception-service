#include "camera_adapter.h"
#include <chrono>
#include <functional>
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace rj = rapidjson;

CameraAdapter::CameraAdapter(const Config& config) : config(config) {
    if (config.debug) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug logging enabled.");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Info logging enabled.");
    }

    // Initialize DDS client.
    dds_ = new Dds("CameraAdapter", config.domain_id, on_message_dds);
    dds_->provision_publisher("cps/objects");
    dds_->provision_publisher("cps/sensors");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Build sensor information using RapidJSON.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 3, alloc);
    sensorDoc.AddMember("sensorType", 3, alloc);
    sensorDoc.AddMember("shadowingApplies", false, alloc);
    sensorDoc.AddMember("semiMajorRangeLength", 75, alloc);
    sensorDoc.AddMember("semiMinorRangeLength", 20, alloc);
    sensorDoc.AddMember("semiMajorRangeOrientation", 3061, alloc);
    sensorDoc.AddMember("range", 100, alloc);
    sensorDoc.AddMember("stationaryHorizontalOpeningAngleStart", 3601, alloc);
    sensorDoc.AddMember("stationaryHorizontalOpeningAngleEnd", 3601, alloc);
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

    int max_retries = 10, retries = 0;
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        spdlog::info("Waiting for MQTT connection, retrying...");
        if (retries++ > max_retries) {
            spdlog::error("Failed to connect to MQTT server, exiting...");
            exit(1);
        }
    }
}

void CameraAdapter::run() {
    spdlog::info("Camera Adapter started running...");

    // Publish sensor information every 5 seconds.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 1, alloc);
    sensorDoc.AddMember("sensorType", 1, alloc);
    sensorDoc.AddMember("shadowingApplies", false, alloc);
    sensorDoc.AddMember("semiMajorRangeLength", 75, alloc);
    sensorDoc.AddMember("semiMinorRangeLength", 20, alloc);
    sensorDoc.AddMember("semiMajorRangeOrientation", 3061, alloc);
    sensorDoc.AddMember("range", 100, alloc);
    sensorDoc.AddMember("stationaryHorizontalOpeningAngleStart", 3601, alloc);
    sensorDoc.AddMember("stationaryHorizontalOpeningAngleEnd", 3601, alloc);
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

void CameraAdapter::on_message_mqtt(const std::string& topic, const std::string& message) {
    spdlog::debug("Received MQTT message on topic: {}", topic);
    std::string parsed_message = parseMessage(message);
    spdlog::debug("Parsed message: {}", parsed_message);
    dds_->publish("cps/objects", parsed_message);
}

std::string CameraAdapter::parseMessage(const std::string& input) {

    auto t1 = std::chrono::high_resolution_clock::now();

    rj::Document doc;
    doc.Parse(input.c_str());
    if (doc.HasParseError()) {
        spdlog::error("Parse error in MQTT message: {}", input);
        return "";
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    if(doc.HasMember("listOfObjects") && doc["listOfObjects"].IsArray()) {

        // Build output JSON document.
        rj::Document outDoc;
        outDoc.SetObject();
        rj::Document::AllocatorType& allocOut = outDoc.GetAllocator();

        rj::Value objects(rj::kArrayType);

        for (rj::SizeType i = 0; i < doc["listOfObjects"].Size(); i++) {
            const rj::Value& rj_obj = doc["listOfObjects"][i];
            Object obj;
            obj.objectID = (rj_obj.HasMember("objectID") && rj_obj["objectID"].IsInt()) ? rj_obj["objectID"].GetInt() : -1;
            if (obj.objectID == -1) { spdlog::error("Mandatory (Object ID) not present in message: {}", input); return ""; }
            obj.sensorID = 3; // Hardcoded for Monovideo
            obj.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;
            obj.classification = (rj_obj.HasMember("classification") && rj_obj["classification"].IsInt()) ? rj_obj["classification"].GetInt() : 0;
            obj.confidence = (rj_obj.HasMember("confidence") && rj_obj["confidence"].IsInt()) ? rj_obj["confidence"].GetInt() : 0;
            obj.speed = (rj_obj.HasMember("speed") && rj_obj["speed"].IsFloat()) ? rj_obj["speed"].GetFloat() : 0.0f;
            obj.heading = (rj_obj.HasMember("heading") && rj_obj["heading"].IsFloat()) ? rj_obj["heading"].GetFloat() : 0.0f;
            obj.latitude = (rj_obj.HasMember("latitude") && rj_obj["latitude"].IsFloat()) ? rj_obj["latitude"].GetFloat() : 0.0f;
            if (obj.latitude == 0.0f) { spdlog::error("Mandatory (Latitude) not present in message: {}", input); return ""; }
            obj.longitude = (rj_obj.HasMember("longitude") && rj_obj["longitude"].IsFloat()) ? rj_obj["longitude"].GetFloat() : 0.0f;
            if (obj.longitude == 0.0f) { spdlog::error("Mandatory (Longitude) not present in message: {}", input); return ""; }
            
            rj::Value objVal(rj::kObjectType);
            objVal.AddMember("objectID", obj.objectID, allocOut);
            objVal.AddMember("sensorID", obj.sensorID, allocOut);
            objVal.AddMember("timestamp", obj.timestamp, allocOut);
            objVal.AddMember("classification", obj.classification, allocOut);
            objVal.AddMember("confidence", obj.confidence, allocOut);
            objVal.AddMember("speed", obj.speed, allocOut);
            objVal.AddMember("heading", obj.heading, allocOut);
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

std::string CameraAdapter::getRandomNumberString() {
    int uniqueVar;
    std::default_random_engine generator(static_cast<unsigned int>(
        std::hash<std::size_t>{}(std::time(0)) ^ reinterpret_cast<std::size_t>(&uniqueVar)));
    std::uniform_int_distribution<int> distribution(0, 10000);
    return std::to_string(distribution(generator));
}