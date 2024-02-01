#include "dds-sub.h"
#include "cpm-processing.h"
#include <chrono>

SubListener::SubListener() : i(0) {
}

SubListener::~SubListener() {
}

std::function<void(string)> publish;
void ddsSetPublishProcessedCpm(const std::function<void(string)>& f)
{
    publish = f;
}

void SubListener::on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus& info) {
    if (info.current_count_change == 1) {
        std::cout << "Subscriber matched." << std::endl;
    } else if (info.current_count_change == -1) {
        std::cout << "Subscriber unmatched." << std::endl;
    } else {
        std::cout << info.current_count_change
            << " is not a valid value for SubscriptionMatchedStatus current count change" << std::endl;
    }
}

void SubListener::on_data_available(DataReader* reader) {
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();
    SampleInfo info;
    if (reader->take_next_sample(&message, &info) == ReturnCode_t::RETCODE_OK) {
        if (info.valid_data) {

            Document aux, received_cpm;

            aux.Parse(message.message().c_str());
            received_cpm.CopyFrom(aux["fields"]["cpm"], received_cpm.GetAllocator());
            string cpm_objects = process_cpm(received_cpm);
            if (cpm_objects.empty()) {
                return;
            }
            //missing publish
            // cout << "CPM objects: " << endl;
            // cout << cpm_objects << endl;
            if(publish) {
                cout << "Publishing CPM objects" << endl;
                publish(cpm_objects);
            }
        }
    }
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    spdlog::info("Total time {}", elapsed.count());
}
