#include "aggregator.h"
#include "generation.h"
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>

int main() {

    Aggregator aggregator(0, 300000, 5000, false);
    aggregator.run();

    Generation generation(aggregator, 1000);
    generation.run();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    aggregator.stop();
    return 0;
}