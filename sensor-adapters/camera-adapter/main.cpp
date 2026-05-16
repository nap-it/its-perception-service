#include <camera_adapter.h>
#include "config_reader.h"
#include <atomic>
#include <csignal>
#include <unistd.h>

static std::atomic<bool> g_shutdown_requested{false};

void signalHandler(int signum) {
    const char* signal_name = (signum == SIGTERM) ? "SIGTERM" : (signum == SIGINT) ? "SIGINT" : "UNKNOWN";
    std::string msg = std::string("[main] Received ") + signal_name + " - initiating graceful shutdown...\n";
    write(STDOUT_FILENO, msg.c_str(), msg.length());
    g_shutdown_requested.store(true);
}

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
    config.mqtt_host = reader.Get("camera-adapter", "mqtt_host", "127.0.0.1");
    spdlog::info("[CAMERA-CONFIG] MQTT Host: {}", config.mqtt_host);
    config.mqtt_port = reader.GetInteger("camera-adapter", "mqtt_port", 1883);
    spdlog::info("[CAMERA-CONFIG] MQTT Port: {}", config.mqtt_port);
    std::string configuration_mqtt_topics = reader.Get("camera-adapter", "mqtt_topics", "jetson/camera/1/tracking/objects,jetson/camera/2/tracking/objects,jetson/camera/3/tracking/objects,jetson/camera/4/tracking/objects");
    std::istringstream ss(configuration_mqtt_topics);
    std::string topic;
    while (std::getline(ss, topic, ',')) {
        config.mqtt_topics.push_back(topic);
        spdlog::info("[CAMERA-CONFIG] MQTT Topic added: {}", topic);
    }
    config.mqtt_client_id = reader.Get("camera-adapter", "mqtt_client_id", "camera-adapter");
    spdlog::info("[CAMERA-CONFIG] MQTT Client ID: {}", config.mqtt_client_id);
}

int main(int argc, char* argv[]) {
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);

    Config config;
    readConfigFile("../config.ini", config);

    CameraAdapter adapter(config);
    std::thread camera_thread(&CameraAdapter::run, &adapter);

    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    adapter.stop();
    if (camera_thread.joinable()) {
        camera_thread.join();
    }

    spdlog::info("[main] Graceful shutdown complete.");
    return 0;
}