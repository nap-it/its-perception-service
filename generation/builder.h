#ifndef BUILDER_H
#define BUILDER_H

#include <vector>
#include <cmath>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "aggregator.h"

using json = nlohmann::json;

constexpr double R = 6371000;
constexpr double PI_RAD = M_PI / 180.0;
constexpr double inv_PI_RAD = 180.0 / M_PI;
constexpr long int time2004ms = 1072915200000;

class Builder {
public:
    /**
     * @brief Generates a CPM JSON object.
     * @param freshObjects A vector of fresh objects to include.
     * @param sensorInfo A map of sensorID to SensorInfo.
     * @param addSensor If true, include sensor information.
     * @param stationLatitude The station latitude.
     * @param stationLongitude The station longitude.
     * @param stationHeading The station heading.
     * @param stationType The station type.
     * @return json The constructed CPM.
     */
    json generateCPM(const std::vector<Object>& freshObjects,
                     const std::unordered_map<int, SensorInfo>& sensorInfo,
                     bool addSensor,
                     double stationLatitude,
                     double stationLongitude,
                     float stationHeading,
                     int stationType);

private:

    /**
     * @brief Calculate the relative positions of two points.
     * @param stationLatitude The latitude of the station.
     * @param stationLongitude The longitude of the station.
     * @param objLatitude The latitude of the object.
     * @param objLongitude The longitude of the object.
     * @param C The cosine of the latitude.
     * @param x The calculated x position.
     * @param y The calculated y position.
     */
    void calculateRelativePositions(double stationLatitude, double stationLongitude, double objLatitude, double objLongitude, double C, double& x, double& y);
};

#endif // BUILDER_H