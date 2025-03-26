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

constexpr float NOT_PRESENT_FLOAT = -999.0f;
constexpr double NOT_PRESENT_DOUBLE = -999.0;
constexpr int NOT_PRESENT_INT = -999;

enum class ProviderType {
    STATIC,
    MQTT,
    DDS
};

struct SenderInfo {
    int station_id;
    float speed;        
    float heading;      
    float altitude;     
    float acceleration; 
    std::chrono::steady_clock::time_point cam_timestamp;
};

class Locator {
public:
    /**
     * @brief Construct a new Locator object.
     *
     * @param provider The provider type: STATIC, MQTT, or DDS.
     * @param configStationId Station id from configuration (when processing own messages)
     * @param mqttBroker MQTT broker address if provider is MQTT.
     * @param mqttTopic MQTT topic to subscribe for CAMs if provider is MQTT.
     * @param ddsDomain DDS domain ID if provider is DDS.
     * @param ddsTopic DDS topic to subscribe for CAMs if provider is DDS.
     */
    Locator(ProviderType provider,
                   int configStationId,
                   std::string mqttBroker,
                   std::string mqttTopic, 
                   int ddsDomain,
                   std::string ddsTopic);
    
    ~Locator();

    void run();

    // Getter for station data.
    SenderInfo getStationData(int stationId);
private:

    std::unordered_map<int, SenderInfo> camDataMap_;
    std::mutex camMtx_;

    // Station ID for own messages.
    int station_id_;

    std::thread locatorThread_;

    // Provider type chosen at construction.
    ProviderType provider_;

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
    void parseAndUpdateData(const std::string& topic, const std::string& message);

    // Cleanup CAM data map.
    void cleanupDataMap();

    void runLoop();
};

#endif // LOCATOR_H