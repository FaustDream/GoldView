#include "source_manager.h"

#include <algorithm>
#include <vector>

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
    std::vector<size_t> healthyIndexes;
    std::vector<size_t> degradedIndexes;

    for (size_t index = 0; index < records_.size(); ++index) {
        if (isUnhealthy(records_[index])) {
            degradedIndexes.push_back(index);
            continue;
        }
        healthyIndexes.push_back(index);
    }

    healthyIndexes.insert(healthyIndexes.end(), degradedIndexes.begin(), degradedIndexes.end());
    return healthyIndexes;
}

size_t SourceManager::selectPreferredCandidateLocked(std::wstring* reason) {
    if (records_.empty()) {
        if (reason) {
            *reason = L"未找到可用数据源";
        }
        return static_cast<size_t>(-1);
    }

    const size_t activeIndex = indexOfSourceLocked(activeSource_);
    if (activeIndex < records_.size() && !isUnhealthy(records_[activeIndex])) {
        if (reason) {
            *reason = L"当前数据源仍然健康";
        }
        return activeIndex;
    }

    const auto ranked = rankedCandidatesLocked();
    if (ranked.empty()) {
        if (reason) {
            *reason = L"未找到可轮询的数据源";
        }
        return static_cast<size_t>(-1);
    }

    if (reason) {
        *reason = isUnhealthy(records_[ranked.front()]) ? L"所有数据源均已降级，按默认顺序重试"
                                                        : L"按默认顺序选择下一个健康数据源";
    }
    return ranked.front();
}

}  // namespace goldview
