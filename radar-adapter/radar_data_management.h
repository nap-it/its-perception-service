#ifndef RADAR_ADAPTER_RADAR_DATA_MANAGEMENT_H
#define RADAR_ADAPTER_RADAR_DATA_MANAGEMENT_H

#include <map>
#include <cmath>
#include <iostream>
#include <mutex>
#include "spdlog/spdlog.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/prettywriter.h>
#include <sstream>



struct radarMqttObject {
    double acceleration;
    int classification;
    int confidence;
    double heading;
    double latitude;
    double length;
    double longitude;
    bool cloudPersist;
    int objectID;
    int receiverID;
    double speed;
    unsigned long int timestamp;
};

struct radarMqttObject json_to_struct(std::string mqtt_radar_object);
std::string struct_to_string(radarMqttObject radar_object);
bool calc_is_new_info(std::mutex* lock, std::map<int, radarMqttObject> * dict, radarMqttObject radar_object);
double calculateDistance(double lat1, double lon1, double lat2, double lon2);
double toRadians(double degrees);
std::string jsonToString(const rapidjson::Document& d);
std::string prepare_reply(const std::string& request, std::mutex* lock, std::map<int, radarMqttObject> * objects, std::map<int, radarMqttObject> * dict_last_sent);
std::string get_reply(const std::string& request, std::mutex* lock, std::map<int, std::string> * serialized_objects);

#endif //RADAR_ADAPTER_RADAR_DATA_MANAGEMENT_H
