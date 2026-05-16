/**
 * @file locator.h
 * @brief Sender station data provider for the CPS Processing service
 * @date 2026
 *
 * This file defines the Locator class used by the Processing service to track
 * the kinematic state (speed, heading, altitude, acceleration) of CPM-originating
 * stations by listening to their CAM broadcasts.
 *
 * The Processing service uses this information to annotate each processed object
 * message with accurate sender metadata (speed, heading, altitude, acceleration).
 *
 * Three provider modes are supported:
 * - STATIC: Serves a fixed, minimal SenderInfo for the configured station ID.
 *           Used when the processing station does not receive external CAMs.
 * - MQTT:   Subscribes to a CAM topic on a local MQTT broker.
 * - DDS:    Subscribes to a CAM topic on the DDS bus.
 *
 * Supported input topics (MQTT and DDS):
 *   vanetza/in/cam, vanetza/own/cam, vanetza/out/cam,
 *   vanetza/in/cam_full (legacy), vanetza/in/vam, vanetza/out/cam_full (legacy)
 *
 * Thread Safety:
 * The CAM data map (camDataMap_) is protected by camMtx_. Updates arrive from
 * the MQTT/DDS callback threads; reads occur from the Processor's message thread.
 */

#ifndef LOCATOR_H
#define LOCATOR_H

#include <string>
#include <mutex>
#include <atomic>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "mqttwrapper.h"
#include "fastdds-cpp-wrapper/dds.hpp"

namespace rj = rapidjson;

constexpr float NOT_PRESENT_FLOAT = -999.0f;
constexpr double NOT_PRESENT_DOUBLE = -999.0;
constexpr int NOT_PRESENT_INT = -999;

enum class ProviderType {
    STATIC,
    MQTT,
    DDS
};

struct SenderInfo {
    int station_id;   ///< Sender station identifier.
    float speed;        ///< Station speed in m/s.
    float heading;      ///< Station heading in degrees.
    float altitude;     ///< Station altitude in meters.
    float acceleration; ///< Station acceleration in m/s².
    std::chrono::steady_clock::time_point cam_timestamp; ///< Timestamp when CAM was received.
};

class Locator {
public:
    /**
     * @brief Construct a new Locator object for the Processing service.
     *
     * @param provider The provider type: STATIC, MQTT, or DDS.
     * @param configStationId Station ID from configuration (used in STATIC mode).
     * @param mqttBroker MQTT broker address if provider is MQTT.
     * @param mqttTopic MQTT topic to subscribe for CAMs if provider is MQTT.
     * @param ddsDomain DDS domain ID if provider is DDS.
     * @param ddsTopic DDS topic to subscribe for CAMs if provider is DDS.
     */
    Locator(ProviderType provider,
                   int configStationId,
                   std::string mqttBroker,
                   std::string mqttTopic, 
                   int ddsDomain,
                   std::string ddsTopic);
    
    /**
     * @brief Destroy the Locator object.
     */
    ~Locator();

    /**
     * @brief Start the Locator's main loop in its own thread.
     * Manages subscriptions to CAM topics depending on the provider mode.
     */
    void run();

    void stop();

    /**
     * @brief Retrieve the latest kinematic state for a given sender station.
     * Returns cached CAM data if available, or default values in STATIC mode.
     * @param stationId The station identifier to query.
     * @return SenderInfo The latest kinematic state for the station.
     */
    SenderInfo getStationData(int stationId);
private:

    std::unordered_map<int, SenderInfo> camDataMap_; ///< Cache of latest CAM data indexed by station ID.
    std::mutex camMtx_;                              ///< Protects access to camDataMap_.

    // Station ID for own messages.
    int station_id_; ///< Configured station ID for handling own CAM broadcasts.

    std::thread locatorThread_; ///< Worker thread that runs the Locator's main loop.
    std::atomic<bool> stopFlag_;

    // Provider type chosen at construction.
    ProviderType provider_; ///< Location provider mode: STATIC, MQTT, or DDS.

    // MQTT client 
    MqttWrapper* mqttClient_; ///< Optional MQTT client used in MQTT provider mode.

    // MQTT topic to subscribe to for CAM location messages.
    std::string mqttTopic_;   ///< MQTT topic for CAM subscription.

    // DDS client
    Dds* dds_;                ///< Optional DDS client used in DDS provider mode.

    // DDS topic to subscribe to for CAM location messages.
    std::string ddsTopic_;    ///< DDS topic for CAM subscription.

    /**
     * @brief Internal callback for MQTT messages.
     * Invoked when a CAM message arrives on the subscribed MQTT topic.
     * @param topic The MQTT topic.
     * @param message The received message (JSON formatted).
     */
    void on_message_mqtt(const std::string& topic, const std::string& message);

    /**
     * @brief Internal callback for DDS messages.
     * Invoked when a CAM message arrives on the subscribed DDS topic.
     * @param topic The DDS topic.
     * @param message The received message (JSON formatted).
     */
    void on_message_dds(const std::string& topic, const std::string& message);

    /**
     * @brief Parse a CAM message and update the sender data cache.
     * Extracts station ID, speed, heading, altitude, and acceleration from incoming CAMs.
     * @param topic The message topic.
     * @param message The raw message (JSON formatted).
     */
    void parseAndUpdateData(const std::string& topic, const std::string& message);

    /**
     * @brief Cleanup stale entries from the CAM data map.
     * Removes cached data for stations from which no CAM has been received recently.
     */
    void cleanupDataMap();

    /**
     * @brief Main loop executed in the worker thread.
     * Manages subscriptions and event processing depending on the provider mode.
     */
    void runLoop();
};

#endif // LOCATOR_H