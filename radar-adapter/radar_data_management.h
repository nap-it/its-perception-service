#ifndef RADAR_ADAPTER_RADAR_DATA_MANAGEMENT_H
#define RADAR_ADAPTER_RADAR_DATA_MANAGEMENT_H

#include <iostream>
#include "spdlog/spdlog.h"
#include "rapidjson/document.h"

struct radarMqttObject {
    double acceleration;
    int classification;
    double confidence;
    double heading;
    double latitude;
    double length;
    double longitude;
    bool newInfo;
    int objectID;
    int receiverID;
    double speed;
    double timestamp;
};

struct radarMqttObject json_to_struct(std::string mqtt_radar_object);
bool calc_is_new_info(radarMqttObject radar_object);

#endif //RADAR_ADAPTER_RADAR_DATA_MANAGEMENT_H
