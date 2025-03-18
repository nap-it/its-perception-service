#include "aggregator.h"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <ctime>    
#include <filesystem>

namespace fs = std::filesystem;

// Define the static instance pointer.
Aggregator* Aggregator::instance_ = nullptr;

// Constructor: sets up DDS and subscribes to the "cps/objects" topic.
Aggregator::Aggregator(int ddsDomain, long maxObjectAge, long cleanInterval, bool ignoreRules, bool performanceLogs)
    : maxObjectAge_(maxObjectAge), cleanInterval_(cleanInterval), stopFlag_(false), ignoreRules_(ignoreRules), currentID_(1), performanceLogs_(performanceLogs)
{
    // Set the static instance pointer to this object.
    instance_ = this;

    // File logger
    if (!fs::exists("./logs")) {
        fs::create_directory("./logs");
    } else if (fs::exists("./logs/aggregator.csv")) {
        fs::remove("./logs/aggregator.csv");
    }

    if(performanceLogs_) {
        aggregator_file_logger_ = spdlog::basic_logger_mt("aggregator_logger", "./logs/aggregator.csv");
        aggregator_file_logger_->set_pattern("%v");
        aggregator_file_logger_->flush_on(spdlog::level::info);
    }

    // Initialize the DDS client.
    dds_ = new Dds("CPS-aggregator", ddsDomain, ddsCallback);
    dds_->subscribe("cps/objects");
    dds_->subscribe("cps/sensors");
    //dds_->provision_publisher("cps/pending");
    spdlog::info("[Aggregator] Initialized on DDS domain {} and subscribed to 'cps/objects'", ddsDomain);

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
    rj::Document doc;
    doc.SetObject();
    rj::Document::AllocatorType& alloc = doc.GetAllocator();
    
    rj::Value objectsArray(rj::kArrayType);
    
    for (const auto& entity : freshObjects) {
        const Object& obj = entity.current;
        rj::Value objJson(rj::kObjectType);
        objJson.AddMember("objectID", obj.objectID, alloc);
        objJson.AddMember("sensorID", obj.sensorID, alloc);
        objJson.AddMember("timestamp", obj.timestamp, alloc);
        objJson.AddMember("classification", obj.classification, alloc);
        objJson.AddMember("confidence", obj.confidence, alloc);
        objJson.AddMember("speed", obj.speed, alloc);
        objJson.AddMember("heading", obj.heading, alloc);
        objJson.AddMember("acceleration", obj.acceleration, alloc);
        objJson.AddMember("latitude", obj.latitude, alloc);
        objJson.AddMember("longitude", obj.longitude, alloc);
        objJson.AddMember("size_x", obj.size_x, alloc);
    
        objectsArray.PushBack(objJson, alloc);
    }
    
    doc.AddMember("objects", objectsArray, alloc);
    
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

int Aggregator::calculateCpmObjectID(int sensorID, int objectID) {
    int combinedId = (sensorID << 16) | (objectID & 0xFFFF);
    if (idMap_.find(combinedId) != idMap_.end()) {
        return idMap_[combinedId];
    } else {
        int newID = currentID_++;
        if (currentID_ > 65535) currentID_ = 1;
        idMap_.emplace(combinedId, newID);
        return newID;
    }
}

void Aggregator::cleanLastSent() {
    std::lock_guard<std::mutex> lock(objMtx_);
    for (auto it = all_objects_.begin(); it != all_objects_.end(); ) {
        // Get the current time in seconds (Unix epoch)
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        double raw_obj_ts = it->second.current.timestamp;
        unsigned long obj_ts = static_cast<unsigned long>(raw_obj_ts);

        if(now < obj_ts) {
            ++it;
            continue;
        }

        if (now - obj_ts > maxObjectAge_) {
            int combinedId = (it->second.current.sensorID << 16) | (it->second.current.objectID & 0xFFFF);
            idMap_.erase(combinedId);
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


    // std::string message = serializeFreshObjects(freshList);
    // dds_->publish("cps/pending", message);
    
    std::vector<Object> freshObjects;
    for (const auto& obj : freshList) {
        freshObjects.push_back(obj.current);
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    spdlog::debug("[Aggregator] getFreshObjects() took {} us", duration);
    if (performanceLogs_)
        aggregator_file_logger_->info("Aggregator,getFreshObjects,{},{},{}", getCurrentTimestampString(), freshObjects.size(), duration);

    return freshObjects;
}

std::unordered_map<int, SensorInfo> Aggregator::getSensorInfo() {
    spdlog::debug("[Aggregator] getSensorInfo()");
    std::lock_guard<std::mutex> lock(sensorMtx_);
    return sensor_info_;
}


bool Aggregator::isFresh(const Object& newObj, const Object& oldObj) {
    if (ignoreRules_) {
        return true;
    }

    long new_ts = static_cast<long>(newObj.timestamp * 1000);
    long old_ts = static_cast<long>(oldObj.timestamp * 1000);
    long dt = new_ts - old_ts;

    // If at least one condition is met, we could return true:
    bool timeCondition = (dt >= minTimeDiff_);
    //spdlog::debug("[Aggregator] (Object ID: {}) Time difference: {} ms", newObj.objectID, dt);
    double distance = calculateHaversineDistance(newObj.latitude, newObj.longitude, oldObj.latitude, oldObj.longitude);
    //spdlog::debug("[Aggregator] (Object ID: {}) Distance difference: {} = calculateHaversineDistance({}, {}, {}, {})", newObj.objectID, distance, newObj.latitude, newObj.longitude, oldObj.latitude, oldObj.longitude);
    bool distanceCondition = (distance >= minDistanceDiff_);
    bool speedCondition = (std::abs(newObj.speed - oldObj.speed) >= minSpeedDiff_);
    //spdlog::debug("[Aggregator] (Object ID: {}) Speed difference: {} = std::abs({} - {})", newObj.objectID, std::abs(newObj.speed - oldObj.speed), newObj.speed, oldObj.speed);
    bool headingCondition = (std::abs(newObj.heading - oldObj.heading) >= minHeadingDiff_);
    //spdlog::debug("[Aggregator] (Object ID: {}) Heading difference: {} = std::abs({} - {})", newObj.objectID, std::abs(newObj.heading - oldObj.heading), newObj.heading, oldObj.heading);

    if (timeCondition || distanceCondition || speedCondition || headingCondition) {
        return true;
    }
    return false;
}

double Aggregator::calculateHaversineDistance(double lat1, double lon1, double lat2, double lon2) {
    double phi1 = deg2rad(lat1);
    double phi2 = deg2rad(lat2);
    double delta_phi = deg2rad(lat2 - lat1);
    double delta_lambda = deg2rad(lon2 - lon1);

    double a = std::sin(delta_phi / 2.0) * std::sin(delta_phi / 2.0) +
               std::cos(phi1) * std::cos(phi2) * std::sin(delta_lambda / 2.0) * std::sin(delta_lambda / 2.0);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return R_E * c;
}


void Aggregator::on_message_dds(const std::string& topic, const std::string& message) {
    spdlog::debug("[Aggregator] Received DDS message on topic '{}': {}", topic, message);
    try {
        auto t1 = std::chrono::high_resolution_clock::now();
        rj::Document doc;
        doc.Parse(message.c_str());
        if (doc.HasParseError()) {
            spdlog::error("[Aggregator] Parse error in message: {}", message);
            return;
        }
        
        if (topic == "cps/objects") {
            if (doc.HasMember("objects") && doc["objects"].IsArray()) {
                const rj::Value& objectsArray = doc["objects"];
                for (auto& objJson : objectsArray.GetArray()) {
                    Object obj;

                    obj.objectID = (objJson.HasMember("objectID") && objJson["objectID"].IsInt()) ? objJson["objectID"].GetInt() : NOT_PRESENT_INT;
                    if ((obj.objectID == NOT_PRESENT_INT || obj.objectID < 0) && (spdlog::error("[Aggregator]: Mandatory (Object ID) not present in message: {}", message), true)) continue;
                    
                    obj.sensorID = (objJson.HasMember("sensorID") && objJson["sensorID"].IsInt()) ? objJson["sensorID"].GetInt() : NOT_PRESENT_INT;
                    if ((obj.sensorID == NOT_PRESENT_INT || obj.sensorID < 0) && (spdlog::error("[Aggregator]: Mandatory (Sensor ID) not present in message: {}", message), true)) continue;
                    obj.cpmObjectID = calculateCpmObjectID(obj.sensorID, obj.objectID);
                    
                    obj.timestamp = (objJson.HasMember("timestamp") && objJson["timestamp"].IsDouble()) ? objJson["timestamp"].GetDouble() : NOT_PRESENT_DOUBLE;
                    if ((obj.timestamp == NOT_PRESENT_DOUBLE || obj.timestamp < 0) && (spdlog::error("[Aggregator]: Mandatory (Timestamp) not present in message: {}", message), true)) continue;
                    
                    obj.classification = (objJson.HasMember("classification") && objJson["classification"].IsInt()) ? objJson["classification"].GetInt() : 0;
                    obj.confidence = (objJson.HasMember("confidence") && objJson["confidence"].IsInt()) ? objJson["confidence"].GetInt() : 0;
                    if (obj.confidence < 0) obj.confidence = 0;
                    if (obj.confidence > 100) obj.confidence = 100;
                    
                    obj.speed = (objJson.HasMember("speed") && objJson["speed"].IsFloat()) ? objJson["speed"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.heading = (objJson.HasMember("heading") && objJson["heading"].IsFloat()) ? objJson["heading"].GetFloat() : NOT_PRESENT_FLOAT;
                    if ((obj.heading != NOT_PRESENT_FLOAT) && (obj.heading < 0 || obj.heading >= 360)) obj.heading = 0;
                    
                    obj.acceleration = (objJson.HasMember("acceleration") && objJson["acceleration"].IsFloat()) ? objJson["acceleration"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.latitude = (objJson.HasMember("latitude") && objJson["latitude"].IsFloat()) ? objJson["latitude"].GetFloat() : NOT_PRESENT_FLOAT;
                    if ((obj.latitude == NOT_PRESENT_FLOAT || obj.latitude < -90 || obj.latitude > 90) && (spdlog::error("[Aggregator]: Mandatory (Latitude) not present in message: {}", message), true)) continue;
                    
                    obj.longitude = (objJson.HasMember("longitude") && objJson["longitude"].IsFloat()) ? objJson["longitude"].GetFloat() : NOT_PRESENT_FLOAT;
                    if ((obj.longitude == NOT_PRESENT_FLOAT || obj.longitude < -180 || obj.longitude > 180) && (spdlog::error("[Aggregator]: Mandatory (Longitude) not present in message: {}", message), true)) continue;
                    
                    obj.altitude = (objJson.HasMember("altitude") && objJson["altitude"].IsFloat()) ? objJson["altitude"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.size_x = (objJson.HasMember("size_x") && objJson["size_x"].IsFloat()) ? objJson["size_x"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.size_y = (objJson.HasMember("size_y") && objJson["size_y"].IsFloat()) ? objJson["size_y"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.size_z = (objJson.HasMember("size_z") && objJson["size_z"].IsFloat()) ? objJson["size_z"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.angular_velocity = (objJson.HasMember("angular_velocity") && objJson["angular_velocity"].IsFloat()) ? objJson["angular_velocity"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.cov_latitude = (objJson.HasMember("cov_latitude") && objJson["cov_latitude"].IsFloat()) ? objJson["cov_latitude"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.cov_longitude = (objJson.HasMember("cov_longitude") && objJson["cov_longitude"].IsFloat()) ? objJson["cov_longitude"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.cov_altitude = (objJson.HasMember("cov_altitude") && objJson["cov_altitude"].IsFloat()) ? objJson["cov_altitude"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.cov_heading = (objJson.HasMember("cov_heading") && objJson["cov_heading"].IsFloat()) ? objJson["cov_heading"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.cov_speed = (objJson.HasMember("cov_speed") && objJson["cov_speed"].IsFloat()) ? objJson["cov_speed"].GetFloat() : NOT_PRESENT_FLOAT;
                    obj.cov_angular_velocity = (objJson.HasMember("cov_angular_velocity") && objJson["cov_angular_velocity"].IsFloat()) ? objJson["cov_angular_velocity"].GetFloat() : NOT_PRESENT_FLOAT;
                    
                    {
                        std::lock_guard<std::mutex> lock(objMtx_);
                        auto it = all_objects_.find(obj.objectID);
                        if (it == all_objects_.end()) {
                            ObjectEntity entity;
                            entity.current = obj;
                            entity.last_sent = obj;
                            entity.to_send = true;
                            entity.priority = 100.0;
                            all_objects_[obj.objectID] = entity;
                        } else {
                            it->second.current = obj;
                            it->second.priority = getPriority(it->second.last_sent, it->second.current);
                            if (!it->second.to_send) {
                                if (isFresh(obj, it->second.last_sent)) {
                                    it->second.to_send = true;
                                }
                            }
                        }
                    }
                }
                auto t2 = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                if (performanceLogs_)
                    aggregator_file_logger_->info("Aggregator,on_message_dds,{},{},{}", getCurrentTimestampString(), objectsArray.Size(), duration);
            }
        } else if (topic == "cps/sensors") {
            SensorInfo sensor;
            sensor.sensorID = (doc.HasMember("sensorID") && doc["sensorID"].IsInt()) ? doc["sensorID"].GetInt() : NOT_PRESENT_INT;
            if ((sensor.sensorID == NOT_PRESENT_INT || sensor.sensorID < 0) && (spdlog::error("[Aggregator]: Mandatory (Sensor ID) not present in message: {}", message), true)) return;
            sensor.sensorType = (doc.HasMember("sensorType") && doc["sensorType"].IsInt()) ? doc["sensorType"].GetInt() : NOT_PRESENT_INT;
            sensor.shadowingApplies = (doc.HasMember("shadowingApplies") && doc["shadowingApplies"].IsBool()) ? doc["shadowingApplies"].GetBool() : false;
            // sensor.semiMajorRangeLength = (doc.HasMember("semiMajorRangeLength") && doc["semiMajorRangeLength"].IsInt()) ? doc["semiMajorRangeLength"].GetInt() : NOT_PRESENT_INT;
            // sensor.semiMinorRangeLength = (doc.HasMember("semiMinorRangeLength") && doc["semiMinorRangeLength"].IsInt()) ? doc["semiMinorRangeLength"].GetInt() : NOT_PRESENT_INT;
            // sensor.semiMajorRangeOrientation = (doc.HasMember("semiMajorRangeOrientation") && doc["semiMajorRangeOrientation"].IsInt()) ? doc["semiMajorRangeOrientation"].GetInt() : NOT_PRESENT_INT;
            // sensor.range = (doc.HasMember("range") && doc["range"].IsInt()) ? doc["range"].GetInt() : NOT_PRESENT_INT;
            // sensor.stationaryHorizontalOpeningAngleStart = (doc.HasMember("stationaryHorizontalOpeningAngleStart") && doc["stationaryHorizontalOpeningAngleStart"].IsInt()) ? doc["stationaryHorizontalOpeningAngleStart"].GetInt() : NOT_PRESENT_INT;
            // sensor.stationaryHorizontalOpeningAngleEnd = (doc.HasMember("stationaryHorizontalOpeningAngleEnd") && doc["stationaryHorizontalOpeningAngleEnd"].IsInt()) ? doc["stationaryHorizontalOpeningAngleEnd"].GetInt() : NOT_PRESENT_INT;
            
            {
                std::lock_guard<std::mutex> lock(sensorMtx_);
                sensor_info_[sensor.sensorID] = sensor;
                spdlog::debug("[Aggregator] Sensor info updated for sensorID: {}", sensor.sensorID);
            }
        } else {
            spdlog::warn("[Aggregator] Received message on unknown topic: {}", topic);
        }
    } catch (const rj::ParseResult& e) {
        spdlog::error("[Aggregator] RapidJSON parse error: {} in message: {}", e.Code(), message);
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

float Aggregator::getPriority(Object last_sent, Object current) {
    if (priorityType_ == "predictor") {
        return priorityMovementPredictor(last_sent, current);
    } else {
        return priorityETSI(last_sent, current);
    } 
}

float Aggregator::priorityMovementPredictor(Object last_sent, Object current) {
    // Calculate prediction based on last data
    double heading_rad = deg2rad(last_sent.heading);
    double dt = current.timestamp - last_sent.timestamp;

    double v_new = last_sent.speed + last_sent.acceleration * dt; // Apply acceleration
    double delta_lat = (v_new * std::cos(heading_rad) * dt) / R_E;
    double delta_lon = (v_new * std::sin(heading_rad) * dt) / (R_E * std::cos(deg2rad(last_sent.latitude)));

    double lat_new = last_sent.latitude + rad2deg(delta_lat);
    double lon_new = last_sent.longitude + rad2deg(delta_lon);

    double prediction_error = calculateHaversineDistance(lat_new, lon_new, current.latitude, current.longitude);

    return prediction_error;

}

float Aggregator::priorityETSI(Object last_sent, Object current) {
    // Position change
    double deltaPos = calculateHaversineDistance(last_sent.latitude, last_sent.longitude, current.latitude, current.longitude);
    double ppos;
    if (deltaPos < P_MIN) {
        ppos = 0;
    } else if (deltaPos < P_MAX) {
        ppos = (deltaPos - P_MIN) / (P_MAX - P_MIN);
    } else{
        ppos = 1;
    }

    // Speed change
    double deltaSpeed = abs(current.speed - last_sent.speed);
    double pspeed;
    if (deltaSpeed < S_MIN) {
        pspeed = 0;
    } else if (deltaSpeed < S_MAX) {
        pspeed = (deltaSpeed - S_MIN) / (S_MAX - S_MIN);
    } else{
        pspeed = 1;
    }

    // Orientation change
    double deltaOrient = abs(current.heading - last_sent.heading);
    double porient;
    if (deltaOrient < S_MIN) {
        porient = 0;
    } else if (deltaOrient < S_MAX) {
        porient = (deltaOrient - S_MIN) / (S_MAX - S_MIN);
    } else{
        porient = 1;
    }

    // Last inclusion time
    double deltaTime = (current.timestamp - last_sent.timestamp)*1000;
    double ptime;
    if (deltaTime < T_MIN) {
        ptime = 0;
    } else if (deltaTime < T_MAX) {
        ptime = (deltaTime - T_MIN) / (T_MAX - T_MIN);
    } else{
        ptime = 1;
    }

    return ppos + pspeed + porient + ptime;

}