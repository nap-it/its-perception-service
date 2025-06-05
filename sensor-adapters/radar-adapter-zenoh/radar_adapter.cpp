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

    // Initialize Zenoh client
    zenoh::Config zconfig = zenoh::Config::create_default();
    zenoh::ZResult *err = nullptr;
    zconfig.insert_json5("mode", "\"peer\"", err);
    if (!config.zenoh_endpoint.empty()) {
        zconfig.insert_json5("connect/endpoints", fmt::format("[\"tcp/{}:7447\"]", config.zenoh_endpoint), err);
    } 
    zconfig.insert_json5("connect/retry", "{\"interval_ms\":1000,\"max_retries\":-1}", err);
    zconfig.insert_json5("connect/exit_on_failure", "false", err);
    zconfig.insert_json5("open/return_conditions/connect_scouted", "true", err);
    if (err) {
        spdlog::error("[Aggregator] Error in Zenoh configuration: {}", static_cast<const void*>(err));
    } else {
        spdlog::debug("[Aggregator] Zenoh configuration: {}", zconfig.to_string());
    }

    spdlog::info("[Aggregator] Opening Zenoh session ...");
    auto tmp_sess = zenoh::Session::open(std::move(zconfig));
    session_ = new zenoh::Session(std::move(tmp_sess));
    spdlog::info("[Aggregator] Zenoh session opened successfully.");
    
    zenoh::KeyExpr topic_objs("cps/objects");
    zenoh::KeyExpr topic_sensors("cps/sensors");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto pub_objs = session_->declare_publisher(topic_objs);
    auto pub_sens = session_->declare_publisher(topic_sensors);
    publisher_objects_ = new zenoh::Publisher(std::move(pub_objs));
    publisher_sensors_ = new zenoh::Publisher(std::move(pub_sens));

    //Initialize Zenoh shared memory provider.
    static constexpr auto SHM_SIZE  = 1024U * 1024U * 10U;
    static constexpr auto SHM_ALIGN = 2U;
    zenoh::MemoryLayout layout(SHM_SIZE, zenoh::AllocAlignment({SHM_ALIGN}));
    shm_provider_ = new zenoh::PosixShmProvider(layout);

    // Build sensor information using RapidJSON.
    rj::Document sensorDoc;
    sensorDoc.SetObject();
    rj::Document::AllocatorType& alloc = sensorDoc.GetAllocator();
    sensorDoc.AddMember("sensorID", 1, alloc);
    sensorDoc.AddMember("sensorType", 1, alloc);
    sensorDoc.AddMember("shadowingApplies", false, alloc);
    rj::StringBuffer sensorBuffer;
    rj::Writer<rj::StringBuffer> sensorWriter(sensorBuffer);
    sensorDoc.Accept(sensorWriter);
    std::string sensorInfoStr = sensorBuffer.GetString();
    
    if (config.shared_memory){
        const size_t payload_len = sensorInfoStr.size();
        auto alloc_result = shm_provider_->alloc_gc_defrag_blocking(payload_len, zenoh::AllocAlignment({0}));
        zenoh::ZShmMut&& buf = std::get<zenoh::ZShmMut>(std::move(alloc_result));
        memcpy(buf.data(), sensorInfoStr.data(), payload_len);
        publisher_sensors_->put(std::move(buf));
    } else {
        publisher_sensors_->put(zenoh::Bytes(sensorInfoStr));
    }
    
    spdlog::info("Sensor information published: {}", sensorInfoStr);

    // MQTT configuration.
    data_mqtt_server mqttInfo;
    mqttInfo.address = "tcp://" + config.mqtt_host + ":" + std::to_string(config.mqtt_port);
    mqttInfo.client_id = config.mqtt_client_id + "-" + getRandomNumberString();
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
    rj::StringBuffer sensorBuffer;
    rj::Writer<rj::StringBuffer> sensorWriter(sensorBuffer);
    sensorDoc.Accept(sensorWriter);
    std::string sensorInfoStr = sensorBuffer.GetString();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (config.shared_memory){
            const size_t payload_len = sensorInfoStr.size();
            auto alloc_result = shm_provider_->alloc_gc_defrag_blocking(payload_len, zenoh::AllocAlignment({0}));
            zenoh::ZShmMut&& buf = std::get<zenoh::ZShmMut>(std::move(alloc_result));
            memcpy(buf.data(), sensorInfoStr.data(), payload_len);
            publisher_sensors_->put(std::move(buf));
        } else {
            publisher_sensors_->put(zenoh::Bytes(sensorInfoStr));
        }

        spdlog::debug("Sensor information published: {}", sensorInfoStr);
        {
            std::lock_guard<std::mutex> lock(counter_mutex_);
            spdlog::info("Received {} radar messages in the last 5 seconds", message_count_);
            message_count_ = 0;
        }
    }
}

void RadarAdapter::on_message_mqtt(const std::string& topic, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(counter_mutex_);
        message_count_++;
    }
    spdlog::debug("Received MQTT message on topic: {}", topic);
    std::string parsed_message = parseMessage(message);
    spdlog::debug("Parsed message: {}", parsed_message);
    if (config.shared_memory){
        const size_t payload_len = parsed_message.size();
        auto alloc_result = shm_provider_->alloc_gc_defrag_blocking(payload_len, zenoh::AllocAlignment({0}));
        zenoh::ZShmMut&& buf = std::get<zenoh::ZShmMut>(std::move(alloc_result));
        memcpy(buf.data(), parsed_message.data(), payload_len);
        publisher_objects_->put(std::move(buf));
        spdlog::debug("Published object data to Zenoh using shared memory");
    } else {
        publisher_objects_->put(zenoh::Bytes(parsed_message));
        spdlog::debug("Published object data to Zenoh");
    }
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