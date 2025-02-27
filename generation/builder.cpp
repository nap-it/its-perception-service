#include "builder.h"
#include <spdlog/spdlog.h>

long getMeasurementDeltaTime(unsigned long referenceTime, unsigned long obj_timestamp){
    long int deltaTimeMilliSecondSigned = referenceTime - obj_timestamp;
    if ((-2048 < deltaTimeMilliSecondSigned) && (deltaTimeMilliSecondSigned < 2047)){
        return deltaTimeMilliSecondSigned;
    } else {
        return -2048;
    }
}

void Builder::calculateRelativePositions(double stationLatitude, double stationLongitude, double objLatitude, double objLongitude, double C, double& x, double& y) {
    x = ((objLatitude - stationLatitude) * R) * PI_RAD;
    y = ((objLongitude - stationLongitude) * C) * PI_RAD;
}

json Builder::generateCPM(const std::vector<Object>& freshObjects,
                             const std::unordered_map<int, SensorInfo>& sensorInfo,
                             bool addSensor,
                             double stationLatitude,
                             double stationLongitude,
                             float stationHeading,
                             int stationType)
{
    json cpm;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    unsigned long referenceTime = now - time2004ms;

    // Management container
    cpm["managementContainer"] = {
        {"referenceTime", referenceTime},
        {"referencePosition", {
            {"latitude", stationLatitude},
            {"longitude", stationLongitude},
            {"altitude", {
                {"altitudeValue", 0.0},
                {"altitudeConfidence", 0}
            }},
            {"positionConfidenceEllipse", {
                {"semiMajorConfidence", 4095},
                {"semiMinorConfidence", 4095},
                {"semiMajorOrientation", 0}
            }}
        }}
    };

    json cpmContainers = json::array();

    // Add station container
    if (stationType == 5) { // OBU
        cpmContainers.push_back({
            {"containerId", 1},
            {"containerData", {
                {"orientationAngle", {
                    {"value", stationHeading},
                    {"confidence", 1}
                }}
            }}
        });
    } else if (stationType == 15) { // RSU
        cpmContainers.push_back({
            {"containerId", 2},
            {"containerData", json::object()}
        });
    } else {
        spdlog::error("Unknown station type: {}", stationType);
        return {};
    }

    // Add sensor container
    if (addSensor) {
        json sensorContainerData = json::array();
        for (const auto& [sensorID, sensor] : sensorInfo) {
            sensorContainerData.push_back({
                {"sensorId", sensor.sensorID},
                {"sensorType", sensor.sensorType},
                {"shadowingApplies", sensor.shadowingApplies},
                {"perceptionRegionShape", {
                    {"radial", {
                        {"range", sensor.range},
                        {"horizontalOpeningAngleStart", sensor.stationaryHorizontalOpeningAngleStart},
                        {"horizontalOpeningAngleEnd", sensor.stationaryHorizontalOpeningAngleEnd}
                    }}
                }}
            });
        }
        cpmContainers.push_back({
            {"containerId", 3},
            {"containerData", sensorContainerData}
        });
    }

    double C = R * cos(stationLatitude * PI_RAD);

    // Add objects container
    json perceivedObjects = json::array();
    for (const auto& obj : freshObjects) {
        json objJson;
        objJson["objectId"] = obj.cpmObjectID;
        objJson["sensorIdList"] = {obj.sensorID};
        objJson["measurementDeltaTime"] = getMeasurementDeltaTime(now, obj.timestamp*1000);
        objJson["objectPerceptionQuality"] = 1.0;

        // position
        double x, y;
        calculateRelativePositions(stationLatitude, stationLongitude, obj.latitude, obj.longitude, C, x, y);

        if (x > 1310.72 || x < -1310.72 || y > 1310.72 || y < -1310.72) {
            spdlog::warn("Object out of bounds: x={}, y={}", x, y);
            continue;
        }

        objJson["position"] = {
            {"xCoordinate", {
                {"value", x},
                {"confidence", obj.cov_latitude != NOT_PRESENT_FLOAT ? sqrt(obj.cov_latitude) : 1}
            }},
            {"yCoordinate", {
                {"value", y},
                {"confidence", obj.cov_longitude != NOT_PRESENT_FLOAT ? sqrt(obj.cov_longitude) : 1}
            }}
        };
        if (obj.altitude != NOT_PRESENT_FLOAT) {
            objJson["position"]["zCoordinate"] = {
                {"value", obj.altitude},
                {"confidence", obj.cov_altitude != NOT_PRESENT_FLOAT ? sqrt(obj.cov_altitude) : 1}
            };
        }

        //velocity
        double xVelocity = 0.0;
        double yVelocity = 0.0;
        if (obj.speed != NOT_PRESENT_FLOAT && obj.heading != NOT_PRESENT_FLOAT) {
            xVelocity = obj.speed * cos(obj.heading * PI_RAD);
            yVelocity = obj.speed * sin(obj.heading * PI_RAD);
        }

        objJson["velocity"]["cartesianVelocity"] = {
            {"xVelocity", {
                {"value", xVelocity},
                {"confidence", obj.cov_speed != NOT_PRESENT_FLOAT ? sqrt(obj.cov_speed) : 1}
            }},
            {"yVelocity", {
                {"value", yVelocity},
                {"confidence", obj.cov_speed != NOT_PRESENT_FLOAT ? sqrt(obj.cov_speed) : 1}
            }}
        };

        if (obj.angular_velocity != NOT_PRESENT_FLOAT) {
            objJson["zAngularVelocity"] = {
                {"value", static_cast<int>(obj.angular_velocity * inv_PI_RAD)},
                {"confidence", obj.cov_angular_velocity != NOT_PRESENT_FLOAT ? sqrt(obj.cov_angular_velocity) : 1}
            };
        }

        //acceleration
        double xAcc = 161.0; // unavailable
        double yAcc = 161.0;
        if (obj.acceleration != NOT_PRESENT_FLOAT && obj.heading != NOT_PRESENT_FLOAT) {
            xAcc = obj.acceleration * cos(obj.heading * PI_RAD);
            yAcc = obj.acceleration * sin(obj.heading * PI_RAD);
        }

        objJson["acceleration"]["cartesianAcceleration"] = {
            {"xAcceleration", {
                {"value", xAcc},
                {"confidence", 1}
            }},
            {"yAcceleration", {
                {"value", yAcc},
                {"confidence", 1}
            }}
        };

        //heading
        objJson["angles"] = {
            {"zAngle", {
                {"value", obj.heading},
                {"confidence", obj.cov_heading != NOT_PRESENT_FLOAT ? sqrt(obj.cov_heading) : 1}
            }}
        };

        //classification
        objJson["classification"] = json::array();
        json classification = {
            {"objectClass", { {"vehicleSubClass", obj.classification} }},
            {"confidence", obj.confidence}
        };
        objJson["classification"].push_back(classification);

        //Object dimensions
        if (obj.size_x != NOT_PRESENT_FLOAT) {
            objJson["objectDimensionX"] = {
                {"value", obj.size_x},
                {"confidence", 1}
            };
        }
        if (obj.size_y != NOT_PRESENT_FLOAT) {
            objJson["objectDimensionY"] = {
                {"value", obj.size_y},
                {"confidence", 1}
            };
        }
        if (obj.size_z != NOT_PRESENT_FLOAT) {
            objJson["objectDimensionZ"] = {
                {"value", obj.size_z},
                {"confidence", 1}
            };
        }
        perceivedObjects.push_back(objJson);
    }
    
    cpmContainers.push_back({
        {"containerId", 5},
        {"containerData", {
            {"numberOfPerceivedObjects", perceivedObjects.size()},
            {"perceivedObjects", perceivedObjects}
        }}
    });

    cpm["cpmContainers"] = cpmContainers;

    return cpm;
}