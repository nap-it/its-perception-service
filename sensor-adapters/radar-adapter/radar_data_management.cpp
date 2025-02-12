#include "radar_data_management.h"

const long int time2004ms = 1072915200000;
const double M_PI180 = M_PI / 180.0;

radarMqttObject json_to_struct(std::string mqtt_radar_object)
{
    // Parse the JSON string
    rapidjson::Document document;

    document.Parse(mqtt_radar_object.c_str());

    std::string document_str = jsonToString(document);

    // Check if parsing succeeded
    if (!document.HasParseError() && document.IsObject())
    {

        radarMqttObject obj;

        try {
            if(document.HasMember("acceleration") && document["acceleration"].IsDouble()) {
                obj.acceleration = document["acceleration"].GetDouble();
            } else {
                spdlog::warn("acceleration not found");
                return {};
            }

        } catch (std::exception e) {
            spdlog::warn("acceleration get exception");
            obj.acceleration = 0;
        }
        try {
            if (document.HasMember("classification") && document["classification"].IsInt()) {
                obj.classification = document["classification"].GetInt();
            } else {
                spdlog::warn("classification not found");
                return {};
            }
        } catch (std::exception e) {
            spdlog::warn("classification get exception");
            obj.classification = 0;
        }
        try {
            if (document.HasMember("confidence") && document["confidence"].IsInt()) {
                obj.confidence = document["confidence"].GetInt();
            } else {
                spdlog::warn("confidence not found");
                return {};
            }
        } catch (std::exception e) {
            spdlog::warn("confidence get exception");
            obj.confidence = 0;
        }
        try {
            if (document.HasMember("heading") && document["heading"].IsDouble()) {
                obj.heading = document["heading"].GetDouble();
            } else {
                spdlog::warn("heading not found");
                return {};
            }
        } catch (std::exception e) {
            spdlog::warn("heading get exception");
            obj.heading = 0;
        }
        try {
            if (document.HasMember("latitude") && document["latitude"].IsDouble()) {
                obj.latitude = document["latitude"].GetDouble();
            } else {
                spdlog::warn("latitude not found");
                return {};
            }
        }
        catch (std::exception e) {
            spdlog::warn("latitude get exception");
            obj.latitude = 0;
        }
        try {

            if (document.HasMember("length") && document["length"].IsDouble()) {
                obj.length = document["length"].GetDouble();
            } else {
                spdlog::warn("length not found");
                return {};
            }
        }
        catch (std::exception e) {
            spdlog::warn("length get exception");
            obj.length = 0;
        }
        try {
            if (document.HasMember("longitude") && document["longitude"].IsDouble()) {
                obj.longitude = document["longitude"].GetDouble();
            } else {
                spdlog::warn("longitude not found");
                return {};
            }
        }
        catch (std::exception e) {
            spdlog::warn("longitude get exception");
            obj.longitude = 0;
        }
        try {
            if (document.HasMember("cloudPersist") && document["cloudPersist"].IsBool()) {
                obj.cloudPersist = document["cloudPersist"].GetBool();
            } else {
                spdlog::warn("cloudPersist not found");
                return {};
            }
        }
        catch (std::exception e) {
            spdlog::warn("cloudPersist get exception");
            obj.cloudPersist = false;
        }
        try {
            if (document.HasMember("objectID") && document["objectID"].IsInt()) {
                obj.objectID = document["objectID"].GetInt();
            } else {
                spdlog::warn("objectID not found");
                return {};
            }
        } catch (std::exception e) {
            spdlog::warn("objectID get exception");
            obj.objectID = -101;
        }
        try {
            if (document.HasMember("receiverID") && document["receiverID"].IsInt()) {
                obj.receiverID = document["receiverID"].GetInt();
            } else {
                spdlog::warn("receiverID not found");
                return {};
            }
        } catch (std::exception e) {
            spdlog::warn("receiverID get exception");
            obj.receiverID = -1;
        }
        try {
            if (document.HasMember("speed") && document["speed"].IsDouble()) {
                obj.speed = document["speed"].GetDouble();
            } else {
                spdlog::warn("speed not found");
                return {};
            }
        }
        catch (std::exception e) {
            spdlog::warn("speed get exception");
            obj.speed = 0;
        }

        if (document.HasMember("timestamp") && document["timestamp"].IsDouble())
        {
            unsigned long int timestamp = static_cast<unsigned long int>(document["timestamp"].GetDouble() * 1000) - time2004ms;
            obj.timestamp = timestamp;
        }
        else
        {
            spdlog::warn("timestamp not found");
            return {};
        }
        return obj;
    } else {
        std::cerr << "Failed to parse JSON" << std::endl;
        return {};
    }
}

