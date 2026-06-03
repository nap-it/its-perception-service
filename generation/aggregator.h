/**
 * @file aggregator.h
 * @brief Object ingestion and ETSI freshness/priority engine for the CPS Generation service
 * @date 2026
 *
 * This file defines the Aggregator class, which is the central data hub of the
 * Generation service. It receives raw object detections from Sensor Adapters
 * over DDS or Zenoh, maintains a live object cache, and determines which objects
 * are "fresh" enough to be included in the next CPM.
 *
 * Key Responsibilities:
 * - Subscribes to the DDS/Zenoh topic "generation/objects" for object data
 * - Subscribes to "generation/sensors" for sensor metadata
 * - Maintains a per-object cache (objectID → ObjectEntity) with last-sent state
 * - Evaluates object freshness against ETSI TS 103 324 priority thresholds:
 *     position change, speed change, heading change, time since last inclusion
 * - Supports two priority modes: "etsi" (standard) and "predictor" (movement-based)
 * - Periodically evicts stale objects from the cache via a background cleanup thread
 * - Assigns stable CPM object IDs independent of sensor IDs
 *
 * Priority Thresholds (ETSI TS 103 324):
 * - P_MIN / P_MAX: position change thresholds (0–8 m)
 * - S_MIN / S_MAX: ground speed change thresholds (0–1 m/s)
 * - O_MIN / O_MAX: velocity orientation change thresholds (0–8°)
 * - T_MIN / T_MAX: time since last inclusion thresholds (100–1000 ms)
 *
 * Thread Safety:
 * Object cache (objMtx_) and sensor cache (sensorMtx_) are individually mutex-protected.
 * DDS and Zenoh callbacks post into these maps from their own threads.
 */

#ifndef AGGREGATOR_H
#define AGGREGATOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <cmath>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "fastdds-cpp-wrapper/dds.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <zenoh.hxx>
#include "metrics.h"

namespace rj = rapidjson;

constexpr float NOT_PRESENT_FLOAT = -999.0f;
constexpr double NOT_PRESENT_DOUBLE = -999.0;
constexpr int NOT_PRESENT_INT = -999;

constexpr double R_E = 6371000.0; // Earth radius in meters;
constexpr double M_PI_180 = M_PI / 180.0;
constexpr double M_180_PI = 180.0 / M_PI;

// ETSI Priority limit values
constexpr double P_MIN = 0.0;    // minPositionChangePriorityThreshold (m)
constexpr double P_MAX = 8.0;    // maxPositionChangePriorityThreshold (m)
constexpr double S_MIN = 0;      // minGroundSpeedChangePriorityThreshold (m/s)
constexpr double S_MAX = 1.0;    // maxGroundSpeedChangePriorityThreshold (m/s)
constexpr double O_MIN = 0;       // minGroundVelocityOrientationChangePriorityThreshold (degress)
constexpr double O_MAX = 8.0;    // maxGroundVelocityOrientationChangePriorityThreshold (degress)
constexpr double T_MIN = 100.0;    // minLastInclusionTimePriorityThreshold (ms)
constexpr double T_MAX = 1000.0;    // minLastInclusionTimePriorityThreshold (ms)

// Sensor types
constexpr int SENSOR_TYPE_RADAR = 1;
constexpr int SENSOR_TYPE_LIDAR = 2;
constexpr int SENSOR_TYPE_CAMERA = 3;

struct Object {
    int objectID;                 ///< Object identifier from the sensor source.
    int cpmObjectID;              ///< CPM object identifier assigned by the Aggregator.
    int sensorID;                 ///< Identifier of the sensor that produced the object.
    double timestamp;             ///< Object timestamp in UNIX seconds.
    int classification;           ///< Object classification code.
    int confidence;               ///< Classification confidence value.
    float speed;                  ///< Speed in m/s.
    float heading;                ///< Heading in degrees.
    float acceleration;           ///< Longitudinal acceleration in m/s².
    float latitude;               ///< Latitude in WGS-84 degrees.
    float longitude;              ///< Longitude in WGS-84 degrees.
    float altitude = NOT_PRESENT_FLOAT; ///< Altitude in meters, or NOT_PRESENT_FLOAT when unavailable.
    float size_x = NOT_PRESENT_FLOAT;   ///< Object length in meters, or NOT_PRESENT_FLOAT when unavailable.
    float size_y = NOT_PRESENT_FLOAT;   ///< Object width in meters, or NOT_PRESENT_FLOAT when unavailable.
    float size_z = NOT_PRESENT_FLOAT;   ///< Object height in meters, or NOT_PRESENT_FLOAT when unavailable.
    float angular_velocity = NOT_PRESENT_FLOAT; ///< Yaw rate in degrees/s, or NOT_PRESENT_FLOAT when unavailable.
    float cov_latitude = NOT_PRESENT_FLOAT;      ///< Latitude covariance.
    float cov_longitude = NOT_PRESENT_FLOAT;     ///< Longitude covariance.
    float cov_altitude = NOT_PRESENT_FLOAT;      ///< Altitude covariance.
    float cov_heading = NOT_PRESENT_FLOAT;       ///< Heading covariance.
    float cov_speed = NOT_PRESENT_FLOAT;         ///< Speed covariance.
    float cov_angular_velocity = NOT_PRESENT_FLOAT; ///< Angular velocity covariance.
};

