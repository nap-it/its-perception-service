#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include "mqttwrapper.h"
#include "fastdds-cpp-wrapper/dds.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "locator.h"
#include "zenoh.hxx"

constexpr long TIME_2004_MS = 1072915200000;
constexpr double M_180_PI = 180.0 / M_PI;
constexpr double M_PI_180 = M_PI / 180.0;
constexpr double R = 6371000; // earth radius

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
    std::string cpm_topics;
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
    std::string zenoh_endpoint;
    std::string zenoh_output_topic;
    std::string zenoh_output_full_topic;
};

class Processor {
public:
    /**
     * @brief Construct a new Processor object.
     *
     * @param config The configuration of the processor.
     * @param locator Shared pointer to the Locator object.
     * @param performanceLogs Enable performance logs (default: false).
     */
    Processor(const Config& config, std::shared_ptr<Locator> locator, bool performanceLogs = false);
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
    std::shared_ptr<Locator> locator_;

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

    // Zenoh session and shared memory provider
    zenoh::Session* session_ = nullptr;
    zenoh::PosixShmProvider* shm_provider_ = nullptr;

    // File logger
    std::shared_ptr<spdlog::logger> processor_file_logger_;
    bool performanceLogs_;           // Enable performance logs
    
    // Thread control for periodic cleanup
    std::atomic<bool> stopFlag_;

    map<int,string> sensor_type_ = {
        {0, "undefined"},
        {1, "radar"},
        {2, "lidar"},
        {3, "monovideo"},
        {4, "stereovision"},
        {5, "nightvision"},
        {6, "ultrasonic"},
        {7, "pmd"},
        {8, "inductionLoop"},
        {9, "sphericalCamera"},
        {10, "uwb"},
        {11, "acoustic"},
        {12, "localAggregation"},
        {13, "itsAggregation"}
    };

    map<int,string> vehicle_classes_ = {
        {0, "unknown"},
        {1, "pedestrian"},
        {2, "cyclist"},
        {3, "moped"},
        {4, "motorcycle"},
        {5, "passengerCar"},
        {6, "bus"},
        {7, "lightTruck"},
        {8, "heavyTruck"},
        {9, "trailer"},
        {10, "Ambulance"},
        {11, "tram"},
        {12, "VRU"},
        {13, "animal"},
        {14, "agricultural vehicles"},
        {15, "roadSideUnit"},
        {16, "SafetyApp"},
        {17, "Moliceiro"},
        {18, "Test Device"},
        {19, "others"}
    };

    map<int, string> person_classes_ = {
        {0, "unknown"},
        {1, "pedestrian"},
        {2, "personInWheelchair"},
        {3, "cyclist"},
        {4, "personWithStroller"},
        {5, "personOnSkates"},
        {6, "personGroup"}
    };

    map<int, string> other_classes_ = {
            {0, "unknown"},
            {1, "roadSideUnit"}
    };

    /**
     * @brief get the current timestamp as a string.
     * @return std::string The current timestamp as a string.
     */
    std::string getCurrentTimestampString() {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&tt);
        std::ostringstream oss;
        // Format as: YYYY-MM-DD HH:MM:SS.MSMSMS
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        // Add milliseconds.
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    /**
     * @brief Process an incoming CPM
     * For a CPM, it looks up the corresponding CAM data and builds a new output message.
     * @param topic The topic of the message.
     * @param message The raw message (JSON formatted).
     * @param output The output message.
     * @param output_full The full output message.  
     * @param message_reception Time when the message is received via DDS.
     */
    void processCPM(const std::string& topic, const std::string& message, std::string& output, std::string& output_full, std::chrono::time_point<std::chrono::high_resolution_clock> message_reception);

};


#endif // PROCESSOR_H