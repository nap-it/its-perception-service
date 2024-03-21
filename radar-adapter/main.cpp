#include <map>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <thread>
#include "fastdds/dds.hpp"
#include "mqtt.h"
#include "config_reader.h"
#include "radar_data_management.h"

using namespace std;

// Global variables
map<int, radarMqttObject> objects_to_send;      // shared between threads
map<int, radarMqttObject> last_sent;            // save data of the last time the object was included in a CPM
map<int, string> serialized_objects_to_send;    // shared between threads
stringstream str_objects_to_send;                     // shared between threads
int n_objects_to_send = 0;                    // shared between threads
std::mutex lock_mutex;
const long int time2004ms = 1072915200000;

//DDS
Dds* dds_;
int domain_id = 0;


mqtt_server readConfigFile(const std::string& path)
{
    mqtt_server mqttInfo;

    INIReader reader (path);

    std::string host = reader.Get("mqtt", "host", "atcll-p35-jetson.nap.av.it.pt");
    std::cout << "Host: " << host << std::endl;
    long port = reader.GetInteger("mqtt", "port", 1883);

    mqttInfo.address = "tcp://" + host + ":" + std::to_string(port);
    std::cout << "Address: " << mqttInfo.address << std::endl;
    string client = reader.Get("mqtt", "client_id", "client") + "-radar";
    cout << "Client: " << client << endl;
    mqttInfo.client_id = client;
    mqttInfo.subscription_topic = reader.Get("mqtt", "radar_topic", "jetson/radar-plus");

    std::cout << "Subscription topic: " << mqttInfo.subscription_topic << std::endl;
    mqttInfo.qos= reader.GetInteger("mqtt", "qos", 1);
    mqttInfo.n_retry_attempts= reader.GetInteger("mqtt", "n_retry_attempts", 5);

    domain_id = reader.GetInteger("dds", "domain_id", 0);

    return mqttInfo;
}

void clean_last_sent(std::map<int, radarMqttObject> * last_sent_dict, unsigned long int current_time) {
    // cout << "Cleaning last_sent_dict..." << endl;
    for(auto it = last_sent_dict->begin(); it != last_sent_dict->end(); ) {
        // cout << "id: " << it->first << " timestamp: " << it->second.timestamp << endl;
        if (current_time - it->second.timestamp > 15000) {
            it = last_sent_dict->erase(it);
        } else {
            ++it;
        }
    }
}

//unused
void on_message_dds(std::string topic, std::string message) {
    std::cout << "Message: " << message << " RECEIVED FROM TOPIC" << topic << std::endl;
}

void on_message_mqtt(std::string topic, std::string message) {
    auto start_jsonToStruct = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    radarMqttObject obj = json_to_struct(message);
    auto end_jsonToStruct = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    //error in json_to_struct
    if (obj.objectID == -101) {
        return;
    }

    string serialized_obj = struct_to_string(obj);

    bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);

    if (is_newInfo) {
        last_sent.insert_or_assign(obj.objectID, obj);
        try{
            dds_->publish("from/adapters", serialized_obj);
            spdlog::info("Object with ID {} published", obj.objectID);
        } catch (std::exception& e) {
            spdlog::error("Error publishing to DDS: {}", e.what());
        }
        
    }
    unsigned long int now = static_cast<unsigned long int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time2004ms);
    clean_last_sent(&last_sent, now);
}

int main() {

     // Read config file
    mqtt_server mqttServerInfo = readConfigFile("/config.ini");

    // DDS
    cout << "Setting up DDS..." << endl;
    dds_ = new Dds("RadarAdapter", domain_id, on_message_dds);
    dds_->provision_publisher("from/adapters");

    //wait half a second for the publisher to be ready
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cout << "Domain ID: " << domain_id << ", publishing to 'from/adapters'" << endl;
    cout << "DDS set up" << endl;

    // Send sensor data to DDS
    cout << "Sending sensor data to DDS..."<< endl;

    rapidjson::Document sensorInfo;
    sensorInfo.SetObject();

    rapidjson::Document::AllocatorType& allocator = sensorInfo.GetAllocator();

    sensorInfo.AddMember("sensorID", 1, allocator);
    sensorInfo.AddMember("sensorType", 1, allocator);
    sensorInfo.AddMember("shadowingApplies", false, allocator);

    rapidjson::Value perceptionRegionShapeObj(rapidjson::kObjectType);  
    perceptionRegionShapeObj.AddMember("semiMajorRangeLength", 75, allocator);
    perceptionRegionShapeObj.AddMember("semiMinorRangeLength", 20, allocator);
    perceptionRegionShapeObj.AddMember("semiMajorRangeOrientation", 3601, allocator);
    perceptionRegionShapeObj.AddMember("range", 0, allocator);
    perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleStart", 0, allocator);
    perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleEnd", 0, allocator);

    sensorInfo.AddMember("perceptionRegionShape", perceptionRegionShapeObj, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    sensorInfo.Accept(writer);

    dds_->publish("from/adapters", buffer.GetString());

    cout << "Sensor data sent to DDS" << endl;

    // MQTT

    cout << "Setting up MQTT..." << endl;

    MqttWrapper * mqtt_wrapper = new MqttWrapper(mqttServerInfo, on_message_mqtt);

    // Wait for the connection to be established before further actions
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Keep the main thread alive
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    /* Disconnect from the MQTT server */
    mqtt_wrapper->disconnect();

    return 0;
}
