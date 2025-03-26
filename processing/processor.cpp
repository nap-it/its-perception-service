#include "processor.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <chrono>
#include <spdlog/spdlog.h>

namespace rj = rapidjson;

Processor::Processor(const Config& config) : config_(config) {
    
    if (config.debug) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    // Set up DDS client.
    dds_ = new Dds("Processor", config.dds_domain, [this](const std::string& topic, const std::string& message){
        this->on_message_dds(topic, message);
    });
    dds_->subscribe(config.cpm_topic);
    if (config.cam_topic != "") dds_->subscribe(config.cam_topic);
    dds_->provision_publisher(config.dds_output_topic);
    dds_->provision_publisher(config.dds_output_full_topic);
    spdlog::info("[Processor] DDS client subscribed to topic {} and {}", config.cpm_topic, config.cam_topic);

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
        cleanupStaleCamData(std::chrono::seconds(config_.cam_freshness_threshold));
    }
    spdlog::info("[Processor] Stopped.");
}

void Processor::on_message_dds(const std::string& topic, const std::string& message) {
    spdlog::debug("[Processor] Received message on topic {}: {}", topic, message);

    if (topic == config_.cpm_topic) {
        spdlog::debug("[Processor] Processing CPM message...");
        std::string output = "";
        std::string full_output = "";
        this->processCPM(topic, message, output, full_output);

        if (output.empty() || full_output.empty()) {
            spdlog::warn("[Processor] Empty output message");
            return;
        }

        dds_->publish(config_.dds_output_topic, output);
        dds_->publish(config_.dds_output_full_topic, full_output);
        spdlog::debug("[Processor] Published DDS message on topic {} and {}", config_.dds_output_topic, config_.dds_output_full_topic);

        if(config_.local_mqtt_enabled) {
            local_mqtt_client_->publish(config_.local_mqtt_output_topic, output);
            local_mqtt_client_->publish(config_.local_mqtt_output_full_topic, full_output);
            spdlog::debug("[Processor] Published Local MQTT message on topic {} and {}", config_.local_mqtt_output_topic, config_.local_mqtt_output_full_topic);
        }

        if(config_.remote_mqtt_enabled) {
            remote_mqtt_client_->publish(config_.remote_mqtt_output_topic, output);
            remote_mqtt_client_->publish(config_.remote_mqtt_output_full_topic, full_output);
            spdlog::debug("[Processor] Published Remote MQTT message on topic {} and {}", config_.remote_mqtt_output_topic, config_.remote_mqtt_output_full_topic);
        }

    } else if (topic == config_.cam_topic) {
        spdlog::debug("[Processor] Processing CAM message...");
        this->processCAM(topic, message);
        
    } else {
        spdlog::warn("[Processor] Received message on unknown topic: {}", topic);
    }
}

