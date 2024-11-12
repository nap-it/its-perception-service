
#include <functional>
#include <memory>


#include <chrono>
#include <cmath>
#include <numbers>
#include <boost/asio.hpp>
#include <thread>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/prettywriter.h>
#include <spdlog/spdlog.h>
#include <tuple>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <cstring>
#include <string>
#include <algorithm> // For std::remove_if
#include "autoware_data_management.h"

//DDS
#include "fastdds-cpp-wrapper/dds.hpp"

//config
#include "config_reader.hpp"

//config variables
int domain_id = 227;
int debug_level = 0;
bool save_time_logs = false;

using std::placeholders::_1;
using namespace std;
using namespace rapidjson;

//DDS
Dds* dds_;

//CAM Variables
unsigned long int timestamp = 0;
double latitude = 0;
double longitude = 0;
double altitude = 0;
double heading = 0;
double heading_rate = 0;
// double yaw_rate;
double linear_acceleration = 0;
double speedVal = 0;
// double length;
// double width;

// //MQTT variables
// MqttWrapper* mqtt_server;
// data_mqtt_server data_mqtt;

// Global variables
map<int, autowareObject> objects_to_send;      // shared between threads
map<int, autowareObject> last_sent;            // save data of the last time the object was included in a CPM
map<int, string> serialized_objects_to_send;    // shared between threads
stringstream str_objects_to_send; 
int n_objects_to_send = 0;                    // shared between threads
std::mutex lock_mutex;

//TIME LOGS
bool write_logs = false;
int sequenceNumber = 0;
long request_time = 0;
long publish_time = 0;
map<unsigned long, long> request_map;
map<unsigned long, tuple<long,int>> reply_map;

using std::placeholders::_1;
using namespace std;

void writeTupleMapToFile(const std::string& filePath, const std::map<unsigned long , tuple<long,int>>& timeMap) {
        std::ofstream outFile(filePath, std::ios::app);
        if (!outFile) {
            std::cerr << "Error: Could not open the file at " << filePath << std::endl;
            return;
        }
        for (const auto& pair : timeMap) {
            outFile << pair.first << ":" << std::get<0>(pair.second) << ":" << std::get<1>(pair.second) << std::endl;
        }
        outFile.close();
    }

void writeMapToFile(const std::string& filePath, const std::map<unsigned long , long>& timeMap) {
    std::ofstream outFile(filePath, std::ios::app);
    if (!outFile) {
        std::cerr << "Error: Could not open the file at " << filePath << std::endl;
        return;
    }
    for (const auto& pair : timeMap) {
        outFile << pair.first << ":" << pair.second << std::endl;
    }
    outFile.close();
}

