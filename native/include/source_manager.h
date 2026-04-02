#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "models.h"
#include "quote_source.h"

namespace goldview {

class SourceManager {
public:
    SourceManager();

    void updateSettings(const AppSettings& settings);
    void clearRuntimeLogs();
    bool tick(PriceSnapshot* snapshot);
    PriceServiceStatus currentStatus() const;
    PriceSnapshot currentSnapshot() const;

private:
    struct AttemptSample {
        bool success{false};
        int latencyMs{0};
    };

    struct SourceRecord {
        QuoteSourceConfig config;
        std::unique_ptr<IQuoteSource> source;
        std::deque<AttemptSample> history;
        std::uint64_t lastRequestAt{0};
        std::uint64_t lastSuccessAt{0};
        int lastLatencyMs{0};
        int requestCount{0};
        int successCount{0};
        int errorCount{0};
        double lastPrice{0.0};
        std::wstring lastError;
    };

    AppSettings settings_;
    PriceSnapshot currentSnapshot_{};
    PriceSnapshot lastSuccessfulSnapshot_{};
    bool hasLastSuccessfulSnapshot_{false};
    QuoteSourceKind activeSource_{QuoteSourceKind::Sina};
    std::uint64_t lastRequestedAt_{0};
    std::uint64_t lastSuccessfulAt_{0};
    std::uint64_t lastSwitchAt_{0};
    std::wstring lastSwitchReason_{L"启动默认主源"};
    std::vector<SourceRecord> records_;
    std::vector<RuntimeLogEntry> runtimeLogs_;
    mutable std::mutex mutex_;

    void rebuildSourcesLocked();
    void appendLogLocked(RuntimeLogLevel level, const std::wstring& message);
    int averageLatencyMs(const SourceRecord& record) const;
    double successRatePercent(const SourceRecord& record) const;
    bool authReady(const QuoteSourceConfig& config) const;
    bool isUnhealthy(const SourceRecord& record) const;
    int activeIntervalMs(const SourceRecord& record) const;
    int standbyIntervalMs(const SourceRecord& record) const;
    std::vector<size_t> rankedCandidatesLocked() const;
    size_t selectPreferredCandidateLocked(std::wstring* reason);
    void recordAttemptLocked(SourceRecord& record, bool success, int latencyMs, const std::wstring& error, double price);
    SourceHealthSnapshot buildHealthSnapshotLocked(const SourceRecord& record) const;
    size_t indexOfSourceLocked(QuoteSourceKind kind) const;
};

}  // namespace goldview
