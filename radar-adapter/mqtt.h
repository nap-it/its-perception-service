#ifndef RADAR_ADAPTER_MQTT_H
#define RADAR_ADAPTER_MQTT_H

#include <cstdlib>
#include <string>
#include <fstream>
#include <cctype>
#include <thread>
#include <chrono>
#include <unistd.h>
#include "mqtt/async_client.h"
#include "config_reader.h"
#include "radar_data_management.h"


struct mqtt_server {
    std::string address;         /* Server address: IP + Port */
    std::string client_id;
    std::string subscription_topic;
    std::string publish_topic;
    int qos;
    int	n_retry_attempts;
};


/* Read a configuration file (.ini) and returns a mqtt_server struct */
mqtt_server readConfigFile(std::string path);

/* Creates a forever loop */
void loop_forever();

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

    mqtt_server mqtt_server_info;
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

    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts, mqtt_server data)
            : n_retry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription"), mqtt_server_info(std::move(data)) {}
    //void publish(string topic, string payload);
};

#endif //RADAR_ADAPTER_MQTT_H
