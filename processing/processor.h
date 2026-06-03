/**
 * @file processor.h
 * @brief CPM consumer and object enrichment component for the CPS Processing service
 * @date 2026
 *
 * This file defines the Processor class, which receives CPMs from the DDS domain,
 * decodes them, enriches each perceived object with geodetic computations, and
 * republishes the result in an application-friendly JSON format.
 *
 * Key Responsibilities:
 * - Subscribes to one or more DDS CPM topics (e.g. vanetza/in/cpm, cps-v2/in/cpm)
 * - Resolves sender station metadata (speed, heading, altitude) from incoming CAMs
 *   via the Locator
 * - Converts ETSI relative x/y distance values back to absolute WGS-84 coordinates
 * - Decomposes speed/heading into x/y velocity components (north/east)
 * - Maps integer sensor type and classification codes to human-readable strings
 * - Publishes enriched object lists on:
 *     - Local MQTT broker (topic: objects)
 *     - Remote MQTT broker (optional)
 *     - DDS topic (objects)
 *     - Zenoh topic (objects)
 * - Collects Prometheus metrics when enabled
 *
 * Output Format:
 * The output JSON (published on the "objects" topic) contains sender metadata,
 * CPM metadata, and a list of enriched objects. See the root README.md for the
 * full field reference.
 *
 * Thread Safety:
 * DDS callbacks arrive on a separate thread. The CAM data map (camMtx_) is
 * protected by a mutex. The stop flag is atomic.
 */

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
#include "metrics.h"
#include "zenoh.hxx"

constexpr long TIME_2004_MS = 1072915200000;
constexpr double M_180_PI = 180.0 / M_PI;
constexpr double M_PI_180 = M_PI / 180.0;
constexpr double R = 6371000; // earth radius

struct Object {
    int id;                     ///< Object identifier in the processed output.
    int unique_id;              ///< Identifier used across processing stages.
    int age;                    ///< Object age in milliseconds.
    float timestamp;            ///< Object timestamp in UNIX seconds.
    float reference_timestamp;  ///< Reference timestamp used for relative calculations.
    int confidence;             ///< Classification confidence value.
    int sensor_type;            ///< Input sensor type code.
    int sensor_id;              ///< Identifier of the source sensor.
    float reference_latitude;   ///< Reference latitude in WGS-84 degrees.
    float reference_longitude;  ///< Reference longitude in WGS-84 degrees.
    float x_distance;           ///< Relative east/west distance in meters.
    float y_distance;           ///< Relative north/south distance in meters.
    float z_distance;           ///< Relative vertical distance in meters.
    float x_distance_cov;       ///< Covariance of x_distance.
    float y_distance_cov;       ///< Covariance of y_distance.
    float z_distance_cov;       ///< Covariance of z_distance.
    float latitude;             ///< Latitude in WGS-84 degrees.
    float longitude;            ///< Longitude in WGS-84 degrees.
    float altitude;             ///< Altitude in meters.
    float x_velocity;           ///< East/west velocity component in m/s.
    float y_velocity;           ///< North/south velocity component in m/s.
    float x_velocity_cov;       ///< Covariance of x_velocity.
    float y_velocity_cov;       ///< Covariance of y_velocity.
    float speed;                ///< Object speed in m/s.
    float x_acceleration;       ///< East/west acceleration component in m/s².
    float y_acceleration;       ///< North/south acceleration component in m/s².
    float heading;              ///< Object heading in degrees.
    float heading_cov;          ///< Heading covariance.
    float size_x;               ///< Object length in meters.
    float size_y;               ///< Object width in meters.
    float size_z;               ///< Object height in meters.
    int classification;         ///< Raw classification code.
    std::string classification_str; ///< Human-readable classification label.
    int sender_id;              ///< Sender station identifier.
    int sender_type;            ///< Sender station type code.
    int receiver_id;            ///< Receiver station identifier.
    int receiver_type;          ///< Receiver station type code.
};

