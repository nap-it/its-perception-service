#ifndef GENERATION_H
#define GENERATION_H

#include "aggregator.h"
#include "builder.h"
#include "locator.h"
#include <memory>
#include <atomic>
#include <thread>
#include "fastdds-cpp-wrapper/dds.hpp"

class Generation {
public:
    /**
     * @brief Construct a new Generation object.
     * @param aggregator Aggregator object.
     * @param locator Locator object.
     * @param requestRateMs Initial request rate in milliseconds.
     * @param ddsDomain DDS domain ID if provider is DDS.
     * @param ddsTopic DDS topic to subscribe for CAMs if provider is DDS.
     */
    Generation(std::shared_ptr<Aggregator> aggregator, std::shared_ptr<Locator> locator, int requestRateMs, int ddsDomain, std::string ddsTopic);

    /**
     * @brief Destroy the Generation object.
     */
    ~Generation();

    /**
     * @brief Start the generation loop.
     */
    void run();

    /**
     * @brief Stop the generation loop.
     */
    void stop();

    /**
     * @brief Dynamically update the request rate.
     * @param newRateMs New request rate in milliseconds.
     */
    void setRequestRate(int newRateMs);

    /**
     * @brief Get the current request rate.
     * @return int Request rate in milliseconds.
     */
    int getRequestRate() const { return requestRateMs_.load(); }

private:
    std::shared_ptr<Aggregator> aggregator_;
    std::shared_ptr<Locator> locator_;
    bool debug_;
    std::atomic<int> requestRateMs_;
    std::atomic<bool> stopFlag_;
    std::thread generationThread_;
    Builder builder_;

    // DDS client
    Dds* dds_;
    std::string ddsTopic_;

    /**
     * @brief Main loop that periodically retrieves fresh objects, sensor data and constructs CPMs.
     */
    void runLoop();
};

#endif // GENERATION_H
