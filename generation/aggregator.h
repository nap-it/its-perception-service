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
    float altitude = 0.0f;
    float size_x = 0.0f;
    float size_y = 0.0f;
    float size_z = 0.0f;
    float angular_velocity = 0.0f;
    float cov_latitude = 0.0f;
    float cov_longitude = 0.0f;
    float cov_altitude = 0.0f;
    float cov_heading = 0.0f;
    float cov_speed = 0.0f;
    float cov_angular_velocity = 0.0f;
};

class Aggregator {
public:
    /**
     * @brief Construct a new Aggregator object.
     * @param domainId DDS domain ID.
     * @param maxObjectAgeMs Maximum age (in ms) for an object in the lastSent list (default: 5 minutes).
     * @param cleanIntervalMs Interval (in ms) to run the cleanup routine (default: 5 seconds).
     * @param debugLevel Verbosity level for logging (default: 0).
     */
    Aggregator(int domainId, long maxObjectAgeMs = 300000, long cleanIntervalMs = 5000, bool debug = false);

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
    std::vector<Object> getFreshObjects();

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
    std::mutex mtx_;
    std::unordered_map<int, Object> lastSent_;     // Objects that have been included in a CPM
    std::unordered_map<int, Object> pendingObjects_; // New objects waiting to be sent

    // Freshness thresholds
    double minTimeDiff_ = 1000;      // Minimum time difference in milliseconds
    double minDistanceDiff_ = 4.0;   // Minimum distance difference in meters
    double minSpeedDiff_ = 0.5;      // Minimum speed difference (m/s)
    double minHeadingDiff_ = 4.0;    // Minimum heading difference (degrees)

    // Cleanup configuration
    long maxObjectAgeMs_;
    long cleanIntervalMs_;

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

    // Static instance pointer for the DDS callback.
    static Aggregator* instance_;
};

#endif // AGGREGATOR_H