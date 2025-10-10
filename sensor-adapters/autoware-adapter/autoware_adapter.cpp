#include "autoware_adapter.h"
#include <chrono>
#include <functional>
#include <sstream>
#include <iomanip>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace rj = rapidjson;

AutowareAdapter::AutowareAdapter(const Config& config) : config(config) {
    if (config.debug) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug logging enabled.");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Info logging enabled.");
    }

    // Initialize DDS client.
    dds_ = new Dds("AutowareCpmAdapter", config.domain_id, [this](std::string topic, std::string message) {
            this->on_message_dds(topic, message);
        });
    dds_->provision_publisher("generation/objects");
    dds_->provision_publisher("generation/sensors");
    dds_->subscribe("aw/out/perceived_objects");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Build sensor information using RapidJSON.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 2, alloc); // Hardcoded for Lidar
    sensorDoc.AddMember("sensorType", 2, alloc); // Hardcoded for Lidar
    sensorDoc.AddMember("shadowingApplies", false, alloc);
    rj::StringBuffer sensorBuffer;
    rj::Writer<rj::StringBuffer> sensorWriter(sensorBuffer);
    sensorDoc.Accept(sensorWriter);
    std::string sensorInfoStr = sensorBuffer.GetString();
    dds_->publish("generation/sensors", sensorInfoStr);
    spdlog::info("Sensor information published: {}", sensorInfoStr);
}

