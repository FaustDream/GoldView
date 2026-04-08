#include "source_manager.h"

#include <algorithm>

namespace goldview {

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
            *reason = L"No source records are configured";
        }
        return static_cast<size_t>(-1);
    }

    if (!settings_.runtime.autoSwitchSource) {
        const size_t preferredIndex = indexOfSourceLocked(settings_.runtime.preferredSource);
        if (preferredIndex < records_.size() && records_[preferredIndex].config.enabled && authReady(records_[preferredIndex].config)) {
            if (reason) {
                *reason = L"Auto switch is disabled; using preferred source";
            }
            return preferredIndex;
        }
    }

    const size_t activeIndex = indexOfSourceLocked(activeSource_);
    if (activeIndex < records_.size() && records_[activeIndex].config.enabled && authReady(records_[activeIndex].config) &&
        !isUnhealthy(records_[activeIndex])) {
        if (reason) {
            *reason = L"Current active source is still healthy";
        }
        return activeIndex;
    }

    const auto ranked = rankedCandidatesLocked();
    if (ranked.empty()) {
        if (reason) {
            *reason = L"No enabled source is currently available";
        }
        return static_cast<size_t>(-1);
    }

    if (reason) {
        *reason = L"Selected the highest-ranked healthy source";
    }
    return ranked.front();
}

}  // namespace goldview
