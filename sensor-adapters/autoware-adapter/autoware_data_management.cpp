#include <map>
#include <cmath>
#include <iostream>
#include <mutex>
#include <list>
#include "spdlog/spdlog.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <cstring>

#include <fstream>

#include "autoware_data_management.h"

using namespace std;

map<string, int> object_id_mapping;
int object_id_counter = 1;

const long int time2004ms = 1072915200000;
const double M_PI180 = M_PI / 180.0;

int vpiNumber = 0;
map<int, long> vpi_map;

//timings
// map<int, unsigned long int> timestamp_difference;
// int map_idx = 0;

map<int, string> classification_labels = {
        {0, "unknown"},
        {1, "car"},
        {2, "truck"},
        {3, "bus"},
        {4, "trailer"},
        {5, "motorcycle"},
        {6, "bicycle"},
        {7, "pedestrian"}
};


map<int, int> classification_converstion = {
        {0, 0},
        {1, 5},
        {2, 7},
        {3, 6},
        {4, 9},
        {5, 4},
        {6, 2},
        {7, 1}
};

int getOrCreateObjectID(const string& object_id) {
    if (object_id_mapping.find(object_id) == object_id_mapping.end()) {
        object_id_mapping[object_id] = object_id_counter;
        object_id_counter++;
        if (object_id_counter >= 65536) {
            object_id_counter = 1;
        }

        return object_id_mapping[object_id];
    }

    return object_id_mapping[object_id];
}

std::string jsonToString(const rapidjson::Document& d) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    return buffer.GetString();
}

std::string valueToString(const rapidjson::Value& v) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    v.Accept(writer);

    return buffer.GetString();
}

void writeVpiMapToFile(const std::string& filePath, const std::map<int , long>& timeMap) {
    std::ofstream outFile(filePath, std::ios::app);
    if (!outFile) {
        std::cerr << "Error: Could not open the file at " << filePath << std::endl;
        return;
    }
    for (const auto& pair : timeMap) {
        outFile << pair.first << ":" << pair.second << std::endl;
    }
    outFile.close();
}

struct std::list<autowareObject> parse_msg(rapidjson::Document & doc, bool save_time_logs) {
    std::list<autowareObject> aw_objects;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
    rapidjson::Value& doc_array = doc["objects"];
    if(doc.HasMember("sequenceNumber")){
        vpiNumber = doc["sequenceNumber"].GetInt();
        vpi_map[vpiNumber] = now;
        spdlog::debug("VPI number: {}", vpiNumber);
    } else {
        vpiNumber = -1;
        spdlog::debug("No sequenceNumber in VPI message");
    }

    if((vpiNumber % 500) == 0){
        if(save_time_logs){
            spdlog::info("VPI number: {}", vpiNumber);
            auto t = time(nullptr);
            auto tm = *localtime(&t);
            ostringstream oss;
            oss << put_time(&tm, "%d-%m-%Y%H-%M");
            auto str = oss.str();
            writeVpiMapToFile("/times/times_2_"+str+".txt", vpi_map);
        }
        vpi_map.clear();
    }

    spdlog::info("Document array size: {}", doc_array.Size());

    if (doc_array.Size() > 0) {
        for(int i = 0; i < doc_array.Size(); i++) { 
            
            rapidjson::Value& object = doc_array[i];
            autowareObject aw_object;

            aw_object.heading = object["heading"].GetFloat();
            aw_object.cov_heading = object["cov_heading"].GetFloat();

            aw_object.latitude = object["latitude"].GetFloat();
            aw_object.longitude = object["longitude"].GetFloat();

            aw_object.speed = object["speed"].GetFloat();
            aw_object.cov_speed = object["cov_speed"].GetFloat();

            aw_object.classification = object["classification"][0]["objectClass"]["vehicleSubClass"].GetInt();
            aw_object.confidence = object["classification"][0]["confidence"].GetInt();
           
            unsigned long int now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            unsigned long int object_timestamp = object["timestamp"].GetInt64();
            unsigned long int object_timestamp_ms = static_cast<unsigned long int>(object_timestamp/1000);
            aw_object.timestamp = object_timestamp_ms - time2004ms;

            aw_object.objectID = object["objID"].GetInt();

            aw_object.size_x = object["size_x"].GetFloat();
            aw_object.size_y = object["size_y"].GetFloat();
            aw_object.size_z = object["size_z"].GetFloat();

            aw_object.x = object["x"].GetFloat();
            aw_object.y = object["y"].GetFloat();
            aw_object.z = object["z"].GetFloat();

            aw_object.cov_x = object["cov_x"].GetFloat();
            aw_object.cov_y = object["cov_y"].GetFloat();
            aw_object.cov_z = object["cov_z"].GetFloat();

            aw_object.twist_angz = object["twist_angz"].GetFloat();
            aw_object.cov_twist_angz = object["cov_twist_angz"].GetFloat();

            aw_objects.push_back(aw_object);
            // spdlog::info("Object {} added to list", aw_object.objectID);
        }   
    }

