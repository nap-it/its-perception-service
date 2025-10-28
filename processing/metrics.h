#pragma once
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>

struct ProcMetricHandles {
    prometheus::Histogram* obj_age_ms       = nullptr;
    prometheus::Histogram* cycle_ms         = nullptr;
    prometheus::Counter*   pub_messages      = nullptr;
};

class MetricsManager {
public:
    static MetricsManager& instance() {
        static MetricsManager inst;
        return inst;
    }

    // Initialize once at startup
    void init(const std::string& listen_addr = "0.0.0.0:9103") {
        if (!exposer_) {
            exposer_  = std::make_shared<prometheus::Exposer>(listen_addr);
            registry_ = std::make_shared<prometheus::Registry>();
            exposer_->RegisterCollectable(registry_);
        }
    }

    // Create Processing metrics and return handles
    ProcMetricHandles createGenerationMetrics(const std::string& version) {
        ProcMetricHandles h;

        // Info gauge
        auto& gen_info = prometheus::BuildGauge()
            .Name("cps_processing_info")
            .Help("Processing info")
            .Register(*registry_);
        gen_info.Add({{"service","processing"},{"version",version}}).Set(1);

        auto& fam_obj_age   = prometheus::BuildHistogram()
            .Name("cps_processing_object_age_ms")
            .Help("Object age in ms").Register(*registry_);
        auto& fam_cycle_ms  = prometheus::BuildHistogram()
            .Name("cps_processing_cycle_ms")
            .Help("Loop cycle duration in ms").Register(*registry_);
        auto& pub_messages   = prometheus::BuildCounter()
            .Name("cps_processing_pub_messages_total")
            .Help("Total published messages").Register(*registry_);


        const std::map<std::string,std::string> labels = {{"service","processing"}};
        const std::vector<double> buckets_cycle_ms= {1,5,10,20,50,100,200,500,1000};
        const std::vector<double> buckets_age_ms = {1,5,10,25,50,100,200,500,1000,2000};

        h.obj_age_ms = &fam_obj_age.Add(labels, buckets_age_ms);
        h.cycle_ms   = &fam_cycle_ms.Add(labels, buckets_cycle_ms);
        h.pub_messages = &pub_messages.Add(labels);

        return h;
    }

private:
    MetricsManager() = default;
    std::shared_ptr<prometheus::Exposer> exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
};