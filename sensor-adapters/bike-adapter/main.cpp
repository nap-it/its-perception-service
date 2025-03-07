#include <camera_adapter.h>
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
    config.domain_id = reader.GetInteger("camera-adapter", "domain_id", 0);
    spdlog::info("[CAMERA-CONFIG] Domain ID: {}", config.domain_id);
    config.debug = reader.GetBoolean("camera-adapter", "debug", false);
    spdlog::info("[CAMERA-CONFIG] Debug: {}", config.debug);
    config.mqtt_host = reader.Get("camera-adapter", "mqtt_host", "");
    spdlog::info("[CAMERA-CONFIG] MQTT Host: {}", config.mqtt_host);
    config.mqtt_port = reader.GetInteger("camera-adapter", "mqtt_port", 0);
    spdlog::info("[CAMERA-CONFIG] MQTT Port: {}", config.mqtt_port);
    config.mqtt_topic = reader.Get("camera-adapter", "mqtt_topic", "");
    spdlog::info("[CAMERA-CONFIG] MQTT Topic: {}", config.mqtt_topic);
    config.mqtt_client_id = reader.Get("camera-adapter", "mqtt_client_id", "");
    spdlog::info("[CAMERA-CONFIG] MQTT Client ID: {}", config.mqtt_client_id);
}

int main(int argc, char* argv[]) {
    Config config;
    readConfigFile("../config.ini", config);

    CameraAdapter adapter(config);
    std::thread camera_thread(&CameraAdapter::run, &adapter);

    camera_thread.join(); 
    return 0;
}