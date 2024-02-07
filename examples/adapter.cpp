#include <map>
#include <iostream>
#include <cstring>
#include <string>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <thread>
#include <sys/ipc.h>
#include <sys/msg.h>

//json
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/prettywriter.h>

//DDS
#include "fastdds/dds.hpp"
#include "fastdds/MQTTMessagePubSubTypes.h"

using namespace std;
using namespace boost::asio;
using namespace rapidjson;


Dds* adapter;


const char* jsonObjectsStr = R"(
    [
        {
            "acceleration": 1.0,
            "heading": null,
            "latitude": null,
            "longitude": null,
            "objID": null,
            "sensorID": null,
            "speed": null,
            "timestamp": null,
            "confidence": null,
            "classification": {
                "confidence": 0,
                "class": {}
            }
        },
        {
            "acceleration": 2.0,
            "heading": null,
            "latitude": null,
            "longitude": null,
            "objID": null,
            "sensorID": null,
            "speed": null,
            "timestamp": null,
            "confidence": null,
            "classification": {
                "confidence": 0,
                "class": {}
            }
        }
    ])";
string jsonToString(const Document& d) {
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    d.Accept(writer);
    return buffer.GetString();
}

//send sensor data
void send_data(string pub_topic, const string& request){
    Document requestJson;
    requestJson.Parse(request.c_str());

    unsigned long int requestID = 0;
    int numberObjects = 0;
    if (requestJson.HasMember("requestID")){
        requestID = requestJson["requestID"].GetUint64();
    }
    if (requestJson.HasMember("numberObjects")){
        numberObjects = requestJson["numberObjects"].GetInt();
    }

    cout << "RequestID: " << requestID << endl;
    cout << "Number of objects: " << numberObjects << endl;
    
    Document replyJson = Document();
    replyJson.SetObject();
    Document::AllocatorType& allocator = replyJson.GetAllocator();
    replyJson.AddMember("requestID", requestID, allocator);
    replyJson.AddMember("numberObjects", numberObjects, allocator);
    Document jsonObjectsDoc = Document();
    jsonObjectsDoc.Parse(jsonObjectsStr);
    
    if (!jsonObjectsDoc.HasParseError() && jsonObjectsDoc.IsArray()) {
        // Properly deep copy the array into replyJson using CopyFrom
        Value objects(kArrayType);
        objects.CopyFrom(jsonObjectsDoc, allocator); // Deep copy the array
        replyJson.AddMember("objects", objects, allocator);
    } else {
        cout << "Failed to parse jsonObjectsStr or it's not an array." << endl;
    }

    string payload = jsonToString(replyJson);

    adapter->publish("from/adapters",payload);
}


void setup_dds(){
    // signal(SIGTERM, pub_sig_handler);
    adapter = new Dds("Adapter1", 0, send_data);
    adapter->provision_publisher("from/adapters");
    adapter->subscribe("to/adapters");
}

int main() {
    setup_dds();
    try {
        while(1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        raise(SIGTERM);
    }
}