struct Config {
    std::string log_level;             ///< Log verbosity level.
    int dds_domain;                    ///< DDS domain identifier.
    int host_station_type;             ///< Station type of the host platform.
    int host_station_id;               ///< Station identifier of the host platform.
    int repeat_id_interval;            ///< Interval for reusing IDs when needed.
    int cam_freshness_threshold;       ///< CAM freshness threshold in milliseconds.
    std::string cpm_topics;            ///< Comma-separated CPM input topics.
    std::string dds_output_topic;      ///< DDS topic used for published output.
    bool local_mqtt_enabled;           ///< Enables publishing to local MQTT.
    std::string local_mqtt_host;       ///< Local MQTT broker host.
    int local_mqtt_port;               ///< Local MQTT broker port.
    std::string local_mqtt_output_topic; ///< Local MQTT output topic.
    bool remote_mqtt_enabled;          ///< Enables publishing to remote MQTT.
    std::string remote_mqtt_host;      ///< Remote MQTT broker host.
    int remote_mqtt_port;              ///< Remote MQTT broker port.
    std::string remote_mqtt_output_topic; ///< Remote MQTT output topic.
    std::string remote_mqtt_username;  ///< Remote MQTT username.
    std::string remote_mqtt_password;  ///< Remote MQTT password.
    std::string zenoh_endpoint;        ///< Zenoh endpoint for publishing.
    std::string zenoh_output_topic;    ///< Zenoh output topic.
};

class Processor {
public:
    /**
     * @brief Construct a new Processor object.
     *
     * @param config The configuration of the processor.
     * @param locator Shared pointer to the Locator object.
     * @param performanceLogs Enable performance logs (default: false).
     * @param prometheus Enable Prometheus metrics (default: false).
     * @param metrics Pointer to metric handles structure (default: nullptr).
     */
    Processor(const Config& config, std::shared_ptr<Locator> locator, bool performanceLogs = false, bool prometheus = false, ProcMetricHandles* metrics = nullptr);
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
    std::shared_ptr<Locator> locator_; ///< Shared Locator used to resolve host station state.

    // Map storing latest CAM data per station.
    std::unordered_map<int, SenderInfo> camDataMap_; ///< Latest CAM data indexed by station ID.
    std::mutex camMtx_; ///< Protects camDataMap_.

    // Configuration
    Config config_; ///< Runtime configuration loaded at startup.

    // Local MQTT client 
    MqttWrapper* local_mqtt_client_; ///< Optional local MQTT publisher.

    // Remote MQTT client
    MqttWrapper* remote_mqtt_client_; ///< Optional remote MQTT publisher.

    // DDS client
    Dds* dds_; ///< DDS client used for subscriptions and publishing.

    // Zenoh session and shared memory provider
    zenoh::Session* session_ = nullptr; ///< Optional Zenoh session for output publishing.
    zenoh::PosixShmProvider* shm_provider_ = nullptr; ///< Zenoh shared-memory provider used when available.

    // File logger
    std::shared_ptr<spdlog::logger> processor_file_logger_; ///< File-backed logger for diagnostics.
    bool performanceLogs_;           ///< Enables performance timing logs.
    
    // Thread control for periodic cleanup
    std::atomic<bool> stopFlag_; ///< Signals worker loops to stop.

    /**
     * @brief Mapping of sensor type codes to human-readable sensor type names.
     * Follows ETSI TS 103 724 sensor type classification scheme.
     */
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

    /**
     * @brief Mapping of vehicle classification codes to human-readable class names.
     * Follows ETSI TS 103 724 vehicle classification scheme.
     */
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

    /**
     * @brief Mapping of person classification codes to human-readable class names.
     * Follows ETSI TS 103 724 person classification scheme.
     */
    map<int, string> person_classes_ = {
        {0, "unknown"},
        {1, "pedestrian"},
        {2, "personInWheelchair"},
        {3, "cyclist"},
        {4, "personWithStroller"},
        {5, "personOnSkates"},
        {6, "personGroup"}
    };

    /**
     * @brief Mapping of other object classification codes to human-readable class names.
     * Covers classifications not falling into vehicle or person categories.
     */
    map<int, string> other_classes_ = {
            {0, "unknown"},
            {1, "roadSideUnit"}
    };

    // Metrics
    bool prometheus_;                ///< Enables Prometheus metrics.
    ProcMetricHandles* metrics_;     ///< Pointer to metric handles provided by the caller.

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
     * @param message_reception Time when the message is received via DDS.
     */
    void processCPM(const std::string& topic, const std::string& message, std::string& output, std::chrono::time_point<std::chrono::high_resolution_clock> message_reception);

};


#endif // PROCESSOR_H