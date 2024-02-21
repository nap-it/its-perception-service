#include "radar_data_management.h"

radarMqttObject json_to_struct(std::string mqtt_radar_object) {
    // Parse the JSON string
    rapidjson::Document document;
    document.Parse(mqtt_radar_object.c_str());

    // Check if parsing succeeded
    if (!document.HasParseError() && document.IsObject()) {

        radarMqttObject obj;

        obj.acceleration = document["acceleration"].GetDouble();
        obj.classification = document["classification"].GetInt();
        obj.confidence = document["confidence"].GetInt();
        obj.heading = document["heading"].GetDouble();
        obj.latitude = document["latitude"].GetDouble();
        obj.length = document["length"].GetDouble();
        obj.longitude = document["longitude"].GetDouble();
        obj.cloudPersist = document["cloudPersist"].GetBool();
        obj.objectID = document["objectID"].GetInt();
        obj.receiverID = document["receiverID"].GetInt();
        obj.speed = document["speed"].GetDouble();
        obj.timestamp = document["timestamp"].GetDouble();

//        spdlog::info("radar mqtt obj: \"{}\"\n", obj.objectID);

        return obj;

    } else {
        std::cerr << "Failed to parse JSON" << std::endl;
    }
}

bool calc_is_new_info(std::mutex* lock, std::map<int, radarMqttObject> *dict, radarMqttObject radar_object) {
    std::lock_guard guard(*lock);

    // if not in "last_sent" is newInfo
    if (dict->find(radar_object.objectID) == dict->end()) {
        return true;
    }

    int obj_id  = radar_object.objectID;

    double delta_timestamp = radar_object.timestamp - dict->at(obj_id).timestamp;

    // Distance calculation between the present information and the last sent in a CPM
    double delta_distance = calculateDistance(dict->at(obj_id).latitude, dict->at(obj_id).longitude, radar_object.latitude, radar_object.longitude);

    // Speed variation between the present information and the last sent in a CPM
    double delta_speed = fabs(dict->at(obj_id).speed - radar_object.speed);

    // Heading variation between the present information and the last sent in a CPM
    double delta_heading = fabs(dict->at(obj_id).speed - radar_object.heading);

    // Calculation of newInfo (according to the CPM rules)
    if ((delta_timestamp > 1) or (delta_distance > 4) or (delta_speed > 0.5) or (delta_heading > 4)) {
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
    return degrees * M_PI / 180.0;
}

std::string prepare_reply(const std::string& request, std::mutex* lock, std::map<int, radarMqttObject> * objects, std::map<int, radarMqttObject> * dict_last_sent) {
    // parse request data
    rapidjson::Document requestJson;
    requestJson.Parse(request.c_str());

    unsigned long int requestID = 0;
    int numberObjects = 0;
    if (requestJson.HasMember("requestID")){
        requestID = requestJson["requestID"].GetUint64();
    }
    if (requestJson.HasMember("numberObjects")){
        numberObjects = requestJson["numberObjects"].GetInt();
    }

//    spdlog::info("RequestID: \"{}\"\n", requestID);
//    spdlog::info("numberObjects: \"{}\"\n", numberObjects);

    // generate reply json
    rapidjson::Document replyJson = rapidjson::Document();
    replyJson.SetObject();
    rapidjson::Document::AllocatorType& allocator = replyJson.GetAllocator();
    replyJson.AddMember("requestID", requestID, allocator);
    replyJson.AddMember("numberObjects", numberObjects, allocator);

    rapidjson::Value objects_json(rapidjson::kArrayType);

    std::lock_guard guard(*lock);
    for (auto const& [key, value] : *objects) {
        rapidjson::Value tmpObject(rapidjson::kObjectType);
        tmpObject.AddMember("acceleration", value.acceleration, allocator);
        tmpObject.AddMember("heading", value.heading, allocator);
        tmpObject.AddMember("latitude", value.latitude, allocator);
        tmpObject.AddMember("longitude", value.longitude, allocator);
        tmpObject.AddMember("objID", value.objectID, allocator);
        tmpObject.AddMember("sensorID", 1, allocator);
        tmpObject.AddMember("speed", value.speed, allocator);
        tmpObject.AddMember("timestamp", value.timestamp, allocator);
        tmpObject.AddMember("confidence", value.confidence, allocator);

        // Classification
        rapidjson::Value classificationArray(rapidjson::kArrayType);
        rapidjson::Value classificationObject(rapidjson::kObjectType);
        rapidjson::Value objectClassObject(rapidjson::kObjectType);
        objectClassObject.AddMember("vehicleSubClass", value.classification, allocator);
        classificationObject.AddMember("objectClass", objectClassObject, allocator);
        classificationObject.AddMember("confidence", 101, allocator);   // unavailable (101)
        classificationArray.PushBack(classificationObject, allocator);
        tmpObject.AddMember("classification", classificationArray, allocator);

        objects_json.PushBack(tmpObject, allocator);

        // update "last_sent" data
        dict_last_sent->insert_or_assign(key, value);
    }

    // add "objects_json" to "replyJson" document
    replyJson.AddMember("objects", objects_json, allocator);

    return jsonToString(replyJson);
}

// Convert RapidJSON document to string
std::string jsonToString(const rapidjson::Document& d) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    return buffer.GetString();
}
