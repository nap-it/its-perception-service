#include <vector>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>
#include <cmath>
#include <tuple>
#include <spdlog/spdlog.h>


//struct
#include "cpm_builder.hpp"

//sensor information
#include "sensor_info.hpp"

using namespace std;
using namespace rapidjson;


//globals
unordered_map<int, CpmObjectId> idMap = unordered_map<int, CpmObjectId>();
int current_id = 1;

//constants
const int sensor_size = 10;
const int object_size = 500;
const int max_message_size = 1500;
const int header_manag_stat_size = 38;
const int R = 6371000;
const long int time2004ms = 1072915200000;

CpmObjectId createCpmId(int combinedId){
    CpmObjectId cpm_id;
    cpm_id.id = current_id++;
    if (current_id > 65535){
        current_id = 1;
    }
    cpm_id.timestamp = static_cast<unsigned long int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    auto result = idMap.emplace(combinedId, cpm_id);
    return cpm_id;
}

CpmObjectId getCpmId(int sensorId, int objectId){
    int combinedId = (sensorId << 16) | (objectId & 0xFFFF);
    auto it = idMap.find(combinedId);
    if(it != idMap.end()){
        return it->second;
    } else {
        return createCpmId(combinedId);
    }
}

void cleanOldObjects(int maxTime){
    auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (auto it = idMap.begin(); it != idMap.end();){
        if (currentTime - it->second.timestamp > maxTime){
            it = idMap.erase(it);
        } else {
            ++it;
        }
    }
}

Document getManagementContainer(int timestamp, float latitude, float longitude, float altitude, int altitudeConfidence){
    Document managementContainer;
    managementContainer.SetObject();
    Document::AllocatorType& allocator = managementContainer.GetAllocator();
    managementContainer.AddMember("referenceTime", timestamp, allocator);

    Value altitudeValue(kObjectType);
    altitudeValue.AddMember("altitudeValue", altitude, allocator);
    altitudeValue.AddMember("altitudeConfidence", altitudeConfidence, allocator);

    Value positionConfidenceEllipse(kObjectType);
    positionConfidenceEllipse.AddMember("semiMajorConfidence", 4095, allocator);
    positionConfidenceEllipse.AddMember("semiMinorConfidence", 4095, allocator);
    positionConfidenceEllipse.AddMember("semiMajorOrientation", 0.0, allocator);

    Value referencePosition(kObjectType);
    referencePosition.AddMember("latitude", latitude, allocator);
    referencePosition.AddMember("longitude", longitude, allocator);
    referencePosition.AddMember("altitude", altitudeValue, allocator);
    referencePosition.AddMember("positionConfidenceEllipse", positionConfidenceEllipse, allocator);

    managementContainer.AddMember("referencePosition", referencePosition, allocator);

    return managementContainer;
}

Document getObjectContainerSegmentInfo(int totalSegments, int segmentID){
    Document objectContainerSegmentInfo;
    objectContainerSegmentInfo.SetObject();
    Document::AllocatorType& allocator = objectContainerSegmentInfo.GetAllocator();
    objectContainerSegmentInfo.AddMember("totalMsgSegments", totalSegments, allocator);
    objectContainerSegmentInfo.AddMember("thisSegmentNum", segmentID, allocator);

    return objectContainerSegmentInfo;
}

int getObjectContainerSize(int objectCount){
    return objectCount * object_size;
}

int getSensorContainerSize(int sensorCount){
    return sensorCount * sensor_size;
}

int getMessageEncodedSize(int numObjects, int numSensors){
    float a = 94.64082;
    float b = 26.35679;
    float c = 7.941537;

    return static_cast<int>(a + b * numObjects + c * numSensors);
}

tuple<float,float> gpsToLocal(float obj_lat, float obj_lon, float cam_lat, float cam_lon, float multConst, float multConst2){
    float north = ((obj_lat - cam_lat) * R) * multConst;
    float east = ((obj_lon - cam_lon) * multConst2) * multConst;

    return make_tuple(north, east);
}

tuple<float,float,float,float> getRelativeValues(float obj_heading, float obj_abs_speed, float obj_abs_acc, float multConst){
    float angle = obj_heading * multConst;

    if (obj_abs_acc != 0) {
        return make_tuple(
            obj_abs_speed * sin(angle),
            obj_abs_speed * cos(angle),
            obj_abs_acc * sin(angle),
            obj_abs_acc * cos(angle)
        );
    }
    return make_tuple(
        obj_abs_speed * sin(angle),
        obj_abs_speed * cos(angle),
        161.0f, // Default x_acc
        161.0f  // Default y_acc
    );
}

int getTimestampITS(float timestamp){
    return static_cast<int>((timestamp - 1072915200) * 1000) % 65536;
}

