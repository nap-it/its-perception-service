#include "aggregator.h"
#include "generation.h"
#include "locator.h"
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include "config_reader.hpp"
#include <memory>

int main() {

    INIReader reader("../config.ini");

    if (reader.ParseError() < 0) {
        spdlog::error("Can't load 'config.ini'");
        return 1;
    }

    // General configuration
    bool debug = reader.GetBoolean("general", "debug", false);
    spdlog::info("[CONFIG] General debug: {}", debug);

    // Locator configuration
    
    
    int locator_dds_domain = reader.GetInteger("locator", "domain_id", 0);
    spdlog::info("[CONFIG] Locator DDS domain: {}", locator_dds_domain);
    int locator_station_type = reader.GetInteger("locator", "station_type", 0);
    spdlog::info("[CONFIG] Locator station type: {}", locator_station_type);
    float locator_station_latitude = reader.GetReal("locator", "station_latitude", 0.0);
    spdlog::info("[CONFIG] Locator station latitude: {}", locator_station_latitude);
    float locator_station_longitude = reader.GetReal("locator", "station_longitude", 0.0);
    spdlog::info("[CONFIG] Locator station longitude: {}", locator_station_longitude);
    std::string locator_mqtt_host = reader.Get("locator", "mqtt_host", "");
    spdlog::info("[CONFIG] Locator MQTT host: {}", locator_mqtt_host);
    int locator_mqtt_port = reader.GetInteger("locator", "mqtt_port", 0);
    spdlog::info("[CONFIG] Locator MQTT port: {}", locator_mqtt_port);
    std::string locator_mqtt_broker = "tcp://" + locator_mqtt_host + ":" + std::to_string(locator_mqtt_port);
    std::string locator_mqtt_topic = reader.Get("locator", "mqtt_topic", "");
    spdlog::info("[CONFIG] Locator MQTT topic: {}", locator_mqtt_topic);
    std::string locator_dds_topic = reader.Get("locator", "dds_topic", "");
    spdlog::info("[CONFIG] Locator DDS topic: {}", locator_dds_topic);
    std::string locator_location_provider = reader.Get("locator", "location_provider", "");
    spdlog::info("[CONFIG] Locator location provider: {}", locator_location_provider);

    ProviderType provider;
    if (locator_location_provider == "mqtt") {
        provider = ProviderType::MQTT;
    } else if (locator_location_provider == "dds") {
        provider = ProviderType::DDS;
    } else if (locator_location_provider == "static") {
        provider = ProviderType::STATIC;
    } else {
        spdlog::error("Unknown location provider: {}", locator_location_provider);
        return 1;
    }

    // Instantiate Locator
    auto locator = std::make_shared<Locator>(provider,
                                                locator_station_latitude,
                                                locator_station_longitude,
                                                locator_station_type,
                                                locator_mqtt_broker,
                                                locator_mqtt_topic,
                                                locator_dds_domain,
                                                locator_dds_topic,
                                                debug);
    locator->run(); // (if you implement a run loop in Locator)

    // Aggregator configuration
    int aggregator_dds_domain = reader.GetInteger("aggregator", "domain_id", 0);
    spdlog::info("[CONFIG] Aggregator DDS domain: {}", aggregator_dds_domain);
    int aggregator_max_object_age = reader.GetInteger("aggregator", "max_object_age", 300);
    spdlog::info("[CONFIG] Aggregator max object age: {}", aggregator_max_object_age);
    int aggregator_clean_interval = reader.GetInteger("aggregator", "clean_interval", 5);
    spdlog::info("[CONFIG] Aggregator clean interval: {}", aggregator_clean_interval);

    auto aggregator = std::make_shared<Aggregator>(aggregator_dds_domain,
                                                    aggregator_max_object_age,
                                                    aggregator_clean_interval,
                                                    debug);
    aggregator->run();

    // Generation configuration
    int generation_interval = reader.GetInteger("generation", "interval", 100);
    spdlog::info("[CONFIG] Generation interval: {}", generation_interval);
    int generation_dds_domain = reader.GetInteger("generation", "domain_id", 0);
    spdlog::info("[CONFIG] Generation DDS domain: {}", generation_dds_domain);
    std::string generation_dds_topic = reader.Get("generation", "dds_topic", "");
    spdlog::info("[CONFIG] Generation DDS topic: {}", generation_dds_topic);

    Generation generation(aggregator, 
                    locator, 
                    generation_interval, 
                    debug, 
                    generation_dds_domain, 
                    generation_dds_topic);
    generation.run();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    generation.stop();
    aggregator->stop();

    return 0;
}