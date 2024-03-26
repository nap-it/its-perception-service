#ifndef CPM_BUILDER_HPP
#define CPM_BUILDER_HPP

#include <iostream>
#include <vector>
#include <rapidjson/document.h>

using namespace std;
using namespace rapidjson;

struct CpmObjectId {
    int id = 0;
    unsigned long int timestamp = 0;
};

CpmObjectId createCpmId(int sensorId, int objectId);

CpmObjectId getCpmId(int sensorId, int objectId);
void cleanOldObjectsIDs(int maxTime);

vector<string> generateCPM(const vector<Document>& receivedObjs, float cam_latitude, float cam_longitude, float cam_altitude, int cam_altitude_conf, float cam_heading, bool add_sensor_data, vector<Document>& sensorArray, int stationType, std::mutex& cpmMutex);
#endif // CPM_BUILDER_HPP