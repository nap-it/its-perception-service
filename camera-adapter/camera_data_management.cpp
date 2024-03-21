#include "camera_data_management.h"

const long int time2004ms = 1072915200000;
const double M_PI180 = M_PI / 180.0;

std::list<cameraMqttObject> parse_json(const std::string& mqtt_camera_object) {
    // Parse the JSON string
    rapidjson::Document document;
    document.Parse(mqtt_camera_object.c_str());

    // Check if parsing succeeded
    if (document.HasParseError() or !document.IsObject()) {
        spdlog::warn("Failed to parse JSON");
        return {};  // Return an empty std::list when parsing fails
    }

    std::list<cameraMqttObject> objs;
    unsigned long int timestamp = static_cast<unsigned long int>(document["timestamp"].GetDouble() * 1000) - time2004ms;

    const rapidjson::Value& mqttListOfObjects = document["listOfObjects"];

    uint list_size = mqttListOfObjects.Size();
    for (rapidjson::SizeType i = 0; i < list_size; i++) {
        const rapidjson::Value& mqtt_obj = mqttListOfObjects[i];

        cameraMqttObject obj{};

        obj.objectID = mqtt_obj["objectID"].GetInt();
        obj.classification = mqtt_obj["classification"].GetInt();
        obj.confidence = mqtt_obj["confidence"].GetInt();
        obj.latitude = mqtt_obj["latitude"].GetDouble();
        obj.longitude = mqtt_obj["longitude"].GetDouble();

        if (!mqtt_obj["heading"].IsNull())
            obj.heading = mqtt_obj["heading"].GetDouble();
        else
            obj.heading = 0.0;

        // Check if speed is null
        if (!mqtt_obj["speed"].IsNull())
            obj.speed = mqtt_obj["speed"].GetDouble();
        else
            obj.speed = 0.0;

        obj.timestamp = timestamp;

        objs.push_back(obj);
//        spdlog::info("camera mqtt obj: \"{}\"\n", obj.objectID);
    }

    return objs;
}
std::list<std::string> structs_to_string(std::list<cameraMqttObject> camera_object){
    std::list<std::string> serialized_list;
    for(auto obj : camera_object) {
        rapidjson::Document document;
        document.SetObject();
        rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

        document.AddMember("heading", obj.heading, allocator);
        document.AddMember("latitude", obj.latitude, allocator);
        document.AddMember("longitude", obj.longitude, allocator);
        document.AddMember("objID", obj.objectID, allocator);
        document.AddMember("sensorID", 2, allocator);
        document.AddMember("speed", obj.speed, allocator);
        document.AddMember("timestamp", obj.timestamp, allocator);
        document.AddMember("confidence", obj.confidence, allocator);

        // Classification
        rapidjson::Value classificationArray(rapidjson::kArrayType);
        rapidjson::Value classificationObject(rapidjson::kObjectType);
        rapidjson::Value objectClassObject(rapidjson::kObjectType);
        objectClassObject.AddMember("vehicleSubClass", obj.classification, allocator);
        classificationObject.AddMember("objectClass", objectClassObject, allocator);
        classificationObject.AddMember("confidence", 101, allocator);   // unavailable (101)
        classificationArray.PushBack(classificationObject, allocator);
        document.AddMember("classification", classificationArray, allocator);

        serialized_list.push_back(jsonToString(document));
    }
    return serialized_list;
}

bool calc_is_new_info(std::mutex* lock, std::map<int, cameraMqttObject> *dict, cameraMqttObject camera_object) {
    std::lock_guard guard(*lock);

    // if not in "last_sent" is newInfo
    if (dict->find(camera_object.objectID) == dict->end()) {
        return true;
    }

    int obj_id  = camera_object.objectID;

    double delta_timestamp = camera_object.timestamp - dict->at(obj_id).timestamp;

    // Distance calculation between the present information and the last sent in a CPM
    double delta_distance = calculateDistance(dict->at(obj_id).latitude, dict->at(obj_id).longitude, camera_object.latitude, camera_object.longitude);

    // Speed variation between the present information and the last sent in a CPM
    double delta_speed = fabs(dict->at(obj_id).speed - camera_object.speed);

    // Heading variation between the present information and the last sent in a CPM
    double delta_heading = fabs(dict->at(obj_id).heading - camera_object.heading);

    // Calculation of newInfo (according to the CPM rules)
    if ((delta_timestamp > 1000) or (delta_distance > 4) or (delta_speed > 0.5) or (delta_heading > 4)) {
        return true;
    }

    return false;
}

// Calculate the distance between two points given their latitude and longitude
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    // Radius of the Earth in kilometers
    constexpr double earthRadiusKm = 6371.0;

    // Convert latitude and longitude from degrees to radians
    lat1 = toRadians(lat1);
    lon1 = toRadians(lon1);
    lat2 = toRadians(lat2);
    lon2 = toRadians(lon2);

    // Haversine formula
    double dLat = lat2 - lat1;
    double dLon = lon2 - lon1;
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1) * cos(lat2) *
               sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    double distance = earthRadiusKm * c;

    return distance * 1000;
}

// Convert degrees to radians
double toRadians(double degrees) {
    return degrees * M_PI180;
}

// Convert RapidJSON document to string
std::string jsonToString(const rapidjson::Document& d) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    return buffer.GetString();
}
