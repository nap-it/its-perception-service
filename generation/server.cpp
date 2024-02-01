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

//DDS
#include "fastdds/DDSSubscriber.hpp"
#include "fastdds/DDSPublisher.hpp"
#include "fastdds/MQTTMessagePubSubTypes.h"

using namespace std;
using namespace boost::asio;

TypeSupport* typeSupport;
MQTTMessagePubSubType mqttMessagePubSubType;
DDSPublisher<MQTTMessage, MQTTMessagePubSubType>* publisher;
DDSSubscriber* subscriber;

//constants
std::chrono::milliseconds request_period(1000);

//globals
int exptected_responses = 0;
int received_responses = 0;

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
                std::cout << "Reponse: " << message.message() << " RECEIVED." << std::endl;
                //TODO: check if message is valid
                if(true){
                    received_responses++;
                    if(received_responses == exptected_responses){
                        cout << "Received all responses" << endl;
                        received_responses = 0;
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

void request_data(){
    exptected_responses = publisher->get_subscribers();
    MQTTMessage* mqttMessage = new MQTTMessage();
    mqttMessage->uuid(1);
    mqttMessage->topic("out/adapters");
    mqttMessage->message("request");
    mqttMessage->datetime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    publisher->publish(mqttMessage);
    cout << "Sent request for " << publisher->get_subscribers() << " adapters" << endl;
}

void setup_pub_dds(){
    signal(SIGTERM, pub_sig_handler);
    typeSupport = new TypeSupport(&mqttMessagePubSubType);
    publisher = new DDSPublisher<MQTTMessage, MQTTMessagePubSubType>(typeSupport);
    publisher->init("ServerPub", 0, "out/adapters", "MQTTMessage", TOPIC_QOS_DEFAULT);
}

void setup_sub_dds(string sub_topic) {
    signal(SIGTERM, sub_sig_handler);
    listener_ = new SubListener();
    typeSupport = new TypeSupport(&mqttMessagePubSubType);
    subscriber = new DDSSubscriber(listener_, typeSupport);
    subscriber->init("ServerSub", 0, "in/adapters", "MQTTMessage", TOPIC_QOS_DEFAULT);
}

int main() {
    setup_pub_dds();
    setup_sub_dds("in/adapters");
    while(1) {
        try {
            request_data();
        } catch(...) {
            raise(SIGTERM);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(request_period));
    }
}
