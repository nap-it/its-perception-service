#include "cpm-processing.h"
#include <map>
#include <vector>
#include <fstream>

const int   R = 6371000; // earth radius
const double PI = 3.141592653589793238463;
const long int time2004ms = 1072915200000;

map<int, long> cpm_map;
int currentIndex = 0;
// bool write_logs = false;

map<int,string> napClassType = {
        {0, "unknown"},
        {1, "pedestrian"},
        {2, "cyclist"},
        {3, "moped"},
        {4, "motorcycle"},
        {5, "passengerCar"},
        {6, "bus"},
        {7, "lightTruck"},
        {8, "heavyTruck"},
        {9, "trailer"},
        {10, "Ambulance"},
        {11, "tram"},
        {12, "VRU"},
        {13, "animal"},
        {14, "agricultural vehicles"},
        {15, "roadSideUnit"},
        {16, "SafetyApp"},
        {17, "Moliceiro"},
        {18, "Test Device"},
        {19, "others"}
};

map<int, string> personSubclassType = {
        {0, "unknown"},
        {1, "pedestrian"},
        {2, "personInWheelchair"},
        {3, "cyclist"},
        {4, "personWithStroller"},
        {5, "personOnSkates"},
        {6, "personGroup"}
};

map<int, string> otherSubclassType = {
        {0, "unknown"},
        {1, "roadSideUnit"}
};

map<int,string> sensorType = {
        {0, "undefined"},
        {1, "radar"},
        {2, "lidar"},
        {3, "monovideo"},
        {4, "stereovision"},
        {5, "nightvision"},
        {6, "ultrasonic"},
        {7, "pmd"},
        {8, "inductionLoop"},
        {9, "sphericalCamera"},
        {10, "uwb"},
        {11, "acoustic"},
        {12, "localAggregation"},
        {13, "itsAggregation"}
};

std::pair<double, double> rotate_axes(int yaw, double x, double y) {
    if (yaw == 32767 || yaw == 3601) {
        yaw = 0;
    }

    double yaw_radians = M_PI / 180.0 * yaw;
    double xl = x * std::cos(yaw_radians) + y * std::sin(yaw_radians);
    double yl = (-x) * std::sin(yaw_radians) + y * std::cos(yaw_radians);

    return std::make_pair(xl, yl);
}

string docToString(const Document& doc) {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);
    string str = buffer.GetString();

    return str;
}

