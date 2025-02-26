#ifndef BUILDER_H
#define BUILDER_H

#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "aggregator.h"    // For Object and SensorInfo

using json = nlohmann::json;

class Builder {
public:
    /**
     * @brief Generates a CPM JSON object.
     * @param freshObjects A vector of fresh objects to include.
     * @param sensorInfo A map of sensorID to SensorInfo.
     * @param addSensor If true, include sensor information.
     * @param stationLatitude The station latitude.
     * @param stationLongitude The station longitude.
     * @param stationType The station type.
     * @return json The constructed CPM.
     */
    json generateCPM(const std::vector<Object>& freshObjects,
                     const std::unordered_map<int, SensorInfo>& sensorInfo,
                     bool addSensor,
                     double stationLatitude,
                     double stationLongitude,
                     int stationType);
};

#endif // BUILDER_H