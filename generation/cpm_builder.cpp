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

using namespace std;
using namespace rapidjson;




//globals
unordered_map<int, CpmObjectId> idMap = unordered_map<int, CpmObjectId>();
int current_id = 1;
unsigned long int lowest_obj_timestamp = 0;

//constants
const float sensor_size = 7.941537;
const float object_size = 26.35679;
const int max_message_size = 1500;
const int header_manag_stat_size = 38;
const int R = 6371000;
const long int time2004ms = 1072915200000;
const double multConst = (M_PI / 180);




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

void cleanOldObjectsIDs(int maxTime){
    auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (auto it = idMap.begin(); it != idMap.end();){
        if (currentTime - it->second.timestamp > maxTime){
            it = idMap.erase(it);
        } else {
            ++it;
        }
    }

    spdlog::debug("Number of objects in map: {}", idMap.size());
}

Document getManagementContainer(unsigned long int timestamp, float latitude, float longitude, float altitude, int altitudeConfidence, int sequenceNumber){
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
    positionConfidenceEllipse.AddMember("semiMajorOrientation", 0, allocator);

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

void printValue(const Value& value){
    cout << "------- Printing Value -------" << endl;
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    value.Accept(writer);
    cout << buffer.GetString() << endl;
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
long int getMeasurementDeltaTime(unsigned long int generationDeltaTime, unsigned long int obj_timestamp){
    long int deltaTimeMilliSecondSigned = generationDeltaTime - obj_timestamp;
    if ((-2048 < deltaTimeMilliSecondSigned) && (deltaTimeMilliSecondSigned < 2047)){
        return deltaTimeMilliSecondSigned;
    } else {
        return -2048;
    }
}

string docToString(const Document& value) {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

Document getPerceivedObject(int objectId, vector<int> sensorIDList, int deltaTimeMilliSecondSigned, int perceptionQuality, float x, float y, float x_speed, float y_speed, float x_acc, float y_acc, Document& objClassification, float heading){

    Document perceivedObject;
    perceivedObject.SetObject();
    Document::AllocatorType& allocator = perceivedObject.GetAllocator();

    //objectID
    perceivedObject.AddMember("objectId", objectId, allocator);

    //sensorIDList
    Value sensorList(kArrayType);
    for (int i = 0; i < sensorIDList.size(); i++){
        sensorList.PushBack(sensorIDList[i], allocator);
    }
    perceivedObject.AddMember("sensorIdList", sensorList, allocator);

    //deltaTimeMilliSecondSigned
    perceivedObject.AddMember("measurementDeltaTime", deltaTimeMilliSecondSigned, allocator);

    //objectPerceptionQuality
    perceivedObject.AddMember("objectPerceptionQuality", 0, allocator);

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

    //velocity
    Value velocity(kObjectType);
    Value xVelocity(kObjectType);
    Value yVelocity(kObjectType);
    Value cartesianVelocity(kObjectType);

    xVelocity.AddMember("value", x_speed, allocator);
    xVelocity.AddMember("confidence", 1, allocator);
    yVelocity.AddMember("value", y_speed, allocator);
    yVelocity.AddMember("confidence", 1, allocator);

    cartesianVelocity.AddMember("xVelocity", xVelocity, allocator);
    cartesianVelocity.AddMember("yVelocity", yVelocity, allocator);

    velocity.AddMember("cartesianVelocity", cartesianVelocity, allocator);

    perceivedObject.AddMember("velocity", velocity, allocator);

    //acceleration
    Value acceleration(kObjectType);
    Value xAcceleration(kObjectType);
    Value yAcceleration(kObjectType);
    Value cartesianAcceleration(kObjectType);

    xAcceleration.AddMember("value", x_acc, allocator);
    xAcceleration.AddMember("confidence", 1, allocator);

    yAcceleration.AddMember("value", y_acc, allocator);
    yAcceleration.AddMember("confidence", 1, allocator);

    cartesianAcceleration.AddMember("xAcceleration", xAcceleration, allocator);
    cartesianAcceleration.AddMember("yAcceleration", yAcceleration, allocator);

    acceleration.AddMember("cartesianAcceleration", cartesianAcceleration, allocator);

    perceivedObject.AddMember("acceleration", acceleration, allocator);

    //heading
    Value angles(kObjectType);
    Value zAngle(kObjectType);

    zAngle.AddMember("value", heading, allocator);
    zAngle.AddMember("confidence", 1, allocator);

    angles.AddMember("zAngle", zAngle, allocator);

    perceivedObject.AddMember("angles", angles, allocator);

    //classification
    Value classification(kObjectType);
    classification.CopyFrom(objClassification, allocator);

    perceivedObject.AddMember("classification", classification, allocator);

    return perceivedObject;
}

vector<Document> getPerceivedObjectsList(unsigned long int timestampIts, vector<Document>& receivedObjs, float cam_latitude, float cam_longitude){

    vector<Document> perceivedObjectsList;

    double multConst2 = R * cos(cam_latitude * M_PI / 180);

    //find lowest obj timestamp, start the temp_lowest_obj_timestamp with the highest possible value
    unsigned long int temp_lowest_obj_timestamp = 18446744073709551615;

    for (int i = 0; i < receivedObjs.size(); i++){

        auto initialTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        Document receivedObj;
        receivedObj.CopyFrom(receivedObjs[i], receivedObj.GetAllocator());

        //received object data variables
        int objectId;
        int sensorId;
        unsigned long int obj_timestamp;
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

        if (obj_timestamp < temp_lowest_obj_timestamp){
            temp_lowest_obj_timestamp = obj_timestamp;
        }

        if(receivedObj.HasMember("latitude") && receivedObj.HasMember("longitude")){
            obj_lat = receivedObj["latitude"].GetFloat();
            obj_lon = receivedObj["longitude"].GetFloat();

            tie(y, x) = gpsToLocal(obj_lat, obj_lon, cam_latitude, cam_longitude, multConst, multConst2);

            if(x > 1310.72){
                x = 1310.72;
            } else if(x < -1310.72){
                x = -1310.72;
                spdlog::debug("X out of bounds");
                continue;
            }

            if(y > 1310.72){
                y = 1310.72;
            } else if(y < -1310.72){
                y = -1310.72;
                spdlog::debug("Y out of bounds");
                continue;
            }
            
        } else {
            x = 0.0;
            y = 0.0;
        } 

        if(receivedObj.HasMember("heading") && receivedObj.HasMember("speed") && receivedObj.HasMember("acceleration")){
            obj_heading = receivedObj["heading"].GetFloat();
            // cout << "Object heading: " << obj_heading << endl;
            obj_abs_speed = receivedObj["speed"].GetFloat();
            // cout << "Object speed: " << obj_abs_speed << endl;
            obj_abs_acc = receivedObj["acceleration"].GetFloat();
            // cout << "Object acceleration: " << obj_abs_acc << endl;

            tie(xSpeed, ySpeed, xAcc, yAcc) = getRelativeValues(obj_heading, obj_abs_speed, obj_abs_acc, multConst);

        } else {
            xSpeed = 16383;
            ySpeed = 16383;
            xAcc = 161;
            yAcc = 161;
            // cout << "No heading/speed OR error" << endl;
        }

        if (receivedObj.HasMember("confidence")){
            confidence = receivedObj["confidence"].GetInt();
        } else {
            confidence = 0;
            // cout << "No confidence OR error" << endl;
        }


        
        sensorId = receivedObj["sensorID"].GetInt();

        classification.CopyFrom(receivedObj["classification"], classification.GetAllocator());

        int deltaTimeMilliSecondSigned = getMeasurementDeltaTime(timestampIts, obj_timestamp);

        if (deltaTimeMilliSecondSigned == -2048){
            continue;
        }

        int cpmObjectId = getCpmId(sensorId, objectId).id;

        vector<int> sensorIDList = {sensorId};

        Document perceivedObject = getPerceivedObject(cpmObjectId, sensorIDList, deltaTimeMilliSecondSigned, confidence, x, y, xSpeed, ySpeed, xAcc, yAcc, classification, obj_heading);

        perceivedObjectsList.push_back(move(perceivedObject));

        auto finalTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // cout << "Time to process object: " << finalTime - initialTime << " microseconds" << endl;

    }

    if(temp_lowest_obj_timestamp != 18446744073709551615){
        lowest_obj_timestamp = temp_lowest_obj_timestamp;
    }

    return perceivedObjectsList;
}

Document segment(unsigned long int timestampIts, const Document& managementContainer ,const vector<Document>& perceivedObjectsList, Document& stationContainer, bool add_sensor_data, const vector<Document>& sensorArray, int totalSegments = 1, int segmentID = 0){
    Document cpm;
    cpm.SetObject();
    Document::AllocatorType& allocator = cpm.GetAllocator();

    // cpm.AddMember("generationDeltaTime", timestampIts, allocator);

    Document cpmParameters;
    cpmParameters.SetObject();

    //ManagementContainer
    Value managementContainerVal(kObjectType);
    managementContainerVal.CopyFrom(move(managementContainer), allocator);


    if (totalSegments > 1){
        Document objectContainerSegmentInfo = getObjectContainerSegmentInfo(totalSegments, segmentID);
        managementContainerVal.AddMember("perceivedObjectContainerSegmentInfo", objectContainerSegmentInfo, allocator);
    }

    cpm.AddMember("managementContainer", managementContainerVal, allocator);

    //cpmContainers (array)
    Value cpmContainers(kArrayType);
    //containerId 1 (stationDataContainer)
    Value containerId1(kObjectType);
    containerId1.CopyFrom(move(stationContainer), allocator);

    cpmContainers.PushBack(containerId1, allocator);

    //containerId 5 (perceivedObjectContainer)
    
    Value containerId5(kObjectType);
    containerId5.AddMember("containerId", 5, allocator);
    //containerData is an array of perceivedObjects
    Value containerData3(kObjectType);
    containerData3.AddMember("numberOfPerceivedObjects", perceivedObjectsList.size(), allocator);
    Value perceivedObjects(kArrayType);
    for (int i = 0; i < perceivedObjectsList.size(); i++){
        Value perceivedObjectVal(kObjectType);
        perceivedObjectVal.CopyFrom(move(perceivedObjectsList[i]), allocator);
        perceivedObjects.PushBack(perceivedObjectVal, allocator);
    }
    containerData3.AddMember("perceivedObjects", perceivedObjects, allocator);
    containerId5.AddMember("containerData", containerData3, allocator);

    cpmContainers.PushBack(containerId5, allocator);

    //containerId 3 (sensorDataContainer)
    if (true){
        Value containerId3(kObjectType);
        containerId3.AddMember("containerId", 3, allocator);

        //containerData is an array of sensors
        Value containerData3(kArrayType);
        for (int i = 0; i < sensorArray.size(); i++){
            Value sensorVal(kObjectType);
            sensorVal.CopyFrom(sensorArray[i], allocator);
            containerData3.PushBack(sensorVal, allocator);
        }
        containerId3.AddMember("containerData", containerData3, allocator);
        
        cpmContainers.PushBack(containerId3, allocator);
    }

    cpm.AddMember("cpmContainers", cpmContainers, allocator);

    // cpm.AddMember("cpmParameters", cpmParameters, allocator);

    return cpm;
}

vector<string> generateCPM(vector<Document>& receivedObjs, float cam_latitude, float cam_longitude, float cam_altitude, int cam_altitude_conf, float cam_heading, bool add_sensor_data, vector<Document>& sensorArray, int stationType, std::mutex& cpmMutex, int sequenceNumber){

    // std::lock_guard<std::mutex> lock(cpmMutex);

    vector<string> cpmList;

    Document cpm;

    int numObjs = receivedObjs.size();
    spdlog::debug("Number of objects: {}", numObjs);

    auto time_before_objs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    unsigned long int deltaTime1970 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    unsigned long int deltaTime = deltaTime1970 - time2004ms;


    
    vector<Document> perceivedObjectsList = getPerceivedObjectsList(deltaTime, receivedObjs, cam_latitude, cam_longitude);

    // if(lowest_obj_timestamp == 18446744073709551615){
    //     lowest_obj_timestamp = 0;
    // }

    // spdlog::debug("Lowest obj timestamp: {}", lowest_obj_timestamp);

    // //Timing measurements
    // lowest_obj_timestamp = deltaTime;

    // for (int i = 0; i < perceivedObjectsList.size(); i++){
    //     printValue(perceivedObjectsList[i]);
    // }

    auto time_after_objs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    spdlog::debug("Time to process objects: {} microseconds", time_after_objs - time_before_objs);

    Document managementContainer = getManagementContainer(deltaTime, cam_latitude, cam_longitude, cam_altitude, cam_altitude_conf, sequenceNumber);

    //TIMING DEBUG
    // perceivedObjectsList.clear();

    // lowest_obj_timestamp = 0;

    receivedObjs.clear();

   

    //StationContainer
    Document stationContainer;
    stationContainer.SetObject();
    Document::AllocatorType& allocator = stationContainer.GetAllocator();

    if(stationType == 5){
        spdlog::debug("stationType == 5");
        stationContainer.AddMember("containerId", 1, allocator);
        Value stationContainerData(kObjectType);
        Value orientationAngle(kObjectType);
        orientationAngle.AddMember("value", cam_heading, allocator);
        orientationAngle.AddMember("confidence", 1, allocator);
        stationContainerData.AddMember("orientationAngle", orientationAngle, allocator);
        stationContainer.AddMember("containerData", stationContainerData, allocator);

        // printValue(stationContainer);
    } else {
        stationContainer.AddMember("containerId", 2, allocator);
        Value stationContainerData(kObjectType);
        stationContainer.AddMember("containerData", stationContainerData, allocator);
    }
   


    int message_size = 0;

    if (add_sensor_data){
        message_size = getMessageEncodedSize(perceivedObjectsList.size(), sensorArray.size());
    } else {
        message_size = getMessageEncodedSize(perceivedObjectsList.size(), 0);
    }

    if (message_size > max_message_size){
        cout << "Message size too big, need to segment" << endl;
        int max_objs_per_segment = (max_message_size - header_manag_stat_size) / object_size;
        cout << "Max objects per segment: " << max_objs_per_segment << endl;
        vector<vector<Document>> segments;
        int size = perceivedObjectsList.size();
        cout << "Total objects: " << size << endl;
        for(int i = 0; i < size; i += max_objs_per_segment){
            int end = min(i + max_objs_per_segment, size);

            vector<Document> segmentObjs;

            for(int j = i; j < end; j++){
                Document obj;
                obj.CopyFrom(perceivedObjectsList[j], obj.GetAllocator());
                segmentObjs.push_back(move(obj));
            }
            
            segments.push_back(move(segmentObjs));
            cout << "Segment " << i / max_objs_per_segment + 1 << " has " << segments[i / max_objs_per_segment].size() << " objects" << endl;
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
            cout << "Segmenting " << i + 1 << " of " << segments.size() << endl;
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

        // spdlog::debug("Time to create CPM: {} microseconds", time_after_cpm - time_before_cpm);

        string cpmString = docToString(cpm);

        auto time_after_cpm_to_string = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // spdlog::debug("Time to convert CPM to string: {} microseconds", time_after_cpm_to_string - time_after_cpm);

        cpmList.push_back(cpmString);
    }

    // managementContainer.RemoveAllMembers();
    perceivedObjectsList.clear();
    // stationContainer.RemoveAllMembers();

    return cpmList;
}