/**
 * @file camera_adapter.h
 * @brief Camera-to-DDS bridge adapter for the CPS sensor pipeline
 * @date 2026
 *
 * This file defines the CameraAdapter class, which bridges a camera detection
 * pipeline into the CPS Generation service.
 *
 * Key Responsibilities:
 * - Subscribes to one or more MQTT topics carrying camera object detections
 * - Parses incoming JSON detections and maps them to the CPS Object format
 * - Publishes the normalised object list to the DDS topic "generation/objects"
 *   for consumption by the Generation service
 *
 * Expected Input (MQTT, JSON):
 * Each message should contain object fields: objectID, sensorID, timestamp,
 * latitude, longitude, heading, speed, confidence, classification.
 * See the root README.md for the full generation/objects format specification.
 *
 * Fields not provided by camera pipelines (altitude, size, covariances) are
 * published as 0.0 and are treated as unavailable by the Generation service.
 */

#ifndef CAMERAADAPTER_H
#define CAMERAADAPTER_H

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
    int domain_id;
    bool debug;
    std::string mqtt_host;
    int mqtt_port;
    std::vector<std::string> mqtt_topics;
    std::string mqtt_client_id;
};

struct Object {
    int objectID;
    int sensorID;
    double timestamp;
    int classification;
    int confidence;
    float speed;
    float heading;
    float acceleration;                 // Not used
    float latitude;
    float longitude;
    float altitude = 0.0f;              // Not used
    float size_x = 0.0f;                // Not used
    float size_y = 0.0f;                // Not used
    float size_z = 0.0f;                // Not used
    float angular_velocity = 0.0f;      // Not used
    float cov_latitude = 0.0f;          // Not used
    float cov_longitude = 0.0f;         // Not used
    float cov_altitude = 0.0f;          // Not used
    float cov_heading = 0.0f;           // Not used
    float cov_speed = 0.0f;             // Not used
    float cov_angular_velocity = 0.0f;  // Not used
};

class CameraAdapter {
public:
    CameraAdapter(const Config& config);
    void run();
    void stop();

private:
    int message_count_ = 0;
    std::mutex counter_mutex_;
    std::atomic<bool> stopFlag_{false};

    Config config;
    Dds* dds_;
    MqttWrapper* mqtt_wrapper;

    /**
     * Callback function for MQTT messages.
     */
    void on_message_mqtt(const std::string& topic, const std::string& message);

    /**
     * Callback function for DDS messages.
     */
    static void on_message_dds(const std::string& topic, const std::string& message) {}

    /**
     * Parse an incoming MQTT message (as JSON text) into an Object,
     * and return the resulting JSON string for DDS publishing.
     */
    std::string parseMessage(const std::string& message);

    /**
     * Returns a random number string.
     */
    std::string getRandomNumberString();
};

#endif // CAMERAADAPTER_H