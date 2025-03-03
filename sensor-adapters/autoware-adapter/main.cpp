#include <autoware_adapter.h>
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
    config.domain_id = reader.GetInteger("autoware-adapter", "domain_id", 0);
    spdlog::info("[AUTOWARE-CONFIG] Domain ID: {}", config.domain_id);
    config.debug = reader.GetBoolean("autoware-adapter", "debug", false);
    spdlog::info("[AUTOWARE-CONFIG] Debug: {}", config.debug);
}

int main(int argc, char* argv[]) {
    Config config;
    readConfigFile("../config.ini", config);

    AutowareAdapter adapter(config);
    std::thread radar_thread(&AutowareAdapter::run, &adapter);

    radar_thread.join(); 
    return 0;
}