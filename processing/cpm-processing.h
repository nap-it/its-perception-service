#ifndef CPM_PROCESSING_C_CPM_PROCESSING_H
#define CPM_PROCESSING_C_CPM_PROCESSING_H

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <utility>
#include "rapidjson/document.h"
#include <spdlog/spdlog.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

using namespace rapidjson;
using namespace std::chrono;
using namespace std;

struct object {
    double acceleration;
    int age;
    string classification;
    int id;
    double latitude;
    double longitude;
    double objectConfidence;
    string sensor;
    int sensorID;
    double speed;
    string test;

    [[nodiscard]] string toString() const {
        return "acceleration: " + to_string(acceleration) + ", age: " + to_string(age) \
            + ", classification: " + classification + ", id: " + to_string(id) \
            + ", latitude: " + to_string(latitude) + ", longitude: " + to_string(longitude) \
            + ", objectConfidence: " + to_string(objectConfidence) + ", sensor: " + sensor \
            + ", sensorID: " + to_string(sensorID) + ", speed: " + to_string(speed) \
            + ", test: " + test;
    }
};

long int getTimestampIts(long int timestamp);


std::pair<double, double> rotate_axes(int yaw, double x, double y);

string process_cpm(Document& cpm);

#endif //CPM_PROCESSING_C_CPM_PROCESSING_H
