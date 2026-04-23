#include "locator.h"
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

Locator::Locator(ProviderType provider,
                   int configStationId,
                   std::string mqttBroker,
                   std::string mqttTopic,
                   int ddsDomain,
                   std::string ddsTopic)
        : provider_(provider),
        station_id_(configStationId),
        mqttClient_(nullptr),
        dds_(nullptr),
        mqttTopic_(mqttTopic),
        ddsTopic_(ddsTopic)
{
    spdlog::info("[Locator] Constructing with provider type: {}", (provider_ == ProviderType::STATIC ? "STATIC" : (provider_ == ProviderType::MQTT ? "MQTT" : "DDS")));

    if(provider_ == ProviderType::STATIC) {
        spdlog::info("[Locator] Using STATIC provider. No CAM data, only station ID {}.", configStationId);
    }
    else if(provider_ == ProviderType::MQTT) {
        // Set up MQTT client.
        data_mqtt_server mqttConfig;
        mqttConfig.address = mqttBroker;
        mqttConfig.client_id = "Locator-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        mqttConfig.subscription_topic.push_back(mqttTopic_);
        // Default subscriptions for incoming CAMs
        mqttConfig.subscription_topic.push_back("vanetza/out/cam");
        mqttConfig.subscription_topic.push_back("vanetza/out/cam_full");
        
        mqttClient_ = new MqttWrapper(mqttConfig, [this](const std::string& topic, const std::string& message){
            this->on_message_mqtt(topic, message);
        });
        
        while(!mqttClient_->is_connected()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        spdlog::info("[Locator] MQTT client connected to broker {} on topic {}", mqttBroker, mqttTopic_);
    }
    else if(provider_ == ProviderType::DDS) {
        // Set up DDS client.
        dds_ = new Dds("Locator", ddsDomain, [this](const std::string& topic, const std::string& message){
            this->on_message_dds(topic, message);
        });
        dds_->subscribe(ddsTopic_);
        spdlog::info("[Locator] DDS client subscribed to topic {}", ddsTopic_);
    }
    else {
        throw std::runtime_error("[Locator] Unknown provider type");
    }

    camDataMap_ = {};
}

Locator::~Locator() {
    if(mqttClient_) {
        delete mqttClient_;
        mqttClient_ = nullptr;
    }
    if(dds_) {
        delete dds_;
        dds_ = nullptr;
    }
}

void Locator::cleanupDataMap(){
    std::lock_guard<std::mutex> lock(camMtx_);
    if (camDataMap_.empty()) {
        return;
    }
    for(auto it = camDataMap_.begin(); it != camDataMap_.end();){
        if(std::chrono::steady_clock::now() - it->second.cam_timestamp > std::chrono::seconds(2)){
            it = camDataMap_.erase(it);
        }
        else{
            ++it;
        }
    }
}

void Locator::run() {
    locatorThread_ = std::thread(&Locator::runLoop, this);
    spdlog::info("[Locator] run loop started.");
}

void Locator::runLoop() {
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cleanupDataMap();
    }
}

SenderInfo Locator::getStationData(int stationId) {
    spdlog::debug("[Locator] getStationData called for station ID: {}", stationId);
    std::lock_guard<std::mutex> lock(camMtx_);
    if(camDataMap_.find(stationId) != camDataMap_.end()) {
        spdlog::debug("[Locator] Found station data for ID: {}", stationId);
        return camDataMap_[stationId];
    }
    SenderInfo empty;
    empty.station_id = stationId;
    empty.acceleration = NOT_PRESENT_FLOAT;
    empty.altitude = NOT_PRESENT_FLOAT;
    empty.heading = NOT_PRESENT_FLOAT;
    empty.speed = NOT_PRESENT_FLOAT;
    spdlog::debug("[Locator] No data found for station ID: {}. Returning empty SenderInfo.", stationId);
    return empty;
}

void Locator::on_message_mqtt(const std::string& topic, const std::string& message) {
    spdlog::debug("[Locator] MQTT message received on topic '{}': {}", topic, message);
    parseAndUpdateData(topic, message);
}

