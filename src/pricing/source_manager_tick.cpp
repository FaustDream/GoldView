#include "source_manager.h"

#include <chrono>

namespace goldview {

namespace {

constexpr std::uint64_t kStaleAfterSeconds = 3;

std::uint64_t unixNow() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

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
                    L"Switched active source to " + std::wstring(quoteSourceLabel(activeSource_)) + L": " + selectionReason);
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
                std::wstring(quoteSourceLabel(config.kind)) + L" request failed: " + (errorMessage.empty() ? L"unknown" : errorMessage));
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
                    lastSwitchReason_ = L"Primary source failed, switched to fallback source";
                    appendLogLocked(RuntimeLogLevel::Info,
                        L"Fallback source activated: " + std::wstring(quoteSourceLabel(fallbackConfig.kind)));
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

}  // namespace goldview
