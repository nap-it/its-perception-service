// sensor_info.hpp
#ifndef SENSOR_INFO_HPP
#define SENSOR_INFO_HPP

#include <vector>
#include <rapidjson/document.h>

struct PerceptionRegionShape {
    int semiMajorRangeLength = 0;
    int semiMinorRangeLength = 0;
    int semiMajorRangeOrientation = 0;
    int range = 0;
    int stationaryHorizontalOpeningAngleStart = 0;
    int stationaryHorizontalOpeningAngleEnd = 0;
};

struct Sensor {
    int sensorID;
    int type;
    PerceptionRegionShape perceptionRegionShape;
    bool shadowingApplies;
};

struct SensorInformationContainer {
    int containerId;
    std::vector<Sensor> containerData;
};

// Declaration of the function
rapidjson::Document getSensorInformationContainer();

#endif // SENSOR_INFO_HPP
