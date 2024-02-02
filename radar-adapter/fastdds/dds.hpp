#ifndef DDS_HPP_PSIGPUTG
#define DDS_HPP_PSIGPUTG

#include <iostream>
#include <cstring>
#include <string>
#include <chrono>
#include <vector>
#include <thread>

#include "fastdds/DDSSubscriber.hpp"
#include "fastdds/DDSPublisher.hpp"

#include "fastdds/MQTTMessagePubSubTypes.h"

using namespace std;

class Application;

class Dds {
private:
    string client_name;
    int domain;
    void from_dds_thread();
    std::function<void(std::string, std::string)> on_message_function;

public:
    Dds(string client_name, int domain, function<void(std::string, std::string)> func);
    ~Dds();
    void publish(string topic, string message);
    void provison_publisher(string topic);
    void subscribe(string topic);
    void on_message(string topic, string message);
};

#endif /* DDS_HPP_PSIGPUTG */

