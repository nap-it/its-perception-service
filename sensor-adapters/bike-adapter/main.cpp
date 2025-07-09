#include <bike_adapter.h>
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
    config.domain_id = reader.GetInteger("bike-adapter", "domain_id", 0);
    spdlog::info("[BIKE-CONFIG] Domain ID: {}", config.domain_id);
    config.debug = reader.GetBoolean("bike-adapter", "debug", false);
    spdlog::info("[BIKE-CONFIG] Debug: {}", config.debug);
    config.mqtt_host = reader.Get("bike-adapter", "mqtt_host", "127.0.0.1");
    spdlog::info("[BIKE-CONFIG] MQTT Host: {}", config.mqtt_host);
    config.mqtt_port = reader.GetInteger("bike-adapter", "mqtt_port", 1883);
    spdlog::info("[BIKE-CONFIG] MQTT Port: {}", config.mqtt_port);
    config.mqtt_topic = reader.Get("bike-adapter", "mqtt_topic", "rpi/cam_detections");
    spdlog::info("[BIKE-CONFIG] MQTT Topic: {}", config.mqtt_topic);
    config.mqtt_client_id = reader.Get("bike-adapter", "mqtt_client_id", "bike-adapter");
    spdlog::info("[BIKE-CONFIG] MQTT Client ID: {}", config.mqtt_client_id);
}

int main(int argc, char* argv[]) {
    Config config;
    readConfigFile("../config.ini", config);

    BikeAdapter adapter(config);
    std::thread camera_thread(&BikeAdapter::run, &adapter);

    camera_thread.join(); 
    return 0;
}