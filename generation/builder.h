/**
 * @file builder.h
 * @brief Stateless CPM JSON constructor for the CPS Generation service
 * @date 2026
 *
 * This file defines the Builder class, which translates a list of fresh Object
 * structs and SensorInfo metadata into a JSON-encoded Collective Perception
 * Message (CPM) that conforms to ETSI TS 103 324.
 *
 * Key Responsibilities:
 * - Constructs the CPM Management Container (station ID, timestamp, position)
 * - Constructs per-object Perceived Object Containers (position, velocity,
 *   acceleration, heading, size, classification, confidence)
 * - Converts absolute lat/lon coordinates to relative x/y distances from the
 *   originating station using a flat-Earth approximation
 * - Converts UNIX timestamps to the ETSI ITS epoch (2004-01-01T00:00:00Z)
 * - Includes a Sensor Information Container when sensor metadata is available
 *
 * The Builder is stateless — it can be called repeatedly without side effects.
 * All coordinate math uses double precision; ETSI unit scaling (0.01 m, 0.01°)
 * is applied internally and the output JSON carries SI-scaled integer values
 * as expected by Vanetza-NAP.
 */

#ifndef BUILDER_H
#define BUILDER_H

#include <vector>
#include <cmath>
#include <unordered_map>
#include "aggregator.h"

// RapidJSON headers for fast JSON generation.
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace rj = rapidjson;

constexpr double R = 6371000;
constexpr double PI_RAD = M_PI / 180.0;
constexpr double inv_PI_RAD = 180.0 / M_PI;
constexpr long int time2004ms = 1072915200000;

class Builder {
public:
    /**
     * @brief Generates a CPM JSON string.
     * @param freshObjects A vector of fresh objects to include.
     * @param sensorInfo A map of sensorID to SensorInfo.
     * @param addSensor If true, include sensor information.
     * @param stationLatitude The station latitude.
     * @param stationLongitude The station longitude.
     * @param stationHeading The station heading.
     * @param stationType The station type.
     * @return std::string The constructed CPM as a JSON string.
     */
    std::string generateCPM(const std::vector<Object>& freshObjects,
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
     * @param C The cosine factor (R * cos(stationLatitude in radians)).
     * @param x The calculated x position (output).
     * @param y The calculated y position (output).
     */
    void calculateRelativePositions(double stationLatitude, double stationLongitude,
                                    double objLatitude, double objLongitude,
                                    double C, double& x, double& y);
};

#endif // BUILDER_H