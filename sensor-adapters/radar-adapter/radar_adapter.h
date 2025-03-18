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
    int domain_id;
    bool debug;
    std::string mqtt_host;
    int mqtt_port;
    std::string mqtt_topic;
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
    float acceleration;
    float latitude;
    float longitude;
    float altitude = 0.0f;   // Not used
    float size_x = 0.0f;
    float size_y = 0.0f;     // Not used
    float size_z = 0.0f;     // Not used
    float angular_velocity = 0.0f; // Not used
    float cov_latitude = 0.0f;     // Not used
    float cov_longitude = 0.0f;    // Not used
    float cov_altitude = 0.0f;     // Not used
    float cov_heading = 0.0f;      // Not used
    float cov_speed = 0.0f;        // Not used
    float cov_angular_velocity = 0.0f; // Not used
};

class RadarAdapter {
public:
    RadarAdapter(const Config& config);
    void run();

private:
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

#endif