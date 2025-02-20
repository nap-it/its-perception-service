#include "sensor_info.hpp"
#include <iostream>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

using namespace std;
using namespace rapidjson;

std::vector<rapidjson::Document> sensorArray;

void printJsonVector2(const vector<Document>& objects){
    // Print vector of objects
    cout << "-------------- Printing Vector<Document> --------------" << endl;
    for (const auto& obj : objects) {
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        obj.Accept(writer);
        cout << buffer.GetString() << endl;   
    }
}

std::vector<rapidjson::Document> initSensorInformation() {
    // List of sensors to be added to the sensorArray
    std::vector<Sensor> sensors = {
        {1, 1, {75, 20, 3601, 0, 0, 0}, false},
        {2, 12, {0, 0, 0, 10, 3601, 3601}, false},
        {3, 10, {0, 0, 0, 30, 3601, 3601}, false},
        {4, 11, {0, 0, 0, 100, 3601, 3601}, false},
    };
 
    for (const auto& sensor : sensors) {
        // Create a sensor object
        rapidjson::Document sensorObj;
        sensorObj.SetObject();

        // Create a rapidjson allocator
        rapidjson::Document::AllocatorType& allocator = sensorObj.GetAllocator();

        // Add sensor data to the sensor object
        sensorObj.AddMember("sensorID", sensor.sensorID, allocator);
        sensorObj.AddMember("type", sensor.type, allocator);
        sensorObj.AddMember("shadowingApplies", sensor.shadowingApplies, allocator);

        // Create a perception region shape object
        rapidjson::Value perceptionRegionShapeObj(rapidjson::kObjectType);  
        perceptionRegionShapeObj.AddMember("semiMajorRangeLength", sensor.perceptionRegionShape.semiMajorRangeLength, allocator);
        perceptionRegionShapeObj.AddMember("semiMinorRangeLength", sensor.perceptionRegionShape.semiMinorRangeLength, allocator);
        perceptionRegionShapeObj.AddMember("semiMajorRangeOrientation", sensor.perceptionRegionShape.semiMajorRangeOrientation, allocator);
        perceptionRegionShapeObj.AddMember("range", sensor.perceptionRegionShape.range, allocator);
        perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleStart", sensor.perceptionRegionShape.stationaryHorizontalOpeningAngleStart, allocator);
        perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleEnd", sensor.perceptionRegionShape.stationaryHorizontalOpeningAngleEnd, allocator);

        // Add perception region shape object to the sensor object
        sensorObj.AddMember("perceptionRegionShape", perceptionRegionShapeObj, allocator);

        // Add sensor object to the

        sensorArray.push_back(std::move(sensorObj));
        
    }
    return std::move(sensorArray);
}

const std::vector<rapidjson::Document>& getSensorInformation() {
    cout << "Getting sensor information" << endl;
    printJsonVector2(sensorArray);
    return std::move(sensorArray);
}
