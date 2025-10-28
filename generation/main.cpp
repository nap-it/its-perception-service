#include "aggregator.h"
#include "generation.h"
#include "locator.h"
#include "metrics.h"
#include "config_reader.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
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

    if (debug) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    // Start Prometheus exposer
    bool prometheus = reader.GetBoolean("general", "prometheus", false);
    spdlog::info("[CONFIG] General prometheus: {}", prometheus);
    int prometheus_port = reader.GetInteger("general", "prometheus_port", 9102);
    spdlog::info("[CONFIG] General prometheus port: {}", prometheus_port);

    GenMetricHandles* metrics;

    if (prometheus) {
        std::string listen_addr = "0.0.0.0:" + std::to_string(prometheus_port);
        MetricsManager::instance().init(listen_addr);
        metrics = new GenMetricHandles(MetricsManager::instance().createGenerationMetrics("v1.0.4"));
        spdlog::info("Prometheus metrics enabled on port {}", prometheus_port);
    }

    // Locator configuration
    int locator_dds_domain = reader.GetInteger("locator", "domain_id", 0);
    spdlog::info("[CONFIG] Locator DDS domain: {}", locator_dds_domain);
    int locator_station_type = reader.GetInteger("locator", "station_type", 15);
    spdlog::info("[CONFIG] Locator station type: {}", locator_station_type);
    float locator_station_latitude = reader.GetReal("locator", "station_latitude", 0.0);
    spdlog::info("[CONFIG] Locator station latitude: {}", locator_station_latitude);
    float locator_station_longitude = reader.GetReal("locator", "station_longitude", 0.0);
    spdlog::info("[CONFIG] Locator station longitude: {}", locator_station_longitude);
    std::string locator_mqtt_host = reader.Get("locator", "mqtt_host", "127.0.0.1");
    spdlog::info("[CONFIG] Locator MQTT host: {}", locator_mqtt_host);
    int locator_mqtt_port = reader.GetInteger("locator", "mqtt_port", 1883);
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
                                                locator_dds_topic);
    locator->run();

    // Aggregator configuration
    int aggregator_dds_domain = reader.GetInteger("aggregator", "domain_id", 0);
    spdlog::info("[CONFIG] Aggregator DDS domain: {}", aggregator_dds_domain);
    int aggregator_max_object_age = reader.GetInteger("aggregator", "max_object_age", 2);
    spdlog::info("[CONFIG] Aggregator max object age: {}", aggregator_max_object_age);
    int aggregator_clean_interval = reader.GetInteger("aggregator", "clean_interval", 1);
    spdlog::info("[CONFIG] Aggregator clean interval: {}", aggregator_clean_interval);
    bool aggregator_ignore_rules = reader.GetBoolean("aggregator", "ignore_rules", false);
    spdlog::info("[CONFIG] Aggregator ignore rules: {}", aggregator_ignore_rules);
    bool aggregator_performance_logs = reader.GetBoolean("aggregator", "performance_logs", false);
    spdlog::info("[CONFIG] Aggregator performance logs: {}", aggregator_performance_logs);
    std::string aggregator_priority_type = reader.Get("aggregator", "priority", "etsi");
    spdlog::info("[CONFIG] Aggregator priority type: {}", aggregator_priority_type);
    std::string zenoh_endpoint = reader.Get("aggregator", "zenoh_endpoint", "");
    spdlog::info("[CONFIG] Aggregator Zenoh endpoint: {}", zenoh_endpoint);
    bool aggregator_add_pending_objects = reader.GetBoolean("aggregator", "add_pending_objects", false);
    spdlog::info("[CONFIG] Aggregator performance logs: {}", aggregator_add_pending_objects);

    auto aggregator = std::make_shared<Aggregator>(aggregator_dds_domain,
                                                    aggregator_max_object_age,
                                                    aggregator_clean_interval,
                                                    aggregator_ignore_rules, 
                                                    aggregator_performance_logs,
                                                    zenoh_endpoint,
                                                    aggregator_priority_type,
                                                    aggregator_add_pending_objects,
                                                    prometheus,
                                                    metrics);

    aggregator->run();

    // Generation configuration
    int generation_interval = reader.GetInteger("generation", "interval", 100);
    spdlog::info("[CONFIG] Generation interval: {}", generation_interval);
    int generation_dds_domain = reader.GetInteger("generation", "domain_id", 0);
    spdlog::info("[CONFIG] Generation DDS domain: {}", generation_dds_domain);
    std::string generation_dds_topic = reader.Get("generation", "dds_topic", "");
    spdlog::info("[CONFIG] Generation DDS topic: {}", generation_dds_topic);
    bool generation_performance_logs = reader.GetBoolean("generation", "performance_logs", false);
    spdlog::info("[CONFIG] Generation performance logs: {}", generation_performance_logs);
    int generation_max_objects = reader.GetInteger("generation", "max_objects", -1);
    spdlog::info("[CONFIG] Generation max objects: {}", generation_max_objects);
    bool generation_mqtt_debug = reader.GetBoolean("generation", "mqtt_debug", false);
    spdlog::info("[CONFIG] Generation mqtt debug: {}", generation_mqtt_debug);

    Generation generation(aggregator, 
                    locator, 
                    generation_interval, 
                    generation_dds_domain, 
                    generation_dds_topic,
                    generation_performance_logs,
                    generation_max_objects, 
                    generation_mqtt_debug,
                    prometheus,
                    metrics);
    generation.run();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    generation.stop();
    aggregator->stop();

    return 0;
}