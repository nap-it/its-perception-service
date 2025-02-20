#ifndef GENERATION_H
#define GENERATION_H

#include "aggregator.h"
#include <atomic>
#include <thread>

class Generation {
public:
    /**
     * @brief Construct a new Generation object.
     * @param aggregator Reference to the Aggregator instance.
     * @param requestRateMs Initial request rate in milliseconds.
     */
    Generation(Aggregator &aggregator, int requestRateMs);

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
    Aggregator &aggregator_;
    std::atomic<int> requestRateMs_;
    std::atomic<bool> stopFlag_;
    std::thread generationThread_;

    /**
     * @brief Main loop that periodically retrieves fresh objects.
     */
    void runLoop();
};

#endif // GENERATION_H