void Locator::on_message_dds(const std::string& topic, const std::string& message) {
    spdlog::debug("[Locator] DDS message received on topic '{}': {}", topic, message);
    parseAndUpdateData(topic, message);
}

void Locator::parseAndUpdateData(const std::string& topic, const std::string& message) {
    try {
        rj::Document doc;
        doc.Parse(message.c_str());
        if (doc.HasParseError()) {
            spdlog::error("[Locator] Parse error in message: {}", message);
            return;
        }
        
        if (topic == "vanetza/in/cam" || topic == "vanetza/own/cam") {
            if (doc.HasMember("camParameters") && doc["camParameters"].IsObject()) {
                const rj::Value& camParameters = doc["camParameters"];
                int id = station_id_;
                float speed = NOT_PRESENT_FLOAT;
                float heading = NOT_PRESENT_FLOAT;
                float altitude = NOT_PRESENT_FLOAT;
                float acceleration = NOT_PRESENT_FLOAT;
                if (camParameters.HasMember("basicContainer") && camParameters["basicContainer"].IsObject()) {
                    const rj::Value& basicContainer = camParameters["basicContainer"];
                    if (basicContainer.HasMember("referencePosition") && basicContainer["referencePosition"].IsObject()) {
                        const rj::Value& referencePosition = basicContainer["referencePosition"];
                        altitude = (referencePosition.HasMember("altitude") && referencePosition["altitude"].IsObject() &&
                                referencePosition["altitude"].HasMember("altitudeValue") &&
                                referencePosition["altitude"]["altitudeValue"].IsFloat())
                            ? referencePosition["altitude"]["altitudeValue"].GetFloat() : NOT_PRESENT_FLOAT;
                        if (altitude > 5000.0f) {
                            spdlog::debug("[Locator] Altitude value is too high: {}", altitude);
                            altitude = NOT_PRESENT_FLOAT;
                        }
                    }
                }
                if (camParameters.HasMember("highFrequencyContainer") && camParameters["highFrequencyContainer"].IsObject()) {
                    const rj::Value& hfContainer = camParameters["highFrequencyContainer"];
                    if (hfContainer.HasMember("basicVehicleContainerHighFrequency") && hfContainer["basicVehicleContainerHighFrequency"].IsObject()) {
                        const rj::Value& basicVehicle = hfContainer["basicVehicleContainerHighFrequency"];
                        if (basicVehicle.HasMember("heading") && basicVehicle["heading"].IsObject()) {
                            const rj::Value& headingObj = basicVehicle["heading"];
                            heading = (headingObj.HasMember("headingValue") && headingObj["headingValue"].IsNumber())
                                    ? headingObj["headingValue"].GetFloat() : 0.0f;
                        }
                        if (basicVehicle.HasMember("speed") && basicVehicle["speed"].IsObject()) {
                            const rj::Value& speedObj = basicVehicle["speed"];
                            speed = (speedObj.HasMember("speedValue") && speedObj["speedValue"].IsFloat())
                                    ? speedObj["speedValue"].GetFloat() : NOT_PRESENT_FLOAT;
                        }
                        if(basicVehicle.HasMember("longitudinalAcceleration") && basicVehicle["longitudinalAcceleration"].IsObject()){
                            const rj::Value& accelerationObj = basicVehicle["longitudinalAcceleration"];
                            acceleration = (accelerationObj.HasMember("longitudinalAccelerationValue") && accelerationObj["longitudinalAccelerationValue"].IsFloat())
                                    ? accelerationObj["longitudinalAccelerationValue"].GetFloat() : NOT_PRESENT_FLOAT;
                            if (acceleration == NOT_PRESENT_FLOAT) {
                                acceleration = (accelerationObj.HasMember("value") && accelerationObj["value"].IsFloat())
                                            ? accelerationObj["value"].GetFloat() : NOT_PRESENT_FLOAT;
                            }
                        }
                    }
                }
                std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
                
                {
                    std::lock_guard<std::mutex> lock(camMtx_);
                    camDataMap_[id] = {id, speed, heading, altitude, acceleration, timestamp};
                }
                spdlog::debug("[Locator] Updated own station data: id={}, speed={}, heading={}, altitude={}, acceleration={}", id, speed, heading, altitude, acceleration);
            }

        } else if (topic == "vanetza/in/vam") {
            if (doc.HasMember("vamParameters") && doc["vamParameters"].IsObject()) {
                const rj::Value& vamParameters = doc["vamParameters"];
                float speed = NOT_PRESENT_FLOAT;
                float heading = NOT_PRESENT_FLOAT;
                float altitude = NOT_PRESENT_FLOAT;
                float acceleration = NOT_PRESENT_FLOAT;
                int id = station_id_;
                if (vamParameters.HasMember("basicContainer") && vamParameters["basicContainer"].IsObject()) {
                    const rj::Value& basicContainer = vamParameters["basicContainer"];
                    if (basicContainer.HasMember("referencePosition") && basicContainer["referencePosition"].IsObject()) {
                        const rj::Value& referencePosition = basicContainer["referencePosition"];
                        altitude = (referencePosition.HasMember("altitude") && referencePosition["altitude"].IsFloat())
                              ? referencePosition["altitude"]["altitudeValue"].GetFloat() : NOT_PRESENT_FLOAT;
                    }
                }
                if (vamParameters.HasMember("vruHighFrequencyContainer") && vamParameters["vruHighFrequencyContainer"].IsObject()) {
                    const rj::Value& hfContainer = vamParameters["vruHighFrequencyContainer"];
                    if (hfContainer.HasMember("basicVehicleContainerHighFrequency") && hfContainer["basicVehicleContainerHighFrequency"].IsObject()) {
                        const rj::Value& basicVehicle = hfContainer["basicVehicleContainerHighFrequency"];
                        if (basicVehicle.HasMember("heading") && basicVehicle["heading"].IsObject()) {
                            const rj::Value& headingObj = basicVehicle["heading"];
                            heading = (headingObj.HasMember("headingValue") && headingObj["headingValue"].IsFloat())
                                      ? headingObj["headingValue"].GetFloat() : 0.0f;
                        }
                        if (basicVehicle.HasMember("speed") && basicVehicle["speed"].IsObject()) {
                            const rj::Value& speedObj = basicVehicle["speed"];
                            speed = (speedObj.HasMember("speedValue") && speedObj["speedValue"].IsFloat())
                                      ? speedObj["speedValue"].GetFloat() : NOT_PRESENT_FLOAT;
                        }
                        if(basicVehicle.HasMember("longitudinalAcceleration") && basicVehicle["longitudinalAcceleration"].IsObject()){
                            const rj::Value& accelerationObj = basicVehicle["longitudinalAcceleration"];
                            acceleration = (accelerationObj.HasMember("longitudinalAccelerationValue") && accelerationObj["longitudinalAccelerationValue"].IsFloat())
                                      ? accelerationObj["longitudinalAccelerationValue"].GetFloat() : NOT_PRESENT_FLOAT;
                        }
                    }
                }
                std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
                
                {
                    std::lock_guard<std::mutex> lock(camMtx_);
                    camDataMap_[id] = {id, speed, heading, altitude, acceleration, timestamp};
                }
                spdlog::debug("[Locator] Updated own station data: id={}, speed={}, heading={}, altitude={}, acceleration={}", id, speed, heading, altitude, acceleration);
            }

        } else if (topic == "vanetza/out/cam") {
            // Extract stationID
            int station_id = (doc.HasMember("fields") && doc["fields"].IsObject() && doc["fields"].HasMember("header") && doc["fields"]["header"].IsObject() && doc["fields"]["header"].HasMember("stationID") && doc["fields"]["header"]["stationID"].IsInt())
                ? doc["fields"]["header"]["stationID"].GetInt() : -1;
            
            // If not found, try alternative stationId
            station_id = (doc.HasMember("fields") && doc["fields"].IsObject() && doc["fields"].HasMember("header") && doc["fields"]["header"].IsObject() && doc["fields"]["header"].HasMember("stationId") && doc["fields"]["header"]["stationId"].IsInt())
                ? doc["fields"]["header"]["stationId"].GetInt() : station_id;
            if (station_id == -1) {
                spdlog::warn("[Locator] No station Id in message: {}", message);
                return;
            }
            if (doc.HasMember("fields") && doc["fields"].IsObject() && doc["fields"].HasMember("cam") && doc["fields"]["cam"].IsObject() && doc["fields"]["cam"].HasMember("camParameters") && doc["fields"]["cam"]["camParameters"].IsObject()) {
                const rj::Value& camParameters = doc["fields"]["cam"]["camParameters"];
                float speed = NOT_PRESENT_FLOAT;
                float heading = NOT_PRESENT_FLOAT;
                float altitude = NOT_PRESENT_FLOAT;
                float acceleration = NOT_PRESENT_FLOAT;
                if (camParameters.HasMember("basicContainer") && camParameters["basicContainer"].IsObject()) {
                    const rj::Value& basicContainer = camParameters["basicContainer"];
                    if (basicContainer.HasMember("referencePosition") && basicContainer["referencePosition"].IsObject()) {
                    const rj::Value& referencePosition = basicContainer["referencePosition"];
                    altitude = (referencePosition.HasMember("altitude") && referencePosition["altitude"].IsObject() &&
                            referencePosition["altitude"].HasMember("altitudeValue") &&
                            referencePosition["altitude"]["altitudeValue"].IsFloat())
                        ? referencePosition["altitude"]["altitudeValue"].GetFloat() : NOT_PRESENT_FLOAT;   
                    }
                }
                if (camParameters.HasMember("highFrequencyContainer") && camParameters["highFrequencyContainer"].IsObject()) {
                    const rj::Value& hfContainer = camParameters["highFrequencyContainer"];
                    if (hfContainer.HasMember("basicVehicleContainerHighFrequency") && hfContainer["basicVehicleContainerHighFrequency"].IsObject()) {
                        const rj::Value& basicVehicle = hfContainer["basicVehicleContainerHighFrequency"];
                        if (basicVehicle.HasMember("heading") && basicVehicle["heading"].IsObject()) {
                            const rj::Value& headingObj = basicVehicle["heading"];
                            heading = (headingObj.HasMember("headingValue") && headingObj["headingValue"].IsFloat())
                                      ? headingObj["headingValue"].GetFloat() : 0.0f;
                        }
                        if (basicVehicle.HasMember("speed") && basicVehicle["speed"].IsObject()) {
                            const rj::Value& speedObj = basicVehicle["speed"];
                            speed = (speedObj.HasMember("speedValue") && speedObj["speedValue"].IsFloat())
                                      ? speedObj["speedValue"].GetFloat() : NOT_PRESENT_FLOAT;
                        }
                        if(basicVehicle.HasMember("longitudinalAcceleration") && basicVehicle["longitudinalAcceleration"].IsObject()){
                            const rj::Value& accelerationObj = basicVehicle["longitudinalAcceleration"];
                            acceleration = (accelerationObj.HasMember("longitudinalAccelerationValue") && accelerationObj["longitudinalAccelerationValue"].IsFloat())
                                      ? accelerationObj["longitudinalAccelerationValue"].GetFloat() : NOT_PRESENT_FLOAT;
                            // Fallback to "value" if "longitudinalAccelerationValue" is not present
                            if (acceleration == NOT_PRESENT_FLOAT) {
                                acceleration = (accelerationObj.HasMember("value") && accelerationObj["value"].IsFloat())
                                               ? accelerationObj["value"].GetFloat() : NOT_PRESENT_FLOAT;
                            }

                        }
                    }
                }
                std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
                
                {
                    std::lock_guard<std::mutex> lock(camMtx_);
                    camDataMap_[station_id] = {station_id, speed, heading, altitude, acceleration, timestamp};
                }
                spdlog::debug("[Locator] Updated station data: id={}, speed={}, heading={}, altitude={}, acceleration={}", station_id, speed, heading, altitude, acceleration);
            }
        } else {
            spdlog::error("[Locator] Unknown topic: {}", topic);
            throw std::runtime_error("[Locator] Unknown topic");
        }
    } catch (const std::exception& e) {
        spdlog::error("[Locator] Error parsing location message: {}", e.what());
    }
}