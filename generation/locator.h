#ifndef LOCATOR_H
#define LOCATOR_H

#include <string>
#include <mutex>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "mqttwrapper.h"
#include "fastdds-cpp-wrapper/dds.hpp"

namespace rj = rapidjson;

enum class ProviderType {
    STATIC,
    MQTT,
    DDS
};

class Locator {
public:
    /**
     * @brief Construct a new Locator object.
     *
     * @param provider The provider type: STATIC, MQTT, or DDS.
     * @param configLatitude Constant latitude from configuration (empty if not provided).
     * @param configLongitude Constant longitude from configuration (empty if not provided).
     * @param configStationType Station type from configuration (0 if not provided).
     * @param mqttBroker MQTT broker address if provider is MQTT.
     * @param mqttTopic MQTT topic to subscribe for CAMs if provider is MQTT.
     * @param ddsDomain DDS domain ID if provider is DDS.
     * @param ddsTopic DDS topic to subscribe for CAMs if provider is DDS.
     * @param debug Debug level (0 = off, 1 = on).
     */
    Locator(ProviderType provider,
                   float configLatitude,
                   float configLongitude,
                   int configStationType,
                   std::string mqttBroker,
                   std::string mqttTopic, 
                   int ddsDomain,
                   std::string ddsTopic,
                   bool debug);
    
    ~Locator();

    void run();

    // Getters for location and station type.
    double getStationLatitude();
    double getStationLongitude();
    float getStationHeading();
    int getStationType();

private:

    std::thread locatorThread_;

    // Provider type chosen at construction.
    ProviderType provider_;

    // Station type
    int stationType_;

    // Latest station dynamic location values (updated by client callbacks).
    double latestLatitude_;
    double latestLongitude_;
    float latestHeading_;

    // Mutex to protect dynamic updates.
    std::mutex mtx_;

    // MQTT client 
    MqttWrapper* mqttClient_;

    // MQTT topic to subscribe to for CAM location messages.
    std::string mqttTopic_;

    // DDS client
    Dds* dds_;

    // DDS topic to subscribe to for CAM location messages.
    std::string ddsTopic_;

    // Internal callback for MQTT messages.
    void on_message_mqtt(const std::string& topic, const std::string& message);

    // Internal callback for DDS messages.
    void on_message_dds(const std::string& topic, const std::string& message);

    // Helper to parse CAM messages and update location.
    void parseAndUpdateLocation(const std::string& topic, const std::string& message);

    void runLoop();
};

#endif // LOCATOR_H