#ifndef CAMERA_ADAPTER_CAMERA_DATA_MANAGEMENT_H
#define CAMERA_ADAPTER_CAMERA_DATA_MANAGEMENT_H

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


struct cameraMqttObject {
    int classification;
    int confidence;
    double heading;
    double latitude;
    double longitude;
    int objectID;
    double speed;
    unsigned long int timestamp;
};

struct std::list<cameraMqttObject> parse_json(const std::string& mqtt_camera_object);
std::list<std::string> structs_to_string(std::list<cameraMqttObject> camera_object);
bool calc_is_new_info(std::mutex* lock, std::map<int, cameraMqttObject> * dict, cameraMqttObject camera_object);
double calculateDistance(double lat1, double lon1, double lat2, double lon2);
double toRadians(double degrees);
std::string jsonToString(const rapidjson::Document& d);
#endif //CAMERA_ADAPTER_CAMERA_DATA_MANAGEMENT_H
