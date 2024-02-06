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
#include "fastdds/DDSSubscriber.hpp"
#include "fastdds/DDSPublisher.hpp"
#include "fastdds/MQTTMessagePubSubTypes.h"


//config
#include "config_reader.hpp"

using namespace std;
using namespace boost::asio;
using namespace rapidjson;

TypeSupport* typeSupport;
MQTTMessagePubSubType mqttMessagePubSubType;
DDSPublisher<MQTTMessage, MQTTMessagePubSubType>* publisher;
DDSSubscriber* subscriber;


//globals
int exptected_responses = 0;
int received_responses = 0;
std::chrono::milliseconds request_deadline(0);
std::chrono::milliseconds request_interval(0);
std::chrono::milliseconds add_sensor_interval(0); 
string sub_topic = "";
string pub_topic = "";
vector<string> received_objects;

void readConfigFile(const string& path){
    INIReader reader (path);
    sub_topic = reader.Get("dds", "topic_subscribe", "from/adapters");
    pub_topic = reader.Get("dds", "topic_publish", "to/adapters");

    request_deadline = std::chrono::milliseconds(reader.GetInteger("dds", "request_deadline", 1000));
    request_interval = std::chrono::milliseconds(reader.GetInteger("dds", "request_interval", 1000));
    add_sensor_interval = std::chrono::milliseconds(reader.GetInteger("dds", "add_sensor_interval", 1000));
}

string convertValueToString(const Value& value) {
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

void generateCpm(const vector<string>& objects){
    cout << "-------------- Generating CPM --------------" << endl;
    for (const auto& obj : objects) {
        cout << obj << endl;
    }
}

void handle_response(const string& response){
    Document doc;
    doc.Parse(response.c_str());

    //TODO: check if response is valid

    // Extract data
    if (doc.HasMember("objects") && doc["objects"].IsArray()) {
        const Value& objs = doc["objects"];
        for (auto& obj : objs.GetArray()) {
            // Proper way to deep copy using the CopyFrom method
            Value obj_copy(kObjectType);
            obj_copy.CopyFrom(obj, doc.GetAllocator());
            string obj_str = convertValueToString(obj_copy);    
            received_objects.push_back(obj_str); // Use std::move to avoid unnecessary copying
        }
    }
    received_responses++;

    if (received_responses == exptected_responses) {
        cout << "-------------- Received all responses --------------" << endl;
        generateCpm(received_objects);
        received_objects.clear();
        received_responses = 0;
    }
}


class SubListener : public DataReaderListener {
public:
    SubListener() {
    }

    ~SubListener() override {}

    void on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& info) override {
        if (info.current_count_change == 1) {
            num_publishers = info.total_count;
            std::cout << "Subscriber matched." << std::endl;
        } else if (info.current_count_change == -1) {
            num_publishers = info.total_count;
            std::cout << "Subscriber unmatched." << std::endl;
        } else {
            std::cout << info.current_count_change
                << " is not a valid value for SubscriptionMatchedStatus current count change" << std::endl;
        }
    }

    void on_data_available(DataReader* reader) override {
        SampleInfo info;
        if (reader->take_next_sample(&message, &info) == ReturnCode_t::RETCODE_OK) {
            if (info.valid_data) {
                handle_response(message.message());
            }
        }
    }
    MQTTMessage message;
    std::atomic_int num_publishers;
};
SubListener* listener_;

void pub_sig_handler(int sig) {
    delete publisher;
}

void sub_sig_handler(int sig) {
    delete subscriber;
}

void request_data(){
    exptected_responses = publisher->get_subscribers();
    std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now();
    string timestamp_str = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch()).count());
    string message = "{timestamp: " + timestamp_str + "}";
    MQTTMessage* mqttMessage = new MQTTMessage();
    mqttMessage->uuid(1);
    mqttMessage->topic(pub_topic);
    mqttMessage->message(message);
    mqttMessage->datetime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    publisher->publish(mqttMessage);
    cout << "Sent request for " << exptected_responses << " adapters" << endl;
}

void setup_pub_dds(){
    signal(SIGTERM, pub_sig_handler);
    typeSupport = new TypeSupport(&mqttMessagePubSubType);
    publisher = new DDSPublisher<MQTTMessage, MQTTMessagePubSubType>(typeSupport);
    publisher->init("ServerPub", 0, pub_topic, "MQTTMessage", TOPIC_QOS_DEFAULT);
}

void setup_sub_dds() {
    signal(SIGTERM, sub_sig_handler);
    listener_ = new SubListener();
    typeSupport = new TypeSupport(&mqttMessagePubSubType);
    subscriber = new DDSSubscriber(listener_, typeSupport);
    subscriber->init("ServerSub", 0, sub_topic, "MQTTMessage", TOPIC_QOS_DEFAULT);
}

int main() {
    readConfigFile("config.ini");
    setup_pub_dds();
    setup_sub_dds();
    while(1) {
        try {
            request_data();
        } catch(...) {
            raise(SIGTERM);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(request_interval));
    }
}
