#include "cpm-processing.h"
#include <map>
#include <vector>

const int   R = 6371000; // earth radius
const double PI = 3.141592653589793238463;


map<int,string> vehicleSubclassType = {
        {0, "unknown"},
        {1, "moped"},
        {2, "motorcycle"},
        {3, "passengerCar"},
        {4, "bus"},
        {5, "lightTruck"},
        {6, "heavyTruck"},
        {7, "trailer"},
        {8, "specialVehicles"},
        {9, "tram"},
        {10, "emergencyVehicle"},
        {11, "agricultural"}
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
        {8, "fusion"},
        {9, "inductionloop"},
        {10, "sphericalCamera"},
        {11, "itssaggregation"},
        {12, "deviceDetection"}
};

long int getTimestampIts(long int timestamp){
    return ((timestamp - 1072915200000)) % 65536;
}



std::pair<double, double> rotate_axes(int yaw, double x, double y) {
    if (yaw == 32767 || yaw == 3601) {
        yaw = 0;
    }

    double yaw_radians = M_PI / 180.0 * yaw;
    double xl = x * std::cos(yaw_radians) + y * std::sin(yaw_radians);
    double yl = (-x) * std::sin(yaw_radians) + y * std::cos(yaw_radians);

    return std::make_pair(xl, yl);
}

