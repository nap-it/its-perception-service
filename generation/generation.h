#ifndef GENERATION_H
#define GENERATION_H

#include "aggregator.h"
#include "builder.h"
#include "locator.h"
#include <memory>
#include <atomic>
#include <thread>
#include "fastdds-cpp-wrapper/dds.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

class Generation {
public:
    /**
     * @brief Construct a new Generation object.
     * @param aggregator Aggregator object.
     * @param locator Locator object.
     * @param requestRateMs Initial request rate in milliseconds.
     * @param ddsDomain DDS domain ID if provider is DDS.
     * @param ddsTopic DDS topic to subscribe for CAMs if provider is DDS.
     * @param performanceLogs Enable performance logs.
     * @param maxObjects Maximum number of objects to get from the aggregator (default: -1 for no limit).
     * @param mqttDebug Enable MQTT debug logs (default: false).
     */
    Generation(std::shared_ptr<Aggregator> aggregator, std::shared_ptr<Locator> locator, int requestRateMs, int ddsDomain, std::string ddsTopic, bool performanceLogs = false, int maxObjects = -1, bool mqttDebug = false);

    /**
     * @brief Destroy the Generation object.
     */
    ~Generation();

    /**
     * @brief Start the generation loop.
     */
    void run();

    /**
     * @brief Stop the generation loop.
     */
    void stop();

    /**
     * @brief Dynamically update the request rate.
     * @param newRateMs New request rate in milliseconds.
     */
    void setRequestRate(int newRateMs);

    /**
     * @brief Get the current request rate.
     * @return int Request rate in milliseconds.
     */
    int getRequestRate() const { return requestRateMs_.load(); }

private:
    std::shared_ptr<Aggregator> aggregator_;
    std::shared_ptr<Locator> locator_;
    bool performanceLogs_;
    int maxObjects_;
    std::atomic<int> requestRateMs_;
    std::atomic<bool> stopFlag_;
    std::thread generationThread_;
    Builder builder_;

    // File logger
    std::shared_ptr<spdlog::logger> generation_file_logger_;

    // DDS client
    Dds* dds_;
    std::string ddsTopic_;

    // MQTT client 
    bool mqttDebug_;
    MqttWrapper* mqttClient_;

    /**
     * @brief Main loop that periodically retrieves fresh objects, sensor data and constructs CPMs.
     */
    void runLoop();
};

#endif // GENERATION_H
