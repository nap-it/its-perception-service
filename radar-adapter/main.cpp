#include <map>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <thread>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "mqtt/async_client.h"
#include "fastdds/dds.hpp"
#include "mqtt.h"

using namespace std;


void on_message(std::string topic, std::string message) {
    std::cout << "Message: " << message << " RECEIVED." << std::endl;
    if (topic == "to/adapters") {
        std::cout << "Received request for adapters" << std::endl;
    }
}

int main() {
    // Read config file
    mqtt_server mqttServerInfo = readConfigFile("/config.ini");

    //Connect to MQTT broker
    spdlog::info("Connecting to MQTT server \"{}\"\n", mqttServerInfo.address);
    mqtt::async_client cli(mqttServerInfo.address, mqttServerInfo.client_id);

    /* Callback creation */
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(false);
    connOpts.set_automatic_reconnect(true);

    callback cb(cli, connOpts, mqttServerInfo);
    cli.set_callback(cb);

    /* Start the connection */
    try {
        spdlog::info("Connecting to the MQTT server...");
        cli.connect(connOpts, nullptr, cb)->wait();

    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Unable to connect to MQTT server: {}", mqttServerInfo.address);
        spdlog::error(exc.to_string());
        return -1;
    }

    /* Stars a loop forerver thread */
    thread thread_loop_mqtt(loop_forever);

    // DDS
    Dds* dds = new Dds("RadarAdapter", 0, on_message);
    dds->subscribe("to/adapters");

    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    /* Don't close the main process while thread is still running */
    thread_loop_mqtt.join();

    /* Disconnect from the MQTT server */
    try {
        spdlog::info("Disconnecting from the MQTT server...");
        cli.disconnect()->wait();
        spdlog::info("Disconnected");
    }
    catch (const mqtt::exception& exc) {
        spdlog::error("Unable to disconnect from MQTT server: {}", mqttServerInfo.address);
        spdlog::error(exc.to_string());
        return -1;
    }

    return 0;
}
