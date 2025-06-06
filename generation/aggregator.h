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

struct Object {
    int objectID;
    int cpmObjectID;
    int sensorID;
    double timestamp;
    int classification;
    int confidence;
    float speed;
    float heading;
    float acceleration;
    float latitude;
    float longitude;
    float altitude = NOT_PRESENT_FLOAT;
    float size_x = NOT_PRESENT_FLOAT;
    float size_y = NOT_PRESENT_FLOAT;
    float size_z = NOT_PRESENT_FLOAT;
    float angular_velocity = NOT_PRESENT_FLOAT;
    float cov_latitude = NOT_PRESENT_FLOAT;
    float cov_longitude = NOT_PRESENT_FLOAT;
    float cov_altitude = NOT_PRESENT_FLOAT;
    float cov_heading = NOT_PRESENT_FLOAT;
    float cov_speed = NOT_PRESENT_FLOAT;
    float cov_angular_velocity = NOT_PRESENT_FLOAT;
};

struct ObjectEntity {
    bool to_send = false;
    Object last_sent;
    Object current;
    double priority = 0.0;
    bool has_updated = false;
};

struct SensorInfo {
    int sensorID = NOT_PRESENT_INT;
    int sensorType = NOT_PRESENT_INT;
    bool shadowingApplies = false;
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
     */
    Aggregator(int domainId, long maxObjectAgeS = 300, long cleanInterval = 5, bool ignoreRules = false, bool performanceLogs = false, const std::string& zenohEndpoint = "");

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
     * Called when a new DDS/Zenoh message arrives on "cps/objects".
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
     * @brief Static Zenoh objects callback that forwards the call to the Aggregator instance.
     * This is needed because the Zenoh client does not allow passing a context pointer.
     */

    static void zenohObjsCallback(zenoh::Sample &sample);

    /**
     * @brief Static Zenoh sensors callback that forwards the call to the Aggregator instance.
     * This is needed because the Zenoh client does not allow passing a context pointer.
     */
    static void zenohSensorsCallback(zenoh::Sample &sample);

    /**
     * @brief get the current timestamp as a string.
     * @return std::string The current timestamp as a string.
     */
    std::string getCurrentTimestampString() {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&tt);
        std::ostringstream oss;
        // Format as: YYYY-MM-DD HH:MM:SS
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

private:
    Dds* dds_;

    zenoh::Session* session_ = nullptr;
    zenoh::Subscriber<void>* subscriber_objects_   = nullptr; 
    zenoh::Subscriber<void>* subscriber_sensors_    = nullptr;
    
    std::mutex objMtx_;
    std::unordered_map<int, ObjectEntity> all_objects_; // All objects received

    std::mutex sensorMtx_;
    std::unordered_map<int, SensorInfo> sensor_info_; // Sensor information

    // Freshness thresholds
    double minTimeDiff_ = 1000;      // Minimum time difference in milliseconds
    double minDistanceDiff_ = 4.0;   // Minimum distance difference in meters
    double minSpeedDiff_ = 0.5;      // Minimum speed difference (m/s)
    double minHeadingDiff_ = 4.0;    // Minimum heading difference (degrees)
    bool ignoreRules_;               // Ignore freshness rules
    bool performanceLogs_;           // Enable performance logs
    std::string zenohEndpoint_;      // Zenoh endpoint for communication
    std::string priorityType_;       // Priority type

    // Cleanup configuration
    long maxObjectAge_;     // Maximum object age in seconds
    long cleanInterval_;    // Cleanup interval in seconds

    // ID map for CPM object IDs
    int currentID_ = 1;
    std::unordered_map<int, int> idMap_;

    // Thread control for periodic cleanup
    std::atomic<bool> stopFlag_;
    std::thread runThread_;

    // File logger
    std::shared_ptr<spdlog::logger> aggregator_file_logger_;

    /**
     * @brief The main loop that periodically cleans the lastSent list.
     */
    void runLoop();

    /**
     * @brief Remove entries from lastSent_ that are older than maxObjectAgeMs_.
     */
    void cleanLastSent();

    /**
     * @brief Check if a new object is fresh compared to the previously sent object.
     */
    bool isFresh(const Object& newObj, const Object& oldObj);

    /**
     * @brief Calculate the distance between two lat/lon coordinates (in meters) using the Haversine formula.
     */
    double calculateHaversineDistance(double lat1, double lon1, double lat2, double lon2);

    /**
     * @brief Calculate the CPM object ID based on the sensor and object IDs.
     */
    int calculateCpmObjectID(int sensorID, int objectID);

    /**
     * @brief Calculate the priority of an object.
     * @return float Priority value.
     */

    float getPriority(Object last_sent, Object current);

    /**
     * @brief Calculate the priority of an object based the ETSI rules.
     * @return float Priority value.
     */
    float priorityETSI(Object last_sent, Object current);

    /**
     * @brief Calculate the priority of an object based on movement predictor.
     * @return float Priority value.
     */
    float priorityMovementPredictor(Object last_sent, Object current);

    /**
     * @brief Convert degrees to radians
     * @return double Angle in radians
     */
    double deg2rad(double deg) {
        return deg * M_PI_180;
    }

    /**
     * @brief Convert radians to degrees
     * @return double Angle in degrees
     */
    double rad2deg(double rad) {
        return rad * M_180_PI;
    }

    // Static instance pointer for the DDS callback.
    static Aggregator* instance_;
};

#endif // AGGREGATOR_H