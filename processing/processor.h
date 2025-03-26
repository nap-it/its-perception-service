#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include "mqttwrapper.h"
#include "fastdds-cpp-wrapper/dds.hpp"

constexpr float NOT_PRESENT_FLOAT = -999.0f;
constexpr double NOT_PRESENT_DOUBLE = -999.0;
constexpr int NOT_PRESENT_INT = -999;
constexpr long TIME_2004_MS = 1072915200000;

struct SenderInfo {
    int station_id;
    int station_type;
    float latitude;
    float longitude;
    float speed;        // optional
    float heading;      // optional
    float altitude;     // optional
    float acceleration; // optional
    std::chrono::steady_clock::time_point cam_timestamp;
};

struct Object {
    int id;
    int unique_id;
    int age;
    float timestamp;
    float reference_timestamp;
    int confidence;
    int sensor_type;
    int sensor_id;
    float reference_latitude;
    float reference_longitude;
    float x_distance;
    float y_distance;
    float z_distance;
    float x_distance_cov; 
    float y_distance_cov;
    float z_distance_cov;
    float latitude;
    float longitude;
    float altitude;
    float x_velocity;
    float y_velocity;
    float x_velocity_cov;
    float y_velocity_cov;
    float speed;
    float x_acceleration;
    float y_acceleration;
    float heading;
    float heading_cov; 
    float size_x;
    float size_y; 
    float size_z;
    int classification;
    std::string classification_str;
    int sender_id;
    int sender_type;
    int receiver_id;
    int receiver_type;
};

struct Config {
    bool debug;
    int dds_domain;
    int host_station_type;
    int host_station_id;
    int repeat_id_interval;
    int cam_freshness_threshold;
    std::string cpm_topic;
    std::string cam_topic;
    std::string dds_output_topic;
    std::string dds_output_full_topic;
    bool local_mqtt_enabled;
    std::string local_mqtt_host;
    int local_mqtt_port;
    std::string local_mqtt_output_topic;
    std::string local_mqtt_output_full_topic;
    bool remote_mqtt_enabled;
    std::string remote_mqtt_host;
    int remote_mqtt_port;
    std::string remote_mqtt_output_topic;
    std::string remote_mqtt_output_full_topic;
    std::string remote_mqtt_username;
    std::string remote_mqtt_password;
};

class Processor {
public:
    /**
     * @brief Construct a new Processor object.
     *
     * @param config The configuration of the processor.
     */
    Processor(const Config& config);
    ~Processor();

    /**
     * @brief Start the generation loop.
     */
    void run();

    /**
     * @brief Stop the generation loop.
     */
    void stop();

    /**
     * @brief Non-static DDS message handler.
     * Called when a new DDS message arrives on CPM or CAM topics.
     * @param topic The DDS topic.
     * @param message The received message (JSON formatted).
     */
    void on_message_dds(const std::string& topic, const std::string& message);

private:
    // Map storing latest CAM data per station.
    std::unordered_map<int, SenderInfo> camDataMap_;
    std::mutex camMtx_;

    // Configuration
    Config config_;

    // Local MQTT client 
    MqttWrapper* local_mqtt_client_;

    // Remote MQTT client
    MqttWrapper* remote_mqtt_client_;

    // DDS client
    Dds* dds_;
    
    // Thread control for periodic cleanup
    std::atomic<bool> stopFlag_;

    /**
     * @brief Process an incoming CPM
     * For a CPM, it looks up the corresponding CAM data and builds a new output message.
     * @param topic The topic of the message.
     * @param message The raw message (JSON formatted).
     * @param output The output message.
     * @param output_full The full output message.  
     */
    void processCPM(const std::string& topic, const std::string& message, std::string& output, std::string& output_full);

    /**
     * @brief Process an incoming CAM
     * For a CAM, it stores the data in the internal map.
     * @param topic The topic of the message.
     * @param message The raw message (JSON formatted).
     */
    void processCAM(const std::string& topic, const std::string& message);

    /**
     * @brief Check if the CAM data is fresh.
     * @param camData The CAM data.
     * @param threshold The threshold for freshness.
     * @return True if the data is fresh, false otherwise.
     */
    bool isCamDataFresh(const SenderInfo& camData, std::chrono::steady_clock::duration threshold);

    /**
     * @brief Clean up stale CAM data.
     * @param threshold The threshold for staleness.
     */
    void cleanupStaleCamData(std::chrono::steady_clock::duration threshold);

};


#endif // PROCESSOR_H