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
    config.domain_id = reader.GetInteger("radar-adapter", "domain_id", 0);
    spdlog::info("[RADAR-CONFIG] Domain ID: {}", config.domain_id);
    config.debug = reader.GetBoolean("radar-adapter", "debug", false);
    spdlog::info("[RADAR-CONFIG] Debug: {}", config.debug);
    config.mqtt_host = reader.Get("radar-adapter", "mqtt_host", "127.0.0.1");
    spdlog::info("[RADAR-CONFIG] MQTT Host: {}", config.mqtt_host);
    config.mqtt_port = reader.GetInteger("radar-adapter", "mqtt_port", 1883);
    spdlog::info("[RADAR-CONFIG] MQTT Port: {}", config.mqtt_port);
    std::string configuration_mqtt_topics = reader.Get("radar-adapter", "mqtt_topics", "jetson/radar-plus");
    std::istringstream ss(configuration_mqtt_topics);
    std::string topic;
    while (std::getline(ss, topic, ',')) {
        config.mqtt_topics.push_back(topic);
        spdlog::info("[RADAR-CONFIG] MQTT Topic added: {}", topic);
    }
    config.mqtt_client_id = reader.Get("radar-adapter", "mqtt_client_id", "radar-adapter");
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