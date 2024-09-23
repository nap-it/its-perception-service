#include "dds.hpp"
#include <map>
#include <signal.h>

class SubListener : public DataReaderListener {
public:
    JSONMessage message;
    Dds* dds;

    SubListener(Dds* dds_) : dds(dds_) {
    }

    ~SubListener() override {}

    void on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& info) override {
        if (info.current_count_change == 1) {
            std::cout << "Subscriber matched." << std::endl;
        } else if (info.current_count_change == -1) {
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
                dds->on_message(reader->get_topicdescription()->get_name(), message.message());
//                std::cout << "Message: " << message.message() << " RECEIVED." << std::endl;
            }
        }
    }
};

SubListener* listener_;
TypeSupport* typeSupport;
JSONMessagePubSubType jsonMessagePubSubType;
std::map<std::string, std::unique_ptr<DDSPublisher<JSONMessage, JSONMessagePubSubType>>> dds_publishers;
std::map<std::string, std::unique_ptr<DDSSubscriber>> dds_subscribers;

void sig_handler(int sig) {
    dds_publishers.clear();
    dds_subscribers.clear();
}

Dds::Dds(std::string client_name, int domain, function<void(std::string, std::string)> func) {
    signal(SIGTERM, sig_handler);
    this->client_name = client_name;
    this->domain = domain;
    this->on_message_function = func;
    listener_ = new SubListener(this);
    typeSupport = new TypeSupport(&jsonMessagePubSubType);
    std::thread dds_th(&Dds::from_dds_thread, this);
    dds_th.detach();
}

Dds::Dds(std::string client_name, int domain, function<void(std::string, std::string)> func, std::string msg_type) {
    signal(SIGTERM, sig_handler);
    this->client_name = client_name;
    this->domain = domain;
    this->on_message_function = func;
    this->message_type = msg_type;
    listener_ = new SubListener(this);
    typeSupport = new TypeSupport(&jsonMessagePubSubType);
    std::thread dds_th(&Dds::from_dds_thread, this);
    dds_th.detach();
}

Dds::~Dds() {

}

void Dds::publish(std::string topic, std::string message) {
    std::unique_ptr<JSONMessage> jsonMessage = std::make_unique<JSONMessage>();
    jsonMessage->uuid(1);
    jsonMessage->topic(topic);
    jsonMessage->message(message);
    jsonMessage->datetime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    dds_publishers[topic].get()->publish(std::move(jsonMessage));
}

void Dds::subscribe(std::string topic) {
    std::unique_ptr<DDSSubscriber> ptr = \
        std::make_unique<DDSSubscriber>(listener_, typeSupport);
    ptr.get()->init(this->client_name, this->domain, topic, "JSONMessage", TOPIC_QOS_DEFAULT);
    dds_subscribers[topic] = std::move(ptr);
}

void Dds::provision_publisher(std::string topic) {
    std::unique_ptr<DDSPublisher<JSONMessage, JSONMessagePubSubType>> ptr = \
        std::make_unique<DDSPublisher<JSONMessage, JSONMessagePubSubType>>(typeSupport);
    ptr.get()->init(this->client_name, this->domain, topic, "JSONMessage", TOPIC_QOS_DEFAULT);
    dds_publishers[topic] = std::move(ptr);
}

void Dds::from_dds_thread() {
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(100));
    }
}

void Dds::on_message(std::string topic, std::string message) {
    on_message_function(topic, message);
}