    return aw_objects;       
}

std::list<std::string> structs_to_string(std::list<autowareObject> objects) {
    std::list<std::string> serialized_list;
    for(auto obj : objects){
        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

        doc.AddMember("heading", obj.heading, allocator);
        doc.AddMember("cov_heading", obj.cov_heading, allocator);
        doc.AddMember("latitude", obj.latitude, allocator);
        doc.AddMember("longitude", obj.longitude, allocator);
        doc.AddMember("speed", obj.speed, allocator);
        doc.AddMember("cov_speed", obj.cov_speed, allocator);
        doc.AddMember("timestamp", obj.timestamp, allocator);
        doc.AddMember("confidence", obj.confidence, allocator);
        doc.AddMember("acceleration", 0, allocator);   // unavailable (0)
        doc.AddMember("sensorID", 12, allocator);   // unavailable (0)
        doc.AddMember("objID", obj.objectID, allocator);
        doc.AddMember("size_x", obj.size_x, allocator);
        doc.AddMember("size_y", obj.size_y, allocator);
        doc.AddMember("size_z", obj.size_z, allocator);
        doc.AddMember("x", obj.x, allocator);
        doc.AddMember("y", obj.y, allocator);
        doc.AddMember("z", obj.z, allocator);
        doc.AddMember("cov_x", obj.cov_x, allocator);
        doc.AddMember("cov_y", obj.cov_y, allocator);
        doc.AddMember("cov_z", obj.cov_z, allocator);
        doc.AddMember("twist_angz", obj.twist_angz, allocator);
        doc.AddMember("cov_twist_angz", obj.cov_twist_angz, allocator);

        rapidjson::Value classificationArray(rapidjson::kArrayType);
        rapidjson::Value classificationObject(rapidjson::kObjectType);
        rapidjson::Value objectClassObject(rapidjson::kObjectType);
        objectClassObject.AddMember("vehicleSubClass", obj.classification, allocator);
        classificationObject.AddMember("objectClass", objectClassObject, allocator);
        classificationObject.AddMember("confidence", obj.confidence, allocator);
        classificationArray.PushBack(classificationObject, allocator);
        doc.AddMember("classification", classificationArray, allocator);

        serialized_list.push_back(jsonToString(doc));
    }   

    return serialized_list;
}

bool calc_is_new_info(std::mutex* lock, std::map<int, autowareObject> * dict, autowareObject aw_object){

    std::lock_guard<std::mutex> guard(*lock);

    if(dict->find(aw_object.objectID) == dict->end()){
        // spdlog::warn("Object {} should be sent (not in last_sent)", aw_object.objectID);
        return true;
    }

    int obj_id = aw_object.objectID;

    autowareObject last_sent = dict->at(obj_id);
    
    double delta_timestamp = aw_object.timestamp - last_sent.timestamp;

    double delta_distance = calculateDistance(last_sent.latitude, last_sent.longitude, aw_object.latitude, aw_object.longitude);

    double delta_speed = fabs(last_sent.speed - aw_object.speed);

    double delta_heading = fabs(last_sent.heading - aw_object.heading);

    if ((delta_timestamp > 1000) or (delta_distance > 4) or (delta_speed > 0.5) or (delta_heading > 4)) {
        // spdlog::warn("Object {} should be sent (CPM rules: delta_timestamp {}, delta_distance {}, delta_speed {}, delta_heading {})", obj_id, delta_timestamp, delta_distance, delta_speed, delta_heading);
        return true;
    }

    // spdlog::warn("Object {} should not be sent", obj_id);
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

std::string get_reply(const std::string& request, std::mutex* lock, std::map<int, std::string> * serialized_objects, unsigned long sequenceNumber){
    std::lock_guard<std::mutex> guard(*lock);
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

    numberObjects = serialized_objects->size();

    replyStream << "{\"requestID\":" << requestID << ",\"numberObjects\":" << numberObjects <<  ",\"sequenceNumber\":"<< sequenceNumber << ",\"objects\":[";
    for(auto const& [key, value] : *serialized_objects) {
        replyStream << value << ",";
    }
    std::string reply = replyStream.str();
    if (reply.back() == ',') reply.pop_back();
    reply += "]}";
    return reply;
}