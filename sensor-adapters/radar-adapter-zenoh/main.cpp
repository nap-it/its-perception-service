#include <radar_adapter.h>
#include "config_reader.h"

/**
 * Read configuration file
 * @param path Path to configuration file
 * @param config Configuration struct
 */
void readConfigFile(const std::string& path, Config& config) {
    INIReader reader (path);

    if (reader.ParseError() < 0) {
        spdlog::error("Can't load 'config.ini'");
        return;
    }

    // Read configuration
    config.debug = reader.GetBoolean("radar-adapter", "debug", false);
    spdlog::info("[RADAR-CONFIG] Debug: {}", config.debug);
    config.shared_memory = reader.GetBoolean("radar-adapter", "shared_memory", true);
    spdlog::info("[RADAR-CONFIG] Shared Memory: {}", config.shared_memory);
    config.zenoh_endpoint = reader.Get("radar-adapter", "zenoh_endpoint", "");
    spdlog::info("[RADAR-CONFIG] Zenoh Endpoint: {}", config.zenoh_endpoint);
    config.mqtt_host = reader.Get("radar-adapter", "mqtt_host", "");
    spdlog::info("[RADAR-CONFIG] MQTT Host: {}", config.mqtt_host);
    config.mqtt_port = reader.GetInteger("radar-adapter", "mqtt_port", 0);
    spdlog::info("[RADAR-CONFIG] MQTT Port: {}", config.mqtt_port);
    config.mqtt_topic = reader.Get("radar-adapter", "mqtt_topic", "");
    spdlog::info("[RADAR-CONFIG] MQTT Topic: {}", config.mqtt_topic);
    config.mqtt_client_id = reader.Get("radar-adapter", "mqtt_client_id", "");
    spdlog::info("[RADAR-CONFIG] MQTT Client ID: {}", config.mqtt_client_id);
}

int main(int argc, char* argv[]) {
    Config config;
    readConfigFile("../config.ini", config);

    RadarAdapter adapter(config);
    std::thread radar_thread(&RadarAdapter::run, &adapter);

    radar_thread.join(); 
    return 0;
}