#include "price_service.h"

#include <algorithm>
#include <chrono>

namespace goldview {

PriceService::PriceService() = default;

PriceService::~PriceService() {
    stop();
}

void PriceService::start(SnapshotCallback callback) {
    stop();
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback_ = std::move(callback);
    }

    running_.store(true);
    worker_ = std::thread(&PriceService::run, this);
}

void PriceService::stop() {
    running_.store(false);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void PriceService::updateSettings(const AppSettings& settings) {
    sourceManager_.updateSettings(settings);
    tickIntervalMs_.store((std::max)(100, settings.runtime.uiRefreshIntervalMs));
}

PriceServiceStatus PriceService::currentStatus() const {
    return sourceManager_.currentStatus();
}

PriceSnapshot PriceService::currentSnapshot() const {
    return sourceManager_.currentSnapshot();
}

void PriceService::run() {
    while (running_.load()) {
        PriceSnapshot snapshot{};
        const bool updated = sourceManager_.tick(&snapshot);
        if (updated) {
            SnapshotCallback callbackCopy;
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                callbackCopy = callback_;
            }
            if (callbackCopy) {
                callbackCopy(snapshot);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds((std::max)(100, tickIntervalMs_.load())));
    }
}

}  // namespace goldview
