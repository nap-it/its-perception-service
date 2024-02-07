#include "sensor_info.hpp"

rapidjson::Document getSensorInformationContainer() {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    // Add container ID
    doc.AddMember("containerId", 3, allocator);

    // Create an array to hold sensor data
    rapidjson::Value sensorArray(rapidjson::kArrayType);

    // List of sensors to be added to the sensorArray
    std::vector<Sensor> sensors = {
        {1, 1, {75, 20, 3601, 0, 0, 0}, false},
        {2, 12, {0, 0, 0, 10, 3601, 3601}, false},
        {3, 10, {0, 0, 0, 30, 3601, 3601}, false},
        {4, 11, {0, 0, 0, 100, 3601, 3601}, false},
    };

    for (const auto& sensor : sensors) {
        rapidjson::Value sensorObj(rapidjson::kObjectType);
        sensorObj.AddMember("sensorID", sensor.sensorID, allocator);
        sensorObj.AddMember("type", sensor.type, allocator);
        // Assuming you want to add perceptionRegionShape data similarly
        rapidjson::Value shapeObj(rapidjson::kObjectType);
        // Populate shapeObj as needed...
        // Example for a radial shape:
        shapeObj.AddMember("range", sensor.perceptionRegionShape.range, allocator);
        sensorObj.AddMember("perceptionRegionShape", shapeObj, allocator);
        sensorObj.AddMember("shadowingApplies", sensor.shadowingApplies, allocator);

        sensorArray.PushBack(sensorObj, allocator);
    }

    doc.AddMember("containerData", sensorArray, allocator);

    return doc;
}
