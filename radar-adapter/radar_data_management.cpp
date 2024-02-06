#include "radar_data_management.h"

radarMqttObject json_to_struct(std::string mqtt_radar_object) {
    // Parse the JSON string
    rapidjson::Document document;
    document.Parse(mqtt_radar_object.c_str());

    // Check if parsing succeeded
    if (!document.HasParseError() && document.IsObject()) {

        radarMqttObject obj;

        obj.acceleration = document["acceleration"].GetDouble();
        obj.classification = document["classification"].GetInt();
        obj.confidence = document["confidence"].GetDouble();
        obj.heading = document["heading"].GetDouble();
        obj.latitude = document["latitude"].GetDouble();
        obj.length = document["length"].GetDouble();
        obj.longitude = document["longitude"].GetDouble();
        obj.newInfo = document["newInfo"].GetBool();
        obj.objectID = document["objectID"].GetInt();
        obj.receiverID = document["receiverID"].GetInt();
        obj.speed = document["speed"].GetDouble();
        obj.timestamp = document["timestamp"].GetDouble();

        spdlog::info("radar mqtt obj: \"{}\"\n", obj.objectID);

        return obj;

    } else {
        std::cerr << "Failed to parse JSON" << std::endl;
    }
}
