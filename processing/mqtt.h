#ifndef MQTT_MQTT_H
#define MQTT_MQTT_H

#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <spdlog/spdlog.h>
#include <cctype>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <sstream>
#include "mqtt/async_client.h"
#include "config_reader.hpp"
#include "rapidjson/document.h"

using namespace rapidjson;
//using json = nlohmann::json;
using namespace std;

struct server{
    string address;         /* Server address: IP + Port */
    string client_id;
    vector<string> subscription_topic;
    string publish_topic;
    int qos;
    int n_retry_attempts;
};


/* Read a configuration file (.ini) and returns a server struct */
server readConfigFile(const string& path);

/* Declaration of global publish_processedCpm function variable */
void mqttSetPublishProcessedCpm(const std::function<void(string)>& f);

/* Creates a forever loop */
[[noreturn]] void loop_forever ();

class action_listener : public virtual mqtt::iaction_listener
{
    std::string name_;

    void on_failure(const mqtt::token& tok) override;

    void on_success(const mqtt::token& tok) override;

public:
    explicit action_listener(std::string name) : name_(std::move(name)) {}
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener {
    // Counter for the number of connection retries

    server data_server;
    int n_retry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;
    // An action listener to display the result of actions.
    action_listener subListener_;

    // This demonstrates manually reconnecting to the broker by calling
    // connect() again. This is a possibility for an application that keeps
    // a copy of its original connect_options, or if the app wants to
    // reconnect with different options.
    // Another way this can be done manually, if using the same options, is
    // to just call the async_client::reconnect() method.
    void reconnect() ;
    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token& tok) override;

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override ;
    // (Re)connection success
    void connected(const std::string& cause) override ;

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override ;
    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override ;

    void delivery_complete(mqtt::delivery_token_ptr token) override;



public:
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts)
            : n_retry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription") {}
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts, server data)
            : n_retry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription"), data_server(std::move(data)) {}
    //void publish(string topic, string payload);
};


#endif //MQTT_MQTT_H
