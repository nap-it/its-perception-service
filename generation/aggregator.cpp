#include "aggregator.h"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <ctime>
#include <spdlog/spdlog.h>

// Define the static instance pointer.
Aggregator* Aggregator::instance_ = nullptr;


// Constructor: sets up DDS and subscribes to the "cps/objects" topic.
Aggregator::Aggregator(int ddsDomain, long maxObjectAge, long cleanInterval, bool debug)
    : maxObjectAge_(maxObjectAge), cleanInterval_(cleanInterval), stopFlag_(false)
{
    // Set the static instance pointer to this object.
    instance_ = this;

    // Initialize the DDS client.
    dds_ = new Dds("CPS-aggregator", ddsDomain, ddsCallback);
    dds_->subscribe("cps/objects");
    dds_->subscribe("cps/sensors");
    dds_->provision_publisher("cps/pending");
    spdlog::info("[Aggregator] Initialized on DDS domain {} and subscribed to 'cps/objects'", ddsDomain);

    // Set the verbosity level for the logger.
    if (debug) spdlog::set_level(spdlog::level::debug);
    else spdlog::set_level(spdlog::level::info);
    spdlog::debug("[Aggregator] Verbosity level set to {}", debug ? "debug" : "info");
}

Aggregator::~Aggregator() {
    stop();
    delete dds_;
    instance_ = nullptr;
}

void Aggregator::run() {
    stopFlag_ = false;
    runThread_ = std::thread(&Aggregator::runLoop, this);
    spdlog::info("[Aggregator] Run loop started.");
}

void Aggregator::stop() {
    stopFlag_ = true;
    if (runThread_.joinable()) {
        runThread_.join();
    }
    spdlog::info("[Aggregator] Run loop stopped.");
}

void Aggregator::runLoop() {
    while (!stopFlag_) {
        cleanLastSent();
        std::this_thread::sleep_for(std::chrono::seconds(cleanInterval_));
    }
}

std::string serializeFreshObjects(const std::vector<ObjectEntity>& freshObjects) {
    json j;
    j["objects"] = json::array();
    for (const auto& obj : freshObjects) {
        json objJson;
        objJson["objectID"] = obj.current.objectID;
        objJson["sensorID"] = obj.current.sensorID;
        objJson["timestamp"] = obj.current.timestamp;
        objJson["classification"] = obj.current.classification;
        objJson["confidence"] = obj.current.confidence;
        objJson["speed"] = obj.current.speed;
        objJson["heading"] = obj.current.heading;
        objJson["acceleration"] = obj.current.acceleration;
        objJson["latitude"] = obj.current.latitude;
        objJson["longitude"] = obj.current.longitude;
        objJson["size_x"] = obj.current.size_x;
        j["objects"].push_back(objJson);
    }
    return j.dump();
}

// Utility function: convert epoch seconds to a formatted string.
std::string formatTimestamp(long epochSeconds) {
    std::time_t t = epochSeconds;
    std::tm tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%F %T"); // Format: YYYY-MM-DD HH:MM:SS
    return oss.str();
}

void Aggregator::cleanLastSent() {
    std::lock_guard<std::mutex> lock(objMtx_);
    for (auto it = all_objects_.begin(); it != all_objects_.end(); ) {
        // Get the current time in seconds (Unix epoch)
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count();

        double raw_obj_ts = it->second.current.timestamp;
        unsigned long obj_ts = static_cast<unsigned long>(raw_obj_ts);

        if(now < obj_ts) {
            ++it;
            continue;
        }

        if (now - obj_ts > maxObjectAge_) {
            it = all_objects_.erase(it);
        } else {
            ++it;
        }
    }
}

void print_all_objects(std::unordered_map<int, ObjectEntity> all_objects_) {
    spdlog::debug("-----------------------------------------");
    for (const auto& pair : all_objects_) {
        spdlog::debug("[Aggregator] Object ID: {}, to_send: {}, current timestamp: {}, last sent timestamp: {}, priority: {}",
                      pair.first, pair.second.to_send, pair.second.current.timestamp, pair.second.last_sent.timestamp, pair.second.priority);
    }
}


