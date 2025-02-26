#include "locator.h"
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

Locator::Locator(ProviderType provider,
                   float configLatitude,
                   float configLongitude,
                   int configStationType,
                   std::string mqttBroker,
                   std::string mqttTopic,
                   int ddsDomain,
                   std::string ddsTopic,
                   bool debug)
    : provider_(provider),
      stationType_(configStationType),
      latestLatitude_(configLatitude),
      latestLongitude_(configLongitude),
      mqttClient_(nullptr),
      dds_(nullptr),
      mqttTopic_(mqttTopic),
      ddsTopic_(ddsTopic)
{
    spdlog::info("[Locator] Constructing with provider type: {}", (provider_ == ProviderType::STATIC ? "STATIC" : (provider_ == ProviderType::MQTT ? "MQTT" : "DDS")));

    if (debug) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("[Locator] Debug logging enabled.");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("[Locator] Info logging enabled.");
    }

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
}

void Locator::run() {
    locatorThread_ = std::thread(&Locator::runLoop, this);
    spdlog::info("[Locator] run loop started.");
}

void Locator::runLoop() {
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

double Locator::getStationLatitude() {
    std::lock_guard<std::mutex> lock(mtx_);
    return latestLatitude_;
}

double Locator::getStationLongitude() {
    std::lock_guard<std::mutex> lock(mtx_);
    return latestLongitude_;
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
        json j = json::parse(message);
        
        if (topic == "vanetza/in/cam" || topic == "vanetza/own/cam") {
            double lat = j.value("latitude", latestLatitude_);
            double lon = j.value("longitude", latestLongitude_);
        
            {
                std::lock_guard<std::mutex> lock(mtx_);
                latestLatitude_ = lat;
                latestLongitude_ = lon;
            }

            spdlog::debug("[Locator] Updated dynamic location: lat {:.6f}, lon {:.6f}", lat, lon);
        
        } else if (topic == "vanetza/in/cam_full") {
            double lat = j.value("camParameters", json::object()).value("basicContainer", json::object()).value("referencePosition", json::object()).value("latitude", latestLatitude_);
            double lon = j.value("camParameters", json::object()).value("basicContainer", json::object()).value("referencePosition", json::object()).value("longitude", latestLongitude_);
        
            {
                std::lock_guard<std::mutex> lock(mtx_);
                latestLatitude_ = lat;
                latestLongitude_ = lon;
            }

            spdlog::debug("[Locator] Updated dynamic location: lat {:.6f}, lon {:.6f}", lat, lon);
        
            } else {
            spdlog::warn("[Locator] Unknown topic: {}", topic);
        }

    } catch (const std::exception& e) {
        spdlog::error("[Locator] Error parsing location message: {}", e.what());
    }
}