std::string struct_to_string(radarMqttObject radar_object)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    document.AddMember("acceleration", radar_object.acceleration, allocator);
    document.AddMember("heading", radar_object.heading, allocator);
    document.AddMember("latitude", radar_object.latitude, allocator);
    document.AddMember("longitude", radar_object.longitude, allocator);
    document.AddMember("size_x", radar_object.length, allocator);
    document.AddMember("objID", radar_object.objectID, allocator);
    document.AddMember("sensorID", 1, allocator);
    document.AddMember("speed", radar_object.speed, allocator);
    document.AddMember("timestamp", static_cast<uint64_t>(radar_object.timestamp), allocator);
    document.AddMember("confidence", radar_object.confidence, allocator);

    // Classification
    rapidjson::Value classificationArray(rapidjson::kArrayType);
    rapidjson::Value classificationObject(rapidjson::kObjectType);
    rapidjson::Value objectClassObject(rapidjson::kObjectType);
    objectClassObject.AddMember("vehicleSubClass", radar_object.classification, allocator);
    classificationObject.AddMember("objectClass", objectClassObject, allocator);
    classificationObject.AddMember("confidence", 101, allocator); // unavailable (101)
    classificationArray.PushBack(classificationObject, allocator);
    document.AddMember("classification", classificationArray, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    return buffer.GetString();
}

bool calc_is_new_info(std::mutex *lock, std::map<int, radarMqttObject> *last_sent_dict, std::map<int, radarMqttObject> *objects_to_send, radarMqttObject radar_object)
{
    std::lock_guard<std::mutex> guard(*lock);

    //if object is already in objects_to_send, update object
    if (objects_to_send->find(radar_object.objectID) != objects_to_send->end()) {
        spdlog::debug ("Object {} already in objects_to_send, updating ...", radar_object.objectID);
        return true;
    }

    // if object is not in "last_sent" 
    if (last_sent_dict->find(radar_object.objectID) == last_sent_dict->end()) {
        spdlog::debug("Object {} should be added (not in last_sent)", radar_object.objectID);
        return true;
    } else {
        // if object is in "last_sent" and the object is not in "objects_to_send"
        int obj_id = radar_object.objectID;

        auto last_sent = last_sent_dict->at(obj_id);

        double delta_timestamp = radar_object.timestamp - last_sent.timestamp;

        // Distance calculation between the present information and the last sent in a CPM
        double delta_distance = calculateDistance(last_sent.latitude, last_sent.longitude, radar_object.latitude, radar_object.longitude);

        // Speed variation between the present information and the last sent in a CPM
        double delta_speed = fabs(last_sent.speed - radar_object.speed);

        // Heading variation between the present information and the last sent in a CPM
        double delta_heading = fabs(last_sent.speed - radar_object.heading);

        // Calculation of newInfo (according to the CPM rules)
        if ((delta_timestamp > 1000) or (delta_distance > 4) or (delta_speed > 0.5) or (delta_heading > 4))
        {
            spdlog::debug("Object {} should be added (CPM rules)", obj_id);
            return true;
        }
    }

    spdlog::debug("Object {} should not be added (info too similar to last sent)", radar_object.objectID);
    return false;
}

// Calculate the distance between two points given their latitude and longitude
double calculateDistance(double lat1, double lon1, double lat2, double lon2)
{
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

std::string get_reply(const std::string &request, std::mutex *lock, std::map<int, std::string> *serialized_objects)
{
    std::lock_guard<std::mutex> guard(*lock);
    std::stringstream replyStream;

    rapidjson::Document requestJson;
    requestJson.Parse(request.c_str());

    unsigned long int requestID = 0;
    int numberObjects = 0;
    if (requestJson.HasMember("requestID") && requestJson["requestID"].IsUint64()) {
        requestID = requestJson["requestID"].GetUint64();
    } else {
        spdlog::error("Request does not have requestID");
    }

    if (requestJson.HasMember("numberObjects") && requestJson["numberObjects"].IsInt()) {
        numberObjects = requestJson["numberObjects"].GetInt();
    } else {
        spdlog::error("Request does not have numberObjects");
    }

    replyStream << "{\"requestID\":" << requestID << ",\"numberObjects\":" << numberObjects << ",\"objects\":[";
    for (auto const &[key, value] : *serialized_objects)
    {
        replyStream << value << ",";
    }
    std::string reply = replyStream.str();
    if (reply.back() == ',')
        reply.pop_back();
    reply += "]}";
    return reply;
}

// Convert RapidJSON document to string
std::string jsonToString(const rapidjson::Document &d)
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    return buffer.GetString();
}
