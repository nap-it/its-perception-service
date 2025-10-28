#include "processor.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace rj = rapidjson;
namespace fs = std::filesystem;

Processor::Processor(const Config& config, std::shared_ptr<Locator> locator, bool performanceLogs, bool prometheus, ProcMetricHandles* metrics)
    : config_(config), locator_(locator), performanceLogs_(performanceLogs), 
        prometheus_(prometheus), metrics_(metrics),
        stopFlag_(false) {
    
    if (config.debug) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    // File logger
    if (!fs::exists("/logs")) {
        fs::create_directory("/logs");
    } else if (fs::exists("/logs/processor.csv")) {
        fs::remove("/logs/processor.csv");
    }

    // Logger initialization
    if(performanceLogs_) {
        processor_file_logger_ = spdlog::basic_logger_mt("processor_logger", "/logs/processor.csv");
        processor_file_logger_->set_pattern("%v");
        processor_file_logger_->flush_on(spdlog::level::info);
    }

    // Set up Local MQTT client.
    if(config.local_mqtt_enabled) {
        data_mqtt_server mqttConfig;
        mqttConfig.address = "tcp://" + config.local_mqtt_host + ":" + std::to_string(config.local_mqtt_port);
        mqttConfig.client_id = "Processor-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        local_mqtt_client_ = new MqttWrapper(mqttConfig);
        
        while(!local_mqtt_client_->is_connected()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        spdlog::info("[Processor] Local MQTT client connected to broker {}", mqttConfig.address);
    }

    // Set up Remote MQTT client.
    if(config.remote_mqtt_enabled) {
        data_mqtt_server mqttConfig;
        mqttConfig.address = "tcp://" + config.remote_mqtt_host + ":" + std::to_string(config.remote_mqtt_port);
        mqttConfig.client_id = "Processor-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        mqttConfig.user_name = config.remote_mqtt_username;
        mqttConfig.password = config.remote_mqtt_password;

        remote_mqtt_client_ = new MqttWrapper(mqttConfig);
        
        while(!remote_mqtt_client_->is_connected()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        spdlog::info("[Processor] Remote MQTT client connected to broker {}", mqttConfig.address);
    }

    // Set up DDS client.
    dds_ = new Dds("Processor", config.dds_domain, [this](const std::string& topic, const std::string& message){
        this->on_message_dds(topic, message);
    });
    dds_->provision_publisher(config.dds_output_topic);
    std::vector<std::string> topics;
    std::istringstream ss(config.cpm_topics); // assume config.cpm_topic contains comma-separated topics
    std::string token;
    while (std::getline(ss, token, ',')) {
        topics.push_back(token);
    }
    for (const auto& topic : topics) {
        if (topic != "") {
            if (topic == "vanetza/out/cpm" || topic == "cps-v2/in/cpm" || topic == "vanetza/in/cpm") {
                dds_->subscribe(topic);
                spdlog::info("[Processor] Subscribing to topic: {}", topic);
            } else {
                spdlog::warn("[Processor] Unknown topic: {}", topic);
                spdlog::warn("[Processor] Only 'vanetza/out/cpm', 'cps-v2/in/cpm' and 'vanetza/in/cpm' are supported");
            }
        }
    }
    spdlog::info("[Processor] DDS client subscribed to topics {}", config.cpm_topics);

    // Initialize Zenoh client
    zenoh::ZResult *err = nullptr;
    spdlog::info("[Processor] Initializing Zenoh client ...");
    zenoh::Config zconfig = zenoh::Config::from_file("./zenoh_config.json5", err); 
    if(!config.zenoh_endpoint.empty()) {
        zconfig.insert_json5("connect/endpoints", "[\"tcp/" + config.zenoh_endpoint + ":7447\"]", err);
    } else {
        spdlog::debug("[Processor] No Zenoh endpoint configured, using default configuration.");
    }

    if (err) {
        spdlog::error("[Processor] Error in Zenoh configuration: {}", static_cast<const void*>(err));
    } else {
        spdlog::debug("[Processor] Zenoh configuration: {}", zconfig.to_string());
    }
    
    spdlog::info("[Processor] Opening Zenoh session ...");
    session_ = new zenoh::Session(std::move(zenoh::Session::open(std::move(zconfig))));
    spdlog::info("[Processor] Zenoh session opened successfully.");

    session_->declare_publisher(config.zenoh_output_topic);

    //Initialize Zenoh shared memory provider with 10 MB size and 2-byte alignment.
    static constexpr auto SHM_SIZE  = 1024U * 1024U * 10U;
    static constexpr auto SHM_ALIGN = 2U;
    zenoh::MemoryLayout layout(SHM_SIZE, zenoh::AllocAlignment({SHM_ALIGN}));
    shm_provider_ = new zenoh::PosixShmProvider(layout);

}

Processor::~Processor() {
    delete dds_;
    delete local_mqtt_client_;
    delete remote_mqtt_client_;
}

void Processor::run() {
    stopFlag_ = false;
    spdlog::info("[Processor] Running...");
    while (!stopFlag_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    spdlog::info("[Processor] Stopped.");
}

void Processor::on_message_dds(const std::string& topic, const std::string& message) {
    
    std::chrono::time_point<std::chrono::high_resolution_clock> t0 = std::chrono::high_resolution_clock::now();

    spdlog::debug("[Processor] Received message on topic {}: {}", topic, message);

    if (topic == "vanetza/out/cpm" || topic == "cps-v2/in/cpm" || topic == "vanetza/in/cpm") {
        spdlog::debug("[Processor] Processing CPM...");
        std::string output = "";
        this->processCPM(topic, message, output, t0);

        if (output.empty()) {
            spdlog::warn("[Processor] Empty output message");
            return;
        } else {
            spdlog::debug("[Processor] /objects message: {}", output);
        }
        
        try {
            if (dds_) {
                dds_->publish(config_.dds_output_topic, output);
                spdlog::info("[Processor] Published DDS message on topic {}", config_.dds_output_topic);
            } else {
                spdlog::error("[Processor] DDS client is not available");
            }
        } catch (const std::exception& e) {
            spdlog::error("[Processor] Exception while publishing DDS message: {}", e.what());
        }

        try {
            if (config_.local_mqtt_enabled && local_mqtt_client_) {
                local_mqtt_client_->publish(config_.local_mqtt_output_topic, output);
                spdlog::info("[Processor] Published Local MQTT message on topic {}", config_.local_mqtt_output_topic);
                if (metrics_ && prometheus_) {
                    metrics_->pub_messages->Increment();
                }
            } else if (config_.local_mqtt_enabled) {
                spdlog::error("[Processor] Local MQTT client is not available");
            }
        } catch (const std::exception& e) {
            spdlog::error("[Processor] Exception while publishing Local MQTT message: {}", e.what());
        }

        try {
            if (config_.remote_mqtt_enabled && remote_mqtt_client_) {
                remote_mqtt_client_->publish(config_.remote_mqtt_output_topic, output);
                spdlog::info("[Processor] Published Remote MQTT message on topic {}", config_.remote_mqtt_output_topic);
            } else if (config_.remote_mqtt_enabled) {
                spdlog::error("[Processor] Remote MQTT client is not available");
            }
        } catch (const std::exception& e) {
            spdlog::error("[Processor] Exception while publishing Remote MQTT message: {}", e.what());
        }

        try {
            if (session_) {
                const size_t output_len = output.size();
                auto output_alloc_result = shm_provider_->alloc_gc_defrag_blocking(output_len, zenoh::AllocAlignment({0}));
                zenoh::ZShmMut&& output_buf = std::get<zenoh::ZShmMut>(std::move(output_alloc_result));
                memcpy(output_buf.data(), output.data(), output_len);
                session_->put(config_.zenoh_output_topic, std::move(output_buf));
                spdlog::info("[Processor] Published Zenoh message on topic {}", config_.zenoh_output_topic);
            } else {
                spdlog::error("[Processor] Zenoh session is not available");
            }
        } catch (const std::exception& e) {
            spdlog::error("[Processor] Exception while publishing Zenoh message: {}", e.what());
        }

    } else {
        spdlog::warn("[Processor] Received message on unknown topic: {}", topic);
    }
}

void Processor::processCPM(const std::string& topic, const std::string& message, std::string& output_full, std::chrono::time_point<std::chrono::high_resolution_clock> message_reception){
    try {
        auto t1 = std::chrono::high_resolution_clock::now();

        rj::Document doc;
        doc.Parse(message.c_str());
        if (doc.HasParseError()) {
            spdlog::error("[Processor] Parse error in CPM: {}", message);
            return;
        }

        rj::Document cpm;
        int sender_id = NOT_PRESENT_INT;
        int sender_type = 5;
        int receiver_id = NOT_PRESENT_INT;
        int receiver_type = NOT_PRESENT_INT;

        // Station data and CPM
        if (topic == "vanetza/out/cpm"){
            spdlog::debug("[Processor] Processing VANETZA CPM from vanetza/out/cpm ...");
            sender_id = (doc.HasMember("stationID") && doc["stationID"].IsInt()) ? doc["stationID"].GetInt() : NOT_PRESENT_INT;
            receiver_id = (doc.HasMember("receiverID") && doc["receiverID"].IsInt()) ? doc["receiverID"].GetInt() : NOT_PRESENT_INT;
            receiver_type = (doc.HasMember("receiverType") && doc["receiverType"].IsInt()) ? doc["receiverType"].GetInt() : NOT_PRESENT_INT;
            if (doc.HasMember("fields") && doc["fields"].IsObject() && doc["fields"].HasMember("payload") && doc["fields"]["payload"].IsObject()) {
                cpm.CopyFrom(doc["fields"]["payload"], cpm.GetAllocator());
            } else {
                spdlog::error("[Processor] CPM message does not have fields/payload");
                return;
            }
            spdlog::debug("[Processor] Sender ID: {}, Sender Type: {}, Receiver ID: {}, Receiver Type: {}", sender_id, sender_type, receiver_id, receiver_type);
        } else if (topic == "cps-v2/in/cpm" || topic == "vanetza/in/cpm") {
            spdlog::debug("[Processor] Processing CPM from cps-v2/in/cpm or vanetza/in/cpm ...");
            sender_id = config_.host_station_id;
            sender_type = config_.host_station_type;
            receiver_id = config_.host_station_id;
            receiver_type = config_.host_station_type;
            cpm.CopyFrom(doc, cpm.GetAllocator());
        } else {
            spdlog::warn("[Processor] Unknown CPM topic: {}", topic);
            return;
        }

        // Timestamps
        unsigned long cpm_reference_time = NOT_PRESENT_INT;
        double cpm_reference_time_double = NOT_PRESENT_DOUBLE;
        if (cpm.HasMember("managementContainer") && cpm["managementContainer"].HasMember("referenceTime")) {
            cpm_reference_time_double = cpm["managementContainer"]["referenceTime"].GetDouble();
            cpm_reference_time = static_cast<unsigned long>(cpm_reference_time_double * 1000.0);
            spdlog::debug("[Processor] CPM referenceTime: {} ms", cpm_reference_time);
        } else {
            spdlog::error("[Processor] CPM does not have referenceTime");
            return;
        }
        double cpm_reference_time_unix = static_cast<double>(cpm_reference_time + TIME_2004_MS) / 1000.0;
        unsigned long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        unsigned long local_gen_delta_time = now - TIME_2004_MS;
        unsigned long age_cpm = local_gen_delta_time - cpm_reference_time;
        if (local_gen_delta_time < cpm_reference_time) {
            spdlog::error("[Processor] CPM referenceTime is in the future: local {} < cpm {}", local_gen_delta_time, cpm_reference_time);
            return;
        } else {
            spdlog::debug("[Processor] CPM age: {} ms", age_cpm);
        }

        int num_containers = cpm["cpmContainers"].GetArray().Size();
        if (num_containers == 0) {
            spdlog::error("[Processor] No containers received!");
            return;
        }

        // Container data
        rj::Document perceivedObjectContainer;
        bool hasPerceivedObjectContainer = false;
        auto allocator = perceivedObjectContainer.GetAllocator();
        
        for (int i = 0; i < num_containers; i++) {
            if (cpm["cpmContainers"][i]["containerId"].GetInt() == 5) {
                perceivedObjectContainer.CopyFrom(cpm["cpmContainers"][i], allocator);
                hasPerceivedObjectContainer = true;
            }
            if (cpm["cpmContainers"][i]["containerId"].GetInt() == 2) {
                sender_type = 15;
            }
        }

        float sender_latitude = (cpm.HasMember("managementContainer") && cpm["managementContainer"].HasMember("referencePosition") && cpm["managementContainer"]["referencePosition"].HasMember("latitude"))
            ? static_cast<float>(cpm["managementContainer"]["referencePosition"]["latitude"].GetFloat()) : NOT_PRESENT_FLOAT;
        float sender_longitude = (cpm.HasMember("managementContainer") && cpm["managementContainer"].HasMember("referencePosition") && cpm["managementContainer"]["referencePosition"].HasMember("longitude"))
            ? static_cast<float>(cpm["managementContainer"]["referencePosition"]["longitude"].GetFloat()) : NOT_PRESENT_FLOAT;
        
        SenderInfo sender_info = locator_->getStationData(sender_id);

        spdlog::debug("[Processor] Getting station data for sender ID {}: type {}, lat {}, lon {}, speed {}, heading {}, altitude {}, acceleration {}",
                      sender_info.station_id, sender_type, sender_latitude, sender_longitude, sender_info.speed, sender_info.heading, sender_info.altitude, sender_info.acceleration);

        rj::Document objects_output_full;
        objects_output_full.SetObject();
        rj::Document::AllocatorType& allocator_output_full = objects_output_full.GetAllocator();

        rj::Value station_data(rj::kObjectType);
        station_data.AddMember("id", sender_info.station_id, allocator_output_full);
        station_data.AddMember("type", sender_type, allocator_output_full);
        station_data.AddMember("latitude", sender_latitude, allocator_output_full);
        station_data.AddMember("longitude", sender_longitude, allocator_output_full);
        if (sender_info.speed == NOT_PRESENT_FLOAT) station_data.AddMember("speed", 0, allocator_output_full);
        else station_data.AddMember("speed", sender_info.speed, allocator_output_full);
        if (sender_info.heading == NOT_PRESENT_FLOAT) station_data.AddMember("heading", rj::Value(rj::kNullType), allocator_output_full);
        else station_data.AddMember("heading", sender_info.heading, allocator_output_full);
        if (sender_info.altitude == NOT_PRESENT_FLOAT) station_data.AddMember("altitude", rj::Value(rj::kNullType), allocator_output_full);
        else station_data.AddMember("altitude", sender_info.altitude, allocator_output_full);
        if (sender_info.acceleration == NOT_PRESENT_FLOAT) station_data.AddMember("acceleration", rj::Value(rj::kNullType), allocator_output_full);
        else station_data.AddMember("acceleration", sender_info.acceleration, allocator_output_full);
        objects_output_full.AddMember("sender", station_data, allocator_output_full);

        rj::Value objects(rj::kArrayType);
        rj::Value objects_full(rj::kArrayType);

        int number_objects = perceivedObjectContainer["containerData"]["numberOfPerceivedObjects"].GetInt();

        if (number_objects == 0) {
            spdlog::warn("[Processor] No objects received!");
            if (performanceLogs_){
                auto t_empty = std::chrono::high_resolution_clock::now();
                auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(t_empty - message_reception).count();
                double message_reception_timestamp = std::chrono::duration<double>(message_reception.time_since_epoch()).count();
                std::string current_timestamp = getCurrentTimestampString();
                processor_file_logger_->info("Processor,total_time,{},{},{},{}", current_timestamp, number_objects, message_reception_timestamp, total_time);
                spdlog::debug("[Processor] No objects to process, returning early.");
            }   
            return;
        }

        for(int i=0; i<number_objects; i++) {
            rj::Value object_full(rj::kObjectType);

            const rj::Value& json_object = perceivedObjectContainer["containerData"]["perceivedObjects"][i];

            // IDs and timestamps
            int object_id = (json_object.HasMember("objectId") && json_object["objectId"].IsInt()) ? json_object["objectId"].GetInt() : NOT_PRESENT_INT;
            if (object_id == NOT_PRESENT_INT) {
                spdlog::error("[Processor] Object ID not found");
                continue;
            }
            long object_age = age_cpm + json_object["measurementDeltaTime"].GetInt();
            spdlog::debug("[Processor] Object ID: {}, object_age ({}) = age_cpm ({}) + measurementDeltaTime ({})", object_id, object_age, age_cpm, json_object["measurementDeltaTime"].GetInt());

            if (prometheus_ && metrics_) {
                metrics_->obj_age_ms->Observe(static_cast<double>(object_age));
            }

            long object_timestamp = now - object_age;
            spdlog::debug("[Processor] Object timestamp: {}", object_timestamp);
            double object_timestamp_sec = static_cast<double>(object_timestamp) / 1000.0;
            spdlog::debug("[Processor] Object timestamp in seconds: {}", object_timestamp_sec);

            long long timestamp_secs = static_cast<long long>(now / 1000);
            long long truncated_timestamp = (timestamp_secs / config_.repeat_id_interval) * config_.repeat_id_interval;
            long long object_unique_id = (truncated_timestamp << 16) | (object_id & 0xFFFF);

            // ------------- Sensor data -------------
            int object_sensor_id = (json_object.HasMember("sensorIdList") && json_object["sensorIdList"].IsArray() && json_object["sensorIdList"].Size() > 0)
                ? json_object["sensorIdList"][0].GetInt() : -1;
            std::string object_sensor_str = "unknown";
            try {
                object_sensor_str = sensor_type_.at(object_sensor_id);
            } catch(...) {
                spdlog::error("[Processor] Sensor ID not found for object ID {}", object_id);
            }

            // ------------- Position data -------------
            float object_x_distance = (json_object.HasMember("position") && json_object["position"].HasMember("xCoordinate") && json_object["position"]["xCoordinate"].HasMember("value"))
                ? json_object["position"]["xCoordinate"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            if (object_x_distance == NOT_PRESENT_FLOAT) {
                spdlog::error("[Processor] Object xCoordinate not found for object ID {}", object_id);
                continue;
            }
            float object_y_distance = (json_object.HasMember("position") && json_object["position"].HasMember("yCoordinate") && json_object["position"]["yCoordinate"].HasMember("value"))
                ? json_object["position"]["yCoordinate"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            if (object_y_distance == NOT_PRESENT_FLOAT) {
                spdlog::error("[Processor] Object yCoordinate not found for object ID {}", object_id);
                continue;
            }
            float object_z_distance = (json_object.HasMember("position") && json_object["position"].HasMember("zCoordinate") && json_object["position"]["zCoordinate"].HasMember("value"))
                ? json_object["position"]["zCoordinate"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_x_cov = (json_object.HasMember("position") && json_object["position"].HasMember("xCoordinate") && json_object["position"]["xCoordinate"].HasMember("confidence"))
                ? json_object["position"]["xCoordinate"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_y_cov = (json_object.HasMember("position") && json_object["position"].HasMember("yCoordinate") && json_object["position"]["yCoordinate"].HasMember("confidence"))
                ? json_object["position"]["yCoordinate"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_z_cov = (json_object.HasMember("position") && json_object["position"].HasMember("zCoordinate") && json_object["position"]["zCoordinate"].HasMember("confidence"))
                ? json_object["position"]["zCoordinate"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;

            float object_latitude = sender_latitude + M_180_PI * (object_y_distance/R);
            float object_longitude = sender_longitude + M_180_PI * (object_x_distance/R) / cos(M_PI_180 * sender_latitude);

            // ------------- Velocity data -------------
            float object_x_velocity = (json_object.HasMember("velocity") && json_object["velocity"].HasMember("cartesianVelocity") && json_object["velocity"]["cartesianVelocity"].HasMember("xVelocity") && json_object["velocity"]["cartesianVelocity"]["xVelocity"].HasMember("value"))
                ? json_object["velocity"]["cartesianVelocity"]["xVelocity"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_y_velocity = (json_object.HasMember("velocity") && json_object["velocity"].HasMember("cartesianVelocity") && json_object["velocity"]["cartesianVelocity"].HasMember("yVelocity") && json_object["velocity"]["cartesianVelocity"]["yVelocity"].HasMember("value"))
                ? json_object["velocity"]["cartesianVelocity"]["yVelocity"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_x_velocity_cov = (json_object.HasMember("velocity") && json_object["velocity"].HasMember("cartesianVelocity") && json_object["velocity"]["cartesianVelocity"].HasMember("xVelocity") && json_object["velocity"]["cartesianVelocity"]["xVelocity"].HasMember("confidence"))
                ? json_object["velocity"]["cartesianVelocity"]["xVelocity"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_y_velocity_cov = (json_object.HasMember("velocity") && json_object["velocity"].HasMember("cartesianVelocity") && json_object["velocity"]["cartesianVelocity"].HasMember("yVelocity") && json_object["velocity"]["cartesianVelocity"]["yVelocity"].HasMember("confidence"))
                ? json_object["velocity"]["cartesianVelocity"]["yVelocity"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;

            float object_speed = NOT_PRESENT_FLOAT;
            if (object_x_velocity != NOT_PRESENT_FLOAT && object_y_velocity != NOT_PRESENT_FLOAT) {
                object_speed = std::sqrt(std::pow(object_x_velocity, 2) + std::pow(object_y_velocity, 2));
            }

            // ------------- Acceleration data -------------
            float object_x_acceleration = (json_object.HasMember("acceleration") && json_object["acceleration"].HasMember("cartesianAcceleration") && json_object["acceleration"]["cartesianAcceleration"].HasMember("xAcceleration") && json_object["acceleration"]["cartesianAcceleration"]["xAcceleration"].HasMember("value"))
                ? json_object["acceleration"]["cartesianAcceleration"]["xAcceleration"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_y_acceleration = (json_object.HasMember("acceleration") && json_object["acceleration"].HasMember("cartesianAcceleration") && json_object["acceleration"]["cartesianAcceleration"].HasMember("yAcceleration") && json_object["acceleration"]["cartesianAcceleration"]["yAcceleration"].HasMember("value"))
                ? json_object["acceleration"]["cartesianAcceleration"]["yAcceleration"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_x_acceleration_cov = (json_object.HasMember("acceleration") && json_object["acceleration"].HasMember("cartesianAcceleration") && json_object["acceleration"]["cartesianAcceleration"].HasMember("xAcceleration") && json_object["acceleration"]["cartesianAcceleration"]["xAcceleration"].HasMember("confidence"))
                ? json_object["acceleration"]["cartesianAcceleration"]["xAcceleration"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_y_acceleration_cov = (json_object.HasMember("acceleration") && json_object["acceleration"].HasMember("cartesianAcceleration") && json_object["acceleration"]["cartesianAcceleration"].HasMember("yAcceleration") && json_object["acceleration"]["cartesianAcceleration"]["yAcceleration"].HasMember("confidence"))
                ? json_object["acceleration"]["cartesianAcceleration"]["yAcceleration"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;

            float object_acceleration = NOT_PRESENT_FLOAT;
            if (object_x_acceleration != NOT_PRESENT_FLOAT && object_y_acceleration != NOT_PRESENT_FLOAT) {
                object_acceleration = std::sqrt(std::pow(object_x_acceleration, 2) + std::pow(object_y_acceleration, 2));
            }

            // ------------- Heading data -------------
            float object_heading = (json_object.HasMember("angles") && json_object["angles"].HasMember("zAngle") && json_object["angles"]["zAngle"].HasMember("value"))
                ? json_object["angles"]["zAngle"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_heading_cov = (json_object.HasMember("angles") && json_object["angles"].HasMember("zAngle") && json_object["angles"]["zAngle"].HasMember("confidence"))
                ? json_object["angles"]["zAngle"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;

            // ------------- Classification data -------------
            int object_classification = (json_object.HasMember("classification") && json_object["classification"].IsArray() && json_object["classification"].Size() > 0 && json_object["classification"][0].HasMember("objectClass") && json_object["classification"][0]["objectClass"].HasMember("vehicleSubClass"))
                ? json_object["classification"][0]["objectClass"]["vehicleSubClass"].GetInt() : -1;
            std::string object_classification_str = "unclassified";
            try {
                object_classification_str = vehicle_classes_.at(object_classification);
            } catch(...) {
                spdlog::error("[Processor] Classification ID not found - vehicle for object ID {}", object_id);
                object_classification_str = "unknown";
            }
            
            if (object_classification == -1) {
                object_classification = (json_object.HasMember("classification") && json_object["classification"].IsArray() && json_object["classification"].Size() > 0 && json_object["classification"][0].HasMember("objectClass") && json_object["classification"][0]["objectClass"].HasMember("vruSubClass"))
                    ? json_object["classification"][0]["objectClass"]["vruSubClass"].GetInt() : -1;
                try {
                    object_classification_str = person_classes_.at(object_classification);
                } catch(...) {
                    spdlog::error("[Processor] Classification ID not found - person for object ID {}", object_id);
                    object_classification_str = "unknown";
                }
            }

            if (object_classification == -1) {
                object_classification = (json_object.HasMember("classification") && json_object["classification"].IsArray() && json_object["classification"].Size() > 0 && json_object["classification"][0].HasMember("objectClass") && json_object["classification"][0]["objectClass"].HasMember("otherSubClass"))
                    ? json_object["classification"][0]["objectClass"]["otherSubClass"].GetInt() : -1;
                try {
                    object_classification_str = other_classes_.at(object_classification);
                } catch(...) {
                    spdlog::error("[Processor] Classification ID not found - other for object ID {}", object_id);
                    object_classification_str = "unknown";
                }
            }

            int object_confidence = (json_object.HasMember("classification") && json_object["classification"].IsArray() && json_object["classification"].Size() > 0 && json_object["classification"][0].HasMember("confidence"))
                ? json_object["classification"][0]["confidence"].GetInt() : -1;

            // ------------- Angular velocity data -------------
            float object_z_angular_velocity = (json_object.HasMember("zAngularVelocity") && json_object["zAngularVelocity"].HasMember("value"))
                ? json_object["zAngularVelocity"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_z_angular_velocity_cov = (json_object.HasMember("zAngularVelocity") && json_object["zAngularVelocity"].HasMember("confidence"))
                ? json_object["zAngularVelocity"]["confidence"].GetFloat() : NOT_PRESENT_FLOAT;

            // ------------- Size data -------------
            float object_size_x = (json_object.HasMember("objectDimensionX") && json_object["objectDimensionX"].HasMember("value"))
                ? json_object["objectDimensionX"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_size_y = (json_object.HasMember("objectDimensionY") && json_object["objectDimensionY"].HasMember("value"))
                ? json_object["objectDimensionY"]["value"].GetFloat() : NOT_PRESENT_FLOAT;
            float object_size_z = (json_object.HasMember("objectDimensionZ") && json_object["objectDimensionZ"].HasMember("value"))
                ? json_object["objectDimensionZ"]["value"].GetFloat() : NOT_PRESENT_FLOAT;


            // ------------- Object full output -------------
            // Misc
            object_full.AddMember("id", object_id, allocator_output_full);
            object_full.AddMember("uniqueID", static_cast<int64_t>(object_unique_id), allocator_output_full);
            object_full.AddMember("sensorType", rj::Value(object_sensor_str.c_str(), allocator_output_full).Move(), allocator_output_full);
            object_full.AddMember("sensorID", object_sensor_id, allocator_output_full);
            if (object_confidence == -1) object_full.AddMember("confidence", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("confidence", object_confidence, allocator_output_full);

            // Attributes
            object_full.AddMember("latitude", object_latitude, allocator_output_full);
            object_full.AddMember("longitude", object_longitude, allocator_output_full);
            if (object_z_distance == NOT_PRESENT_FLOAT) object_full.AddMember("altitude", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("altitude", object_z_distance, allocator_output_full);
            // object_full.AddMember("referenceLatitude", sender_latitude, allocator_output_full);
            // object_full.AddMember("referenceLongitude", sender_longitude, allocator_output_full);
            object_full.AddMember("xDistance", object_x_distance, allocator_output_full);
            object_full.AddMember("yDistance", object_y_distance, allocator_output_full);
            if (object_x_cov == NOT_PRESENT_FLOAT) object_full.AddMember("xDistanceCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("xDistanceCov", object_x_cov, allocator_output_full);
            if (object_y_cov == NOT_PRESENT_FLOAT) object_full.AddMember("yDistanceCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("yDistanceCov", object_y_cov, allocator_output_full);
            if (object_z_cov == NOT_PRESENT_FLOAT) object_full.AddMember("altitudeCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("altitudeCov", object_z_cov, allocator_output_full);
            if (object_speed == NOT_PRESENT_FLOAT) object_full.AddMember("speed", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("speed", object_speed, allocator_output_full);
            if (object_x_velocity == NOT_PRESENT_FLOAT) object_full.AddMember("xVelocity", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("xVelocity", object_x_velocity, allocator_output_full);
            if (object_y_velocity == NOT_PRESENT_FLOAT) object_full.AddMember("yVelocity", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("yVelocity", object_y_velocity, allocator_output_full);
            if (object_x_velocity_cov == NOT_PRESENT_FLOAT) object_full.AddMember("xVelocityCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("xVelocityCov", object_x_velocity_cov, allocator_output_full);
            if (object_y_velocity_cov == NOT_PRESENT_FLOAT) object_full.AddMember("yVelocityCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("yVelocityCov", object_y_velocity_cov, allocator_output_full);
            if (object_acceleration == NOT_PRESENT_FLOAT) object_full.AddMember("acceleration", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("acceleration", object_acceleration, allocator_output_full);
            if (object_x_acceleration == NOT_PRESENT_FLOAT) object_full.AddMember("xAcceleration", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("xAcceleration", object_x_acceleration, allocator_output_full);
            if (object_y_acceleration == NOT_PRESENT_FLOAT) object_full.AddMember("yAcceleration", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("yAcceleration", object_y_acceleration, allocator_output_full);
            if (object_x_acceleration_cov == NOT_PRESENT_FLOAT) object_full.AddMember("xAccelerationCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("xAccelerationCov", object_x_acceleration_cov, allocator_output_full);
            if (object_y_acceleration_cov == NOT_PRESENT_FLOAT) object_full.AddMember("yAccelerationCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("yAccelerationCov", object_y_acceleration_cov, allocator_output_full);
            if (object_heading == NOT_PRESENT_FLOAT) object_full.AddMember("heading", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("heading", object_heading, allocator_output_full);
            if (object_heading_cov == NOT_PRESENT_FLOAT) object_full.AddMember("headingCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("headingCov", object_heading_cov, allocator_output_full);
            if (object_size_x == NOT_PRESENT_FLOAT) object_full.AddMember("xSize", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("xSize", object_size_x, allocator_output_full);
            if (object_size_y == NOT_PRESENT_FLOAT) object_full.AddMember("ySize", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("ySize", object_size_y, allocator_output_full);
            if (object_size_z == NOT_PRESENT_FLOAT) object_full.AddMember("zSize", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("zSize", object_size_z, allocator_output_full);
            if (object_z_angular_velocity == NOT_PRESENT_FLOAT) object_full.AddMember("angularVelocity", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("angularVelocity", object_z_angular_velocity, allocator_output_full);
            if (object_z_angular_velocity_cov == NOT_PRESENT_FLOAT) object_full.AddMember("angularVelocityCov", rj::Value(rj::kNullType), allocator_output_full);
            else object_full.AddMember("angularVelocityCov", object_z_angular_velocity_cov, allocator_output_full);
            object_full.AddMember("classification", rj::Value(object_classification_str.c_str(), allocator_output_full).Move(), allocator_output_full);
            object_full.AddMember("classificationID", object_classification, allocator_output_full);
            
            // Station
            // object_full.AddMember("stationSenderID", sender_id, allocator_output_full);
            // object_full.AddMember("stationSenderType", sender_type, allocator_output_full);
            object_full.AddMember("stationReceiverID", receiver_id, allocator_output_full);
            object_full.AddMember("stationReceiverType", receiver_type, allocator_output_full);

            // Timestamps
            object_full.AddMember("objectAge", object_age, allocator_output_full);
            object_full.AddMember("objectTimestamp", object_timestamp_sec, allocator_output_full);
            object_full.AddMember("cpmAge", age_cpm, allocator_output_full);
            object_full.AddMember("cpmTimestamp", cpm_reference_time_double, allocator_output_full);
            object_full.AddMember("cpmTimestampUnix", cpm_reference_time_unix, allocator_output_full);

            objects_full.PushBack(object_full, allocator_output_full);

        }

        // ------------- Serialize outputs -------------
        objects_output_full.AddMember("objects", objects_full, allocator_output_full);
        spdlog::debug("[Processor] Parsing {} full objects ...", objects_output_full["objects"].Size());

        auto t2 = std::chrono::high_resolution_clock::now();

        rj::StringBuffer buffer_full;
        rj::Writer<rj::StringBuffer> writer_full(buffer_full);
        objects_output_full.Accept(writer_full);
        output_full = buffer_full.GetString();

        auto t3 = std::chrono::high_resolution_clock::now();

        auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        auto serialization_full_time = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(t3 - message_reception).count();
        spdlog::debug ("[Processor] Processing time: {} us\n[Processor] Serialization full time: {} us\n[Processor] Total time: {} us", processing_time, serialization_full_time, total_time);

        if (prometheus_ && metrics_) {
            metrics_->cycle_ms->Observe(static_cast<double>(total_time) / 1000.0);
        }
        
        std::string current_timestamp = getCurrentTimestampString();
        if (performanceLogs_){
            // Convert to timestamp since epoch in seconds
            double message_reception_timestamp = std::chrono::duration<double>(message_reception.time_since_epoch()).count();
            double t1_timestamp = std::chrono::duration<double>(t1.time_since_epoch()).count();
            double t2_timestamp = std::chrono::duration<double>(t2.time_since_epoch()).count();
            double t3_timestamp = std::chrono::duration<double>(t3.time_since_epoch()).count();
            processor_file_logger_->info("Processor,processing_time,{},{},{},{}", current_timestamp, number_objects, t1_timestamp, processing_time);
            processor_file_logger_->info("Processor,serialization_full_time,{},{},{},{}", current_timestamp, number_objects, t3_timestamp, serialization_full_time);
            processor_file_logger_->info("Processor,total_time,{},{},{},{}", current_timestamp, number_objects, message_reception_timestamp, total_time); 
        }
    } catch (const rj::ParseResult& e) {
        spdlog::error("[Processor] RapidJSON parse error: {} in message: {}", static_cast<int>(e.Code()), message);
    }
}
    