struct ObjectEntity {
    bool to_send = false;   ///< True when the object should be included in the next CPM.
    Object last_sent;       ///< Last object state that was included in a CPM.
    Object current;         ///< Most recent object state received from the sensor.
    double priority = 0.0;  ///< Computed freshness/priority score.
    bool has_updated = false; ///< True when current differs from last_sent.
};

struct SensorInfo {
    int sensorID = NOT_PRESENT_INT;      ///< Sensor identifier.
    int sensorType = NOT_PRESENT_INT;    ///< Sensor type code.
    bool shadowingApplies = false;       ///< True when shadowing rules apply to this sensor.
};

class Aggregator {
public:
    /**
     * @brief Construct a new Aggregator object.
     * @param domainId DDS domain ID.
     * @param maxObjectAge Maximum age (in seconds) for an object in the lastSent list (default: 5 minutes).
     * @param cleanInterval Interval (in seconds) to run the cleanup routine (default: 5 seconds).
     * @param ignoreRules Ignore freshness rules (default: false).
     * @param performanceLogs Enable performance logs (default: false).
     * @param zenohEndpoint Zenoh endpoint to connect to (default: none).
     * @param priorityType Type of priority calculation to use (default: "etsi"), options: "etsi", "predictor".
     * @param addPendingObjects Adds past objects that are not meant to go in a CPM (default: false)
     * @param prometheus Enable Prometheus metrics (default: false).
     * @param metrics Pointer to metric handles structure (default: nullptr).
     */
    Aggregator(int domainId, long maxObjectAgeS = 300, long cleanInterval = 5, bool ignoreRules = false, bool performanceLogs = false, const std::string& zenohEndpoint = "", const std::string& priorityType = "etsi", bool addPendingObjects = false, bool prometheus = false, GenMetricHandles* metrics = nullptr);

    /**
     * @brief Destroy the Aggregator object.
     */
    ~Aggregator();

    /**
     * @brief Start the Aggregator's run loop in its own thread.
     */
    void run();

    /**
     * @brief Stop the Aggregator's run loop.
     */
    void stop();

    /**
     * @brief Retrieve the list of fresh objects (pending for a CPM) and update internal state.
     * @return std::vector<Object> Fresh objects.
     */
    std::vector<Object> getFreshObjects(int maxObjects = -1);

    /**
     * @brief Retrieve the list of sensor information.
     * @return std::unordered_map<int, SensorInfo> Sensor information.
     */
    std::unordered_map<int, SensorInfo> getSensorInfo();

    /**
     * @brief Non-static message handler.
     * Called when a new DDS/Zenoh message arrives on "generation/objects".
     * @param topic The DDS/Zenoh topic.
     * @param message The received message (JSON formatted).
     */
    void on_message(const std::string& topic, const std::string& message);

    /**
     * @brief Static DDS callback that forwards the call to the Aggregator instance.
     * This is needed because the DDS client does not allow passing a context pointer.
     */
    static void ddsCallback(const std::string& topic, const std::string& message);

    /**
     * @brief Static Zenoh callback that forwards the call to the Aggregator instance.
     * This is needed because the Zenoh client does not allow passing a context pointer.
     */

    static void zenohCallback(zenoh::Sample &sample);

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

private:
    Dds* dds_;  ///< DDS client used to publish and subscribe.

    zenoh::Session* session_ = nullptr;  ///< Optional Zenoh session used when Zenoh transport is enabled.
    
    std::mutex objMtx_;  ///< Protects access to all_objects_.
    std::unordered_map<int, ObjectEntity> all_objects_; ///< Cache of all received objects keyed by object ID.

    std::mutex sensorMtx_;  ///< Protects access to sensor_info_.
    std::unordered_map<int, SensorInfo> sensor_info_; ///< Cache of sensor metadata keyed by sensor ID.