std::vector<Object> Aggregator::getFreshObjects(int maxObjects) {

    auto t1 = std::chrono::high_resolution_clock::now();
    spdlog::debug("[Aggregator] getFreshObjects()");
    std::lock_guard<std::mutex> lock(objMtx_);
    std::vector<ObjectEntity> freshList;
    std::vector<ObjectEntity> pendingList;

    print_all_objects(all_objects_);
    // Transfer pending objects to freshList
    for (const auto& pair : all_objects_) {
        if (pair.second.to_send) {
            freshList.push_back(pair.second);
        } else {
            pendingList.push_back(pair.second);
        }
    }

    if (freshList.empty()) {
        spdlog::debug("[Aggregator] No fresh objects available.");
        return {};
    }

    // If maxObjects is set, limit the number of fresh objects.
    if (maxObjects != -1){
        if (freshList.size() > maxObjects) {
            spdlog::warn("[Aggregator] Requested {} fresh objects, but {} available. Limiting to {}.", maxObjects, freshList.size(), maxObjects);
            //Sort freshList by priority (descending)
            std::sort(freshList.begin(), freshList.end(), [](const ObjectEntity& a, const ObjectEntity& b) {
                return a.priority > b.priority;
            });

            for (int i = 0; i < maxObjects; i++) {
                all_objects_[freshList[i].current.objectID].to_send = false;
                all_objects_[freshList[i].current.objectID].last_sent = freshList[i].current;
                all_objects_[freshList[i].current.objectID].priority = 0.0;
            }

            freshList.resize(maxObjects);

        } else if (freshList.size() < maxObjects) {
            spdlog::warn("[Aggregator] Requested {} fresh objects, but only {} available.", maxObjects, freshList.size());
            // Sort pendingList by priority (descending)

            if (pendingList.size() == 0) {
                // Update all_objects_ with the freshList
                for (int i = 0; i < freshList.size(); i++) {
                    all_objects_[freshList[i].current.objectID].to_send = false;
                    all_objects_[freshList[i].current.objectID].last_sent = freshList[i].current;
                    all_objects_[freshList[i].current.objectID].priority = 0.0;
                }
            } else {
                std::sort(pendingList.begin(), pendingList.end(), [](const ObjectEntity& a, const ObjectEntity& b) {
                    return a.priority > b.priority;
                });

                spdlog::debug("[Aggregator] Pending list size: {}", pendingList.size());

                // Update all_objects_ firstly from the freshList
                for (int i = 0; i < freshList.size(); i++) {
                    all_objects_[freshList[i].current.objectID].to_send = false;
                    all_objects_[freshList[i].current.objectID].last_sent = freshList[i].current;
                    all_objects_[freshList[i].current.objectID].priority = 0.0;
                }

                // Then, add the remaining objects from pendingList to freshList
                // However, check if there are enough objects in pendingList

                int remaining = maxObjects - freshList.size();
                if (remaining > pendingList.size()) {
                    spdlog::warn("[Aggregator] Requested {} fresh objects, but only {} available in total. Limiting to {}.", maxObjects, freshList.size() + pendingList.size(), freshList.size() + pendingList.size());
                    remaining = pendingList.size();
                }

                for (int i = 0; i < remaining; i++) {
                    freshList.push_back(pendingList[i]);
                    all_objects_[pendingList[i].current.objectID].to_send = false;
                    all_objects_[pendingList[i].current.objectID].last_sent = pendingList[i].current;
                    all_objects_[pendingList[i].current.objectID].priority = 0.0;
                }
            }
                
        } else {
            spdlog::debug("[Aggregator] Requested {} fresh objects, and {} available.", maxObjects, freshList.size());
            for (int i = 0; i < freshList.size(); i++) {
                all_objects_[freshList[i].current.objectID].to_send = false;
                all_objects_[freshList[i].current.objectID].last_sent = freshList[i].current;
                all_objects_[freshList[i].current.objectID].priority = 0.0;
            }
        }


    } else {
        // Update all_objects_ with the freshList
        for (int i = 0; i < freshList.size(); i++) {
            all_objects_[freshList[i].current.objectID].to_send = false;
            all_objects_[freshList[i].current.objectID].last_sent = freshList[i].current;
            all_objects_[freshList[i].current.objectID].priority = 0.0;
        }
    }


    std::string message = serializeFreshObjects(freshList);


    dds_->publish("cps/pending", message);

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    spdlog::debug("[Aggregator] getFreshObjects() took {} us", duration);
    
    std::vector<Object> freshObjects;
    for (const auto& obj : freshList) {
        freshObjects.push_back(obj.current);
    }
    return freshObjects;
}

