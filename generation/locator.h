/**
 * @file locator.h
 * @brief Station position provider for the CPS Generation and Processing services
 * @date 2026
 *
 * This file defines the Locator class, which supplies the current station
 * latitude, longitude, heading, and type to both the Generation and Processing
 * services.
 *
 * Three provider modes are supported:
 * - STATIC: Fixed coordinates read directly from config.ini. Suitable for RSUs.
 * - MQTT:   Subscribes to a CAM/VAM topic on a local MQTT broker and extracts
 *           position from incoming messages. Suitable for OBUs.
 * - DDS:    Subscribes to a CAM/VAM topic on the DDS bus. Alternative to MQTT
 *           for deployments where the V2X stack uses DDS natively.
 *
 * Supported input topics (MQTT and DDS):
 *   vanetza/in/cam, vanetza/in/cam_full (legacy), vanetza/own/cam, vanetza/in/vam
 *
 * Thread Safety:
 * In MQTT and DDS modes the location is updated from a callback thread.
 * All reads and writes of the position fields are protected by mtx_.
 */

#ifndef LOCATOR_H
#define LOCATOR_H

#include <string>
#include <mutex>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "mqttwrapper.h"
#include "fastdds-cpp-wrapper/dds.hpp"

namespace rj = rapidjson;

enum class ProviderType {
    STATIC,
    MQTT,
    DDS
};

class Locator {
public:
    /**
     * @brief Construct a new Locator object.
     *
     * @param provider The provider type: STATIC, MQTT, or DDS.
     * @param configLatitude Constant latitude from configuration (empty if not provided).
     * @param configLongitude Constant longitude from configuration (empty if not provided).
     * @param configStationType Station type from configuration (0 if not provided).
     * @param mqttBroker MQTT broker address if provider is MQTT.
     * @param mqttTopic MQTT topic to subscribe for CAMs if provider is MQTT.
     * @param ddsDomain DDS domain ID if provider is DDS.
     * @param ddsTopic DDS topic to subscribe for CAMs if provider is DDS.
     */
    Locator(ProviderType provider,
                   float configLatitude,
                   float configLongitude,
                   int configStationType,
                   std::string mqttBroker,
                   std::string mqttTopic, 
                   int ddsDomain,
                   std::string ddsTopic);
    
    ~Locator();

    void run();

    // Getters for location and station type.
    double getStationLatitude();
    double getStationLongitude();
    float getStationHeading();
    int getStationType();

private:

    std::thread locatorThread_; ///< Worker thread that runs the Locator's main loop.

    // Provider type chosen at construction.
    ProviderType provider_;     ///< Location provider mode: STATIC, MQTT, or DDS.

    // Station type
    int stationType_;           ///< Station type code (RSU, vehicle, etc.).

    // Latest station dynamic location values (updated by client callbacks).
    double latestLatitude_;     ///< Current latitude in WGS-84 degrees.
    double latestLongitude_;    ///< Current longitude in WGS-84 degrees.
    float latestHeading_;       ///< Current heading in degrees.

    // Mutex to protect dynamic updates.
    std::mutex mtx_; ///< Protects access to latitude, longitude, and heading fields.

    // MQTT client 
    MqttWrapper* mqttClient_; ///< Optional MQTT client used in MQTT provider mode.

    // MQTT topic to subscribe to for CAM location messages.
    std::string mqttTopic_;   ///< MQTT topic for CAM/VAM subscription.

    // DDS client
    Dds* dds_;                ///< Optional DDS client used in DDS provider mode.

    // DDS topic to subscribe to for CAM location messages.
    std::string ddsTopic_;    ///< DDS topic for CAM/VAM subscription.

    /**
     * @brief Internal callback for MQTT messages.
     * Invoked when a CAM/VAM message arrives on the subscribed MQTT topic.
     * @param topic The MQTT topic.
     * @param message The received message (JSON formatted).
     */
    void on_message_mqtt(const std::string& topic, const std::string& message);

    /**
     * @brief Internal callback for DDS messages.
     * Invoked when a CAM/VAM message arrives on the subscribed DDS topic.
     * @param topic The DDS topic.
     * @param message The received message (JSON formatted).
     */
    void on_message_dds(const std::string& topic, const std::string& message);

    /**
     * @brief Helper to parse CAM/VAM messages and update location.
     * Extracts latitude, longitude, and heading from incoming messages and updates internal state.
     * @param topic The message topic.
     * @param message The raw message (JSON formatted).
     */
    void parseAndUpdateLocation(const std::string& topic, const std::string& message);

    /**
     * @brief Main loop executed in the worker thread.
     * Manages subscriptions and event processing depending on the provider mode.
     */
    void runLoop();
};

#endif // LOCATOR_H