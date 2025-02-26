#include "builder.h"
#include <spdlog/spdlog.h>

json Builder::generateCPM(const std::vector<Object>& freshObjects,
                             const std::unordered_map<int, SensorInfo>& sensorInfo,
                             bool addSensor,
                             double stationLatitude,
                             double stationLongitude,
                             int stationType)
{
    json cpm;
    // Add station location and type
    cpm["stationLatitude"] = stationLatitude;
    cpm["stationLongitude"] = stationLongitude;
    cpm["stationType"] = stationType;

    // Add sensor information if required
    if (addSensor) {
        cpm["sensors"] = json::array();
        for (const auto& [sensorID, sensor] : sensorInfo) {
            json sensorJson;
            sensorJson["sensorID"] = sensor.sensorID;
            sensorJson["sensorType"] = sensor.sensorType;
            sensorJson["shadowingApplies"] = sensor.shadowingApplies;
            sensorJson["semiMajorRangeLength"] = sensor.semiMajorRangeLength;
            sensorJson["semiMinorRangeLength"] = sensor.semiMinorRangeLength;
            sensorJson["semiMajorRangeOrientation"] = sensor.semiMajorRangeOrientation;
            sensorJson["range"] = sensor.range;
            sensorJson["stationaryHorizontalOpeningAngleStart"] = sensor.stationaryHorizontalOpeningAngleStart;
            sensorJson["stationaryHorizontalOpeningAngleEnd"] = sensor.stationaryHorizontalOpeningAngleEnd;
            cpm["sensors"].push_back(sensorJson);
        }
    }

    // Add fresh objects
    cpm["objects"] = json::array();
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
        objJson["altitude"] = obj.altitude;
        objJson["size_x"] = obj.size_x;
        objJson["size_y"] = obj.size_y;
        objJson["size_z"] = obj.size_z;
        // Add other attributes as needed...
        cpm["objects"].push_back(objJson);
    }
    return cpm;
}