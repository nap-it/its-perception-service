#include <autoware_adapter.h>
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
    config.domain_id = reader.GetInteger("autoware-adapter", "domain_id", 0);
    spdlog::info("[AUTOWARE-CONFIG] Domain ID: {}", config.domain_id);
    config.debug = reader.GetBoolean("autoware-adapter", "debug", false);
    spdlog::info("[AUTOWARE-CONFIG] Debug: {}", config.debug);
}

int main(int argc, char* argv[]) {
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);

    Config config;
    readConfigFile("../config.ini", config);

    AutowareAdapter adapter(config);
    std::thread adapter_thread(&AutowareAdapter::run, &adapter);

    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    adapter.stop();
    if (adapter_thread.joinable()) {
        adapter_thread.join();
    }

    spdlog::info("[main] Graceful shutdown complete.");
    return 0;
}