std::unordered_map<int, SensorInfo> Aggregator::getSensorInfo() {
    spdlog::debug("[Aggregator] getSensorInfo()");
    std::lock_guard<std::mutex> lock(sensorMtx_);
    return sensor_info_;
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
    //spdlog::debug("[Aggregator] Received DDS message on topic '{}': {}", topic, message);
    try {
        json j = json::parse(message);
        if (topic == "cps/objects") {
            if (j.contains("objects") && j["objects"].is_array()) {
                auto raw = j["objects"][0]["timestamp"];
                for (const auto& objJson : j["objects"]) {

                    // Parse the object attributes.
                    Object obj;
                    obj.objectID = objJson.value("objectID", NOT_PRESENT_INT);
                    if ((obj.objectID == NOT_PRESENT_INT || obj.objectID < 0) && (spdlog::error("[Aggregator]: Mandatory (Object ID) not present in message: {}", message), true)) continue;
                    obj.sensorID = objJson.value("sensorID", NOT_PRESENT_INT);
                    if (obj.sensorID == NOT_PRESENT_INT && (spdlog::error("[Aggregator]: Mandatory (Sensor ID) not present in message: {}", message), true)) continue;
                    obj.timestamp = objJson.value("timestamp", NOT_PRESENT_DOUBLE);
                    if (obj.timestamp == NOT_PRESENT_DOUBLE && (spdlog::error("[Aggregator]: Mandatory (Timestamp) not present in message: {}", message), true)) continue;
                    obj.classification = objJson.value("classification", 0);
                    obj.confidence = objJson.value("confidence", 0);
                    obj.speed = objJson.value("speed", 0.0f);
                    obj.heading = objJson.value("heading", 0.0f);
                    obj.acceleration = objJson.value("acceleration", 0.0f);
                    obj.latitude = objJson.value("latitude", NOT_PRESENT_FLOAT);
                    if (obj.latitude == NOT_PRESENT_FLOAT && (spdlog::error("[Aggregator]: Mandatory (Latitude) not present in message: {}", message), true)) continue;
                    obj.longitude = objJson.value("longitude", NOT_PRESENT_FLOAT);
                    if (obj.longitude == NOT_PRESENT_FLOAT && (spdlog::error("[Aggregator]: Mandatory (Longitude) not present in message: {}", message), true)) continue;
                    obj.altitude = objJson.value("altitude", NOT_PRESENT_FLOAT);
                    obj.size_x = objJson.value("size_x", NOT_PRESENT_FLOAT);
                    obj.size_y = objJson.value("size_y", NOT_PRESENT_FLOAT);
                    obj.size_z = objJson.value("size_z", NOT_PRESENT_FLOAT);
                    obj.angular_velocity = objJson.value("angular_velocity", NOT_PRESENT_FLOAT);
                    obj.cov_latitude = objJson.value("cov_latitude", NOT_PRESENT_FLOAT);
                    obj.cov_longitude = objJson.value("cov_longitude", NOT_PRESENT_FLOAT);
                    obj.cov_altitude = objJson.value("cov_altitude", NOT_PRESENT_FLOAT);
                    obj.cov_heading = objJson.value("cov_heading", NOT_PRESENT_FLOAT);
                    obj.cov_speed = objJson.value("cov_speed", NOT_PRESENT_FLOAT);
                    obj.cov_angular_velocity = objJson.value("cov_angular_velocity", NOT_PRESENT_FLOAT);

                    // spdlog::debug("[Aggregator] Received object (ID: {}) from sensor (ID: {})", obj.objectID, obj.sensorID);

                    {
                        std::lock_guard<std::mutex> lock(objMtx_);
                        auto it = all_objects_.find(obj.objectID);

                        if (it == all_objects_.end()) {
                            // Object not present: create a new entity
                            ObjectEntity entity;
                            entity.current = obj;
                            entity.last_sent = obj;
                            entity.to_send = true;  // Mark it to be sent
                            entity.priority = 100.0; // Default max priority
                            
                            all_objects_[obj.objectID] = entity;
                            //spdlog::debug("[Aggregator] New object (ID: {}) added to all objects.", obj.objectID);
                        } else {
                            // Object already exists: update the current attributes.
                            it->second.current = obj;

                            // Future: 
                            // it->second.priority = calculatePriority(obj, it->second.last_sent);

                            // If not already marked for sending, check if it is fresh compared to last_sent.
                            if (!it->second.to_send) {
                                if(isFresh(obj, it->second.last_sent)) {
                                    it->second.to_send = true;
                                    //spdlog::debug("[Aggregator] Object (ID: {}) marked as fresh and to_send set to true.", obj.objectID);
                                } else {
                                    //spdlog::debug("[Aggregator] Object (ID: {}) discarded as not fresh enough., not makred for sending", obj.objectID);
                                }
                            } else {
                                //spdlog::debug("[Aggregator] Object (ID: {}) already marked for sending, updated current attributes.", obj.objectID);
                            }
                        }
                    }
                }
            }
        } else if (topic == "cps/sensors") {
            SensorInfo sensor;
            sensor.sensorID = j.value("sensorID", NOT_PRESENT_INT);
            if (sensor.sensorID == NOT_PRESENT_INT && (spdlog::error("[Aggregator]: Mandatory (Sensor ID) not present in message: {}", message), true)) return;
            sensor.sensorType = j.value("sensorType", NOT_PRESENT_INT);
            sensor.shadowingApplies = j.value("shadowingApplies", false);
            sensor.semiMajorRangeLength = j.value("semiMajorRangeLength", NOT_PRESENT_INT);
            sensor.semiMinorRangeLength = j.value("semiMinorRangeLength", NOT_PRESENT_INT);
            sensor.semiMajorRangeOrientation = j.value("semiMajorRangeOrientation", NOT_PRESENT_INT);
            sensor.range = j.value("range", NOT_PRESENT_INT);
            sensor.stationaryHorizontalOpeningAngleStart = j.value("stationaryHorizontalOpeningAngleStart", NOT_PRESENT_INT);
            sensor.stationaryHorizontalOpeningAngleEnd = j.value("stationaryHorizontalOpeningAngleEnd", NOT_PRESENT_INT);

            {
                std::lock_guard<std::mutex> lock(sensorMtx_);
                sensor_info_[sensor.sensorID] = sensor;
                spdlog::debug("[Aggregator] Sensor info updated for sensorID: {}", sensor.sensorID);
            }
        } else {
            spdlog::warn("[Aggregator] Received message on unknown topic: {}", topic);
        }
    } catch (const json::parse_error& e) {
        spdlog::error("[Aggregator] JSON parse error: {} in message: {}", e.what(), message);
    } catch (const std::exception& e) {
        spdlog::error("[Aggregator] Error processing DDS message: {}", e.what());
    }
}


void Aggregator::ddsCallback(const std::string& topic, const std::string& message) {
    // Forward the callback to the instance method.
    if (instance_) {
        instance_->on_message_dds(topic, message);
    }
}