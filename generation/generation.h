/**
 * @file generation.h
 * @brief CPM generation loop component for the Collective Perception Service
 * @date 2026
 *
 * This file defines the Generation class, responsible for driving the periodic
 * CPM (Collective Perception Message) production cycle.
 *
 * Key Responsibilities:
 * - Runs a configurable-rate loop (default 100 ms / 10 Hz)
 * - Queries the Aggregator for fresh objects on each cycle
 * - Retrieves current station location from the Locator
 * - Delegates CPM JSON construction to the Builder
 * - Publishes the resulting CPM on a DDS topic (e.g. vanetza/in/cpm)
 * - Optionally echoes the CPM to a local MQTT topic for debugging
 * - Collects Prometheus metrics when enabled
 *
 * Thread Safety:
 * Runs in its own thread via run()/stop(). The request rate can be updated
 * dynamically from any thread via setRequestRate().
 */

#ifndef GENERATION_H
#define GENERATION_H

#include "aggregator.h"
#include "builder.h"
#include "locator.h"
#include "metrics.h"
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
     * @param aggregator Shared pointer to Aggregator for querying fresh objects.
     * @param locator Shared pointer to Locator for station metadata.
     * @param requestRateMs Initial CPM generation rate in milliseconds.
     * @param ddsDomain DDS domain ID for CPM publication.
     * @param ddsTopic DDS topic name for CPM publication (e.g., "vanetza/in/cpm").
     * @param performanceLogs Enable performance timing logs (default: false).
     * @param maxObjects Maximum objects per CPM, -1 for unlimited (default: -1).
     * @param mqttDebug Enable MQTT debug output (default: false).
     * @param prometheus Enable Prometheus metrics collection (default: false).
     * @param metrics Pointer to metric handles provided by the caller (default: nullptr).
     */
    Generation(std::shared_ptr<Aggregator> aggregator, std::shared_ptr<Locator> locator, int requestRateMs, int ddsDomain, std::string ddsTopic, bool performanceLogs = false, int maxObjects = -1, bool mqttDebug = false, bool prometheus = false, GenMetricHandles* metrics = nullptr);

    /**
     * @brief Destroy the Generation object.
     */
    ~Generation();

    /**
     * @brief Start the generation loop in a worker thread.
     */
    void run();

    /**
     * @brief Stop the generation loop.
     */
    void stop();

    /**
     * @brief Dynamically update the CPM generation rate.
     * Can be called from any thread; the change takes effect on the next cycle.
     * @param newRateMs New generation rate in milliseconds.
     */
    void setRequestRate(int newRateMs);

    /**
     * @brief Get the current CPM generation rate.
     * @return int Current generation rate in milliseconds.
     */
    int getRequestRate() const { return requestRateMs_.load(); }

private:
    std::shared_ptr<Aggregator> aggregator_;  ///< Shared reference to the Aggregator for querying fresh objects.
    std::shared_ptr<Locator> locator_;        ///< Shared reference to the Locator for station metadata.
    bool performanceLogs_;                    ///< Enables performance timing logs.
    int maxObjects_;                          ///< Maximum objects per CPM, -1 for no limit.
    std::atomic<int> requestRateMs_;          ///< Current request rate in milliseconds.
    std::atomic<bool> stopFlag_;              ///< Signals the generation loop to stop.
    std::thread generationThread_;            ///< Worker thread that executes the main generation loop.
    Builder builder_;                         ///< Stateless CPM JSON builder.

    // File logger
    std::shared_ptr<spdlog::logger> generation_file_logger_; ///< File-backed logger for diagnostics.

    // DDS client
    Dds* dds_;                ///< DDS client used for publishing CPMs.
    std::string ddsTopic_;    ///< DDS topic for CPM publication.

    // MQTT client 
    bool mqttDebug_;          ///< When true, echo CPMs to MQTT for debugging.
    MqttWrapper* mqttClient_; ///< Optional MQTT client for debug output.

    // Metrics
    bool prometheus_;           ///< Enables Prometheus metrics collection.
    GenMetricHandles* metrics_; ///< Pointer to metric handles provided by the caller.

    /**
     * @brief Main loop that periodically retrieves fresh objects, sensor data and constructs CPMs.
     */
    void runLoop();
};

#endif // GENERATION_H
