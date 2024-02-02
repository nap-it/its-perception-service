#include <map>
#include <chrono>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <thread>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "fastdds/dds.hpp"

using namespace std;


void on_message(string topic, string message) {
    std::cout << "Message: " << message << " RECEIVED." << std::endl;
    if (topic == "to/adapters") {
        cout << "Received request for adapters" << endl;
    }
}

int main() {
    Dds* dds = new Dds("RadarAdapter", 0, on_message);

    dds->subscribe("to/adapters");

    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
