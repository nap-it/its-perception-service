#include <map>
#include <chrono>
#include <boost/asio.hpp>
#include <thread>
#include <list>
#include "fastdds/dds.hpp"
#include "mqtt.h"
#include "config_reader.h"
#include "camera_data_management.h"

using namespace std;

// Global variables
map<int, cameraMqttObject> objects_to_send;      // shared between threads
map<int, cameraMqttObject> last_sent;            // save data of the last time the object was included in a CPM
map<int, string> serialized_objects_to_send;    // shared between threads
stringstream str_objects_to_send; 
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
    string client = reader.Get("mqtt", "client_id", "client") + "-camera";
    mqttInfo.client_id = client;
    mqttInfo.subscription_topic = reader.Get("mqtt", "camera_topic", "jetson/camera/tracking/objects");

    std::cout << "Subscription topic: " << mqttInfo.subscription_topic << std::endl;
    mqttInfo.qos= reader.GetInteger("mqtt", "qos", 1);
    mqttInfo.n_retry_attempts= reader.GetInteger("mqtt", "n_retry_attempts", 5);

    domain_id = reader.GetInteger("dds", "domain_id", 0);

    return mqttInfo;
}

void clean_last_sent(std::map<int, cameraMqttObject> * last_sent_dict, unsigned long int current_time) {
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
}

void on_message_mqtt(std::string topic, std::string message) {
//    std::cout << "Message: " << message << " RECEIVED." << std::endl;

    std::list<cameraMqttObject> camera_objects = parse_json(message);

    std::list<string> serialized_list = structs_to_string(camera_objects);

    for(int i = 0; i < serialized_list.size(); i++) {
        cameraMqttObject obj = camera_objects.front();
        bool is_newInfo = calc_is_new_info(&lock_mutex, &last_sent, obj);

        if (is_newInfo) {
            auto first = serialized_list.front();
            last_sent.insert_or_assign(obj.objectID, obj);

            try{
                dds_->publish("from/adapters", first);
                spdlog::info("Object with ID {} published", obj.objectID);
            } catch (std::exception& e) {
                spdlog::error("Error publishing to DDS: {}", e.what());
            }

        } 
        
        camera_objects.pop_front();
        serialized_list.pop_front();
    }

    unsigned long int now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time2004ms;
    clean_last_sent(&last_sent, now);
}

int main() {

    // Read config file
    mqtt_server mqttServerInfo = readConfigFile("/config.ini");

    // DDS
    cout << "Setting up DDS..." << endl;
    dds_ = new Dds("RadarAdapter", domain_id, on_message_dds);
    dds_->provision_publisher("from/adapters");
    // dds_->subscribe("to/adapters");
    //wait half a second for the subscriber to be ready
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cout << "Domain ID: " << domain_id << ", publishing to 'from/adapters'" << endl;
    cout << "DDS set up" << endl;
    
    cout << "Sending sensor data to DDS..."<< endl;

    // Send sensor data to DDS
    rapidjson::Document sensorInfo;
    sensorInfo.SetObject();

    rapidjson::Document::AllocatorType& allocator = sensorInfo.GetAllocator();

    sensorInfo.AddMember("sensorID", 2, allocator);
    sensorInfo.AddMember("sensorType", 12, allocator);
    sensorInfo.AddMember("shadowingApplies", false, allocator);

    rapidjson::Value perceptionRegionShapeObj(rapidjson::kObjectType);  
    perceptionRegionShapeObj.AddMember("semiMajorRangeLength", 0, allocator);
    perceptionRegionShapeObj.AddMember("semiMinorRangeLength", 0, allocator);
    perceptionRegionShapeObj.AddMember("semiMajorRangeOrientation", 0, allocator);
    perceptionRegionShapeObj.AddMember("range", 10, allocator);
    perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleStart", 3601, allocator);
    perceptionRegionShapeObj.AddMember("stationaryHorizontalOpeningAngleEnd", 3601, allocator);

    sensorInfo.AddMember("perceptionRegionShape", perceptionRegionShapeObj, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    sensorInfo.Accept(writer);

    dds_->publish("from/adapters", buffer.GetString());

    cout << "Sensor data sent to DDS" << endl;

    cout << "Setting up MQTT..." << endl;

    MqttWrapper * mqtt_wrapper = new MqttWrapper(mqttServerInfo, on_message_mqtt);

    // Wait for the connection to be established before further actions
    while (!mqtt_wrapper->is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    //Keep the main thread alive
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    /* Disconnect from the MQTT server */
    mqtt_wrapper->disconnect();


    return 0;
}