string valueToString(const Value& value)  {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

string simplify_cpm(int senderID, int receiverID, int receiverType, int repeatIntervalSec, Document& cpm){

    spdlog::debug("Simplifying CPM... {}", docToString(cpm));

    unsigned long int cpmTimestamp = -1;

    if(cpm.HasMember("managementContainer") && cpm["managementContainer"].HasMember("referenceTime")) {
        cpmTimestamp = cpm["managementContainer"]["referenceTime"].GetInt64();
        spdlog::info("CPM referenceTime: {}", cpmTimestamp);

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time2004ms;
        
        auto latency = now - cpmTimestamp;
        spdlog::info("{} -> CPM latency: {} ms",currentIndex, latency);
    }
    else {
        spdlog::error("CPM does not have referenceTime");
    }

    unsigned long int timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    
    unsigned long int localGenDeltaTime = timestamp - time2004ms;

    // calculate cpm age
    int ageCpm = localGenDeltaTime - cpmTimestamp;
    // int ageCpm = cpm["managementContainer"]["referenceTime"].GetInt64(); //TESTING TIME LOGS



    if (ageCpm < 0) {
        ageCpm = (65536 + localGenDeltaTime) - cpmTimestamp;
    }

    // spdlog::debug("localGenDeltaTime {} - referenceTime {} = ageCpm {}", localGenDeltaTime, cpmTimestamp, ageCpm);

    int number_containers = cpm["cpmContainers"].GetArray().Size();

    if (number_containers == 0) {
        spdlog::error("No containers received!");
        return "";
    }

    // look for the perceivedObjectContainer and sensorInformationContainer
    Document perceivedObjectContainer, sensorInformationContainer;
    bool hasPerceivedObjectContainer, hasSensorInformationContainer = false;
    bool isRSU = false;
    auto allocator = perceivedObjectContainer.GetAllocator();
    
    for (int i = 0; i < number_containers; i++) {
        if (cpm["cpmContainers"][i]["containerId"].GetInt() == 5) {
            perceivedObjectContainer.CopyFrom(cpm["cpmContainers"][i], allocator);
            hasPerceivedObjectContainer = true;
            spdlog::debug("PerceivedObjectContainer found!");
        }
        if (cpm["cpmContainers"][i]["containerId"].GetInt() == 2) {
            isRSU = true;

        }
        if (cpm["cpmContainers"][i]["containerId"].GetInt() == 3) {
            sensorInformationContainer.CopyFrom(cpm["cpmContainers"][i], sensorInformationContainer.GetAllocator());
            hasSensorInformationContainer = true;
            spdlog::debug("SensorInformationContainer found!");
        }
    }

    if (!hasPerceivedObjectContainer) {
        spdlog::debug("No perceived object received!");
        return "";
    }

    int sender_stationType;
    if (isRSU) {
        sender_stationType = 15;
    }
    else {
        sender_stationType = 5;
    }

    // double yaw;
    // if (sender_stationType != 15) {
    //     yaw = cpm["stationDataContainer"]["originatingVehicleContainer"]["heading"]["headingValue"].GetDouble();
    // }

    double lat = cpm["managementContainer"]["referencePosition"]["latitude"].GetDouble();
    double lon = cpm["managementContainer"]["referencePosition"]["longitude"].GetDouble();

    // spdlog::info("lat {} lon {}", lat, lon);

    //cout << "lon " << lon << endl;
    int number_objects = perceivedObjectContainer["containerData"]["numberOfPerceivedObjects"].GetInt();
    
    spdlog::debug("Number of objects: {}", number_objects);

    if (number_objects == 0) {
        spdlog::debug("No objects received!");
        return "";
    }

    
    spdlog::debug("Processing perceived objects...");

    string cpm_objects_str = "[";

    for(int i=0; i<number_objects; i++)
    {
        
        Document json_obj(rapidjson::kObjectType);
        auto objAlloc = json_obj.GetAllocator();
        int objectId = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectId"].GetInt();
        json_obj.AddMember("id", objectId, objAlloc);

        unsigned long int objAge = ageCpm + perceivedObjectContainer["containerData"]["perceivedObjects"][i]["measurementDeltaTime"].GetInt64();
        unsigned long int objTimestamp = timestamp - objAge; // timestamp of the object in milliseconds (integer 64)

        double objTimestampSec = 0.0;

        try {
            objTimestampSec = static_cast<double>(objTimestamp) / 1000.0;
        } catch(...) {
            spdlog::error("Error converting timestamp to seconds");
        }
    
        json_obj.AddMember("age", objAge, objAlloc);
        spdlog::debug("Object age: {}", objAge);
        json_obj.AddMember("objectTimestamp", objTimestampSec, objAlloc);
        spdlog::debug("Object timestamp: {}", objTimestampSec);
        json_obj.AddMember("objectPerceptionQuality", perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectPerceptionQuality"].GetInt(), objAlloc);
        spdlog::debug("Object perception quality: {}", perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectPerceptionQuality"].GetInt());
        json_obj.AddMember("detectionStationType", sender_stationType, objAlloc);

        int sensorID = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["sensorIdList"][0].GetInt();
        spdlog::debug("Sensor ID: {}", sensorID);

        string sensor;
        try{
            sensor = sensorType.at(sensorID);
        }
        catch(...){
            sensor = "unknown";
        }

        json_obj.AddMember("sensor", rapidjson::Value(sensor.c_str(), objAlloc).Move(), objAlloc);
        json_obj.AddMember("sensorID", sensorID, objAlloc);


        double xDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["xCoordinate"]["value"].GetDouble();
        spdlog::debug("xDistance: {}", xDistance);
        double yDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["yCoordinate"]["value"].GetDouble();
        spdlog::debug("yDistance: {}", yDistance);

        double latitude;
        double longitude;
        double N = yDistance;
        double E = xDistance;

        latitude = lat + (180/PI) * (N/R);
        longitude = lon + (180/PI) * (E/R) / cos((PI/180) * lat);

        json_obj.AddMember("latitude", latitude, objAlloc);
        json_obj.AddMember("longitude", longitude, objAlloc);

        double xVelocity = 16383;
        double yVelocity = 16383;
        

        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("velocity") && (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["velocity"].HasMember("cartesianVelocity"))){
            xVelocity = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["velocity"]["cartesianVelocity"]["xVelocity"]["value"].GetDouble();
            yVelocity = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["velocity"]["cartesianVelocity"]["yVelocity"]["value"].GetDouble();
        }

        double speed;
        if ((xVelocity != 16383) && (yVelocity != 16383)) {
            speed = std::sqrt(std::pow(xVelocity, 2) + std::pow(yVelocity, 2));
        }
        else {
            speed = 0.0;
            spdlog::error("Speed not found");
        }
        json_obj.AddMember("speed", speed, objAlloc);

        double xAcceleration = 161;
        double yAcceleration = 161;

        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("acceleration") && (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["acceleration"].HasMember("cartesianAcceleration"))){
            xAcceleration = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["acceleration"]["cartesianAcceleration"]["xAcceleration"]["value"].GetDouble();
            yAcceleration = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["acceleration"]["cartesianAcceleration"]["yAcceleration"]["value"].GetDouble();
        }
        
        double acceleration;
        if ((xAcceleration != 161) && (yAcceleration != 161)) {
            acceleration = std::sqrt(std::pow(xAcceleration, 2) + std::pow(yAcceleration, 2));
        }
        else {
            acceleration = 0.0;
            spdlog::error("Acceleration not found");
        }
        json_obj.AddMember("acceleration", acceleration, objAlloc);

        string classification;
        int classificationID = 0;
        string obj_type;
        // cout << "------------------- " << sensor << " -------------------" << endl; 
        if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"].HasMember("vehicleSubClass")) {
            Value &vehicleSubClass = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vehicleSubClass"];
            // cout << "Has member subclass -> " << valueToString(vehicleSubClass) <<  " ;" << endl;
            spdlog::debug("VehicleSubClass found");
            try {
                int napId = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vehicleSubClass"].GetInt();
                spdlog::debug("IncomingID {} = {}", napId, napClassType.at(napId));
                // cout << "IncomingID " << perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vehicleSubClass"].GetInt() << ", napId " << napId << " = " << napClassType.at(napId) << endl;
                classification = napClassType.at(napId);
                classificationID = napId;
                if(napId == 0){
                    spdlog::error("Classficication ID 0");
                }
            }
            catch(...){
                classification = "unclassified";
                spdlog::error("No classification found");
            }
        }
        else if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"].HasMember("vruSubClass")) {
            try {
                classification = personSubclassType.at(perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vruSubClass"].GetInt());
            }
            catch(...){
                classification = "unclassified";
            }
        }
        else if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"].HasMember("otherSubClass")) {
            try {
                classification = otherSubclassType.at(perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["otherSubClass"].GetInt());
            }
            catch(...){
                classification = "unclassified";
            }
        }

        //Classification
        json_obj.AddMember("classification", rapidjson::Value(classification.c_str(), objAlloc).Move(), objAlloc);
        json_obj.AddMember("classificationID", classificationID, objAlloc);

        //Stations informtaion
        json_obj.AddMember("stationSenderID", senderID, objAlloc);
        json_obj.AddMember("stationReceiverID", receiverID, objAlloc);
        json_obj.AddMember("stationReceiverType", receiverType, objAlloc);

        //UniqueID
        long long timestampSecs = static_cast<long long>(timestamp / 1000);
        long long truncatedTimestamp = (timestampSecs / repeatIntervalSec) * repeatIntervalSec;

        long long uniqueID = (truncatedTimestamp << 16) | (objectId & 0xFFFF);

        json_obj.AddMember("uniqueID", static_cast<int64_t>(uniqueID), objAlloc);

        string objStr = docToString(json_obj);

        cpm_objects_str += objStr + ",";
    }

    if(cpm_objects_str.back() == ','){
        cpm_objects_str.pop_back();
    }

    cpm_objects_str += "]";

    return cpm_objects_str;

}