void on_message_dds(std::string topic, std::string message) {

    request_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    

    if (topic == "to/adapters") {

        rapidjson::Document requestJson;
        requestJson.Parse(message.c_str());

        unsigned long int requestID = 0;
        int numberObjects = 0;
        if (requestJson.HasMember("requestID") && requestJson["requestID"].IsUint64()){
            requestID = requestJson["requestID"].GetUint64();
        } else {
            spdlog::error("requestID is not an unsigned long int or does not exist");
            return;
        }
        if (requestJson.HasMember("numberObjects") && requestJson["numberObjects"].IsInt()){
            numberObjects = requestJson["numberObjects"].GetInt();
            // spdlog::info("Received request_iteration for {} objects", numberObjects);
        } else {
            spdlog::error("numberObjects is not an int or does not exist");
            return;
        }

        auto start_getReply = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        request_map[requestID] = request_time;
        sequenceNumber++;
        auto t = time(nullptr);
        auto tm = *localtime(&t);
        ostringstream oss;
        oss << put_time(&tm, "%d-%m-%Y%H-%M");
        auto str = oss.str();
        if((sequenceNumber%500) == 0) {
            if(save_time_logs) {
                writeMapToFile("/times/times_4_"+str+".txt", request_map);
            }
            request_map.clear();
        }


        string reply2 = get_reply(message, &lock_mutex, &serialized_objects_to_send, requestID);

        //remove whitespaces
        reply2.erase(std::remove_if(reply2.begin(), reply2.end(), ::isspace), reply2.end());

        if(reply2.empty()) {
            return;
        }

        // auto start_publish = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        publish_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        dds_->publish("from/adapters", reply2);

        reply_map[requestID] = make_tuple(publish_time, serialized_objects_to_send.size());

        if((sequenceNumber%500) == 0) {
            if(save_time_logs) {
                writeTupleMapToFile("/times/times_5_"+str+".txt", reply_map);
            }
            request_map.clear();
        }

        // spdlog::info("publish {} objects: {} microseconds", n_objects_to_send, end_publish - start_publish);
        spdlog::debug("[{}] Published message: {}", numberObjects, reply2);

        //update last_sent
        std::lock_guard<std::mutex> guard(lock_mutex);
        for(auto const& [key, value] : objects_to_send) {
            last_sent.insert_or_assign(key, value);
        }
        
        // clear "objects_to_send"
        objects_to_send.clear();
        serialized_objects_to_send.clear();
        n_objects_to_send = 0;

    } else if (topic == "aw/out/perceived_objects") {
        
        Document d;
        d.Parse(message.c_str());


        //parse the message
        std::list<autowareObject> aw_objects = parse_msg(d, save_time_logs);

        //convert to string
        std::list<std::string> serialized_list = structs_to_string(aw_objects);

        spdlog::debug("Received {}/{} objects from VPI", aw_objects.size(), serialized_list.size());
        int n_objects = aw_objects.size();

        for(int i = 0; i < n_objects; i++) {
            autowareObject obj = aw_objects.front();

            if(objects_to_send.find(obj.objectID) != objects_to_send.end()) {
            // spdlog::warn("Object {} already in objects_to_send, updating info...", obj.objectID);
            std::lock_guard<std::mutex> guard(lock_mutex);
            objects_to_send[obj.objectID] = obj;
            serialized_objects_to_send[obj.objectID] = serialized_list.front();
            aw_objects.pop_front();
            serialized_list.pop_front();
            continue;

            }

            // spdlog::warn("Checking if object {} is new...", obj.objectID);
            bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);

            //DEBUG PURPOSES
            is_newInfo = true;

            if (is_newInfo) {
                // save to objects to send objects
                // spdlog::warn("Object {} is new, adding to objects_to_send", obj.objectID);
                std::lock_guard<std::mutex> guard(lock_mutex);
                objects_to_send[obj.objectID] = obj;

                auto first = serialized_list.front();
                serialized_objects_to_send[obj.objectID] = first;

                n_objects_to_send++;

            } else {
                // spdlog::warn("Object {} is not new, not adding to objects_to_send", obj.objectID);
            }

            aw_objects.pop_front();
            serialized_list.pop_front();
        }
    }
}

void readConfigFile(const string& path){
    INIReader reader (path);

    if (reader.ParseError() < 0) {
        spdlog::error("Can't load config file");
        return;
    }

    domain_id = reader.GetInteger("aw-cpm-adapter", "dds_domain_id", 227);
    cout << "[Config] Fast-DDS Wrapper Domain ID: " << domain_id << endl;
    debug_level = reader.GetInteger("aw-cpm-adapter", "debug", 1);
    cout << "[Config] Debug Level: " << debug_level << endl;
    save_time_logs = reader.GetBoolean("aw-cpm-adapter", "save_time_logs", false);
    cout << "[Config] Save Time Logs: " << save_time_logs << endl;
    
}


int main(int argc, char * argv[]) {

    cout << "Starting AW-CPM-Adapter..." << endl;
    readConfigFile("/cpm_adapter/config.ini");
    if(debug_level){
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    cout << "Setting up DDS..." << endl;
    dds_ = new Dds("AwCpmAdapter", domain_id, on_message_dds);
    dds_->provision_publisher("from/adapters");
    dds_->subscribe("to/adapters");
    dds_->subscribe("aw/out/perceived_objects");

    cout << "DDS set up" << endl;

    while (true) {

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return 0;
}
