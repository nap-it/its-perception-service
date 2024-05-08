#include "camera_data_management.h"

const long int time2004ms = 1072915200000;
const double M_PI180 = M_PI / 180.0;


std::string valueToString(const rapidjson::Value& v) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    v.Accept(writer);

    return buffer.GetString();
}

std::list<cameraMqttObject> parse_json(const std::string& mqtt_camera_object) {
    // Parse the JSON string
    rapidjson::Document document;
    document.Parse(mqtt_camera_object.c_str());

    // Check if parsing succeeded
    if (document.HasParseError() or !document.IsObject()) {
        spdlog::error("Failed to parse JSON");
        return {};  // Return an empty std::list when parsing fails
    }

    std::list<cameraMqttObject> objs;
    if(!document.HasMember("timestamp") && document["timestamp"].IsDouble()) {
        spdlog::error("timestamp is not a double or does not exist");
        return objs;
    }
    unsigned long int timestamp = static_cast<unsigned long int>(document["timestamp"].GetDouble() * 1000) - time2004ms;

    if (!document.HasMember("listOfObjects") && !document["listOfObjects"].IsArray()) {
        spdlog::error("listOfObjects is not an array or does not exist");
        return objs;
    }
    const rapidjson::Value& mqttListOfObjects = document["listOfObjects"];

    uint list_size = mqttListOfObjects.Size();
    for (rapidjson::SizeType i = 0; i < list_size; i++) {
        const rapidjson::Value& mqtt_obj = mqttListOfObjects[i];

        cameraMqttObject obj{};

        if (mqtt_obj.HasMember("objectID") && mqtt_obj["objectID"].IsInt()) {
            obj.objectID = mqtt_obj["objectID"].GetInt();
            spdlog::debug("Object ID: {}", obj.objectID);
        } else {
            spdlog::error("objectID is not an int or does not exist");
            return objs;
        }
        if (mqtt_obj.HasMember("classification") && mqtt_obj["classification"].IsInt()) {
            obj.classification = mqtt_obj["classification"].GetInt();
        } else {
            spdlog::error("classification is not an int or does not exist");
            return objs;
        }
        if (mqtt_obj.HasMember("confidence") && mqtt_obj["confidence"].IsInt()) {
            obj.confidence = mqtt_obj["confidence"].GetInt();
        } else {
            spdlog::error("confidence is not an int or does not exist");
            return objs;
        }
        if (mqtt_obj.HasMember("latitude") && mqtt_obj["latitude"].IsDouble()) {
            obj.latitude = mqtt_obj["latitude"].GetDouble();
        } else {
            spdlog::error("latitude is not a double or does not exist");
            return objs;
        }
        if (mqtt_obj.HasMember("longitude") && mqtt_obj["longitude"].IsDouble()) {
            obj.longitude = mqtt_obj["longitude"].GetDouble();
        } else {
            spdlog::error("longitude is not a double or does not exist");
            return objs;
        }
        //usually null
        if (mqtt_obj.HasMember("heading") && mqtt_obj["heading"].IsDouble()) {
            if (!mqtt_obj["heading"].IsNull())  obj.heading = mqtt_obj["heading"].GetDouble();
        } else {
            // spdlog::error("heading is not a double or does not exist");
            obj.heading = 0;
        }
        // Check if speed is null
        if (mqtt_obj.HasMember("speed") && mqtt_obj["speed"].IsDouble()) {
            if (!mqtt_obj["speed"].IsNull()) obj.speed = mqtt_obj["speed"].GetDouble();
        } else {
            // spdlog::error("speed is not a double or does not exist");
            obj.speed = 0;
        }
        obj.timestamp = timestamp;

        objs.push_back(obj);
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
        spdlog::debug("Object {} should be sent (not in last_sent)", camera_object.objectID);
        return true;
    }

    int obj_id  = camera_object.objectID;

    double delta_timestamp = camera_object.timestamp - dict->at(obj_id).timestamp;

    // Distance calculation between the present information and the last sent in a CPM
    double delta_distance = calculateDistance(dict->at(obj_id).latitude, dict->at(obj_id).longitude, camera_object.latitude, camera_object.longitude);

    // Speed variation between the present information and the last sent in a CPM
    double delta_speed = fabs(dict->at(obj_id).speed - camera_object.speed);

    // Heading variation between the present information and the last sent in a CPM
    double delta_heading = fabs(dict->at(obj_id).speed - camera_object.heading);

    // Calculation of newInfo (according to the CPM rules)
    if ((delta_timestamp > 1000) or (delta_distance > 4) or (delta_speed > 0.5) or (delta_heading > 4)) {
        spdlog::debug("Object {} should be sent (CPM rules: delta_timestamp {}, delta_distance {}, delta_speed {}, delta_heading {})", obj_id, delta_timestamp, delta_distance, delta_speed, delta_heading);
        return true;
    }

    spdlog::debug("Object {} should not be sent", obj_id);
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

std::string prepare_reply(const std::string& request, std::mutex* lock, std::map<int, cameraMqttObject> * objects, std::map<int, cameraMqttObject> * dict_last_sent) {
    // parse request data
    rapidjson::Document requestJson;
    requestJson.Parse(request.c_str());

    unsigned long int requestID = 0;
    int numberObjects = 0;
    if (requestJson.HasMember("requestID") && requestJson["requestID"].IsUint64()){
        requestID = requestJson["requestID"].GetUint64();
    } else {
        spdlog::error("requestID is not an unsigned long int or does not exist");
        return "";
    }
    if (requestJson.HasMember("numberObjects") && requestJson["numberObjects"].IsInt()){
        numberObjects = requestJson["numberObjects"].GetInt();
    } else {
        spdlog::error("numberObjects is not an int or does not exist");
        return "";
    }

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
        tmpObject.AddMember("heading", value.heading, allocator);
        tmpObject.AddMember("latitude", value.latitude, allocator);
        tmpObject.AddMember("longitude", value.longitude, allocator);
        tmpObject.AddMember("objID", value.objectID, allocator);
        tmpObject.AddMember("sensorID", 2, allocator);
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

std::string get_reply(const std::string& request, std::mutex* lock, std::map<int, std::string> * serialized_objects){
    std::lock_guard guard(*lock);
    std::stringstream replyStream;

    rapidjson::Document requestJson;
    requestJson.Parse(request.c_str());

    unsigned long int requestID = 0;
    int numberObjects = 0;
    if (requestJson.HasMember("requestID") && requestJson["requestID"].IsUint64()){
        requestID = requestJson["requestID"].GetUint64();
    } else {
        spdlog::error("requestID is not an unsigned long int or does not exist");
        return "";
    }
    if (requestJson.HasMember("numberObjects") && requestJson["numberObjects"].IsInt()){
        numberObjects = requestJson["numberObjects"].GetInt();
    } else {
        spdlog::error("numberObjects is not an int or does not exist");
        return "";
    }

    replyStream << "{\"requestID\":" << requestID << ",\"numberObjects\":" << numberObjects << ",\"objects\":[";
    for(auto const& [key, value] : *serialized_objects) {
        replyStream << value << ",";
    }
    std::string reply = replyStream.str();
    if (reply.back() == ',') reply.pop_back();
    reply += "]}";
    return reply;
}
// Convert RapidJSON document to string
std::string jsonToString(const rapidjson::Document& d) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    return buffer.GetString();
}