string process_cpm(Document &cpm){

    long int time_now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    /* get timestampITS (localGenDeltaTime)*/
    long int localGenDeltaTime = getTimestampIts(time_now);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);

    std::vector<object> perceived_objs;

    // Document wrappedCpmContainer;
    // wrappedCpmContainer.CopyFrom(cpm["wrappedCpmContainer"], wrappedCpmContainer.GetAllocator());

    // number of containers in the wrappedCpmContainer
    int number_containers = cpm["cpmParameters"]["wrappedCpmContainer"].GetArray().Size();

    if (number_containers == 0) {
        spdlog::info("No containers received!");
        return "";
    }

    // look for the perceivedObjectContainer and sensorInformationContainer
    Document perceivedObjectContainer, sensorInformationContainer;
    bool hasPerceivedObjectContainer, hasSensorInformationContaine = false;
    bool isRSU = false;
    
    for (int i = 0; i < number_containers; i++) {
        if (cpm["cpmParameters"]["wrappedCpmContainer"][i]["containerId"].GetInt() == 5) {
            perceivedObjectContainer.CopyFrom(cpm["cpmParameters"]["wrappedCpmContainer"][i], perceivedObjectContainer.GetAllocator());
            hasPerceivedObjectContainer = true;
        }
        if (cpm["cpmParameters"]["wrappedCpmContainer"][i]["containerId"].GetInt() == 2) {
            isRSU = true;
        }
        if (cpm["cpmParameters"]["wrappedCpmContainer"][i]["containerId"].GetInt() == 3) {
            sensorInformationContainer.CopyFrom(cpm["cpmParameters"]["wrappedCpmContainer"][i], sensorInformationContainer.GetAllocator());
            hasSensorInformationContaine = true;
        }
    }

    if (!hasPerceivedObjectContainer) {
        spdlog::info("No objects received!");
        return "";
    }

    // calculate cpm age
    int ageCpm = localGenDeltaTime - cpm["generationDeltaTime"].GetInt();

    if (ageCpm < 0) {
        ageCpm = (65536 + localGenDeltaTime) - cpm["generationDeltaTime"].GetInt();
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

    //cout << "lon " << lon << endl;
    int number_objects = perceivedObjectContainer["containerData"]["numberOfPerceivedObjects"].GetInt();
    //cout << "number_objects " << number_objects << endl;


    map<int, string> sensor_info;
    if (hasSensorInformationContaine) {
        int size = sensorInformationContainer["containerData"].GetArray().Size();
        //cout << size << flush << endl;
        for (int i = 0; i < size; i++) {
            //cout << "dentro do for" << endl;
            int sensor_id = sensorInformationContainer["containerData"][i]["sensorID"].GetInt();
            //cout << "ID " << sensor_id << endl;
            int sensor_type = sensorInformationContainer["containerData"][i]["type"].GetInt();
            //cout << "Type " << sensor_type << endl;
            try {
                //cout << "Type " << sensor_type << endl;
                sensor_info.insert(pair<int,string>(sensor_id, sensorType[sensor_type]));
                //cout << "sensorType  " << sensor_info[sensor_id] << endl;
            }
            catch (int n) {
                spdlog::error("Unknown sensor type");
            }
        }
    }

    //std::vector<object> objs;
    rapidjson::Document cpm_objects;
    cpm_objects.SetArray();

    for(int i=0; i<number_objects; i++)
    {
        
        rapidjson::Value json_obj(rapidjson::kObjectType);
        json_obj.AddMember("id", perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectID"].GetInt(), perceivedObjectContainer.GetAllocator());
        json_obj.AddMember("age", ageCpm + perceivedObjectContainer["containerData"]["perceivedObjects"][i]["timeOfMeasurement"].GetInt(), perceivedObjectContainer.GetAllocator());
        json_obj.AddMember("objectPerceptionQuality", perceivedObjectContainer["containerData"]["perceivedObjects"][i]["objectPerceptionQuality"].GetDouble(), perceivedObjectContainer.GetAllocator());
        

        int sensorID = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["sensorIDList"][0].GetInt();
        string sensor;
        try{
            sensor = sensor_info.at(sensorID);
        }
        catch(...){
            sensor = "unknown";
        }
        json_obj.AddMember("sensor", rapidjson::Value(sensor.c_str(), perceivedObjectContainer.GetAllocator()).Move(), perceivedObjectContainer.GetAllocator());
        json_obj.AddMember("sensorID", sensorID, perceivedObjectContainer.GetAllocator());


        double xDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["xCoordinate"]["value"].GetDouble();
        double yDistance = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["position"]["yCoordinate"]["value"].GetDouble();

        double latitude;
        double longitude;
        // if (sender_stationType == 15) {
        double N = yDistance;
        double E = xDistance;

        latitude = lat + (180/PI) * (N/R);
        longitude = lon + (180/PI) * (E/R) / cos((PI/180) * lat);
        // }
        // else {
        //     double x_cpm = xDistance;
        //     double y_cpm = yDistance;

        //     std::pair<int, int> point = rotate_axes(-yaw, -y_cpm, x_cpm);

        //     double N = point.second;
        //     double E = point.first;

        //     latitude = lat + (180/PI) * (N/R);
        //     longitude = lon + (180/PI) * (E/R) / cos((PI/180) * lat);
        // }

        json_obj.AddMember("latitude", latitude, perceivedObjectContainer.GetAllocator());
        json_obj.AddMember("longitude", longitude, perceivedObjectContainer.GetAllocator());


        double xSpeed = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["xSpeed"]["value"].GetDouble();
        double ySpeed = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["ySpeed"]["value"].GetDouble();

        double speed;
        if ((xSpeed != 16383) && (ySpeed != 16383)) {
            speed = std::sqrt(std::pow(xSpeed, 2) + std::pow(ySpeed, 2));
        }
        else {
            speed = 0.0;
        }
        json_obj.AddMember("speed", speed, perceivedObjectContainer.GetAllocator());

        double xAcceleration = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["xAcceleration"]["longitudinalAccelerationValue"].GetDouble();
        double yAcceleration = perceivedObjectContainer["containerData"]["perceivedObjects"][i]["yAcceleration"]["lateralAccelerationValue"].GetDouble();

        double acceleration;
        if ((xAcceleration != 161) && (yAcceleration != 161)) {
            acceleration = std::sqrt(std::pow(xAcceleration, 2) + std::pow(yAcceleration, 2));
        }
        else {
            acceleration = 0.0;
        }
        json_obj.AddMember("acceleration", acceleration, perceivedObjectContainer.GetAllocator());

        string classification;
        string obj_type;
        if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["class"].HasMember("vehicle")) {
            classification = vehicleSubclassType.at(perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["class"]["vehicle"]["type"].GetInt());
        }
        else if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["class"].HasMember("person")) {
            classification = personSubclassType.at(perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["class"]["person"]["type"].GetInt());
        }
        else if (perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["class"].HasMember("other")) {
            classification = otherSubclassType.at(perceivedObjectContainer["containerData"]["perceivedObjects"][i]["classification"][0]["class"]["other"]["type"].GetInt());
        }
        json_obj.AddMember("classification", rapidjson::Value(classification.c_str(), perceivedObjectContainer.GetAllocator()).Move(), perceivedObjectContainer.GetAllocator());

        cpm_objects.PushBack(json_obj, perceivedObjectContainer.GetAllocator());
    }



    // Convert the cpm_objects to a string
    buffer.Clear();
    cpm_objects.Accept(writer);

    return buffer.GetString();


}
