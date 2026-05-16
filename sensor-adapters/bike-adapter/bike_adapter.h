/**
 * @file bike_adapter.h
 * @brief Bike Raspberry Pi camera-to-DDS bridge adapter for the CPS sensor pipeline
 * @date 2026
 *
 * This file defines the BikeAdapter class, which bridges object detections from
 * a Raspberry Pi camera mounted on a bicycle into the CPS Generation service.
 *
 * Key Responsibilities:
 * - Subscribes to an MQTT topic carrying object detections from the bike camera
 * - Parses incoming JSON detections and maps them to the CPS Object format
 * - Publishes the normalised object list to the DDS topic "generation/objects"
 *   for consumption by the Generation service
 *
 * Expected Input (MQTT, JSON):
 * Each message should contain object fields: objectID, sensorID, timestamp,
 * latitude, longitude, heading, speed, confidence, classification.
 * See the root README.md for the full generation/objects format specification.
 *
 * Fields not provided by the bike camera pipeline (altitude, size, covariances)
 * are published as 0.0 and treated as unavailable by the Generation service.
 */

#ifndef BIKEADAPTER_H
#define BIKEADAPTER_H

#include <string>
#include <vector>
#include <random>
#include <thread>
#include <atomic>
#include "mqttwrapper.h"
#include "fastdds-cpp-wrapper/dds.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace rj = rapidjson;

struct Config {
    int domain_id;                 ///< DDS domain identifier.
    bool debug;                    ///< Enables debug logging.
    std::string mqtt_host;         ///< MQTT broker host.
    int mqtt_port;                 ///< MQTT broker port.
    std::string mqtt_topic;        ///< MQTT topic to subscribe to.
    std::string mqtt_client_id;    ///< MQTT client identifier.
};

struct Object {
    int objectID;                      ///< Raw object identifier from the bike camera pipeline.
    int sensorID;                      ///< Source sensor identifier.
    double timestamp;                  ///< Object timestamp in UNIX seconds.
    int classification;                ///< Classification code.
    int confidence;                    ///< Classification confidence.
    float speed;                       ///< Object speed in m/s.
    float heading;                     ///< Object heading in degrees.
    float acceleration;                ///< Not used by this adapter.
    float latitude;                    ///< Latitude in WGS-84 degrees.
    float longitude;                   ///< Longitude in WGS-84 degrees.
    float altitude = 0.0f;             ///< Not used by this adapter.
    float size_x = 0.0f;               ///< Not used by this adapter.
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

class BikeAdapter {
public:
    /**
     * @brief Construct a new Bike Adapter object.
     * @param config Configuration with MQTT broker, topic, and DDS domain.
     */
    BikeAdapter(const Config& config);

    /**
     * @brief Start the adapter main loop.
     * Subscribes to the MQTT topic and processes incoming bike camera detections.
     */
    void run();
    void stop();

private:
    std::atomic<bool> stopFlag_{false};
    Config config;                    ///< Runtime configuration.
    Dds* dds_;                        ///< DDS client for publishing normalized objects.
    MqttWrapper* mqtt_wrapper;        ///< MQTT client for subscribing to bike camera topic.

    /**
     * @brief Callback function invoked when an MQTT message arrives.
     * Parses the message and publishes normalized objects on DDS.
     * @param topic The MQTT topic.
     * @param message The raw message (JSON formatted).
     */
    void on_message_mqtt(const std::string& topic, const std::string& message);

    /**
     * @brief Static callback function for DDS messages.
     * Not used in this adapter (bike camera input is MQTT only).
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

#endif // BIKEADAPTER_H