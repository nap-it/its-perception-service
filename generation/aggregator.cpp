#include "aggregator.h"
#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>

// Define the static instance pointer.
Aggregator* Aggregator::instance_ = nullptr;


// Constructor: sets up DDS and subscribes to the "cps/objects" topic.
Aggregator::Aggregator(int domainId, long maxObjectAgeMs, long cleanIntervalMs, bool debug)
    : maxObjectAgeMs_(maxObjectAgeMs), cleanIntervalMs_(cleanIntervalMs), stopFlag_(false)
{
    // Set the static instance pointer to this object.
    instance_ = this;

    // Initialize the DDS client.
    dds_ = new Dds("CPS-aggregator", domainId, ddsCallback);
    dds_->subscribe("cps/objects");
    dds_->provision_publisher("cps/pending");
    spdlog::info("Aggregator initialized on DDS domain {} and subscribed to 'cps/objects'", domainId);

    // Set the verbosity level for the logger.
    if (debug) spdlog::set_level(spdlog::level::debug);
    else spdlog::set_level(spdlog::level::info);
    spdlog::info("Aggregator verbosity level set to {}", debug ? "debug" : "info");
}

Aggregator::~Aggregator() {
    stop();
    delete dds_;
    instance_ = nullptr;
}

void Aggregator::run() {
    stopFlag_ = false;
    runThread_ = std::thread(&Aggregator::runLoop, this);
    spdlog::info("Aggregator run loop started.");
}

void Aggregator::stop() {
    stopFlag_ = true;
    if (runThread_.joinable()) {
        runThread_.join();
    }
    spdlog::info("Aggregator run loop stopped.");
}

std::string serializeFreshObjects(const std::vector<Object>& freshObjects) {
    json j;
    j["objects"] = json::array();
    for (const auto& obj : freshObjects) {
        json objJson;
        objJson["objectID"] = obj.objectID;
        objJson["sensorID"] = obj.sensorID;
        objJson["timestamp"] = obj.timestamp;
        objJson["classification"] = obj.classification;
        objJson["confidence"] = obj.confidence;
        objJson["speed"] = obj.speed;
        objJson["heading"] = obj.heading;
        objJson["acceleration"] = obj.acceleration;
        objJson["latitude"] = obj.latitude;
        objJson["longitude"] = obj.longitude;
        objJson["size_x"] = obj.size_x;
        j["objects"].push_back(objJson);
    }
    return j.dump();
}

void Aggregator::runLoop() {
    while (!stopFlag_) {
        cleanLastSent();
        std::this_thread::sleep_for(std::chrono::milliseconds(cleanIntervalMs_));
    }
}

void Aggregator::cleanLastSent() {
    // Get the current time in milliseconds.
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(mtx_);
    for (auto it = lastSent_.begin(); it != lastSent_.end(); ) {
        unsigned long obj_ts = static_cast<unsigned long>(it->second.timestamp * 1000);
        if (now - obj_ts > maxObjectAgeMs_) {
            spdlog::debug("Cleaning object (ID: {}) from lastSent (age > {} ms)", it->first, maxObjectAgeMs_);
            it = lastSent_.erase(it);
        } else {
            ++it;
        }
    }
}


std::vector<Object> Aggregator::getFreshObjects() {
    spdlog::debug("Aggregator getFreshObjects()");
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<Object> freshList;

    // Transfer pending objects to freshList and update lastSent_.
    for (const auto& pair : pendingObjects_) {
        freshList.push_back(pair.second);
        lastSent_[pair.first] = pair.second;
    }
    pendingObjects_.clear();

    std::string message = serializeFreshObjects(freshList);
    dds_->publish("cps/pending", message);
    return freshList;
}


bool Aggregator::isFresh(const Object& newObj, const Object& oldObj) {
    long new_ts = static_cast<long>(newObj.timestamp * 1000);
    long old_ts = static_cast<long>(oldObj.timestamp * 1000);
    long dt = new_ts - old_ts;

    // If at least one condition is met, we could return true:
    bool timeCondition = (dt >= minTimeDiff_);
    double distance = calculateDistance(newObj.latitude, newObj.longitude, oldObj.latitude, oldObj.longitude);
    bool distanceCondition = (distance >= minDistanceDiff_);
    bool speedCondition = (std::abs(newObj.speed - oldObj.speed) >= minSpeedDiff_);
    bool headingCondition = (std::abs(newObj.heading - oldObj.heading) >= minHeadingDiff_);

    if (timeCondition || distanceCondition || speedCondition || headingCondition) {
        return true;
    }
    return false;
}



double Aggregator::calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000; // Earth's radius in meters
    double radLat1 = lat1 * M_PI / 180.0;
    double radLat2 = lat2 * M_PI / 180.0;
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(radLat1) * std::cos(radLat2) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return R * c;
}


void Aggregator::on_message_dds(const std::string& topic, const std::string& message) {
    spdlog::debug("Aggregator received DDS message on topic '{}': {}", topic, message);
    try {
        json j = json::parse(message);

        if (j.contains("objects") && j["objects"].is_array()) {
            for (const auto& objJson : j["objects"]) {
                Object obj;
                obj.objectID = objJson.value("objectID", -1);
                obj.sensorID = objJson.value("sensorID", 1);
                obj.timestamp = objJson.value("timestamp", 0.0);
                obj.classification = objJson.value("classification", 0);
                obj.confidence = objJson.value("confidence", 0);
                obj.speed = objJson.value("speed", 0.0f);
                obj.heading = objJson.value("heading", 0.0f);
                obj.acceleration = objJson.value("acceleration", 0.0f);
                obj.latitude = objJson.value("latitude", 0.0f);
                obj.longitude = objJson.value("longitude", 0.0f);
                obj.size_x = objJson.value("size_x", 0.0f);

                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    auto itPending = pendingObjects_.find(obj.objectID);
                    auto itLast = lastSent_.find(obj.objectID);

                    if (itPending != pendingObjects_.end()) {
                        // Object is already pending, update its attributes
                        itPending->second = obj;
                        spdlog::debug("Object (ID: {}) updated in pending objects.", obj.objectID);
                    } else if (itLast != lastSent_.end()) {
                        // Object is in last sent, check freshness rules
                        if (isFresh(obj, itLast->second)) {
                            pendingObjects_[obj.objectID] = obj;
                            spdlog::debug("Object (ID: {}) added to pending objects as fresh.", obj.objectID);
                        } else {
                            spdlog::debug("Object (ID: {}) discarded as not fresh enough.", obj.objectID);
                        }
                    } else {
                        // Completely new object
                        pendingObjects_[obj.objectID] = obj;
                        spdlog::debug("New object (ID: {}) added to pending objects.", obj.objectID);
                    }
                }
            }
        }
    } catch (const json::parse_error& e) {
        spdlog::error("JSON parse error: {} in message: {}", e.what(), message);
    } catch (const std::exception& e) {
        spdlog::error("Error processing DDS message: {}", e.what());
    }
}


void Aggregator::ddsCallback(const std::string& topic, const std::string& message) {
    // Forward the callback to the instance method.
    if (instance_) {
        instance_->on_message_dds(topic, message);
    }
}