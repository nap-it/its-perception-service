#include "locator.h"
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

Locator::Locator(ProviderType provider,
                   float configLatitude,
                   float configLongitude,
                   int configStationType,
                   std::string mqttBroker,
                   std::string mqttTopic,
                   int ddsDomain,
                   std::string ddsTopic)
        : provider_(provider),
        stationType_(configStationType),
        latestLatitude_(configLatitude),
        latestLongitude_(configLongitude),
        latestHeading_(0.0),
        mqttClient_(nullptr),
        dds_(nullptr),
        mqttTopic_(mqttTopic),
        ddsTopic_(ddsTopic),
        stopFlag_(false)
{
    spdlog::info("[Locator] Constructing with provider type: {}", (provider_ == ProviderType::STATIC ? "STATIC" : (provider_ == ProviderType::MQTT ? "MQTT" : "DDS")));

    if(provider_ == ProviderType::STATIC) {
        spdlog::info("[Locator] Using STATIC provider. Fixed location: lat {:.6f}, lon {:.6f}, type {}", latestLatitude_, latestLongitude_, stationType_);
    }
    else if(provider_ == ProviderType::MQTT) {
        // Set up MQTT client.
        data_mqtt_server mqttConfig;
        mqttConfig.address = mqttBroker;
        mqttConfig.client_id = "Locator-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        mqttConfig.subscription_topic.push_back(mqttTopic_);
        
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
    if(locatorThread_.joinable()) {
        locatorThread_.join();
    }
}

void Locator::run() {
    stopFlag_ = false;
    locatorThread_ = std::thread(&Locator::runLoop, this);
    spdlog::info("[Locator] run loop started.");
}

void Locator::stop() {
    stopFlag_ = true;
    spdlog::info("[Locator] stop requested.");
}

void Locator::runLoop() {
    while(!stopFlag_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    spdlog::info("[Locator] run loop exited.");
}

double Locator::getStationLatitude() {
    std::lock_guard<std::mutex> lock(mtx_);
    return latestLatitude_;
}

double Locator::getStationLongitude() {
    std::lock_guard<std::mutex> lock(mtx_);
    return latestLongitude_;
}

float Locator::getStationHeading() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (provider_ == ProviderType::STATIC) return 0.0;
    if (latestHeading_ < 0.0) return 0.0;
    if (latestHeading_ > 360.0) return 0.0;
    return latestHeading_;
}

int Locator::getStationType() {
    std::lock_guard<std::mutex> lock(mtx_);
    return stationType_;
}

void Locator::on_message_mqtt(const std::string& topic, const std::string& message) {
    spdlog::debug("[Locator] MQTT message received on topic '{}': {}", topic, message);
    parseAndUpdateLocation(topic, message);
}

void Locator::on_message_dds(const std::string& topic, const std::string& message) {
    spdlog::debug("[Locator] DDS message received on topic '{}': {}", topic, message);
    parseAndUpdateLocation(topic, message);
}

void Locator::parseAndUpdateLocation(const std::string& topic, const std::string& message) {
    try {
        rj::Document doc;
        doc.Parse(message.c_str());
        if (doc.HasParseError()) {
            spdlog::error("[Locator] Parse error in message: {}", message);
            return;
        }
        
        double lat = latestLatitude_;
        double lon = latestLongitude_;
        float heading = 0.0f;
        
        if (topic == "vanetza/in/cam") {
            if (doc.HasMember("camParameters") && doc["camParameters"].IsObject()) {
                const rj::Value& camParameters = doc["camParameters"];
                if (camParameters.HasMember("basicContainer") && camParameters["basicContainer"].IsObject()) {
                    const rj::Value& basicContainer = camParameters["basicContainer"];
                    if (basicContainer.HasMember("referencePosition") && basicContainer["referencePosition"].IsObject()) {
                        const rj::Value& referencePosition = basicContainer["referencePosition"];
                        lat = (referencePosition.HasMember("latitude") && referencePosition["latitude"].IsDouble())
                              ? referencePosition["latitude"].GetDouble() : latestLatitude_;
                        lon = (referencePosition.HasMember("longitude") && referencePosition["longitude"].IsDouble())
                              ? referencePosition["longitude"].GetDouble() : latestLongitude_;
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
                    }
                }
            }

        } else if (topic == "vanetza/in/vam") {
            if (doc.HasMember("vamParameters") && doc["vamParameters"].IsObject()) {
                const rj::Value& vamParameters = doc["vamParameters"];
                if (vamParameters.HasMember("basicContainer") && vamParameters["basicContainer"].IsObject()) {
                    const rj::Value& basicContainer = vamParameters["basicContainer"];
                    if (basicContainer.HasMember("referencePosition") && basicContainer["referencePosition"].IsObject()) {
                        const rj::Value& referencePosition = basicContainer["referencePosition"];
                        lat = (referencePosition.HasMember("latitude") && referencePosition["latitude"].IsNumber())
                              ? referencePosition["latitude"].GetDouble() : latestLatitude_;
                        lon = (referencePosition.HasMember("longitude") && referencePosition["longitude"].IsNumber())
                              ? referencePosition["longitude"].GetDouble() : latestLongitude_;
                    }
                }
            }
        } else {
            spdlog::warn("[Locator] Unknown topic: {}, trying to parse lat/lon/heading if available", topic);
            lat = doc.HasMember("latitude") && doc["latitude"].IsNumber() ? doc["latitude"].GetDouble() : latestLatitude_;
            lon = doc.HasMember("longitude") && doc["longitude"].IsNumber() ? doc["longitude"].GetDouble() : latestLongitude_;
            heading = doc.HasMember("heading") && doc["heading"].IsNumber() ? doc["heading"].GetFloat() : 0.0f;
            // throw std::runtime_error("[Locator] Unknown topic");
        }
        
        {
            std::lock_guard<std::mutex> lock(mtx_);
            latestLatitude_ = lat;
            latestLongitude_ = lon;
            latestHeading_ = heading;
        }
        
        spdlog::debug("[Locator] Updated dynamic location: lat {:.6f}, lon {:.6f}, heading {:.2f}",
                      lat, lon, heading);
    } catch (const std::exception& e) {
        spdlog::error("[Locator] Error parsing location message: {}", e.what());
    }
}