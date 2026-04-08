#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

#include "models.h"
#include "source_manager.h"

namespace goldview {

class PriceService {
public:
    using SnapshotCallback = std::function<void(const PriceSnapshot&)>;

    PriceService();
    ~PriceService();

    void start(SnapshotCallback callback);
    void stop();
    void updateSettings(const AppSettings& settings);
    PriceServiceStatus currentStatus() const;
    PriceSnapshot currentSnapshot() const;

private:
    void run();

    std::atomic<bool> running_{false};
    std::atomic<int> tickIntervalMs_{200};
    std::thread worker_;
    SnapshotCallback callback_;
    mutable std::mutex callbackMutex_;
    SourceManager sourceManager_;
};

}  // namespace goldview
