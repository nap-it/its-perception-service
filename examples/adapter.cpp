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

#include "fastdds/DDSSubscriber.hpp"
#include "fastdds/DDSPublisher.hpp"
#include "fastdds/MQTTMessagePubSubTypes.h"

using namespace std;
using namespace boost::asio;

DDSPublisher<MQTTMessage, MQTTMessagePubSubType>* publisher;
TypeSupport* typeSupport;
MQTTMessagePubSubType mqttMessagePubSubType;
DDSSubscriber* subscriber;


//send sensor data
void send_data(string pub_topic, string payload){
    MQTTMessage* mqttMessage = new MQTTMessage();
    mqttMessage->uuid(1);
    mqttMessage->topic(pub_topic);
    mqttMessage->message(payload);
    mqttMessage->datetime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    publisher->publish(mqttMessage);
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
                std::cout << "Message: " << message.message() << " RECEIVED." << std::endl;
                if (message.topic() == "out/adapters") {
                    cout << "Received request for adapters" << endl;
                    try {
                        send_data("in/adapters", "adapter sensor data");
                    } catch (const std::exception& e) {
                        std::cerr << e.what() << '\n';
                        raise(SIGTERM);
                    }
                }
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

//setup publisher to send sensor data
void setup_pub_dds(string pub_topic){
    signal(SIGTERM, pub_sig_handler);
    typeSupport = new TypeSupport(&mqttMessagePubSubType);
    publisher = new DDSPublisher<MQTTMessage, MQTTMessagePubSubType>(typeSupport);
    publisher->init("AdapterPub", 0, pub_topic, "MQTTMessage", TOPIC_QOS_DEFAULT);

}

//setup subscriber to receive cps requests
void setup_sub_dds(string sub_topic) {
    signal(SIGTERM, sub_sig_handler);
    listener_ = new SubListener();
    typeSupport = new TypeSupport(&mqttMessagePubSubType);
    subscriber = new DDSSubscriber(listener_, typeSupport);
    subscriber->init("AdapterSub", 0, sub_topic, "MQTTMessage", TOPIC_QOS_DEFAULT);
}


int main() {
    setup_pub_dds("in/adapters");
    setup_sub_dds("out/adapters");
    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
