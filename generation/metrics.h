/**
 * @file metrics.h
 * @brief Prometheus metrics manager and handle structs for the CPS services
 * @date 2026
 *
 * This file provides a singleton MetricsManager and two handle structs
 * (GenMetricHandles, ProcMetricHandles) that give the Generation and Processing
 * services direct access to their respective Prometheus counters, gauges, and
 * histograms.
 *
 * Exposed metrics (Generation, port 9102):
 * - cps_generation_cpm_publish_total        Counter  — total CPMs published
 * - cps_generation_last_cpm_ts_seconds      Gauge    — UNIX timestamp of last CPM
 * - cps_generation_cpm_build_ms             Histogram — CPM build latency (ms)
 * - cps_generation_cycle_ms                 Histogram — full loop cycle duration (ms)
 * - cps_generation_objects_per_msg          Histogram — objects included per CPM
 * - cps_generation_radar_messages_total     Counter  — radar object messages received
 * - cps_generation_camera_messages_total    Counter  — camera object messages received
 *
 * Usage:
 *   MetricsManager::instance().init("0.0.0.0:9102");
 *   GenMetricHandles h = MetricsManager::instance().createGenerationMetrics(version);
 */

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

struct GenMetricHandles {
    prometheus::Counter*   cpm_built        = nullptr;
    prometheus::Gauge*     cpm_last_ts      = nullptr;
    prometheus::Histogram* cpm_build_ms   = nullptr;
    prometheus::Histogram* cycle_ms         = nullptr;
    prometheus::Histogram* objects_per_msg  = nullptr;
    prometheus::Counter*   radar_messages   = nullptr;
    prometheus::Counter*   camera_messages  = nullptr;
};

class MetricsManager {
public:
    static MetricsManager& instance() {
        static MetricsManager inst;
        return inst;
    }

    // Initialize once at startup
    void init(const std::string& listen_addr = "0.0.0.0:9102") {
        if (!exposer_) {
            exposer_  = std::make_shared<prometheus::Exposer>(listen_addr);
            registry_ = std::make_shared<prometheus::Registry>();
            exposer_->RegisterCollectable(registry_);
        }
    }

    // Create Generation metrics and return handles
    GenMetricHandles createGenerationMetrics(const std::string& version) {
        GenMetricHandles h;

        // Info gauge
        auto& gen_info = prometheus::BuildGauge()
            .Name("cps_generation_info")
            .Help("Generation info")
            .Register(*registry_);
        gen_info.Add({{"service","generation"},{"version",version}}).Set(1);

        auto& fam_cpm_built   = prometheus::BuildCounter()
            .Name("cps_generation_cpm_publish_total")
            .Help("Total CPMs published").Register(*registry_);
        auto& fam_last_cpm_ts = prometheus::BuildGauge()
            .Name("cps_generation_last_cpm_ts_seconds")
            .Help("Unix ts of last CPM").Register(*registry_);
        auto& fam_build_secs  = prometheus::BuildHistogram()
            .Name("cps_generation_cpm_build_ms")
            .Help("CPM build duration in ms").Register(*registry_);
        auto& fam_cycle_ms    = prometheus::BuildHistogram()
            .Name("cps_generation_cycle_ms")
            .Help("Loop cycle duration in ms").Register(*registry_);
        auto& fam_objects     = prometheus::BuildHistogram()
            .Name("cps_generation_objects_per_msg")
            .Help("Objects per CPM").Register(*registry_);
        auto& radar_messages = prometheus::BuildCounter()
            .Name("cps_generation_radar_messages_total")
            .Help("Total radar messages received").Register(*registry_);
        auto& camera_messages = prometheus::BuildCounter()
            .Name("cps_generation_camera_messages_total")
            .Help("Total camera messages received").Register(*registry_);

        const std::map<std::string,std::string> labels = {{"service","generation"}};
        const std::vector<double> buckets_build_ms = {1,5,10,50,100,500,1000};
        const std::vector<double> buckets_cycle_ms= {1,5,10,20,50,100,200,500,1000};
        const std::vector<double> buckets_objects = {0,1,2,3,4,5,8,12,16,24,32,48,64,100};

        h.cpm_built       = &fam_cpm_built.Add(labels);
        h.cpm_last_ts     = &fam_last_cpm_ts.Add(labels);
        h.cpm_build_ms    = &fam_build_secs.Add(labels, buckets_build_ms);
        h.cycle_ms        = &fam_cycle_ms.Add(labels, buckets_cycle_ms);
        h.objects_per_msg = &fam_objects.Add(labels, buckets_objects);
        h.radar_messages  = &radar_messages.Add(labels);
        h.camera_messages = &camera_messages.Add(labels);

        return h;
    }

private:
    MetricsManager() = default;
    std::shared_ptr<prometheus::Exposer> exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
};