#include "generation.h"
#include <chrono>
#include <spdlog/spdlog.h>

Generation::Generation(std::shared_ptr<Aggregator> aggregator, std::shared_ptr<Locator> locator, int requestRateMs, bool debug, int ddsDomain, std::string ddsTopic)
    : aggregator_(aggregator), locator_(locator), requestRateMs_(requestRateMs), stopFlag_(false), ddsTopic_(ddsTopic) {

    if (debug) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("[Generation] Debug logging enabled.");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("[Generation] Info logging enabled.");
    }
    
    spdlog::info("[Generation] initialized with request rate {} ms", requestRateMs);

    dds_ = new Dds("Generation", ddsDomain, nullptr);
    dds_->provision_publisher(ddsTopic);
    spdlog::info("[Generation] DDS client initialized on domain {} and topic {}", ddsDomain, ddsTopic);
}

Generation::~Generation() {
    stop();
    delete dds_;
}

void Generation::run() {
    stopFlag_ = false;
    generationThread_ = std::thread(&Generation::runLoop, this);
    spdlog::info("[Generation] run loop started.");
}

void Generation::stop() {
    stopFlag_ = true;
    if (generationThread_.joinable()) {
        generationThread_.join();
    }
    spdlog::info("[Generation] run loop stopped.");
}

void Generation::setRequestRate(int newRateMs) {
    requestRateMs_ = newRateMs;
    spdlog::info("[Generation] request rate updated to {} ms", newRateMs);
}

void Generation::runLoop() {
    auto last_sensor_ts = std::chrono::system_clock::now();

    std::unordered_map<int, SensorInfo> sensorInfo;
    bool addSensor = false;
    vector<Object> freshObjects;
    double stationLatitude;
    double stationLongitude;
    float stationHeading;
    int stationType;

    while (!stopFlag_) {
        auto start = std::chrono::high_resolution_clock::now();

        // Retrieve fresh objects from the aggregator.
        freshObjects = aggregator_->getFreshObjects();
        addSensor = false;

        auto now = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_sensor_ts).count() > 1000) {
            sensorInfo = aggregator_->getSensorInfo();
            addSensor = true;
            for (const auto& [sensorID, sensor] : sensorInfo) {
                spdlog::info("[Generation]: Sensor ID: {}, Type: {}", sensor.sensorID, sensor.sensorType);
            }
            last_sensor_ts = now;
        }

        if (freshObjects.empty()) {
            spdlog::info("[Generation]: No fresh objects retrieved this cycle.");
        } else {
            spdlog::info("[Generation]: Retrieved {} fresh objects.", freshObjects.size());
            
            stationLatitude = locator_->getStationLatitude();
            stationLongitude = locator_->getStationLongitude();
            stationHeading = locator_->getStationHeading();
            stationType = locator_->getStationType();
            spdlog::info("[Generation]: Station Latitude: {}, Longitude: {}, Type: {}", stationLatitude, stationLongitude, stationType);

            // Generate CPM
            std::string cpm_str = builder_.generateCPM(freshObjects, sensorInfo, addSensor, stationLatitude, stationLongitude, stationHeading, stationType);
            //std::string cpm_str = cpm.dump();

            // spdlog::info("[Generation]: CPM generation took {} us, serialization took {} us", std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count(), std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count());
            spdlog::info("[Generation]: Publising CPM: {}", cpm_str);

            // Publish CPM
            dds_->publish(ddsTopic_, cpm_str);
        }
        // Sleep for the current request rate - time taken to process this cycle.
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        spdlog::info("[Generation]: Cycle took {} ms, sleeping for {} ms", duration, requestRateMs_.load() - duration);
        if (requestRateMs_.load() - duration > 0) std::this_thread::sleep_for(std::chrono::milliseconds(requestRateMs_.load() - duration));
    }
}