string process_cpm(int senderID, int receiverID, int receiverType, int repeatIntervalSec, Document& cpm){

    spdlog::debug("Processing CPM... {}", docToString(cpm));

    //Timestamps
    unsigned long int cpmTimestamp = -1;
    if(cpm.HasMember("managementContainer") && cpm["managementContainer"].HasMember("referenceTime")) {
        cpmTimestamp = cpm["managementContainer"]["referenceTime"].GetInt64();
        spdlog::info("CPM referenceTime: {}", cpmTimestamp);
    }
    else spdlog::error("CPM does not have referenceTime");
    
    unsigned long int currentTimestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    unsigned long int localGenDeltaTime = currentTimestamp - time2004ms;
    int ageCpm = localGenDeltaTime - cpmTimestamp;

    int number_containers = cpm["cpmContainers"].GetArray().Size();

    if (number_containers == 0) {
        spdlog::error("No containers received!");
        return "";
    }

    // Containers
    Document perceivedObjectContainer, sensorInformationContainer;
    bool hasPerceivedObjectContainer, hasSensorInformationContainer = false;
    bool isRSU = false;
    auto allocator = perceivedObjectContainer.GetAllocator();
    
    for (int i = 0; i < number_containers; i++) {
        if (cpm["cpmContainers"][i]["containerId"].GetInt() == 5) {
            perceivedObjectContainer.CopyFrom(cpm["cpmContainers"][i], allocator);
            hasPerceivedObjectContainer = true;
        }
        if (cpm["cpmContainers"][i]["containerId"].GetInt() == 2) {
            isRSU = true;
        }
        if (cpm["cpmContainers"][i]["containerId"].GetInt() == 3) {
            sensorInformationContainer.CopyFrom(cpm["cpmContainers"][i], sensorInformationContainer.GetAllocator());
            hasSensorInformationContainer = true;
        }
    }

    if (!hasPerceivedObjectContainer) {
        spdlog::debug("No perceived object received!");
        return "";
    }

    // Station types
    int sender_stationType;
    if (isRSU) sender_stationType = 15;
    else sender_stationType = 5;

    //Station position
    double lat = cpm["managementContainer"]["referencePosition"]["latitude"].GetDouble();
    double lon = cpm["managementContainer"]["referencePosition"]["longitude"].GetDouble();

    int number_objects = perceivedObjectContainer["containerData"]["numberOfPerceivedObjects"].GetInt();
    
    spdlog::debug("Number of objects: {}", number_objects);

    if (number_objects == 0) return "";

    string cpm_objects_str = "[";

    for(int i=0; i<number_objects; i++)
    {
        
        Document json_obj(rapidjson::kObjectType);
        auto objAlloc = json_obj.GetAllocator();

        int objectId = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectId"].GetInt();
        unsigned long int objAge = ageCpm + perceivedObjectContainer["containerData"]["perceivedObjects"][i]["measurementDeltaTime"].GetInt64();
        unsigned long int objTimestamp = currentTimestamp - objAge;
        double objTimestampSec = static_cast<double>(objTimestamp) / 1000.0;

         //UniqueID
        long long timestampSecs = static_cast<long long>(currentTimestamp / 1000);
        long long truncatedTimestamp = (timestampSecs / repeatIntervalSec) * repeatIntervalSec;

        long long uniqueID = (truncatedTimestamp << 16) | (objectId & 0xFFFF);

        int objectPerceptionQuality = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectPerceptionQuality"].GetInt();        

        int sensorID = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["sensorIdList"][0].GetInt();
        string sensor;
        try{
            sensor = sensorType.at(sensorID);
        }
        catch(...){
            sensor = "unknown";
        }

        double xDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["xCoordinate"]["value"].GetDouble();
        double yDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["yCoordinate"]["value"].GetDouble();
        double xCov = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["xCoordinate"]["confidence"].GetDouble();
        double yCov = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["yCoordinate"]["confidence"].GetDouble();

        double zDistance = 0.0;
        double zCov = 0.0;
        if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"].HasMember("zCoordinate")) {
            zDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["zCoordinate"]["value"].GetDouble();
            zCov = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["zCoordinate"]["confidence"].GetDouble();
        }

        double latitude;
        double longitude;
        double N = yDistance;
        double E = xDistance;

        latitude = lat + (180/PI) * (N/R);
        longitude = lon + (180/PI) * (E/R) / cos((PI/180) * lat);

        double xVelocity = 16383;
        double yVelocity = 16383;
        double xCovVelocity = 0.0;
        double yCovVelocity = 0.0;

        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("velocity") && (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["velocity"].HasMember("cartesianVelocity"))){
            xVelocity = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["velocity"]["cartesianVelocity"]["xVelocity"]["value"].GetDouble();
            xCovVelocity = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["velocity"]["cartesianVelocity"]["xVelocity"]["confidence"].GetDouble();
            yVelocity = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["velocity"]["cartesianVelocity"]["yVelocity"]["value"].GetDouble();
            yCovVelocity = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["velocity"]["cartesianVelocity"]["yVelocity"]["confidence"].GetDouble();
        }

        double speed = 0.0;
        if ((xVelocity != 16383) && (yVelocity != 16383)) speed = std::sqrt(std::pow(xVelocity, 2) + std::pow(yVelocity, 2));
        else speed = 0.0;

        double xAcceleration = 161;
        double yAcceleration = 161;

        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("acceleration") && (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["acceleration"].HasMember("cartesianAcceleration"))){
            xAcceleration = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["acceleration"]["cartesianAcceleration"]["xAcceleration"]["value"].GetDouble();
            yAcceleration = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["acceleration"]["cartesianAcceleration"]["yAcceleration"]["value"].GetDouble();
        }
        
        double acceleration;
        if ((xAcceleration != 161) && (yAcceleration != 161)) acceleration = std::sqrt(std::pow(xAcceleration, 2) + std::pow(yAcceleration, 2));
        else acceleration = 0.0;

        float heading;
        float heading_cov = 0.0;
        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("angles")) heading = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["angles"]["zAngle"]["value"].GetFloat();
        else heading = 0.0;

        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("angles")) heading_cov = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["angles"]["zAngle"]["confidence"].GetFloat();
        else heading_cov = 0.0;

        string classification;
        int classificationID = 0;
        string obj_type;

        if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"].HasMember("vehicleSubClass")) {
            Value &vehicleSubClass = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vehicleSubClass"];
            try {
                int napId = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vehicleSubClass"].GetInt();
                classification = napClassType.at(napId);
                classificationID = napId;
            }
            catch(...){
                classification = "unclassified";
                spdlog::error("No classification found");
            }
        }
        else if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"].HasMember("vruSubClass")) {
            try { classification = personSubclassType.at(perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vruSubClass"].GetInt()); }
            catch(...){ classification = "unclassified";}
        }
        else if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"].HasMember("otherSubClass")) {
            try { classification = otherSubclassType.at(perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["otherSubClass"].GetInt());}
            catch(...){classification = "unclassified";}
        }

        float zAngularVelocity = 0.0;
        float zAngularVelocity_cov = 0.0;

        if (perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("zAngularVelocity")) {
            zAngularVelocity = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["zAngularVelocity"]["value"].GetFloat();
            zAngularVelocity_cov = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["zAngularVelocity"]["confidence"].GetFloat();
        }

        float size_x = 0.0;
        float size_y = 0.0;
        float size_z = 0.0;

        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("objectDimensionX")) size_x = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectDimensionX"]["value"].GetFloat();
        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("objectDimensionY")) size_y = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectDimensionY"]["value"].GetFloat();
        if(perceivedObjectContainer["containerData"]["perceivedObjects"][i].HasMember("objectDimensionZ")) size_z = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectDimensionZ"]["value"].GetFloat();

        //Misc
        json_obj.AddMember("id", objectId, objAlloc);
        json_obj.AddMember("uniqueID", static_cast<int64_t>(uniqueID), objAlloc);
        json_obj.AddMember("age", objAge, objAlloc);
        json_obj.AddMember("objectTimestamp", objTimestampSec, objAlloc);
        json_obj.AddMember("objectPerceptionQuality", objectPerceptionQuality, objAlloc);
        json_obj.AddMember("sensorType", rapidjson::Value(sensor.c_str(), objAlloc).Move(), objAlloc);
        json_obj.AddMember("sensorID", sensorID, objAlloc);

        //Position
        json_obj.AddMember("referenceLatitude", lat, objAlloc);
        json_obj.AddMember("referenceLongitude", lon, objAlloc);
        json_obj.AddMember("xDistance", xDistance, objAlloc);
        json_obj.AddMember("yDistance", yDistance, objAlloc);
        json_obj.AddMember("xDistanceCov", xCov, objAlloc);
        json_obj.AddMember("yDistanceCov", yCov, objAlloc);
        json_obj.AddMember("latitude", latitude, objAlloc);
        json_obj.AddMember("longitude", longitude, objAlloc);

        //Velocity
        json_obj.AddMember("xVelocity", xVelocity, objAlloc);
        json_obj.AddMember("xVelocityCov", xCovVelocity, objAlloc);
        json_obj.AddMember("yVelocity", yVelocity, objAlloc);
        json_obj.AddMember("yVelocityCov", yCovVelocity, objAlloc);
        json_obj.AddMember("speed", speed, objAlloc);

        //zAngularVelocity
        json_obj.AddMember("zAngularVelocity", zAngularVelocity, objAlloc);
        json_obj.AddMember("zAngularVelocityCov", zAngularVelocity_cov, objAlloc);

        //Acceleration
        json_obj.AddMember("xAcceleration", xAcceleration, objAlloc);
        json_obj.AddMember("yAcceleration", yAcceleration, objAlloc);
        json_obj.AddMember("acceleration", acceleration, objAlloc);

        //Heading
        json_obj.AddMember("heading", heading, objAlloc);
        json_obj.AddMember("headingCov", heading_cov, objAlloc);

        //Size
        json_obj.AddMember("size_x", size_x, objAlloc);
        json_obj.AddMember("size_y", size_y, objAlloc);
        json_obj.AddMember("size_z", size_z, objAlloc);

        //Classification
        json_obj.AddMember("classification", rapidjson::Value(classification.c_str(), objAlloc).Move(), objAlloc);
        json_obj.AddMember("classificationID", classificationID, objAlloc);

        //Stations informtaion
        json_obj.AddMember("stationSenderID", senderID, objAlloc);
        json_obj.AddMember("stationSenderType", sender_stationType, objAlloc);
        json_obj.AddMember("stationReceiverID", receiverID, objAlloc);
        json_obj.AddMember("stationReceiverType", receiverType, objAlloc);


        string objStr = docToString(json_obj);

        cpm_objects_str += objStr + ",";
    }

    if(cpm_objects_str.back() == ',')cpm_objects_str.pop_back();
    cpm_objects_str += "]";

    return cpm_objects_str;

}
