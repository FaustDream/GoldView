#include "source_manager.h"

#include <algorithm>

namespace goldview {

int SourceManager::averageLatencyMs(const SourceRecord& record) const {
    if (record.history.empty()) {
        return 0;
    }
    int total = 0;
    int count = 0;
    for (const auto& sample : record.history) {
        if (!sample.success) {
            continue;
        }
        total += sample.latencyMs;
        count += 1;
    }
    return count > 0 ? total / count : 0;
}

double SourceManager::successRatePercent(const SourceRecord& record) const {
    if (record.history.empty()) {
        return 100.0;
    }
    int successCount = 0;
    for (const auto& sample : record.history) {
        if (sample.success) {
            successCount += 1;
        }
    }
    return static_cast<double>(successCount) * 100.0 / static_cast<double>(record.history.size());
}

bool SourceManager::authReady(const QuoteSourceConfig&) const {
    return true;
}

bool SourceManager::isUnhealthy(const SourceRecord& record) const {
    const size_t minimumWindow = static_cast<size_t>((std::max)(3, settings_.runtime.healthWindowSize / 4));
    if (record.history.size() < minimumWindow) {
        return false;
    }
    if (successRatePercent(record) < static_cast<double>(settings_.runtime.successRateThreshold)) {
        return true;
    }
    return averageLatencyMs(record) > settings_.runtime.latencyThresholdMs;
}

SourceHealthSnapshot SourceManager::buildHealthSnapshotLocked(const SourceRecord& record) const {
    SourceHealthSnapshot snapshot{};
    snapshot.kind = record.config.kind;
    snapshot.transport = record.config.transport;
    snapshot.enabled = record.config.enabled;
    snapshot.authReady = authReady(record.config);
    snapshot.active = record.config.kind == activeSource_;
    snapshot.priority = record.config.priority;
    snapshot.weight = record.config.weight;
    snapshot.requestCount = record.requestCount;
    snapshot.successCount = record.successCount;
    snapshot.errorCount = record.errorCount;
    snapshot.averageLatencyMs = averageLatencyMs(record);
    snapshot.lastLatencyMs = record.lastLatencyMs;
    snapshot.lastRequestAt = record.lastRequestAt;
    snapshot.lastSuccessAt = record.lastSuccessAt;
    snapshot.lastPrice = record.lastPrice;
    snapshot.lastError = record.lastError;
    return snapshot;
}

void SourceManager::recordAttemptLocked(SourceRecord& record, bool success, int latencyMs, const std::wstring& error, double price) {
    record.requestCount += 1;
    record.lastLatencyMs = latencyMs;
    record.history.push_back(AttemptSample{success, latencyMs});
    while (record.history.size() > static_cast<size_t>((std::max)(5, settings_.runtime.healthWindowSize))) {
        record.history.pop_front();
    }
    if (success) {
        record.successCount += 1;
        record.lastSuccessAt = record.lastRequestAt;
        record.lastError.clear();
        record.lastPrice = price;
    } else {
        record.errorCount += 1;
        record.lastError = error;
    }
}

}  // namespace goldview
