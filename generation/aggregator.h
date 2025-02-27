#ifndef AGGREGATOR_H
#define AGGREGATOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>
#include "fastdds-cpp-wrapper/dds.hpp"  // Adjust include if needed

using json = nlohmann::json;

constexpr float NOT_PRESENT_FLOAT = -999.0f;
constexpr double NOT_PRESENT_DOUBLE = -999.0;
constexpr int NOT_PRESENT_INT = -999;

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
};

struct SensorInfo {
    int sensorID = NOT_PRESENT_INT;
    int sensorType = NOT_PRESENT_INT;
    bool shadowingApplies = false;
    int semiMajorRangeLength = NOT_PRESENT_INT;
    int semiMinorRangeLength = NOT_PRESENT_INT;
    int semiMajorRangeOrientation = NOT_PRESENT_INT;
    int range = NOT_PRESENT_INT;
    int stationaryHorizontalOpeningAngleStart = NOT_PRESENT_INT;
    int stationaryHorizontalOpeningAngleEnd = NOT_PRESENT_INT;
};

class Aggregator {
public:
    /**
     * @brief Construct a new Aggregator object.
     * @param domainId DDS domain ID.
     * @param maxObjectAge Maximum age (in seconds) for an object in the lastSent list (default: 5 minutes).
     * @param cleanInterval Interval (in seconds) to run the cleanup routine (default: 5 seconds).
     * @param debugLevel Verbosity level for logging (default: 0).
     */
    Aggregator(int domainId, long maxObjectAgeS = 300, long cleanInterval = 5, bool debug = false);

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
     * @brief Non-static DDS message handler.
     * Called when a new DDS message arrives on "cps/objects".
     * @param topic The DDS topic.
     * @param message The received message (JSON formatted).
     */
    void on_message_dds(const std::string& topic, const std::string& message);

    /**
     * @brief Static DDS callback that forwards the call to the Aggregator instance.
     * This is needed because the DDS client does not allow passing a context pointer.
     */
    static void ddsCallback(const std::string& topic, const std::string& message);

private:
    Dds* dds_;
    std::mutex objMtx_;
    std::unordered_map<int, ObjectEntity> all_objects_; // All objects received

    std::mutex sensorMtx_;
    std::unordered_map<int, SensorInfo> sensor_info_; // Sensor information

    // Freshness thresholds
    double minTimeDiff_ = 1000;      // Minimum time difference in milliseconds
    double minDistanceDiff_ = 4.0;   // Minimum distance difference in meters
    double minSpeedDiff_ = 0.5;      // Minimum speed difference (m/s)
    double minHeadingDiff_ = 4.0;    // Minimum heading difference (degrees)

    // Cleanup configuration
    long maxObjectAge_;     // Maximum object age in seconds
    long cleanInterval_;    // Cleanup interval in seconds

    // ID map for CPM object IDs
    int currentID_ = 1;
    std::unordered_map<int, int> idMap_;

    // Thread control for periodic cleanup
    std::atomic<bool> stopFlag_;
    std::thread runThread_;

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
    double calculateDistance(double lat1, double lon1, double lat2, double lon2);

    /**
     * @brief Calculate the CPM object ID based on the sensor and object IDs.
     */
    int calculateCpmObjectID(int sensorID, int objectID);

    // Static instance pointer for the DDS callback.
    static Aggregator* instance_;
};

#endif // AGGREGATOR_H