#ifndef AUTOWARE_ADAPTER_AUTOWARE_DATA_MANAGEMENT_H
#define AUTOWARE_ADAPTER_AUTOWARE_DATA_MANAGEMENT_H

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

struct autowareObject {
    int classification;
    int confidence;
    float heading;
    float cov_heading;

    double latitude;
    double longitude;

    float speed;
    float cov_speed;

    unsigned long int timestamp;
    int objectID;

    float size_x;
    float size_y;
    float size_z;

    float x;
    float y;
    float z;
    float cov_x;
    float cov_y;
    float cov_z;
    
    float twist_angz;
    float cov_twist_angz;
};

struct std::list<autowareObject> parse_msg(rapidjson::Document & doc, bool save_time_logs);
std::list<std::string> structs_to_string(std::list<autowareObject> objects);
bool calc_is_new_info(std::mutex* lock, std::map<int, autowareObject> * dict, autowareObject camera_object);
double calculateDistance(double lat1, double lon1, double lat2, double lon2);
double toRadians(double degrees);
std::string jsonToString(const rapidjson::Document& d);
std::string get_reply(const std::string& request, std::mutex* lock, std::map<int, std::string> * serialized_objects, unsigned long sequenceNumber);

#endif // AUTOWARE_ADAPTER_AUTOWARE_DATA_MANAGEMENT_H