#include "locator.h"
#include "processor.h"
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

    int locator_domain = reader.GetInteger("locator", "domain_id", 0);
    spdlog::info("[CONFIG] Locator DDS domain: {}", locator_domain);
    int locator_station_id = reader.GetInteger("locator", "station_id", 1);
    spdlog::info("[CONFIG] Locator station ID: {}", locator_station_id);

    std::string locator_location_provider = reader.Get("locator", "location_provider", "static");
    ProviderType provider;
    if (locator_location_provider == "static") {
        provider = ProviderType::STATIC;
    } else if (locator_location_provider == "mqtt") {
        provider = ProviderType::MQTT;
    } else if (locator_location_provider == "dds") {
        provider = ProviderType::DDS;
    } else {
        spdlog::error("[CONFIG] Unknown locator location provider: {}", locator_location_provider);
        return 1;
    }

    spdlog::info("[CONFIG] Locator location provider: {}", locator_location_provider);
    std::string locator_mqtt_host = reader.Get("locator", "mqtt_host", "127.0.0.1");
    spdlog::info("[CONFIG] Locator MQTT host: {}", locator_mqtt_host);
    int locator_mqtt_port = reader.GetInteger("locator", "mqtt_port", 1883);

    std::string mqtt_broker = "tcp://" + locator_mqtt_host + ":" + std::to_string(locator_mqtt_port);

    spdlog::info("[CONFIG] Locator MQTT port: {}", locator_mqtt_port);
    std::string locator_mqtt_topic = reader.Get("locator", "mqtt_topic", "");
    spdlog::info("[CONFIG] Locator MQTT topic: {}", locator_mqtt_topic);
    std::string locator_dds_topic = reader.Get("locator", "dds_topic", "");
    spdlog::info("[CONFIG] Locator DDS topic: {}", locator_dds_topic);

    std::shared_ptr<Locator> locator = std::make_shared<Locator>(provider, locator_station_id, mqtt_broker, locator_mqtt_topic, locator_domain, locator_dds_topic);

    locator->run();

    Config config;
    config.debug = reader.GetBoolean("processing", "debug", false);
    spdlog::info("[CONFIG] Processir debug mode: {}", config.debug);
    config.dds_domain = reader.GetInteger("processing", "domain_id", 0);
    spdlog::info("[CONFIG] DDS domain: {}", config.dds_domain);
    config.host_station_type = reader.GetInteger("processing", "station_type", 5);
    spdlog::info("[CONFIG] Host station type: {}", config.host_station_type);
    config.host_station_id = reader.GetInteger("processing", "station_id", 1);
    spdlog::info("[CONFIG] Host station id: {}", config.host_station_id);
    config.cpm_topic = reader.Get("processing", "cpm_topic", "cps-v2/in/cpm");
    spdlog::info("[CONFIG] CPM topic: {}", config.cpm_topic);
    config.dds_output_topic = reader.Get("processing", "dds_output_topic", "objects");
    spdlog::info("[CONFIG] DDS output topic: {}", config.dds_output_topic);
    config.dds_output_full_topic = reader.Get("processing", "dds_output_full_topic", "objects_full");
    spdlog::info("[CONFIG] DDS output full topic: {}", config.dds_output_full_topic);
    config.repeat_id_interval = reader.GetInteger("processing", "repeat_id_interval", 3600);
    spdlog::info("[CONFIG] Repeat ID interval: {}", config.repeat_id_interval);
    config.local_mqtt_enabled = reader.GetBoolean("processing", "local_mqtt_enable_publisher", true);
    spdlog::info("[CONFIG] Local MQTT enabled: {}", config.local_mqtt_enabled);
    config.local_mqtt_host = reader.Get("processing", "local_mqtt_host", "127.0.0.1");
    spdlog::info("[CONFIG] Local MQTT host: {}", config.local_mqtt_host);
    config.local_mqtt_port = reader.GetInteger("processing", "local_mqtt_port", 1883);
    spdlog::info("[CONFIG] Local MQTT port: {}", config.local_mqtt_port);
    config.local_mqtt_output_topic = reader.Get("processing", "local_mqtt_output_topic", "objects");
    spdlog::info("[CONFIG] Local MQTT output topic: {}", config.local_mqtt_output_topic);
    config.local_mqtt_output_full_topic = reader.Get("processing", "local_mqtt_output_full_topic", "objects_full");
    spdlog::info("[CONFIG] Local MQTT output full topic: {}", config.local_mqtt_output_full_topic);
    config.remote_mqtt_enabled = reader.GetBoolean("processing", "remote_mqtt_enable_publisher", false);
    spdlog::info("[CONFIG] Remote MQTT enabled: {}", config.remote_mqtt_enabled);
    config.remote_mqtt_host = reader.Get("processing", "remote_mqtt_host", "127.0.0.1");
    spdlog::info("[CONFIG] Remote MQTT host: {}", config.remote_mqtt_host);
    config.remote_mqtt_port = reader.GetInteger("processing", "remote_mqtt_port", 1883);
    spdlog::info("[CONFIG] Remote MQTT port: {}", config.remote_mqtt_port);
    config.remote_mqtt_username = reader.Get("processing", "remote_mqtt_username", "");
    spdlog::info("[CONFIG] Remote MQTT username: {}", config.remote_mqtt_username);
    config.remote_mqtt_password = reader.Get("processing", "remote_mqtt_password", "");
    spdlog::info("[CONFIG] Remote MQTT password: {}", config.remote_mqtt_password);
    config.remote_mqtt_output_topic = reader.Get("processing", "remote_mqtt_output_topic", "objects");
    spdlog::info("[CONFIG] Remote MQTT output topic: {}", config.remote_mqtt_output_topic);
    config.remote_mqtt_output_full_topic = reader.Get("processing", "remote_mqtt_output_full_topic", "objects_full");
    spdlog::info("[CONFIG] Remote MQTT output full topic: {}", config.remote_mqtt_output_full_topic);

    std::shared_ptr<Processor> processor = std::make_shared<Processor>(config, locator);

    processor->run();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}

