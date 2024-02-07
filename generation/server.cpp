#include <map>
#include <iostream>
#include <cstring>
#include <string>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <thread>
#include <signal.h>
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


//config
#include "config_reader.hpp"

//sensor
#include "sensor_info.hpp"

using namespace std;
using namespace boost::asio;
using namespace rapidjson;

//DDS variables
Dds* server;


//globals
int exptected_responses = 0;
int received_responses = 0;
std::chrono::milliseconds request_deadline(0);
std::chrono::milliseconds request_interval(0);
std::chrono::milliseconds add_sensor_interval(0); 
std::chrono::milliseconds last_request(0);
std::chrono::milliseconds last_sensor(0);
std::chrono::milliseconds max_interval(0);
string sub_topic = "";
string pub_topic = "";
unsigned long int timestamp_milliseconds = 0;

vector<Document> received_objects = vector<Document>();

void readConfigFile(const string& path){
    INIReader reader (path);
    sub_topic = reader.Get("dds", "topic_subscribe", "from/adapters");
    pub_topic = reader.Get("dds", "topic_publish", "to/adapters");

    request_deadline = std::chrono::milliseconds(reader.GetInteger("dds", "request_deadline", 1000));
    request_interval = std::chrono::milliseconds(reader.GetInteger("dds", "request_interval", 1000));
    add_sensor_interval = std::chrono::milliseconds(reader.GetInteger("dds", "add_sensor_interval", 1000));
    max_interval = std::chrono::milliseconds(reader.GetInteger("dds", "max_interval", 1000));
}

string printJsonValue(const Value& value) {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

void printJsonVector(const vector<Document>& objects){
    cout << "-------------- Printing Vector<Document> --------------" << endl;
    for (const auto& obj : objects) {
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        obj.Accept(writer);
        cout << buffer.GetString() << endl;   
    }
}

void handle_response(string topic, const string& response){
    Document doc;
    doc.Parse(response.c_str());
    cout << "New response: " << response << endl;

    //TODO: check if response is valid
    
    if (doc.HasMember("requestID") && doc["requestID"].IsUint64()) {
        unsigned long int requestID = doc["requestID"].GetUint64();
        if (requestID != timestamp_milliseconds) {
            cout << "Invalid requestID" << endl;
            return;
        }
    } else {
        cout << "Invalid requestID" << endl;
        return;
    }

    // Extract data
    if (doc.HasMember("objects") && doc["objects"].IsArray()) {
        const Value& objs = doc["objects"];

        for (auto& obj : objs.GetArray()) {
            // Proper way to deep copy using the CopyFrom method    
            Document copyDoc;
            copyDoc.CopyFrom(obj, copyDoc.GetAllocator());
            received_objects.push_back(move(copyDoc));
        }
    } else {
        cout << "No objects on response" << endl;
    }
    received_responses++;
}


void request_data(){
    exptected_responses = 2;
    std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
    timestamp_milliseconds = static_cast<unsigned long int>(milliseconds);

    // Create request
    Document request;
    request.SetObject();
    Document::AllocatorType& allocator = request.GetAllocator();
    request.AddMember("requestID", timestamp_milliseconds, allocator);
    request.AddMember("numberObjects", 1, allocator);
    
    string payload = printJsonValue(request);
    
    server->publish(pub_topic, payload);
    cout << "Sent request for " << exptected_responses << " adapters" << endl;
}


void setup_dds(){
    // signal(SIGTERM, pub_sig_handler);
    server = new Dds("Server", 0, handle_response);
    server->provision_publisher(pub_topic);
    server->subscribe(sub_topic);
}


int main() {
    readConfigFile("config.ini");
    setup_dds();
    last_request = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - request_interval;
    last_sensor = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - add_sensor_interval;
    while(1) {
        try {
            auto current_request = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
            if (current_request - last_request > request_interval) {
                last_request = current_request;
                request_data();
                received_responses = 0;
                while(received_responses < exptected_responses) {
                    if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - current_request > request_deadline) {
                        cout << "Request deadline reached" << endl;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (received_responses == exptected_responses) {
                    cout << "Received all responses" << endl;
                } else {
                    cout << "Received " << received_responses << " responses" << endl;
                }

                if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - last_sensor > add_sensor_interval) {
                    last_sensor = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                    cout << "Adding sensor" << endl;
                    Document sensor_data = getSensorInformationContainer();
                } 

                printJsonVector(received_objects);
                received_objects.clear();

            }

        } catch(...) {
            raise(SIGTERM);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
