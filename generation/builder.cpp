#include "builder.h"
#include <chrono>
#include <sstream>
#include <spdlog/spdlog.h>
#include "rapidjson/prettywriter.h"

namespace rj = rapidjson;

int getMeasurementDeltaTime(unsigned long referenceTime, unsigned long obj_timestamp) {
    long deltaTimeMilliSecondSigned = referenceTime - obj_timestamp;
    spdlog::debug("[Builder] Delta time: {} = {} - {}", deltaTimeMilliSecondSigned, referenceTime, obj_timestamp);
    if ((-2048 < deltaTimeMilliSecondSigned) && (deltaTimeMilliSecondSigned < 2047)) {
        return deltaTimeMilliSecondSigned;
    } else {
        spdlog::warn("[Builder] Delta time out of range: {} ms, setting to -2048", deltaTimeMilliSecondSigned);
        return -2048;
    }
}

void Builder::calculateRelativePositions(double stationLatitude, double stationLongitude,
                                           double objLatitude, double objLongitude,
                                           double C, double& x, double& y) {
    y = ((objLatitude - stationLatitude) * R) * PI_RAD;
    x = ((objLongitude - stationLongitude) * C) * PI_RAD;
}

std::string Builder::generateCPM(const std::vector<Object>& freshObjects,
                                 const std::unordered_map<int, SensorInfo>& sensorInfo,
                                 bool addSensor,
                                 double stationLatitude,
                                 double stationLongitude,
                                 float stationHeading,
                                 int stationType)
{

    auto t1 = std::chrono::high_resolution_clock::now();

    // Create a RapidJSON document.
    rj::Document cpm;
    cpm.SetObject();
    rj::Document::AllocatorType& alloc = cpm.GetAllocator();

    // Get current time (in milliseconds) and calculate the reference time.
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    unsigned long referenceTime = now - time2004ms;
    double referenceTimeSec = static_cast<double>(referenceTime) / 1000.0;

    // Build the management container.
    rj::Value managementContainer(rj::kObjectType);
    managementContainer.AddMember("referenceTime", referenceTimeSec, alloc);

    rj::Value referencePosition(rj::kObjectType);
    referencePosition.AddMember("latitude", stationLatitude, alloc);
    referencePosition.AddMember("longitude", stationLongitude, alloc);

    rj::Value altitude(rj::kObjectType);
    altitude.AddMember("altitudeValue", 0.0, alloc);
    altitude.AddMember("altitudeConfidence", 0, alloc);
    referencePosition.AddMember("altitude", altitude, alloc);

    rj::Value positionConfidenceEllipse(rj::kObjectType);
    positionConfidenceEllipse.AddMember("semiMajorConfidence", 4095, alloc);
    positionConfidenceEllipse.AddMember("semiMinorConfidence", 4095, alloc);
    positionConfidenceEllipse.AddMember("semiMajorOrientation", 0, alloc);
    referencePosition.AddMember("positionConfidenceEllipse", positionConfidenceEllipse, alloc);

    managementContainer.AddMember("referencePosition", referencePosition, alloc);
    cpm.AddMember("managementContainer", managementContainer, alloc);

    // Build the CPM containers array.
    rj::Value cpmContainers(rj::kArrayType);

    // Add station container based on station type.
    if (stationType == 5) { // OBU
        rj::Value container1(rj::kObjectType);
        container1.AddMember("containerId", 1, alloc);
        rj::Value containerData(rj::kObjectType);
        rj::Value orientationAngle(rj::kObjectType);
        orientationAngle.AddMember("value", stationHeading, alloc);
        orientationAngle.AddMember("confidence", 1, alloc);
        containerData.AddMember("orientationAngle", orientationAngle, alloc);
        container1.AddMember("containerData", containerData, alloc);
        cpmContainers.PushBack(container1, alloc);
    } else if (stationType == 15) { // RSU
        rj::Value container2(rj::kObjectType);
        container2.AddMember("containerId", 2, alloc);
        rj::Value containerData(rj::kObjectType);
        container2.AddMember("containerData", containerData, alloc);
        cpmContainers.PushBack(container2, alloc);
    } else {
        spdlog::error("[Builder] Unknown station type: {}", stationType);
        return "";
    }

    // Add sensor container if required.
    if (addSensor) {
        rj::Value sensorContainerData(rj::kArrayType);
        for (const auto& pair : sensorInfo) {
            const SensorInfo& sensor = pair.second;
            rj::Value sensorJson(rj::kObjectType);
            sensorJson.AddMember("sensorId", sensor.sensorID, alloc);
            sensorJson.AddMember("sensorType", sensor.sensorType, alloc);
            sensorJson.AddMember("shadowingApplies", sensor.shadowingApplies, alloc);
            sensorContainerData.PushBack(sensorJson, alloc);
        }
        rj::Value container3(rj::kObjectType);
        container3.AddMember("containerId", 3, alloc);
        container3.AddMember("containerData", sensorContainerData, alloc);
        cpmContainers.PushBack(container3, alloc);
    }

    double C = R * cos(stationLatitude * PI_RAD);

    // Add objects container.
    rj::Value perceivedObjects(rj::kArrayType);
    for (const auto& obj : freshObjects) {
        rj::Value objJson(rj::kObjectType);
        objJson.AddMember("objectId", obj.cpmObjectID, alloc);
        
        // sensorIdList as an array
        rj::Value sensorIdList(rj::kArrayType);
        sensorIdList.PushBack(obj.sensorID, alloc);
        objJson.AddMember("sensorIdList", sensorIdList, alloc);
        
        long deltaTime = getMeasurementDeltaTime(now, static_cast<unsigned long>(obj.timestamp * 1000));
        objJson.AddMember("measurementDeltaTime", deltaTime, alloc);
        
        /**
         * Position
        */

        // Calculate relative positions.
        double x, y;
        calculateRelativePositions(stationLatitude, stationLongitude, obj.latitude, obj.longitude, C, x, y);
        if (x > 1310.72 || x < -1310.72 || y > 1310.72 || y < -1310.72) {
            spdlog::warn("[Builder] Object [{}] out of bounds: x={}, y={}",obj.cpmObjectID, x, y);
            continue;
        }

        rj::Value position(rj::kObjectType);
        rj::Value xCoordinate(rj::kObjectType);
        xCoordinate.AddMember("value", x, alloc);
        double xConf = (obj.cov_latitude != NOT_PRESENT_FLOAT) ? sqrt(obj.cov_latitude) : 1;
        if (xConf == 0) xConf = 40.96;
        if(xConf > 40.94) xConf = 40.95;
        xCoordinate.AddMember("confidence", xConf, alloc);
        position.AddMember("xCoordinate", xCoordinate, alloc);
        
        rj::Value yCoordinate(rj::kObjectType);
        yCoordinate.AddMember("value", y, alloc);
        double yConf = (obj.cov_longitude != NOT_PRESENT_FLOAT) ? sqrt(obj.cov_longitude) : 1;
        if (yConf == 0) yConf = 40.96;
        if(yConf > 40.94) yConf = 40.95;
        yCoordinate.AddMember("confidence", yConf, alloc);
        position.AddMember("yCoordinate", yCoordinate, alloc);
        
        if (obj.altitude != NOT_PRESENT_FLOAT) {
            rj::Value zCoordinate(rj::kObjectType);
            zCoordinate.AddMember("value", obj.altitude, alloc);
            double zConf = (obj.cov_altitude != NOT_PRESENT_FLOAT) ? sqrt(obj.cov_altitude) : 1;
            if (zConf == 0) zConf = 40.96;
            if(zConf > 40.94) zConf = 40.95;
            zCoordinate.AddMember("confidence", zConf, alloc);
            position.AddMember("zCoordinate", zCoordinate, alloc);
        }
        objJson.AddMember("position", position, alloc);
        
        /** 
         * Velocity
        */
        double xVelocity = 0.0, yVelocity = 0.0;
        if (obj.speed != NOT_PRESENT_FLOAT && obj.heading != NOT_PRESENT_FLOAT) {
            xVelocity = obj.speed * cos(obj.heading * PI_RAD);
            yVelocity = obj.speed * sin(obj.heading * PI_RAD);
            
            if (xVelocity == -0.0) xVelocity = 0.0;
            if (yVelocity == -0.0) yVelocity = 0.0;
            if (xVelocity < -163) xVelocity = -163.0;
            if (xVelocity > 163) xVelocity = 163.0;
            if (yVelocity < -163) yVelocity = -163.0;
            if (yVelocity > 163) yVelocity = 163.0;
            
            rj::Value velocity(rj::kObjectType);
            rj::Value cartesianVelocity(rj::kObjectType);

            rj::Value xVelocityVal(rj::kObjectType);
            xVelocityVal.AddMember("value", xVelocity, alloc);
            double speedConf = (obj.cov_speed != NOT_PRESENT_FLOAT) ? sqrt(obj.cov_speed) : 1;
            if (speedConf <= 0) speedConf = 1.27;
            if(speedConf > 1.25) speedConf = 1.26;
            xVelocityVal.AddMember("confidence", speedConf, alloc);
            cartesianVelocity.AddMember("xVelocity", xVelocityVal, alloc);

            rj::Value yVelocityVal(rj::kObjectType);
            yVelocityVal.AddMember("value", yVelocity, alloc);
            yVelocityVal.AddMember("confidence", speedConf, alloc);
            cartesianVelocity.AddMember("yVelocity", yVelocityVal, alloc);

            velocity.AddMember("cartesianVelocity", cartesianVelocity, alloc);
            
            objJson.AddMember("velocity", velocity, alloc);
        }
        
        
        /**
         * Angular velocity
         */
        if (obj.angular_velocity != NOT_PRESENT_FLOAT) {
            rj::Value zAngularVelocity(rj::kObjectType);
            int ang = static_cast<int>(obj.angular_velocity * inv_PI_RAD);
            zAngularVelocity.AddMember("value", ang, alloc);
            //double angConf = (obj.cov_angular_velocity != NOT_PRESENT_FLOAT) ? sqrt(obj.cov_angular_velocity) : 1;
            zAngularVelocity.AddMember("confidence", 1, alloc);
            objJson.AddMember("zAngularVelocity", zAngularVelocity, alloc);
        }
        
        /**
         * Acceleration
         */
        double xAcc = 16.1, yAcc = 16.1;
        if (obj.acceleration != NOT_PRESENT_FLOAT && obj.heading != NOT_PRESENT_FLOAT) {
            xAcc = obj.acceleration * cos(obj.heading * PI_RAD);
            yAcc = obj.acceleration * sin(obj.heading * PI_RAD);

            if (xAcc == -0.0) xAcc = 0.0;
            if (yAcc == -0.0) yAcc = 0.0;
            if (xAcc < -16) xAcc = -16.0;
            if (xAcc > 16) xAcc = 16.0;
            if (yAcc < -16) yAcc = -16.0;
            if (yAcc > 16) yAcc = 16.0;

            rj::Value acceleration(rj::kObjectType);
            rj::Value cartesianAcceleration(rj::kObjectType);
            rj::Value xAccVal(rj::kObjectType);
            xAccVal.AddMember("value", xAcc, alloc);
            xAccVal.AddMember("confidence", 1, alloc);
            cartesianAcceleration.AddMember("xAcceleration", xAccVal, alloc);
            rj::Value yAccVal(rj::kObjectType);
            yAccVal.AddMember("value", yAcc, alloc);
            yAccVal.AddMember("confidence", 1, alloc);
            cartesianAcceleration.AddMember("yAcceleration", yAccVal, alloc);
            acceleration.AddMember("cartesianAcceleration", cartesianAcceleration, alloc);
            objJson.AddMember("acceleration", acceleration, alloc);
        }
        
        
        /**
         * Heading
         */
        if (obj.heading != NOT_PRESENT_FLOAT) {
            rj::Value angles(rj::kObjectType);
            rj::Value zAngle(rj::kObjectType);
            zAngle.AddMember("value", obj.heading, alloc);
            double headingConf = (obj.cov_heading != NOT_PRESENT_FLOAT) ? sqrt(obj.cov_heading) : 1;
            if (headingConf == 0) headingConf = 12.7;
            if(headingConf > 12.5) headingConf = 12.6;
            zAngle.AddMember("confidence", headingConf, alloc);
            angles.AddMember("zAngle", zAngle, alloc);
            objJson.AddMember("angles", angles, alloc);
        }
        
        /**
         * Classification
         */
        rj::Value classificationArr(rj::kArrayType);
        rj::Value classification(rj::kObjectType);
        rj::Value objectClass(rj::kObjectType);
        objectClass.AddMember("vehicleSubClass", obj.classification, alloc);
        classification.AddMember("objectClass", objectClass, alloc);
        classification.AddMember("confidence", obj.confidence, alloc);
        classificationArr.PushBack(classification, alloc);
        objJson.AddMember("classification", classificationArr, alloc);
        
        /**
         * Object dimensions
         */
        if (obj.size_x != NOT_PRESENT_FLOAT) {
            rj::Value objectDimensionX(rj::kObjectType);
            float size_x = obj.size_x;
            if (size_x < 0.1) size_x = 0.5;
            if (size_x > 25.0) size_x = 25.0; // Limit size to a maximum of 10m
            objectDimensionX.AddMember("value", size_x, alloc);
            objectDimensionX.AddMember("confidence", 1, alloc);
            objJson.AddMember("objectDimensionX", objectDimensionX, alloc);
        }
        if (obj.size_y != NOT_PRESENT_FLOAT) {
            rj::Value objectDimensionY(rj::kObjectType);
            float size_y = obj.size_y;
            if (size_y < 0.1) size_y = 0.5;
            if (size_y > 25.0) size_y = 25.0; // Limit size to a maximum of 10m
            objectDimensionY.AddMember("value", size_y, alloc);
            objectDimensionY.AddMember("confidence", 1, alloc);
            objJson.AddMember("objectDimensionY", objectDimensionY, alloc);
        }
        if (obj.size_z != NOT_PRESENT_FLOAT) {
            rj::Value objectDimensionZ(rj::kObjectType);
            float size_z = obj.size_z;
            if (size_z < 0.1) size_z = 0.5;
            if (size_z > 25.0) size_z = 25.0; // Limit size to a maximum of 10m
            objectDimensionZ.AddMember("value", size_z, alloc);
            objectDimensionZ.AddMember("confidence", 1, alloc);
            objJson.AddMember("objectDimensionZ", objectDimensionZ, alloc);
        }
        
        perceivedObjects.PushBack(objJson, alloc);
    }
    
    rj::Value container5(rj::kObjectType);
    container5.AddMember("containerId", 5, alloc);
    rj::Value containerData(rj::kObjectType);
    containerData.AddMember("numberOfPerceivedObjects", static_cast<unsigned int>(perceivedObjects.Size()), alloc);
    containerData.AddMember("perceivedObjects", perceivedObjects, alloc);
    container5.AddMember("containerData", containerData, alloc);
    cpmContainers.PushBack(container5, alloc);
    
    cpm.AddMember("cpmContainers", cpmContainers, alloc);

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    spdlog::info("[Builder] CPM generation took {} us", duration);

    // Convert document to string.
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    cpm.Accept(writer);
    std::string cpm_str = buffer.GetString();
    
    auto t3 = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
    spdlog::info("[Builder] CPM serialization took {} us", duration);

    return cpm_str;
}