    // Freshness thresholds
    double minTimeDiff_ = 1000;      ///< Minimum time difference in milliseconds.
    double minDistanceDiff_ = 4.0;   ///< Minimum distance difference in meters.
    double minSpeedDiff_ = 0.5;      ///< Minimum speed difference in m/s.
    double minHeadingDiff_ = 4.0;    ///< Minimum heading difference in degrees.
    bool ignoreRules_;               ///< When true, bypass freshness checks.
    bool performanceLogs_;           ///< When true, emit performance timing logs.
    std::string zenohEndpoint_;      ///< Zenoh endpoint used for communication.
    std::string priorityType_;       ///< Priority calculation mode.

    // Cleanup configuration
    long maxObjectAge_;     ///< Maximum object age in seconds before eviction.
    long cleanInterval_;    ///< Cleanup interval in seconds.
    bool addPendingObjects_; ///< When true, include pending historical objects.

    // ID map for CPM object IDs
    int currentID_ = 1;  ///< Next CPM object ID to assign.
    std::unordered_map<int, int> idMap_; ///< Mapping from raw object ID to stable CPM object ID.

    // Thread control for periodic cleanup
    std::atomic<bool> stopFlag_;  ///< Signals the run loop and cleanup thread to stop.
    std::thread runThread_;       ///< Worker thread that executes the main run loop.

    // File logger
    std::shared_ptr<spdlog::logger> aggregator_file_logger_; ///< File-backed logger for diagnostics.

    // Metrics
    bool prometheus_;           ///< When true, publish Prometheus metrics.
    GenMetricHandles* metrics_; ///< Metric handles owned by the caller.

    /**
     * @brief The main loop that periodically cleans the object cache.
     * Executed in a background worker thread, evicts stale objects at regular intervals.
     */
    void runLoop();

    /**
     * @brief Remove entries from lastSent_ cache that are older than maxObjectAge_.
     * Prevents unbounded memory growth from inactive objects.
     */
    void cleanLastSent();

    /**
     * @brief Check if a new object is fresh compared to the previously sent object.
     * Uses ETSI or movement-predictor priority thresholds to determine freshness.
     * @param newObj The most recent object state.
     * @param oldObj The previously sent object state.
     * @return true if the object meets freshness criteria, false otherwise.
     */
    bool isFresh(const Object& newObj, const Object& oldObj);

    /**
     * @brief Calculate the geodetic distance between two lat/lon coordinates using the Haversine formula.
     * Returns distance in meters, accurate for small distances (< 500 km).
     * @param lat1 Latitude of point 1 in degrees.
     * @param lon1 Longitude of point 1 in degrees.
     * @param lat2 Latitude of point 2 in degrees.
     * @param lon2 Longitude of point 2 in degrees.
     * @return double Distance in meters.
     */
    double calculateHaversineDistance(double lat1, double lon1, double lat2, double lon2);

    /**
     * @brief Calculate the CPM object ID based on sensor and object identifiers.
     * Ensures consistent ID mapping across CPM cycles independent of sensor IDs.
     * @param sensorID Identifier of the source sensor.
     * @param objectID Raw object identifier from the sensor.
     * @return int CPM object ID.
     */
    int calculateCpmObjectID(int sensorID, int objectID);

    /**
     * @brief Calculate the priority/freshness score of an object.
     * Routes to either ETSI or movement-predictor priority calculation based on priorityType_.
     * @param last_sent The previously sent object state.
     * @param current The most recent object state.
     * @return float Priority value (higher = more urgent to include in CPM).
     */
    float getPriority(Object last_sent, Object current);

    /**
     * @brief Calculate object priority using ETSI TS 103 324 rules.
     * Evaluates position change, speed change, heading change, and time since last inclusion.
     * @param last_sent The previously sent object state.
     * @param current The most recent object state.
     * @return float Priority value based on ETSI thresholds.
     */
    float priorityETSI(Object last_sent, Object current);

    /**
     * @brief Calculate object priority using movement prediction.
     * Estimates position based on last dynamics sent and computed the error of the prediction.
     * @param last_sent The previously sent object state.
     * @param current The most recent object state.
     * @return float Priority value based on predicted movement.
     */
    float priorityMovementPredictor(Object last_sent, Object current);

    /**
     * @brief Convert degrees to radians.
     * @param deg Angle in degrees.
     * @return double Angle in radians.
     */
    double deg2rad(double deg) {
        return deg * M_PI_180;
    }

    /**
     * @brief Convert radians to degrees.
     * @param rad Angle in radians.
     * @return double Angle in degrees.
     */
    double rad2deg(double rad) {
        return rad * M_180_PI;
    }

    // Static instance pointer for the DDS callback.
    static Aggregator* instance_;
};

#endif // AGGREGATOR_H