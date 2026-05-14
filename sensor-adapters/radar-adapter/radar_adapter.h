/**
 * @file radar_adapter.h
 * @brief Radar-to-DDS bridge adapter for the CPS sensor pipeline
 * @date 2026
 *
 * This file defines the RadarAdapter class, which bridges a radar detection
 * pipeline into the CPS Generation service.
 *
 * Key Responsibilities:
 * - Subscribes to one or more MQTT topics carrying radar object detections
 * - Parses incoming JSON detections and maps them to the CPS Object format
 * - Publishes the normalised object list to the DDS topic "generation/objects"
 *   for consumption by the Generation service
 *
 * Expected Input (MQTT, JSON):
 * Each message should contain object fields: objectID, sensorID, timestamp,
 * latitude, longitude, heading, speed, confidence, classification, size_x.
 * See the root README.md for the full generation/objects format specification.
 *
 * Fields not provided by radar pipelines (altitude, size_y, size_z,
 * angular velocity, covariances) are published as 0.0 and are treated as
 * unavailable by the Generation service.
 */

#ifndef RADARADAPTER_H
#define RADARADAPTER_H

#include <string>
#include <vector>
#include <random>
#include <thread>
#include "mqttwrapper.h"
#include "fastdds-cpp-wrapper/dds.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace rj = rapidjson;

struct Config {
    int domain_id;                     ///< DDS domain identifier.
    bool debug;                        ///< Enables debug logging.
    std::string mqtt_host;             ///< MQTT broker host.
    int mqtt_port;                     ///< MQTT broker port.
    std::vector<std::string> mqtt_topics; ///< MQTT topics to subscribe to.
    std::string mqtt_client_id;        ///< MQTT client identifier.
};

struct Object {
    int objectID;                      ///< Raw object identifier from the radar pipeline.
    int sensorID;                      ///< Source sensor identifier.
    double timestamp;                  ///< Object timestamp in UNIX seconds.
    int classification;                ///< Classification code.
    int confidence;                    ///< Classification confidence.
    float speed;                       ///< Object speed in m/s.
    float heading;                     ///< Object heading in degrees.
    float acceleration;                ///< Longitudinal acceleration in m/s².
    float latitude;                    ///< Latitude in WGS-84 degrees.
    float longitude;                   ///< Longitude in WGS-84 degrees.
    float altitude = 0.0f;             ///< Not used by this adapter.
    float size_x = 0.0f;               ///< Object length in meters.
    float size_y = 0.0f;               ///< Not used by this adapter.
    float size_z = 0.0f;               ///< Not used by this adapter.
    float angular_velocity = 0.0f;     ///< Not used by this adapter.
    float cov_latitude = 0.0f;         ///< Not used by this adapter.
    float cov_longitude = 0.0f;        ///< Not used by this adapter.
    float cov_altitude = 0.0f;         ///< Not used by this adapter.
    float cov_heading = 0.0f;          ///< Not used by this adapter.
    float cov_speed = 0.0f;            ///< Not used by this adapter.
    float cov_angular_velocity = 0.0f; ///< Not used by this adapter.
};

class RadarAdapter {
public:
    /**
     * @brief Construct a new Radar Adapter object.
     * @param config Configuration with MQTT broker, topics, and DDS domain.
     */
    RadarAdapter(const Config& config);

    /**
     * @brief Start the adapter main loop.
     * Subscribes to MQTT topics and processes incoming radar detections.
     */
    void run();

private:
    int message_count_ = 0;           ///< Counter for received MQTT messages.
    std::mutex counter_mutex_;        ///< Protects message_count_.

    Config config;                    ///< Runtime configuration.
    Dds* dds_;                        ///< DDS client for publishing normalized objects.
    MqttWrapper* mqtt_wrapper;        ///< MQTT client for subscribing to radar topics.

    /**
     * @brief Callback function invoked when an MQTT message arrives.
     * Parses the message and publishes normalized objects on DDS.
     * @param topic The MQTT topic.
     * @param message The raw message (JSON formatted).
     */
    void on_message_mqtt(const std::string& topic, const std::string& message);

    /**
     * @brief Static callback function for DDS messages.
     * Not used in this adapter (radar input is MQTT only).
     * @param topic The DDS topic.
     * @param message The raw message (JSON formatted).
     */
    static void on_message_dds(const std::string& topic, const std::string& message) {}

    /**
     * @brief Parse an incoming MQTT message into CPS Object format.
     * Extracts fields from JSON and normalizes to the standard schema.
     * @param message The raw message (JSON formatted).
     * @return std::string Normalized object as JSON suitable for DDS publication.
     */
    std::string parseMessage(const std::string& message);

    /**
     * @brief Generate a random number string.
     * Used for generating unique message identifiers or request IDs.
     * @return std::string A random number as a string.
     */
    std::string getRandomNumberString();
};

#endif