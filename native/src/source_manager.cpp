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

}  // namespace

SourceManager::SourceManager()
    : settings_(defaultAppSettings()) {
    rebuildSourcesLocked();
}

void SourceManager::updateSettings(const AppSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = settings;
    rebuildSourcesLocked();
    appendLogLocked(RuntimeLogLevel::Info, L"已应用新的数据源与阈值设置");
}

bool SourceManager::tick(PriceSnapshot* snapshot) {
    QuoteSourceConfig primaryConfig{};
    QuoteSourceConfig probeConfig{};
    bool autoRefreshEnabled = true;
    bool shouldPollPrimary = false;
    bool shouldPollProbe = false;
    std::wstring selectionReason;
    size_t probeIndex = static_cast<size_t>(-1);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        autoRefreshEnabled = settings_.runtime.autoRefreshEnabled;
        if (!autoRefreshEnabled) {
            currentSnapshot_.delayed = false;
            if (snapshot) {
                *snapshot = currentSnapshot_;
            }
            return false;
        }

        const size_t selectedIndex = selectPreferredCandidateLocked(&selectionReason);
        if (selectedIndex < records_.size()) {
            primaryConfig = records_[selectedIndex].config;
            if (activeSource_ != primaryConfig.kind) {
                activeSource_ = primaryConfig.kind;
                lastSwitchAt_ = unixNow();
                lastSwitchReason_ = selectionReason;
                appendLogLocked(RuntimeLogLevel::Info,
                    L"切换数据源到 " + std::wstring(quoteSourceLabel(activeSource_)) + L"：" + selectionReason);
            }
            shouldPollPrimary = unixNow() * 1000 >= records_[selectedIndex].lastRequestAt * 1000 + activeIntervalMs(records_[selectedIndex]);
        }

        for (size_t index = 0; index < records_.size(); ++index) {
            if (records_[index].config.kind == activeSource_ || !records_[index].config.enabled) {
                continue;
            }
            if (unixNow() * 1000 >= records_[index].lastRequestAt * 1000 + standbyIntervalMs(records_[index])) {
                probeIndex = index;
                probeConfig = records_[index].config;
                shouldPollProbe = true;
                break;
            }
        }
    }

    auto fetchConfig = [&](const QuoteSourceConfig& config, bool allowSwitch, bool logFailure, PriceSnapshot* fetchedSnapshot) {
        PriceSnapshot localSnapshot{};
        std::wstring errorMessage;
        bool success = false;
        const auto startedAt = std::chrono::steady_clock::now();

        try {
            auto source = createQuoteSource(config.kind);
            localSnapshot = source->fetch(config);
            success = true;
        } catch (const std::exception& error) {
            const std::string text = error.what();
            errorMessage.assign(text.begin(), text.end());
        } catch (...) {
            errorMessage = L"unknown source error";
        }

        const int latencyMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt).count());

        std::lock_guard<std::mutex> lock(mutex_);
        const size_t index = indexOfSourceLocked(config.kind);
        if (index >= records_.size()) {
            return false;
        }

        SourceRecord& record = records_[index];
        record.lastRequestAt = unixNow();
        lastRequestedAt_ = record.lastRequestAt;
        recordAttemptLocked(record, success, latencyMs, errorMessage, localSnapshot.currentPrice);

        if (success) {
            currentSnapshot_ = localSnapshot;
            currentSnapshot_.delayed = false;
            lastSuccessfulSnapshot_ = currentSnapshot_;
            hasLastSuccessfulSnapshot_ = true;
            lastSuccessfulAt_ = record.lastSuccessAt;
            if (allowSwitch) {
                activeSource_ = config.kind;
            }
            if (fetchedSnapshot) {
                *fetchedSnapshot = currentSnapshot_;
            }
            return true;
        }

        if (logFailure) {
            appendLogLocked(RuntimeLogLevel::Warn,
                std::wstring(quoteSourceLabel(config.kind)) + L" 请求失败：" + (errorMessage.empty() ? L"unknown" : errorMessage));
        }
        return false;
    };

    bool snapshotUpdated = false;
    if (shouldPollPrimary) {
        PriceSnapshot fetched{};
        snapshotUpdated = fetchConfig(primaryConfig, true, true, &fetched);
        if (snapshotUpdated && snapshot) {
            *snapshot = fetched;
        }

        if (!snapshotUpdated) {
            std::vector<size_t> candidates;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                candidates = rankedCandidatesLocked();
            }
            for (const size_t index : candidates) {
                QuoteSourceConfig fallbackConfig{};
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (index >= records_.size() || records_[index].config.kind == primaryConfig.kind) {
                        continue;
                    }
                    fallbackConfig = records_[index].config;
                }
                PriceSnapshot fallbackSnapshot{};
                if (fetchConfig(fallbackConfig, true, true, &fallbackSnapshot)) {
                    snapshotUpdated = true;
                    if (snapshot) {
                        *snapshot = fallbackSnapshot;
                    }
                    std::lock_guard<std::mutex> lock(mutex_);
                    lastSwitchAt_ = unixNow();
                    lastSwitchReason_ = L"主源失败后切换到健康候选";
                    appendLogLocked(RuntimeLogLevel::Info,
                        L"主源失败，切换到 " + std::wstring(quoteSourceLabel(fallbackConfig.kind)));
                    break;
                }
            }
        }
    }

    if (shouldPollProbe && probeIndex != static_cast<size_t>(-1)) {
        fetchConfig(probeConfig, false, false, nullptr);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (hasLastSuccessfulSnapshot_) {
            currentSnapshot_ = lastSuccessfulSnapshot_;
            currentSnapshot_.delayed = unixNow() > lastSuccessfulAt_ + kStaleAfterSeconds;
        }
        if (snapshot && !snapshotUpdated) {
            *snapshot = currentSnapshot_;
        }
    }

    return snapshotUpdated;
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

