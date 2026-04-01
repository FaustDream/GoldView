#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "models.h"

namespace goldview {

class PriceService {
public:
    using SnapshotCallback = std::function<void(const PriceSnapshot&)>;

    PriceService();
    ~PriceService();

    void start(SnapshotCallback callback);
    void stop();
    void updateSettings(const DisplaySettings& settings);
    void startBenchmark();
    PriceServiceStatus currentStatus() const;
    std::vector<BenchmarkSample> benchmarkSamples() const;
    std::wstring statusSummary() const;

private:
    void run();
    PriceSnapshot fetchSnapshot();
    PriceProviderKind chooseProvider(std::wstring* reason) const;
    std::vector<PriceProviderKind> fallbackProviders(PriceProviderKind primary) const;
    PriceSnapshot fetchFromProvider(PriceProviderKind provider);
    PriceSnapshot fetchFromGoldApi() const;
    PriceSnapshot fetchFromSinaApi() const;
    PriceSnapshot fetchFromXwteamApi() const;
    std::wstring httpGet(const std::wstring& host, const std::wstring& path, bool secure = true) const;
    std::string httpGetUtf8(const std::wstring& host, const std::wstring& path, bool secure = true,
        const std::wstring& extraHeaders = L"") const;
    std::vector<PriceProviderKind> allProviders() const;
    void noteProviderSuccess(PriceProviderKind provider, const PriceSnapshot& snapshot, int latencyMs);
    void noteProviderFailure(PriceProviderKind provider, int latencyMs, const std::wstring& error);
    void switchActiveProvider(PriceProviderKind provider, const std::wstring& reason);
    PriceProviderKind benchmarkRecommendedProvider() const;
    void runBenchmarkAsync();
    BenchmarkProviderResult runProviderBenchmark(PriceProviderKind provider, std::vector<BenchmarkSample>* samples) const;
    ProviderStats* findProviderStatsLocked(PriceProviderKind provider);
    const ProviderStats* findProviderStatsLocked(PriceProviderKind provider) const;

    std::atomic<bool> running_{false};
    std::atomic<int> intervalMs_{1000};
    std::atomic<bool> benchmarkStopRequested_{false};
    std::thread worker_;
    std::thread benchmarkThread_;
    SnapshotCallback callback_;
    mutable std::mutex callbackMutex_;
    mutable std::mutex snapshotMutex_;
    mutable std::mutex settingsMutex_;
    mutable std::mutex stateMutex_;
    DisplaySettings settings_{};
    PriceSnapshot lastSuccessfulSnapshot_{};
    bool hasLastSuccessfulSnapshot_{false};
    PriceProviderKind activeProvider_{PriceProviderKind::GoldApi};
    std::uint64_t lastSwitchAt_{0};
    std::wstring lastSwitchReason_;
    std::vector<ProviderStats> providerStats_;
    bool benchmarkRunning_{false};
    std::vector<BenchmarkProviderResult> benchmarkResults_;
    std::vector<BenchmarkSample> benchmarkSamples_;
};

}  // namespace goldview
