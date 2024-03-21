#include "cpm-processing.h"
#include <map>
#include <vector>

const int   R = 6371000; // earth radius
const double PI = 3.141592653589793238463;
const long int time2004ms = 1072915200000;

map<int,string> napClassType = {
        {0, "unknown"},
        {1, "pedestrian"},
        {2, "cyclist"},
        {3, "moped"},
        {4, "motorcycle"},
        {5, "passangerCar"},
        {6, "bus"},
        {7, "lightTruck"},
        {8, "heavyTruck"},
        {9, "trailer"},
        {10, "specialVehicles"},
        {11, "tram"},
        {15, "roadSideUnit"},
        {16, "SafetyApp"},
        {17, "Moliceiro"},
        {18, "Test Device"},
        {19, "others"}
};

map<int,int> vehicleSubclassType = {
        {0, 0},
        {1, 3},
        {2, 4},
        {3, 5},
        {4, 6},
        {5, 7},
        {6, 8},
        {7, 9},
        {8, 10},
        {9, 11},
        {10, 10},
        {11, 8}
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
        {2, "camera"},
        {10, "sphericalCamera"},
        {11, "itssaggregation"},
        {12, "deviceDetection"}
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

string process_cpm(Document& cpm){

    
    unsigned long int localGenDeltaTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - time2004ms;

    std::vector<object> perceived_objs;

    // calculate cpm age
    int ageCpm = localGenDeltaTime - cpm["generationDeltaTime"].GetInt64();

    // spdlog::info("localGenDeltaTime {} - generationDeltaTime {} = ageCpm {}", localGenDeltaTime, cpm["generationDeltaTime"].GetInt64(), ageCpm);

    int number_containers = cpm["cpmParameters"]["cpmContainers"].GetArray().Size();

    if (number_containers == 0) {
        spdlog::error("No containers received!");
        return "";
    } else {
        // spdlog::info("Number of containers: {}", number_containers);
    }

    // look for the perceivedObjectContainer and sensorInformationContainer
    Document perceivedObjectContainer, sensorInformationContainer;
    bool hasPerceivedObjectContainer, hasSensorInformationContainer = false;
    bool isRSU = false;
    auto allocator = perceivedObjectContainer.GetAllocator();
    
    for (int i = 0; i < number_containers; i++) {
        if (cpm["cpmParameters"]["cpmContainers"][i]["containerId"].GetInt() == 5) {
            perceivedObjectContainer.CopyFrom(cpm["cpmParameters"]["cpmContainers"][i], allocator);
            hasPerceivedObjectContainer = true;
            // spdlog::info("PerceivedObjectContainer found!");
        }
        if (cpm["cpmParameters"]["cpmContainers"][i]["containerId"].GetInt() == 2) {
            isRSU = true;

        }
        if (cpm["cpmParameters"]["cpmContainers"][i]["containerId"].GetInt() == 3) {
            sensorInformationContainer.CopyFrom(cpm["cpmParameters"]["cpmContainers"][i], sensorInformationContainer.GetAllocator());
            hasSensorInformationContainer = true;
            // spdlog::info("SensorInformationContainer found!");
        }
    }

    if (!hasPerceivedObjectContainer) {
        spdlog::info("No perceived object received!");
        return "";
    }

    if (ageCpm < 0) {
        ageCpm = (65536 + localGenDeltaTime) - cpm["generationDeltaTime"].GetInt64();
    }
    //sender_stationType == 15 -> RSU
    //sender_stationType == 5 -> OBU

    int sender_stationType;
    if (isRSU) {
        sender_stationType = 15;
    }
    else {
        sender_stationType = 5;
    }

    // double yaw;
    // if (sender_stationType != 15) {
    //     yaw = cpm["cpmParameters"]["stationDataContainer"]["originatingVehicleContainer"]["heading"]["headingValue"].GetDouble();
    // }

    double lat = cpm["cpmParameters"]["managementContainer"]["referencePosition"]["latitude"].GetDouble();
    double lon = cpm["cpmParameters"]["managementContainer"]["referencePosition"]["longitude"].GetDouble();

    // spdlog::info("lat {} lon {}", lat, lon);

    //cout << "lon " << lon << endl;
    int number_objects = perceivedObjectContainer["containerData"]["numberOfPerceivedObjects"].GetInt();
    
    spdlog::info("Number of objects: {}", number_objects);

    if (number_objects == 0) {
        spdlog::info("No objects received!");
        return "";
    }

    string cpm_objects_str = "[";

    for(int i=0; i<number_objects; i++)
    {
        
        Document json_obj(rapidjson::kObjectType);
        auto objAlloc = json_obj.GetAllocator();
        json_obj.AddMember("id", perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectID"].GetInt(), objAlloc);
        json_obj.AddMember("age", ageCpm + abs(perceivedObjectContainer["containerData"]["perceivedObjects"][i]["measurementDeltaTime"].GetInt64()), objAlloc);
        json_obj.AddMember("objectPerceptionQuality", perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectPerceptionQuality"].GetDouble(), objAlloc);
        json_obj.AddMember("detectionStationType", sender_stationType, objAlloc);

        // spdlog::info("Object ID: {}", perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectID"].GetInt());
        

        int sensorID = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["sensorIDList"][0].GetInt();

        // spdlog::info("Sensor ID: {}", sensorID);

        string sensor;
        try{
            sensor = sensorType.at(sensorID);
        }
        catch(...){
            sensor = "unknown";
        }

        // spdlog::info("Sensor: {}", sensor);

        json_obj.AddMember("sensor", rapidjson::Value(sensor.c_str(), objAlloc).Move(), objAlloc);
        json_obj.AddMember("sensorID", sensorID, objAlloc);


        double xDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["xCoordinate"]["value"].GetDouble();
        double yDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["yCoordinate"]["value"].GetDouble();

        // spdlog::info("xDistance: {} yDistance: {}", xDistance, yDistance);

        double latitude;
        double longitude;
        double N = yDistance;
        double E = xDistance;

        latitude = lat + (180/PI) * (N/R);
        longitude = lon + (180/PI) * (E/R) / cos((PI/180) * lat);

        json_obj.AddMember("latitude", latitude, objAlloc);
        json_obj.AddMember("longitude", longitude, objAlloc);


        double xSpeed = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["xSpeed"]["value"].GetDouble();
        double ySpeed = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["ySpeed"]["value"].GetDouble();

        // spdlog::info("xSpeed: {} ySpeed: {}", xSpeed, ySpeed);

        double speed;
        if ((xSpeed != 16383) && (ySpeed != 16383)) {
            speed = std::sqrt(std::pow(xSpeed, 2) + std::pow(ySpeed, 2));
        }
        else {
            speed = 0.0;
        }
        json_obj.AddMember("speed", speed, objAlloc);

        double xAcceleration = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["xAcceleration"]["longitudinalAccelerationValue"].GetDouble();
        double yAcceleration = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["yAcceleration"]["lateralAccelerationValue"].GetDouble();

        // spdlog::info("xAcceleration: {} yAcceleration: {}", xAcceleration, yAcceleration);

        double acceleration;
        if ((xAcceleration != 161) && (yAcceleration != 161)) {
            acceleration = std::sqrt(std::pow(xAcceleration, 2) + std::pow(yAcceleration, 2));
        }
        else {
            acceleration = 0.0;
        }
        json_obj.AddMember("acceleration", acceleration, objAlloc);

        string classification;
        string obj_type;
        if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"].HasMember("vehicleSubClass")) {
            try {
                int napId = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vehicleSubClass"].GetInt();
                // cout << "IncomingID " << perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["objectClass"]["vehicleSubClass"].GetInt() << ", napId " << napId << " = " << napClassType.at(napId) << endl;
                classification = napClassType.at(napId);
            }
            catch(...){
                classification = "unclassified";
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

        // spdlog::info("Classification: {}", classification);

        json_obj.AddMember("classification", rapidjson::Value(classification.c_str(), objAlloc).Move(), objAlloc);

        string objStr = docToString(json_obj);

        cpm_objects_str += objStr + ",";
    }

    if(cpm_objects_str.back() == ','){
        cpm_objects_str.pop_back();
    }

    cpm_objects_str += "]";

    return cpm_objects_str;

}