void Processor::processCAM(const std::string& topic, const std::string& message) {
    try {
        rj::Document doc;
        doc.Parse(message.c_str());
        if (doc.HasParseError()) {
            spdlog::error("[Processor] Parse error in CAM message: {}", message);
            return;
        }

        int station_id = NOT_PRESENT_INT;
        float speed = NOT_PRESENT_FLOAT;
        float acceleration = NOT_PRESENT_FLOAT;
        float heading = NOT_PRESENT_FLOAT;

        if (topic == "vanetza/in/cam" || topic == "vanetza/own/cam" || topic == "vanetza/out/cam") {
            station_id = config_.host_station_id;
            speed = (doc.HasMember("speed") && doc["speed"].IsFloat()) ? static_cast<float>(doc["speed"].GetFloat()) : NOT_PRESENT_FLOAT;
            acceleration = (doc.HasMember("acceleration") && doc["acceleration"].IsFloat()) ? static_cast<float>(doc["acceleration"].GetFloat()) : NOT_PRESENT_FLOAT;
            heading = (doc.HasMember("heading") && doc["heading"].IsFloat()) ? static_cast<float>(doc["heading"].GetFloat()) : NOT_PRESENT_FLOAT;
        } else if (topic == "vanetza/in/cam_full") {
            station_id = config_.host_station_id;
            if (doc.HasMember("camParameters") && doc["camParameters"].IsObject()) {
                const rj::Value& camParameters = doc["camParameters"];
                if (camParameters.HasMember("highFrequencyContainer") && camParameters["highFrequencyContainer"].IsObject()) {
                    const rj::Value& highFrequencyContainer = camParameters["highFrequencyContainer"];
                    if (highFrequencyContainer.HasMember("basicVehicleContainerHighFrequency") && highFrequencyContainer["basicVehicleContainerHighFrequency"].IsObject()) {
                        const rj::Value& basicVehicle = highFrequencyContainer["basicVehicleContainerHighFrequency"];
                        heading = (basicVehicle.HasMember("heading") && basicVehicle["heading"].IsObject())
                                  ? static_cast<float>(basicVehicle["heading"]["headingValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                        if (heading == 3601) heading = NOT_PRESENT_FLOAT;
                        speed = (basicVehicle.HasMember("speed") && basicVehicle["speed"].IsObject())
                                ? static_cast<float>(basicVehicle["speed"]["speedValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                        if (speed == 16383) speed = NOT_PRESENT_FLOAT;
                        acceleration = (basicVehicle.HasMember("longitudinalAcceleration") && basicVehicle["longitudinalAcceleration"].IsObject())
                                    ? static_cast<float>(basicVehicle["longitudinalAcceleration"]["longitudinalAccelerationValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                    }
                }
            }
        } else if (topic == "vanetza/in/vam") {
            station_id = config_.host_station_id;
            if (doc.HasMember("vamParameters") && doc["vamParameters"].IsObject()) {
                const rj::Value& vamParameters = doc["vamParameters"];
                if (vamParameters.HasMember("vruHighFrequencyContainer") && vamParameters["vruHighFrequencyContainer"].IsObject()) {
                    const rj::Value& vruHighFrequencyContainer = vamParameters["vruHighFrequencyContainer"];
                    heading = (vruHighFrequencyContainer.HasMember("heading") && vruHighFrequencyContainer["heading"].IsObject())
                                ? static_cast<float>(vruHighFrequencyContainer["heading"]["headingValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                    if (heading == 3601) heading = NOT_PRESENT_FLOAT;
                    speed = (vruHighFrequencyContainer.HasMember("speed") && vruHighFrequencyContainer["speed"].IsObject())
                            ? static_cast<float>(vruHighFrequencyContainer["speed"]["speedValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                    if (speed == 16383) speed = NOT_PRESENT_FLOAT;
                    acceleration = (vruHighFrequencyContainer.HasMember("longitudinalAcceleration") && vruHighFrequencyContainer["longitudinalAcceleration"].IsObject())
                                ? static_cast<float>(vruHighFrequencyContainer["longitudinalAcceleration"]["longitudinalAccelerationValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                }
            }
        } else if (topic == "vanetza/out/cam_full") {
            station_id = (doc.HasMember("fields") && doc["fields"].IsObject())
                ? static_cast<float>(doc["fields"]["header"]["statioNID"].GetDouble()) : NOT_PRESENT_FLOAT;
            if (doc.HasMember("fields") && doc["fields"].IsObject() && doc["fields"].HasMember("cam") && doc["fields"]["cam"].IsObject()) {
                const rj::Value& cam = doc["fields"]["cam"];
                if (cam.HasMember("highFrequencyContainer") && cam["highFrequencyContainer"].IsObject()) {
                    const rj::Value& highFrequencyContainer = cam["highFrequencyContainer"];
                    if (highFrequencyContainer.HasMember("basicVehicleContainerHighFrequency") && highFrequencyContainer["basicVehicleContainerHighFrequency"].IsObject()) {
                        const rj::Value& basicVehicle = highFrequencyContainer["basicVehicleContainerHighFrequency"];
                        heading = (basicVehicle.HasMember("heading") && basicVehicle["heading"].IsObject())
                                    ? static_cast<float>(basicVehicle["heading"]["headingValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                        if (heading == 3601) heading = NOT_PRESENT_FLOAT;
                        speed = (basicVehicle.HasMember("speed") && basicVehicle["speed"].IsObject())
                                ? static_cast<float>(basicVehicle["speed"]["speedValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                        if (speed == 16383) speed = NOT_PRESENT_FLOAT;
                        acceleration = (basicVehicle.HasMember("longitudinalAcceleration") && basicVehicle["longitudinalAcceleration"].IsObject())
                                    ? static_cast<float>(basicVehicle["longitudinalAcceleration"]["longitudinalAccelerationValue"].GetDouble()) : NOT_PRESENT_FLOAT;
                    }
                }

            }
        } else {
            spdlog::warn("[Processor] Unknown CAM topic: {}", topic);
            return;
        }
        
        if (station_id == NOT_PRESENT_INT) {
            spdlog::warn("[Processor] Missing station ID in CAM message: {}", message);
            return;
        }

        auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(camMtx_);
            auto it = camDataMap_.find(station_id);
            if (it != camDataMap_.end()) {
                it->second.speed = speed;
                it->second.acceleration = acceleration;
                it->second.heading = heading;
                it->second.cam_timestamp = now;
            } 
        }

    } catch (const rj::ParseResult& e) {
        spdlog::error("[Processor] RapidJSON parse error: {} in message: {}", e.Code(), message);
    } catch (const std::exception& e) {
        spdlog::error("[Processor] Error processing DDS message: {}", e.what());
    }
}

void Processor::cleanupStaleCamData(std::chrono::steady_clock::duration threshold) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(camMtx_);
    for (auto it = camDataMap_.begin(); it != camDataMap_.end(); ) {
        if (now - it->second.cam_timestamp > threshold) {
            spdlog::debug("[Processor] Removing stale CAM data for station {}", it->first);
            it = camDataMap_.erase(it);
        } else {
            ++it;
        }
    }
}

void Processor::processCPM(const std::string& topic, const std::string& message, std::string& output, std::string& output_full){
    try {
        rj::Document doc;
        doc.Parse(message.c_str());
        if (doc.HasParseError()) {
            spdlog::error("[Processor] Parse error in CAM message: {}", message);
            return;
        }

        rj::Document cpm;
        float sender_id = NOT_PRESENT_FLOAT;
        float sender_type = 5;
        float receiver_id = NOT_PRESENT_FLOAT;
        float receiver_type = NOT_PRESENT_FLOAT;

        // Station data and CPM
        if (topic == "vanetza/out/cpm"){
            sender_id = (doc.HasMember("stationID") && doc["stationID"].IsFloat()) ? static_cast<float>(doc["stationID"].GetFloat()) : NOT_PRESENT_FLOAT;
            receiver_id = (doc.HasMember("receiverID") && doc["receiverID"].IsFloat()) ? static_cast<float>(doc["receiverID"].GetFloat()) : NOT_PRESENT_FLOAT;
            receiver_type = (doc.HasMember("receiverType") && doc["receiverType"].IsFloat()) ? static_cast<float>(doc["receiverType"].GetFloat()) : NOT_PRESENT_FLOAT;
            if (doc.HasMember("fields") && doc["fields"].IsObject() && doc["fields"].HasMember("payload") && doc["fields"]["payload"].IsObject()) {
                cpm.CopyFrom(doc["fields"]["payload"], cpm.GetAllocator());
            } else {
                spdlog::error("[Processor] CPM message does not have fields/payload");
                return;
            }
        } else if (topic == "cps-v2/in/cpm" || topic == "vanetza/in/cpm") {
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
        if (cpm.HasMember("managementContainer") && cpm["managementContainer"].HasMember("referenceTime")) {
            cpm_reference_time = cpm["managementContainer"]["referenceTime"].GetInt64();
        } else {
            spdlog::error("[Processor] CPM does not have referenceTime");
            return;
        }
        unsigned long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        unsigned long local_gen_delta_time = now - TIME_2004_MS;
        int age_cpm = local_gen_delta_time - cpm_reference_time;

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
        
        SenderInfo sender_info;
        sender_info.station_id = sender_id;
        sender_info.station_type = sender_type;
        sender_info.latitude = sender_latitude;
        sender_info.longitude = sender_longitude;

        {
            std::lock_guard<std::mutex> lock(camMtx_);
            auto it = camDataMap_.find(sender_id);
            // If SenderInfo is found, update the map with the latest CAM data.
            if (it != camDataMap_.end()) {
                it->second.latitude = sender_info.latitude;
                

            }
        }
        

    } catch (const rj::ParseResult& e) {
        spdlog::error("[Processor] RapidJSON parse error: {} in message: {}", e.Code(), message);
    } catch (const std::exception& e) {
        spdlog::error("[Processor] Error processing DDS message: {}", e.what());
    }
}
    

