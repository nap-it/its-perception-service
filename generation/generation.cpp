#include "generation.h"
#include <chrono>
#include <spdlog/spdlog.h>

Generation::Generation(Aggregator &aggregator, int requestRateMs)
    : aggregator_(aggregator), requestRateMs_(requestRateMs), stopFlag_(false) {
    spdlog::info("Generation initialized with request rate {} ms", requestRateMs);
}

Generation::~Generation() {
    stop();
}

void Generation::run() {
    stopFlag_ = false;
    generationThread_ = std::thread(&Generation::runLoop, this);
    spdlog::info("Generation run loop started.");
}

void Generation::stop() {
    stopFlag_ = true;
    if (generationThread_.joinable()) {
        generationThread_.join();
    }
    spdlog::info("Generation run loop stopped.");
}

void Generation::setRequestRate(int newRateMs) {
    requestRateMs_ = newRateMs;
    spdlog::info("Generation request rate updated to {} ms", newRateMs);
}

void Generation::runLoop() {
    while (!stopFlag_) {
        // Retrieve fresh objects from the aggregator.
        auto freshObjects = aggregator_.getFreshObjects();

        if (!freshObjects.empty()) {
            spdlog::info("Generation: Retrieved {} fresh objects", freshObjects.size());
        } else {
            spdlog::debug("Generation: No fresh objects retrieved this cycle.");
        }

        // Sleep for the current request rate
        spdlog::debug("Generation: Sleeping for {} ms", requestRateMs_.load());
        std::this_thread::sleep_for(std::chrono::milliseconds(requestRateMs_.load()));
    }
}