void AutowareAdapter::run() {
    spdlog::info("Autoware Adapter started running...");

    // Publish sensor information every 5 seconds.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 2, alloc);
    sensorDoc.AddMember("sensorType", 2, alloc);
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

void AutowareAdapter::on_message_dds(std::string topic, std::string message) {
    spdlog::debug("Received DDS message on topic: {}", topic);
    std::string parsed_message = parseMessage(message);
    spdlog::debug("Parsed message: {}", parsed_message);
    if(!parsed_message.empty()) dds_->publish("generation/objects", parsed_message);
}

std::string AutowareAdapter::parseMessage(const std::string& input) {

    auto t1 = std::chrono::high_resolution_clock::now();

    rj::Document doc;
    doc.Parse(input.c_str());
    if (doc.HasParseError()) {
        spdlog::error("Parse error in MQTT message: {}", input);
        return "";
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    if(doc.HasMember("objects") && doc["objects"].IsArray()) {

        // Build output JSON document.
        rj::Document outDoc;
        outDoc.SetObject();
        rj::Document::AllocatorType& allocOut = outDoc.GetAllocator();

        rj::Value objects(rj::kArrayType);

        for (rj::SizeType i = 0; i < doc["objects"].Size(); i++) {
            const rj::Value& rj_obj = doc["objects"][i];
            Object obj;
            obj.objectID = (rj_obj.HasMember("objID") && rj_obj["objID"].IsInt()) ? rj_obj["objID"].GetInt() : -1;
            if (obj.objectID == -1) { spdlog::error("Mandatory (Object ID) not present in message: {}", input); return ""; }
            obj.sensorID = 2; // Hardcoded for Lidar
            obj.timestamp = (rj_obj.HasMember("timestamp") && rj_obj["timestamp"].IsInt64()) ? rj_obj["timestamp"].GetInt64()/1000000.0 : 0.0;
            spdlog::debug("Input timestamp: {}, Output timestamp: {}", rj_obj["timestamp"].GetInt64(), obj.timestamp);
            if (obj.timestamp == 0.0) { spdlog::error("Mandatory (Timestamp) not present in message: {}", input); return ""; }
            obj.classification = (rj_obj.HasMember("classification") && rj_obj["classification"].IsInt()) ? rj_obj["classification"].GetInt() : 0;
            obj.confidence = (rj_obj.HasMember("confidence") && rj_obj["confidence"].IsInt()) ? rj_obj["confidence"].GetInt() : 0;
            obj.speed = (rj_obj.HasMember("speed") && rj_obj["speed"].IsFloat()) ? rj_obj["speed"].GetFloat() : 0.0f;
            obj.cov_speed = (rj_obj.HasMember("cov_speed") && rj_obj["cov_speed"].IsFloat()) ? rj_obj["cov_speed"].GetFloat() : 0.0f;
            obj.heading = (rj_obj.HasMember("heading") && rj_obj["heading"].IsFloat()) ? rj_obj["heading"].GetFloat() : 0.0f;
            obj.cov_heading = (rj_obj.HasMember("cov_heading") && rj_obj["cov_heading"].IsFloat()) ? rj_obj["cov_heading"].GetFloat() : 0.0f;
            obj.latitude = (rj_obj.HasMember("latitude") && rj_obj["latitude"].IsFloat()) ? rj_obj["latitude"].GetFloat() : 0.0f;
            if (obj.latitude == 0.0f) { spdlog::error("Mandatory (Latitude) not present in message: {}", input); return ""; }
            obj.cov_latitude = (rj_obj.HasMember("cov_y") && rj_obj["cov_y"].IsFloat()) ? rj_obj["cov_y"].GetFloat() : 0.0f;
            obj.longitude = (rj_obj.HasMember("longitude") && rj_obj["longitude"].IsFloat()) ? rj_obj["longitude"].GetFloat() : 0.0f;
            if (obj.longitude == 0.0f) { spdlog::error("Mandatory (Longitude) not present in message: {}", input); return ""; }
            obj.cov_longitude = (rj_obj.HasMember("cov_x") && rj_obj["cov_x"].IsFloat()) ? rj_obj["cov_x"].GetFloat() : 0.0f;
            obj.altitude = (rj_obj.HasMember("z") && rj_obj["z"].IsFloat()) ? rj_obj["z"].GetFloat() : 0.0f;
            obj.cov_altitude = (rj_obj.HasMember("cov_z") && rj_obj["cov_z"].IsFloat()) ? rj_obj["cov_z"].GetFloat() : 0.0f;
            obj.size_x = (rj_obj.HasMember("size_x") && rj_obj["size_x"].IsFloat()) ? rj_obj["size_x"].GetFloat() : 0.0f;
            obj.size_y = (rj_obj.HasMember("size_y") && rj_obj["size_y"].IsFloat()) ? rj_obj["size_y"].GetFloat() : 0.0f;
            obj.size_z = (rj_obj.HasMember("size_z") && rj_obj["size_z"].IsFloat()) ? rj_obj["size_z"].GetFloat() : 0.0f;
            obj.angular_velocity = (rj_obj.HasMember("twist_angz") && rj_obj["twist_angz"].IsFloat()) ? rj_obj["twist_angz"].GetFloat() : 0.0f;
            obj.cov_angular_velocity = (rj_obj.HasMember("cov_twist_angz") && rj_obj["cov_twist_angz"].IsFloat()) ? rj_obj["cov_twist_angz"].GetFloat() : 0.0f;
            obj.acceleration = 0.0f; // Not available
            
            rj::Value objVal(rj::kObjectType);
            objVal.AddMember("objectID", obj.objectID, allocOut);
            objVal.AddMember("sensorID", obj.sensorID, allocOut);
            objVal.AddMember("timestamp", obj.timestamp, allocOut);
            objVal.AddMember("classification", obj.classification, allocOut);
            objVal.AddMember("confidence", obj.confidence, allocOut);
            objVal.AddMember("speed", obj.speed, allocOut);
            objVal.AddMember("heading", obj.heading, allocOut);
            //objVal.AddMember("acceleration", obj.acceleration, allocOut);
            objVal.AddMember("latitude", obj.latitude, allocOut);
            objVal.AddMember("longitude", obj.longitude, allocOut);
            objVal.AddMember("altitude", obj.altitude, allocOut);
            objVal.AddMember("size_x", obj.size_x, allocOut);
            objVal.AddMember("size_y", obj.size_y, allocOut);
            objVal.AddMember("size_z", obj.size_z, allocOut);
            objVal.AddMember("angular_velocity", obj.angular_velocity, allocOut);
            objVal.AddMember("cov_latitude", obj.cov_latitude, allocOut);
            objVal.AddMember("cov_longitude", obj.cov_longitude, allocOut);
            objVal.AddMember("cov_altitude", obj.cov_altitude, allocOut);
            objVal.AddMember("cov_heading", obj.cov_heading, allocOut);
            objVal.AddMember("cov_speed", obj.cov_speed, allocOut);
            objVal.AddMember("cov_angular_velocity", obj.cov_angular_velocity, allocOut);
            
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