//TODO alter
int getMeasurementDeltaTime(int generationDeltaTime, int obj_timestamp){
    int deltaTimeMilliSecondSigned = generationDeltaTime - obj_timestamp;
    if (-2048 < deltaTimeMilliSecondSigned && deltaTimeMilliSecondSigned < 2047){
        return deltaTimeMilliSecondSigned;
    } else {
        cout << "Too old" << endl;
        return -2048;
    }
}

string docToString(const Document& value) {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

Document getPerceivedObject(int objectId, vector<int> sensorIDList, int deltaTimeMilliSecondSigned, int perceptionQuality, float x, float y, float x_speed, float y_speed, float x_acc, float y_acc, Document& objClassification){

    Document perceivedObject;
    perceivedObject.SetObject();
    Document::AllocatorType& allocator = perceivedObject.GetAllocator();

    //objectID
    perceivedObject.AddMember("objectID", objectId, allocator);

    //sensorIDList
    Value sensorList(kArrayType);
    for (int i = 0; i < sensorIDList.size(); i++){
        sensorList.PushBack(sensorIDList[i], allocator);
    }
    perceivedObject.AddMember("sensorIDList", sensorList, allocator);

    //deltaTimeMilliSecondSigned
    perceivedObject.AddMember("measurementDeltaTime", deltaTimeMilliSecondSigned, allocator);

    //objectPerceptionQuality
    perceivedObject.AddMember("objectPerceptionQuality", perceptionQuality, allocator);

    //position
    Value position(kObjectType);
    Value xCoordinate(kObjectType);
    Value yCoordinate(kObjectType);

    xCoordinate.AddMember("value", x, allocator);
    xCoordinate.AddMember("confidence", 1, allocator);
    yCoordinate.AddMember("value", y, allocator);
    yCoordinate.AddMember("confidence", 1, allocator);

    position.AddMember("xCoordinate", xCoordinate, allocator);
    position.AddMember("yCoordinate", yCoordinate, allocator);

    perceivedObject.AddMember("position", position, allocator);

    //xSpeed
    Value xSpeed(kObjectType);
    xSpeed.AddMember("value", x_speed, allocator);
    xSpeed.AddMember("confidence", 1, allocator);

    perceivedObject.AddMember("xSpeed", xSpeed, allocator);

    //ySpeed
    Value ySpeed(kObjectType);
    ySpeed.AddMember("value", y_speed, allocator);
    ySpeed.AddMember("confidence", 1, allocator);

    perceivedObject.AddMember("ySpeed", ySpeed, allocator);

    //xAcceleration
    Value xAcceleration(kObjectType);
    xAcceleration.AddMember("longitudinalAccelerationValue", x_acc, allocator);
    xAcceleration.AddMember("longitudinalAccelerationConfidence", 102, allocator);

    perceivedObject.AddMember("xAcceleration", xAcceleration, allocator);

    //yAcceleration
    Value yAcceleration(kObjectType);
    yAcceleration.AddMember("lateralAccelerationValue", y_acc, allocator);
    yAcceleration.AddMember("lateralAccelerationConfidence", 102, allocator);

    perceivedObject.AddMember("yAcceleration", yAcceleration, allocator);

    //classification
    Value classification(kArrayType);
    Value objClassificationVal;
    objClassificationVal.CopyFrom(objClassification, allocator);
    
    classification.PushBack(objClassificationVal, allocator);

    perceivedObject.AddMember("classification", classification, allocator);

    return perceivedObject;
}

vector<Document> getPerceivedObjectsList(unsigned long int timestampIts, const vector<Document>& receivedObjs, float cam_latitude, float cam_longitude){
    vector<Document> perceivedObjectsList;

    float multConst = (M_PI / 180);
    float multConst2 = R * cos(cam_latitude * M_PI / 180);

    for (int i = 0; i < receivedObjs.size(); i++){

        auto initialTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        Document receivedObj;
        receivedObj.CopyFrom(receivedObjs[i], receivedObj.GetAllocator());

        //received object data variables
        int objectId;
        int sensorId;
        float obj_timestamp;
        float obj_heading;
        float obj_abs_speed;
        float obj_abs_acc;
        float obj_lat;
        float obj_lon;
        int confidence;
        Document classification;

        //final variables
        float x, y, xSpeed, ySpeed, xAcc, yAcc;


        //Extract object data to variables
        objectId = receivedObj["objID"].GetInt();
        obj_timestamp = receivedObj["timestamp"].GetInt64();

        if(receivedObj.HasMember("latitude") && receivedObj.HasMember("longitude")){
            obj_lat = receivedObj["latitude"].GetFloat();
            obj_lon = receivedObj["longitude"].GetFloat();

            tie(y, x) = gpsToLocal(obj_lat, obj_lon, cam_latitude, cam_longitude, multConst, multConst2);
        } else {
            x = 0.0;
            y = 0.0;
            cout << "No latitude/longitude OR error" << endl;
        } 

        if(receivedObj.HasMember("heading") && receivedObj.HasMember("speed") && receivedObj.HasMember("acceleration")){
            obj_heading = receivedObj["heading"].GetFloat();
            obj_abs_speed = receivedObj["speed"].GetFloat();
            obj_abs_acc = receivedObj["acceleration"].GetFloat();

            tie(xSpeed, ySpeed, xAcc, yAcc) = getRelativeValues(obj_heading, obj_abs_speed, obj_abs_acc, multConst);

        } else {
            xSpeed = 16383;
            ySpeed = 16383;
            xAcc = 161;
            yAcc = 161;
            cout << "No heading/speed OR error" << endl;
        }

        if (receivedObj.HasMember("confidence")){
            confidence = receivedObj["confidence"].GetInt();
        } else {
            confidence = 0;
            cout << "No confidence OR error" << endl;
        }


        
        sensorId = receivedObj["sensorID"].GetInt();


        classification;
        classification.CopyFrom(receivedObj["classification"], classification.GetAllocator());

        int deltaTimeMilliSecondSigned = getMeasurementDeltaTime(timestampIts, obj_timestamp);

        if (deltaTimeMilliSecondSigned == -2048){
            continue;
        }

        int cpmObjectId = getCpmId(sensorId, objectId).id;

        vector<int> sensorIDList = {sensorId};

        Document perceivedObject = getPerceivedObject(cpmObjectId, sensorIDList, deltaTimeMilliSecondSigned, confidence, x, y, xSpeed, ySpeed, xAcc, yAcc, classification);

        perceivedObjectsList.push_back(move(perceivedObject));

        auto finalTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // cout << "Time to process object: " << finalTime - initialTime << " microseconds" << endl;

    }

    return perceivedObjectsList;
}

Document segment(unsigned long int timestampIts, const Document& managementContainer ,const vector<Document>& perceivedObjectsList, Document& stationContainer, bool add_sensor_data, const vector<Document>& sensorArray, int totalSegments = 1, int segmentID = 0){
    Document cpm;
    cpm.SetObject();
    Document::AllocatorType& allocator = cpm.GetAllocator();

    cpm.AddMember("generationDeltaTime", timestampIts, allocator);

    Document cpmParameters;
    cpmParameters.SetObject();

    //ManagementContainer
    Value managementContainerVal(kObjectType);
    managementContainerVal.CopyFrom(managementContainer, allocator);


    if (totalSegments > 1){
        Document objectContainerSegmentInfo = getObjectContainerSegmentInfo(totalSegments, segmentID);
        managementContainerVal.AddMember("perceivedObjectContainerSegmentInfo", objectContainerSegmentInfo, allocator);
    }

    cpmParameters.AddMember("managementContainer", managementContainerVal, allocator);

    //wrappedCpmContainer (array)
    Value wrappedCpmContainer(kArrayType);
    //containerId 1 (stationDataContainer)
    Value containerId1(kObjectType);
    containerId1.CopyFrom(stationContainer, allocator);

    wrappedCpmContainer.PushBack(containerId1, allocator);

    //containerId 5 (perceivedObjectContainer)
    Value containerId3(kObjectType);
    containerId3.AddMember("containerId", 5, allocator);
    //containerData is an array of perceivedObjects
    Value containerData3(kObjectType);
    containerData3.AddMember("numberOfPerceivedObjects", perceivedObjectsList.size(), allocator);
    Value perceivedObjects(kArrayType);
    for (int i = 0; i < perceivedObjectsList.size(); i++){
        Value perceivedObjectVal(kObjectType);
        perceivedObjectVal.CopyFrom(perceivedObjectsList[i], allocator);
        perceivedObjects.PushBack(perceivedObjectVal, allocator);
    }
    containerData3.AddMember("perceivedObjects", perceivedObjects, allocator);
    containerId3.AddMember("containerData", containerData3, allocator);

    wrappedCpmContainer.PushBack(containerId3, allocator);

    //containerId 3 (sensorDataContainer)
    if (add_sensor_data){
        Value containerId2(kObjectType);
        containerId2.AddMember("containerId", 3, allocator);
        //containerData is an array of sensors
        Value containerData2(kArrayType);
        for (int i = 0; i < sensorArray.size(); i++){
            Value sensorVal(kObjectType);
            sensorVal.CopyFrom(sensorArray[i], allocator);
            containerData2.PushBack(sensorVal, allocator);
        }
        containerId2.AddMember("containerData", containerData2, allocator);
        
        wrappedCpmContainer.PushBack(containerId2, allocator);
    }

    cpmParameters.AddMember("wrappedCpmContainer", wrappedCpmContainer, allocator);

    cpm.AddMember("cpmParameters", cpmParameters, allocator);

    return cpm;
}

vector<string> generateCPM(const vector<Document>& receivedObjs, float cam_latitude, float cam_longitude, float cam_altitude, int cam_altitude_conf, float cam_heading, bool add_sensor_data, vector<Document>& sensorArray){

    vector<string> cpmList;

    Document cpm;

    int numObjs = receivedObjs.size();

    auto time_before_objs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    unsigned long int deltaTime1970 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    unsigned long int deltaTime = deltaTime1970 - time2004ms;

    vector<Document> perceivedObjectsList = getPerceivedObjectsList(deltaTime, receivedObjs, cam_latitude, cam_longitude);

    auto time_after_objs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    spdlog::info("Time to process objects: {} microseconds", time_after_objs - time_before_objs);


    Document managementContainer = getManagementContainer(deltaTime, cam_latitude, cam_longitude, cam_altitude, cam_altitude_conf);

    auto time_after_management = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    spdlog::info("Time to get management container: {} microseconds", time_after_management - time_after_objs);

    //StationContainer
    Document stationContainer;
    stationContainer.SetObject();
    Document::AllocatorType& allocator = stationContainer.GetAllocator();
    stationContainer.AddMember("containerId", 1, allocator);
    Value stationContainerData(kObjectType);
    stationContainerData.AddMember("orientationAngle", cam_heading, allocator);
    stationContainer.AddMember("containerData", stationContainerData, allocator);

    auto time_after_station = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    spdlog::info("Time to get station container: {} microseconds", time_after_station - time_after_management);

    int message_size = 0;

    if (add_sensor_data){
        auto time_before_encodedSize = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        message_size = getMessageEncodedSize(perceivedObjectsList.size(), sensorArray.size());
        auto time_after_encodedSize = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        spdlog::info("Time to get message encoded size (with sensor): {} microseconds", time_after_encodedSize - time_before_encodedSize);
    } else {
        auto time_before_encodedSize = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        message_size = getMessageEncodedSize(perceivedObjectsList.size(), 0);
        auto time_after_encodedSize = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        spdlog::info("Time to get message encoded size (without sensor): {} microseconds", time_after_encodedSize - time_before_encodedSize);
    }

    if (message_size > max_message_size){
        cout << "Message size too big, need to segment" << endl;
        int max_objs_per_segment = (max_message_size - header_manag_stat_size) / object_size;
        vector<vector<Document>> segments;
        int size = perceivedObjectsList.size();
        for(int i = 0; i < size; i += max_objs_per_segment){
            int end = min(i + max_objs_per_segment, size);

            vector<Document> segmentObjs;

            for(int j = i; j < end; j++){
                Document obj;
                obj.CopyFrom(perceivedObjectsList[j], obj.GetAllocator());
                segmentObjs.push_back(move(obj));
            }
            
            segments.push_back(move(segmentObjs));
        }

        int sensor_segment = -1;

        if (add_sensor_data){
            for(int i = 0; i < segments.size(); i++){
                int aux = max_message_size - (header_manag_stat_size + getObjectContainerSize(segments[i].size()));
                if(aux >= getSensorContainerSize(sensorArray.size())){
                    sensor_segment = i + 1;
                    break;
                }
            }

            if (sensor_segment == -1){
                vector<Document> empty;
                segments.push_back(move(empty));
                sensor_segment = segments.size();
            }
        }

        for (int i = 0; i < segments.size(); i++){
            Document cpm;
            if (i == sensor_segment - 1){
                cpm = segment(deltaTime, managementContainer, segments[i], stationContainer, add_sensor_data, sensorArray, segments.size(), i+1);
            } else {
                cpm = segment(deltaTime, managementContainer, segments[i], stationContainer, false, sensorArray, segments.size(), i+1);
            }
            cpmList.push_back(docToString(cpm));
        }

    } else {    
        auto time_before_cpm = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        cpm = segment(deltaTime, managementContainer, perceivedObjectsList, stationContainer, add_sensor_data, sensorArray);

        auto time_after_cpm = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        spdlog::info("Time to create CPM: {} microseconds", time_after_cpm - time_before_cpm);

        string cpmString = docToString(cpm);

        auto time_after_cpm_to_string = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        spdlog::info("Time to convert CPM to string: {} microseconds", time_after_cpm_to_string - time_after_cpm);

        cpmList.push_back(cpmString);

        auto time_after_cpm_pushback = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        spdlog::info("Time to convert CPM to pushback: {} microseconds", time_after_cpm_pushback - time_after_cpm_to_string);
    }

    return cpmList;
}