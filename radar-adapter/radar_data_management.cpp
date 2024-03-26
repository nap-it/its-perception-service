#include "radar_data_management.h"

const long int time2004ms = 1072915200000;
const double M_PI180 = M_PI / 180.0;

radarMqttObject json_to_struct(std::string mqtt_radar_object) {
    // Parse the JSON string
    rapidjson::Document document;

    document.Parse(mqtt_radar_object.c_str());

    // Check if parsing succeeded
    if (!document.HasParseError() && document.IsObject()) {

        radarMqttObject obj;

        try { obj.acceleration = document["acceleration"].GetDouble(); } catch (std::exception e) { obj.acceleration = 0; }
        try { obj.classification = document["classification"].GetInt(); } catch (std::exception e) { obj.classification = 0; }
        try { obj.confidence = document["confidence"].GetInt(); } catch (std::exception e) { obj.confidence = 0; }
        try { obj.heading = document["heading"].GetDouble(); } catch (std::exception e) { obj.heading = 0; }
        try { obj.latitude = document["latitude"].GetDouble(); } catch (std::exception e) { obj.latitude = 0; }
        try { obj.length = document["length"].GetDouble(); } catch (std::exception e) { obj.length = 0; }
        try { obj.longitude = document["longitude"].GetDouble(); } catch (std::exception e) { obj.longitude = 0; }
        try { obj.cloudPersist = document["cloudPersist"].GetBool(); } catch (std::exception e) { obj.cloudPersist = false; }
        try { obj.objectID = document["objectID"].GetInt(); } catch (std::exception e) { obj.objectID = -101; }
        try { obj.receiverID = document["receiverID"].GetInt(); } catch (std::exception e) { obj.receiverID = -1; }
        try { obj.speed = document["speed"].GetDouble(); } catch (std::exception e) { obj.speed = 0; }
        unsigned long int timestamp = static_cast<unsigned long int>(document["timestamp"].GetDouble() * 1000) - time2004ms;
        obj.timestamp = timestamp;

        return obj;

    } else {
        std::cerr << "Failed to parse JSON" << std::endl;
        return {};
    }
}

std::string struct_to_string(radarMqttObject radar_object){
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("acceleration", radar_object.acceleration, allocator);
    document.AddMember("heading", radar_object.heading, allocator);
    document.AddMember("latitude", radar_object.latitude, allocator);
    document.AddMember("longitude", radar_object.longitude, allocator);
    document.AddMember("objID", radar_object.objectID, allocator);
    document.AddMember("sensorID", 1, allocator);
    document.AddMember("speed", radar_object.speed, allocator);
    document.AddMember("timestamp", radar_object.timestamp, allocator);
    document.AddMember("confidence", radar_object.confidence, allocator);

    // Classification
    rapidjson::Value classificationArray(rapidjson::kArrayType);
    rapidjson::Value classificationObject(rapidjson::kObjectType);
    rapidjson::Value objectClassObject(rapidjson::kObjectType);
    objectClassObject.AddMember("vehicleSubClass", radar_object.classification, allocator);
    classificationObject.AddMember("objectClass", objectClassObject, allocator);
    classificationObject.AddMember("confidence", 101, allocator);   // unavailable (101)
    classificationArray.PushBack(classificationObject, allocator);
    document.AddMember("classification", classificationArray, allocator);


    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    
    return buffer.GetString();
}

bool calc_is_new_info(std::mutex* lock, std::map<int, radarMqttObject> *last_sent_dict, radarMqttObject radar_object) {
    std::lock_guard guard(*lock);

    // if not in "last_sent" is newInfo
    if (last_sent_dict->find(radar_object.objectID) == last_sent_dict->end()) {
        return true;
    }

    int obj_id  = radar_object.objectID;

    auto last_sent = last_sent_dict->at(obj_id);

    double delta_timestamp = radar_object.timestamp - last_sent.timestamp;

    // Distance calculation between the present information and the last sent in a CPM
    double delta_distance = calculateDistance(last_sent.latitude, last_sent.longitude, radar_object.latitude, radar_object.longitude);

    // Speed variation between the present information and the last sent in a CPM
    double delta_speed = fabs(last_sent.speed - radar_object.speed);

    // Heading variation between the present information and the last sent in a CPM
    double delta_heading = fabs(last_sent.speed - radar_object.heading);

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

        dict_last_sent->insert_or_assign(key, value);
    }

    // add "objects_json" to "replyJson" document
    replyJson.AddMember("objects", objects_json, allocator);

    std::string reply = jsonToString(replyJson);

    

    return reply;
}


std::string get_reply(const std::string& request, std::mutex* lock, std::map<int, std::string> * serialized_objects){
    std::lock_guard guard(*lock);
    std::stringstream replyStream;

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

    replyStream << "{\"requestID\":" << requestID << ",\"numberObjects\":" << numberObjects << ",\"objects\":[";
    for (auto const& [key, value] : *serialized_objects) {
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