int SourceManager::activeIntervalMs(const SourceRecord& record) const {
    if (record.config.transport == QuoteSourceTransport::Api) {
        return (std::max)(settings_.runtime.activeRequestIntervalMs, settings_.runtime.fallbackApiIntervalMs);
    }
    return settings_.runtime.activeRequestIntervalMs;
}

int SourceManager::standbyIntervalMs(const SourceRecord& record) const {
    if (record.config.transport == QuoteSourceTransport::Api) {
        return settings_.runtime.fallbackApiIntervalMs;
    }
    return settings_.runtime.standbyRequestIntervalMs;
}

std::vector<size_t> SourceManager::rankedCandidatesLocked() const {
    std::vector<size_t> indexes;
    for (size_t index = 0; index < records_.size(); ++index) {
        if (!records_[index].config.enabled || !authReady(records_[index].config)) {
            continue;
        }
        indexes.push_back(index);
    }

    std::stable_sort(indexes.begin(), indexes.end(), [&](size_t leftIndex, size_t rightIndex) {
        const SourceRecord& left = records_[leftIndex];
        const SourceRecord& right = records_[rightIndex];
        const bool leftUnhealthy = isUnhealthy(left);
        const bool rightUnhealthy = isUnhealthy(right);
        if (leftUnhealthy != rightUnhealthy) {
            return !leftUnhealthy;
        }
        if (left.config.priority != right.config.priority) {
            return left.config.priority < right.config.priority;
        }
        if (left.config.weight != right.config.weight) {
            return left.config.weight > right.config.weight;
        }
        const int leftLatency = averageLatencyMs(left) == 0 ? 100000 : averageLatencyMs(left);
        const int rightLatency = averageLatencyMs(right) == 0 ? 100000 : averageLatencyMs(right);
        if (leftLatency != rightLatency) {
            return leftLatency < rightLatency;
        }
        return successRatePercent(left) > successRatePercent(right);
    });

    return indexes;
}

size_t SourceManager::selectPreferredCandidateLocked(std::wstring* reason) {
    if (records_.empty()) {
        if (reason) {
            *reason = L"无可用数据源";
        }
        return static_cast<size_t>(-1);
    }

    if (!settings_.runtime.autoSwitchSource) {
        const size_t preferredIndex = indexOfSourceLocked(settings_.runtime.preferredSource);
        if (preferredIndex < records_.size() && records_[preferredIndex].config.enabled && authReady(records_[preferredIndex].config)) {
            if (reason) {
                *reason = L"手动固定首选数据源";
            }
            return preferredIndex;
        }
    }

    const size_t activeIndex = indexOfSourceLocked(activeSource_);
    if (activeIndex < records_.size() && records_[activeIndex].config.enabled && authReady(records_[activeIndex].config) &&
        !isUnhealthy(records_[activeIndex])) {
        if (reason) {
            *reason = L"当前主源健康，继续保持";
        }
        return activeIndex;
    }

    const auto ranked = rankedCandidatesLocked();
    if (ranked.empty()) {
        if (reason) {
            *reason = L"无启用且可认证的数据源";
        }
        return static_cast<size_t>(-1);
    }

    if (reason) {
        *reason = L"按优先级、权重和健康度选择最佳候选";
    }
    return ranked.front();
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
        record.lastSuccessAt = unixNow();
        record.lastError.clear();
        record.lastPrice = price;
    } else {
        record.errorCount += 1;
        record.lastError = error;
    }
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

size_t SourceManager::indexOfSourceLocked(QuoteSourceKind kind) const {
    for (size_t index = 0; index < records_.size(); ++index) {
        if (records_[index].config.kind == kind) {
            return index;
        }
    }
    return static_cast<size_t>(-1);
}

}  // namespace goldview
