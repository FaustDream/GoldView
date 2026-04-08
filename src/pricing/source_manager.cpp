#include "source_manager.h"

#include <algorithm>
#include <chrono>

namespace goldview {

namespace {

constexpr std::uint64_t kStaleAfterSeconds = 3;

std::uint64_t unixNow() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

}

SourceManager::SourceManager()
    : settings_(defaultAppSettings()) {
    rebuildSourcesLocked();
}

void SourceManager::updateSettings(const AppSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = settings;
    rebuildSourcesLocked();
    appendLogLocked(RuntimeLogLevel::Info, L"Settings updated; rebuilding source manager state");
}

PriceServiceStatus SourceManager::currentStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    PriceServiceStatus status{};
    status.autoRefreshEnabled = settings_.runtime.autoRefreshEnabled;
    status.delayed = hasLastSuccessfulSnapshot_ ? unixNow() > lastSuccessfulAt_ + kStaleAfterSeconds : true;
    status.requestIntervalMs = settings_.runtime.activeRequestIntervalMs;
    status.activeSource = activeSource_;
    status.lastRequestedAt = lastRequestedAt_;
    status.lastSuccessfulAt = lastSuccessfulAt_;
    status.lastSwitchAt = lastSwitchAt_;
    status.lastSwitchReason = lastSwitchReason_;
    for (const auto& record : records_) {
        status.sourceHealth.push_back(buildHealthSnapshotLocked(record));
    }
    status.logs = runtimeLogs_;
    return status;
}

PriceSnapshot SourceManager::currentSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentSnapshot_;
}

void SourceManager::rebuildSourcesLocked() {
    std::vector<SourceRecord> rebuilt;
    rebuilt.reserve(settings_.sources.size());

    for (const auto& config : settings_.sources) {
        SourceRecord record{};
        record.config = config;
        record.source = createQuoteSource(config.kind);
        const size_t oldIndex = indexOfSourceLocked(config.kind);
        if (oldIndex < records_.size()) {
            record.history = records_[oldIndex].history;
            record.lastRequestAt = records_[oldIndex].lastRequestAt;
            record.lastSuccessAt = records_[oldIndex].lastSuccessAt;
            record.lastLatencyMs = records_[oldIndex].lastLatencyMs;
            record.requestCount = records_[oldIndex].requestCount;
            record.successCount = records_[oldIndex].successCount;
            record.errorCount = records_[oldIndex].errorCount;
            record.lastPrice = records_[oldIndex].lastPrice;
            record.lastError = records_[oldIndex].lastError;
        }
        rebuilt.push_back(std::move(record));
    }

    records_ = std::move(rebuilt);
    const size_t activeIndex = indexOfSourceLocked(activeSource_);
    if (activeIndex >= records_.size() && !records_.empty()) {
        activeSource_ = records_.front().config.kind;
    }
}

void SourceManager::appendLogLocked(RuntimeLogLevel level, const std::wstring& message) {
    runtimeLogs_.push_back(RuntimeLogEntry{level, unixNow(), message});
    const size_t maxLogs = static_cast<size_t>((std::max)(20, settings_.runtime.logLimit));
    while (runtimeLogs_.size() > maxLogs) {
        runtimeLogs_.erase(runtimeLogs_.begin());
    }
}

size_t SourceManager::indexOfSourceLocked(QuoteSourceKind kind) const {
    for (size_t index = 0; index < records_.size(); ++index) {
        if (records_[index].config.kind == kind) {
            return index;
        }
    }
    return static_cast<size_t>(-1);
}

}  // namespace goldview
