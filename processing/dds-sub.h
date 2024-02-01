// dds-sub.h

#ifndef DDS_SUB_H
#define DDS_SUB_H

#include <iostream>
#include <map>
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
using namespace eprosima::fastdds::dds;

void ddsSetPublishProcessedCpm(const std::function<void(string)>& f);

class SubListener : public DataReaderListener {
public:
    SubListener();
    ~SubListener() override;

    void on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus& info) override;
    void on_data_available(DataReader* reader) override;

private:
    int i;
    MQTTMessage message;
};

#endif // DDS_SUB_H
