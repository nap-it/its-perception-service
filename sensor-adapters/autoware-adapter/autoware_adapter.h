/**
 * @file autoware_adapter.h
 * @brief Autoware VPI-to-DDS bridge adapter for the CPS sensor pipeline
 * @date 2026
 *
 * This file defines the AutowareAdapter class, which bridges object detections
 * from the Autoware VPI autonomous driving platform into the CPS Generation service.
 *
 * Key Responsibilities:
 * - Subscribes to a DDS topic carrying object detections from Autoware VPI
 * - Parses incoming JSON detections and re-publishes them in the CPS Object format
 * - Publishes the normalised object list to the DDS topic "generation/objects"
 *   for consumption by the Generation service
 *
 * Unlike the camera and radar adapters, this adapter uses DDS as its input
 * transport (no MQTT), since Autoware VPI already publishes on DDS.
 *
 * Expected Input (DDS, JSON):
 * Each message should contain object fields: objectID, sensorID, timestamp,
 * latitude, longitude, heading, speed, confidence, classification, size_x.
 * See the root README.md for the full generation/objects format specification.
 */

#ifndef AUTOWAREADAPTER_H
#define AUTOWAREADAPTER_H

#include <string>
#include <vector>
#include <random>
#include <thread>
#include <spdlog/spdlog.h>
#include "fastdds-cpp-wrapper/dds.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace rj = rapidjson;

struct Config {
    int domain_id; ///< DDS domain identifier.
    bool debug;    ///< Enables debug logging.
};

struct Object {
    int objectID;                      ///< Raw object identifier from the Autoware pipeline.
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

class AutowareAdapter {
public:
    /**
     * @brief Construct a new Autoware Adapter object.
     * @param config Configuration with DDS domain and debug settings.
     */
    AutowareAdapter(const Config& config);

    /**
     * @brief Start the adapter main loop.
     * Subscribes to DDS topics and processes incoming Autoware VPI detections.
     */
    void run();

private:
    Config config;    ///< Runtime configuration.
    Dds* dds_;        ///< DDS client for subscribing to Autoware and publishing normalized objects.

    /**
     * @brief Callback function invoked when a DDS message arrives.
     * Parses the message and publishes normalized objects on DDS.
     * @param topic The DDS topic.
     * @param message The raw message (JSON formatted).
     */
    void on_message_dds(string topic, string message);

    /**
     * @brief Parse an incoming DDS message into CPS Object format.
     * Extracts fields from JSON and normalizes to the standard schema.
     * @param message The raw message (JSON formatted).
     * @return std::string Normalized object as JSON suitable for DDS publication.
     */
    std::string parseMessage(const std::string& message);
};

#endif // AUTOWAREADAPTER_H