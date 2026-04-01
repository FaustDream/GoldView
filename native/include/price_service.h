#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

#include "models.h"

namespace goldview {

class PriceService {
public:
    using SnapshotCallback = std::function<void(const PriceSnapshot&)>;

    PriceService();
    ~PriceService();

    void start(const DisplaySettings& settings, SnapshotCallback callback);
    void stop();
    void updateInterval(int intervalMs);
    void updateSettings(const DisplaySettings& settings);

private:
    void run();
    PriceSnapshot fetchSnapshot() const;
    PriceSnapshot fetchFromGoldApi() const;
    PriceSnapshot fetchFromExchangeRateHost() const;
    PriceSnapshot fetchFromMetalsDev() const;
    PriceSnapshot fetchFromMetalPriceApi() const;
    std::wstring httpGet(const std::wstring& host, const std::wstring& path, bool secure = true) const;
    DisplaySettings settingsCopy() const;

    std::atomic<bool> running_{false};
    std::atomic<int> intervalMs_{1000};
    std::thread worker_;
    SnapshotCallback callback_;
    mutable std::mutex callbackMutex_;
    mutable std::mutex settingsMutex_;
    DisplaySettings settings_{};
};

}  // namespace goldview
