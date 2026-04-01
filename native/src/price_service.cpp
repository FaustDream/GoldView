#include "price_service.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <numeric>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace goldview {

namespace {

constexpr int kRequestIntervalMs = 1000;
constexpr std::uint64_t kFailureSwitchThreshold = 3;
constexpr std::uint64_t kNoChangeSwitchSeconds = 15;
constexpr std::uint64_t kAlternativeActivityWindowSeconds = 30;
constexpr std::uint64_t kSwitchObservationWindowSeconds = 20;
constexpr int kBenchmarkSampleCount = 120;

std::uint64_t unixNow() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

double parseJsonDouble(const std::string& content, const std::string& key) {
    const auto pattern = std::regex("\"" + key + "\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(content, match, pattern) && match.size() > 1) {
        return std::stod(match[1].str());
    }
    return 0.0;
}

std::wstring widenUtf8(const std::string& input) {
    if (input.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(input.begin(), input.end());
    }

    std::wstring output(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), output.data(), size);
    return output;
}

std::vector<std::string> split(const std::string& content, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(content);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

std::wstring formatTimestamp(std::uint64_t value) {
    if (value == 0) {
        return L"--";
    }

    std::time_t timeValue = static_cast<std::time_t>(value);
    std::tm localTime{};
    localtime_s(&localTime, &timeValue);
    wchar_t buffer[64]{};
    wcsftime(buffer, std::size(buffer), L"%H:%M:%S", &localTime);
    return buffer;
}

std::wstring providerShort(PriceProviderKind provider) {
    switch (provider) {
    case PriceProviderKind::GoldApi:
        return L"gold-api";
    case PriceProviderKind::Sina:
        return L"sina";
    case PriceProviderKind::Xwteam:
        return L"xwteam";
    case PriceProviderKind::Auto:
    default:
        return L"auto";
    }
}

double successRate(const ProviderStats& stats) {
    const int total = stats.successCount + stats.errorCount;
    if (total <= 0) {
        return 0.0;
    }
    return static_cast<double>(stats.successCount) / static_cast<double>(total);
}

double successRate(const BenchmarkProviderResult& result) {
    if (result.sampleCount <= 0) {
        return 0.0;
    }
    return static_cast<double>(result.successCount) / static_cast<double>(result.sampleCount);
}

double benchmarkScore(const BenchmarkProviderResult& result) {
    return successRate(result) * 1000.0 +
        static_cast<double>(result.changeCount) * 20.0 -
        static_cast<double>(result.medianLatencyMs) * 0.5 -
        result.averageChangeIntervalSec * 10.0;
}

}  // namespace

PriceService::PriceService()
    : providerStats_{
          ProviderStats{PriceProviderKind::GoldApi},
          ProviderStats{PriceProviderKind::Sina},
          ProviderStats{PriceProviderKind::Xwteam},
      } {
    intervalMs_.store(kRequestIntervalMs);
    settings_.preferredProvider = PriceProviderKind::GoldApi;
    activeProvider_ = PriceProviderKind::GoldApi;
    lastSwitchReason_ = L"Startup default provider";
}

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
    benchmarkStopRequested_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
    if (benchmarkThread_.joinable()) {
        benchmarkThread_.join();
    }
}

void PriceService::updateSettings(const DisplaySettings& settings) {
    std::wstring switchReason;
    {
        std::lock_guard<std::mutex> settingsLock(settingsMutex_);
        settings_ = settings;
    }
    intervalMs_.store(kRequestIntervalMs);

    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        if (!settings.autoSourceSelection && activeProvider_ != settings.preferredProvider) {
            switchReason = L"Manual preferred provider selected";
        }
    }

    if (!switchReason.empty()) {
        switchActiveProvider(settings.preferredProvider, switchReason);
    }
}

void PriceService::startBenchmark() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (benchmarkRunning_) {
        return;
    }
    if (benchmarkThread_.joinable()) {
        benchmarkThread_.join();
    }

    benchmarkStopRequested_.store(false);
    benchmarkRunning_ = true;
    benchmarkResults_.clear();
    benchmarkSamples_.clear();
    benchmarkThread_ = std::thread(&PriceService::runBenchmarkAsync, this);
}

void PriceService::run() {
    while (running_.load()) {
        const auto startedAt = std::chrono::steady_clock::now();
        const auto snapshot = fetchSnapshot();
        SnapshotCallback callbackCopy;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callbackCopy = callback_;
        }
        if (callbackCopy) {
            callbackCopy(snapshot);
        }

        const auto elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt).count());
        const int sleepMs = (std::max)(50, intervalMs_.load() - elapsedMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

std::vector<PriceProviderKind> PriceService::allProviders() const {
    return {
        PriceProviderKind::GoldApi,
        PriceProviderKind::Xwteam,
        PriceProviderKind::Sina,
    };
}

ProviderStats* PriceService::findProviderStatsLocked(PriceProviderKind provider) {
    const auto it = std::find_if(providerStats_.begin(), providerStats_.end(), [&](const ProviderStats& stats) {
        return stats.provider == provider;
    });
    return it == providerStats_.end() ? nullptr : &(*it);
}

const ProviderStats* PriceService::findProviderStatsLocked(PriceProviderKind provider) const {
    const auto it = std::find_if(providerStats_.begin(), providerStats_.end(), [&](const ProviderStats& stats) {
        return stats.provider == provider;
    });
    return it == providerStats_.end() ? nullptr : &(*it);
}

PriceProviderKind PriceService::benchmarkRecommendedProvider() const {
    if (benchmarkResults_.empty()) {
        return activeProvider_;
    }

    auto best = benchmarkResults_.front();
    for (const auto& result : benchmarkResults_) {
        if (benchmarkScore(result) > benchmarkScore(best)) {
            best = result;
        }
    }
    return best.provider;
}

std::vector<PriceProviderKind> PriceService::fallbackProviders(PriceProviderKind primary) const {
    auto providers = allProviders();
    std::stable_sort(providers.begin(), providers.end(), [&](PriceProviderKind left, PriceProviderKind right) {
        if (left == primary) {
            return true;
        }
        if (right == primary) {
            return false;
        }

        const auto leftResult = std::find_if(benchmarkResults_.begin(), benchmarkResults_.end(),
            [&](const BenchmarkProviderResult& result) { return result.provider == left; });
        const auto rightResult = std::find_if(benchmarkResults_.begin(), benchmarkResults_.end(),
            [&](const BenchmarkProviderResult& result) { return result.provider == right; });

        const double leftScore = leftResult == benchmarkResults_.end() ? 0.0 : benchmarkScore(*leftResult);
        const double rightScore = rightResult == benchmarkResults_.end() ? 0.0 : benchmarkScore(*rightResult);
        return leftScore > rightScore;
    });
    return providers;
}

PriceProviderKind PriceService::chooseProvider(std::wstring* reason) const {
    DisplaySettings settingsCopy{};
    {
        std::lock_guard<std::mutex> settingsLock(settingsMutex_);
        settingsCopy = settings_;
    }

    std::lock_guard<std::mutex> stateLock(stateMutex_);
    const std::uint64_t now = unixNow();

    if (!settingsCopy.autoSourceSelection) {
        if (activeProvider_ != settingsCopy.preferredProvider) {
            const auto* activeStats = findProviderStatsLocked(activeProvider_);
            if (lastSwitchAt_ != 0 && now - lastSwitchAt_ < kSwitchObservationWindowSeconds) {
                if (reason) {
                    *reason = L"Manual mode keeps fallback provider during observation window";
                }
                return activeProvider_;
            }
            if (activeStats && activeStats->consecutiveFailures < static_cast<int>(kFailureSwitchThreshold)) {
                if (reason) {
                    *reason = L"Manual preferred provider currently using fallback source";
                }
                return activeProvider_;
            }
        }
        if (reason) {
            *reason = L"Manual preferred provider active";
        }
        return settingsCopy.preferredProvider;
    }

    const auto recommended = benchmarkRecommendedProvider();
    const auto* activeStats = findProviderStatsLocked(activeProvider_);

    if (!activeStats) {
        if (reason) {
            *reason = L"Missing active provider stats, fallback to benchmark recommendation";
        }
        return recommended;
    }

    if (activeStats->consecutiveFailures >= static_cast<int>(kFailureSwitchThreshold)) {
        for (const auto provider : fallbackProviders(recommended)) {
            if (provider != activeProvider_) {
                if (reason) {
                    *reason = L"Switched after 3 consecutive failures";
                }
                return provider;
            }
        }
    }

    if (lastSwitchAt_ != 0 && now - lastSwitchAt_ < kSwitchObservationWindowSeconds) {
        if (reason) {
            *reason = L"Observation window keeps current provider";
        }
        return activeProvider_;
    }

    if (activeStats->lastChangeAt != 0 && now - activeStats->lastChangeAt >= kNoChangeSwitchSeconds) {
        PriceProviderKind alternative = activeProvider_;
        double alternativeScore = -1.0;
        for (const auto provider : allProviders()) {
            if (provider == activeProvider_) {
                continue;
            }

            const auto* candidateStats = findProviderStatsLocked(provider);
            double candidateScore = 0.0;
            bool candidateActive = false;
            if (candidateStats && candidateStats->lastChangeAt != 0 &&
                now - candidateStats->lastChangeAt <= kAlternativeActivityWindowSeconds) {
                candidateScore += static_cast<double>(candidateStats->changeCount) * 10.0;
                candidateScore += successRate(*candidateStats) * 100.0;
                candidateActive = true;
            }

            const auto benchmarkIt = std::find_if(benchmarkResults_.begin(), benchmarkResults_.end(),
                [&](const BenchmarkProviderResult& result) { return result.provider == provider; });
            if (benchmarkIt != benchmarkResults_.end()) {
                candidateScore += benchmarkScore(*benchmarkIt);
                if (benchmarkIt->changeCount > 0) {
                    candidateActive = true;
                }
            }

            if (candidateActive && candidateScore > alternativeScore) {
                alternativeScore = candidateScore;
                alternative = provider;
            }
        }

        if (alternative != activeProvider_) {
            if (reason) {
                *reason = L"Switched because current provider had no price change for 15 seconds";
            }
            return alternative;
        }
    }

    if (recommended != activeProvider_ && lastSwitchAt_ == 0) {
        if (reason) {
            *reason = L"Startup benchmark recommendation";
        }
        return recommended;
    }

    if (reason) {
        *reason = L"Keep current provider";
    }
    return activeProvider_;
}

void PriceService::switchActiveProvider(PriceProviderKind provider, const std::wstring& reason) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (activeProvider_ == provider && !reason.empty()) {
        lastSwitchReason_ = reason;
        return;
    }
    activeProvider_ = provider;
    lastSwitchAt_ = unixNow();
    lastSwitchReason_ = reason;
}

PriceSnapshot PriceService::fetchSnapshot() {
    std::wstring switchReason;
    const auto desiredProvider = chooseProvider(&switchReason);
    bool shouldSwitch = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (activeProvider_ != desiredProvider && !switchReason.empty()) {
            shouldSwitch = true;
        } else if (!switchReason.empty()) {
            lastSwitchReason_ = switchReason;
        }
    }

    if (shouldSwitch) {
        switchActiveProvider(desiredProvider, switchReason);
    }

    PriceProviderKind providerToFetch{};
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        providerToFetch = activeProvider_;
    }

    const auto startedAt = std::chrono::steady_clock::now();
    try {
        auto snapshot = fetchFromProvider(providerToFetch);
        const auto latencyMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt).count());
        noteProviderSuccess(providerToFetch, snapshot, latencyMs);
        std::lock_guard<std::mutex> snapshotLock(snapshotMutex_);
        lastSuccessfulSnapshot_ = snapshot;
        hasLastSuccessfulSnapshot_ = true;
        return snapshot;
    } catch (const std::exception& error) {
        const auto latencyMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt).count());
        noteProviderFailure(providerToFetch, latencyMs, widenUtf8(error.what()));
    } catch (...) {
        const auto latencyMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt).count());
        noteProviderFailure(providerToFetch, latencyMs, L"unknown provider error");
    }

    std::lock_guard<std::mutex> stateLock(stateMutex_);
    const auto* failedStats = findProviderStatsLocked(providerToFetch);
    if (failedStats && failedStats->consecutiveFailures >= static_cast<int>(kFailureSwitchThreshold)) {
        for (const auto provider : fallbackProviders(benchmarkRecommendedProvider())) {
            if (provider != providerToFetch) {
                activeProvider_ = provider;
                lastSwitchAt_ = unixNow();
                lastSwitchReason_ = L"Prepared fallback after 3 consecutive failures";
                break;
            }
        }
    }

    std::lock_guard<std::mutex> snapshotLock(snapshotMutex_);
    if (hasLastSuccessfulSnapshot_) {
        return lastSuccessfulSnapshot_;
    }

    PriceSnapshot fallback{};
    fallback.source = L"offline";
    fallback.updatedAt = unixNow();
    fallback.sourceTimestamp = fallback.updatedAt;
    return fallback;
}

PriceSnapshot PriceService::fetchFromProvider(PriceProviderKind provider) {
    switch (provider) {
    case PriceProviderKind::GoldApi:
        return fetchFromGoldApi();
    case PriceProviderKind::Sina:
        return fetchFromSinaApi();
    case PriceProviderKind::Xwteam:
        return fetchFromXwteamApi();
    case PriceProviderKind::Auto:
    default:
        throw std::runtime_error("auto provider is not directly fetchable");
    }
}

PriceSnapshot PriceService::fetchFromGoldApi() const {
    const auto content = httpGetUtf8(L"api.gold-api.com", L"/price/XAU");
    PriceSnapshot snapshot{};
    snapshot.currentPrice = parseJsonDouble(content, "price");
    snapshot.openPrice = snapshot.currentPrice;
    snapshot.lowPrice = snapshot.currentPrice;
    snapshot.source = L"gold-api";
    snapshot.updatedAt = unixNow();
    snapshot.sourceTimestamp = snapshot.updatedAt;
    if (snapshot.currentPrice <= 0.0) {
        throw std::runtime_error("gold-api returned invalid price");
    }
    return snapshot;
}

PriceSnapshot PriceService::fetchFromSinaApi() const {
    const auto content = httpGetUtf8(L"hq.sinajs.cn", L"/list=hf_XAU", true,
        L"Referer: https://finance.sina.com.cn\r\n");
    const auto start = content.find('"');
    const auto end = content.rfind('"');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        throw std::runtime_error("sina payload missing quoted body");
    }

    const auto parts = split(content.substr(start + 1, end - start - 1), ',');
    if (parts.size() <= 8) {
        throw std::runtime_error("sina payload too short");
    }

    PriceSnapshot snapshot{};
    snapshot.currentPrice = std::stod(parts[0]);
    snapshot.lowPrice = std::stod(parts[5]);
    snapshot.openPrice = std::stod(parts[8]);
    snapshot.source = L"sina";
    snapshot.updatedAt = unixNow();
    snapshot.sourceTimestamp = snapshot.updatedAt;
    if (snapshot.currentPrice <= 0.0) {
        throw std::runtime_error("sina returned invalid price");
    }
    return snapshot;
}

PriceSnapshot PriceService::fetchFromXwteamApi() const {
    const auto content = httpGetUtf8(L"free.xwteam.cn", L"/api/gold/trade");
    if (content.find("\"code\":500") != std::string::npos) {
        throw std::runtime_error("xwteam upstream request failed");
    }
    if (content.find("\"GJ_Au\"") == std::string::npos && content.find("\"Symbol\":\"GJ_Au\"") == std::string::npos) {
        throw std::runtime_error("xwteam payload missing GJ_Au");
    }

    std::smatch match;
    const auto pattern = std::regex(R"gv("Symbol"\s*:\s*"GJ_Au".*?"BP"\s*:\s*"?([0-9]+(?:\.[0-9]+)?)"?.*?"SP"\s*:\s*"?([0-9]+(?:\.[0-9]+)?)"?)gv");
    if (!std::regex_search(content, match, pattern) || match.size() < 3) {
        throw std::runtime_error("xwteam GJ_Au quote parse failed");
    }

    PriceSnapshot snapshot{};
    const double buyPrice = std::stod(match[1].str());
    const double sellPrice = std::stod(match[2].str());
    snapshot.currentPrice = sellPrice > 0.0 ? sellPrice : buyPrice;
    snapshot.openPrice = snapshot.currentPrice;
    snapshot.lowPrice = buyPrice > 0.0 ? buyPrice : snapshot.currentPrice;
    snapshot.source = L"xwteam";
    snapshot.updatedAt = unixNow();
    snapshot.sourceTimestamp = snapshot.updatedAt;
    if (snapshot.currentPrice <= 0.0) {
        throw std::runtime_error("xwteam returned invalid price");
    }
    return snapshot;
}

void PriceService::noteProviderSuccess(PriceProviderKind provider, const PriceSnapshot& snapshot, int latencyMs) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    const auto now = unixNow();
    auto* stats = findProviderStatsLocked(provider);
    if (!stats) {
        return;
    }

    stats->successCount += 1;
    stats->consecutiveFailures = 0;
    stats->lastLatencyMs = latencyMs;
    stats->lastResponseAt = now;
    stats->lastSuccessAt = now;
    stats->lastError.clear();
    if (stats->lastValue <= 0.0 || std::abs(stats->lastValue - snapshot.currentPrice) > 0.0001) {
        stats->changeCount += 1;
        stats->lastChangeAt = now;
    }
    stats->lastValue = snapshot.currentPrice;

    activeProvider_ = provider;
}

void PriceService::noteProviderFailure(PriceProviderKind provider, int latencyMs, const std::wstring& error) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    const auto now = unixNow();
    auto* stats = findProviderStatsLocked(provider);
    if (!stats) {
        return;
    }

    stats->errorCount += 1;
    stats->consecutiveFailures += 1;
    stats->lastLatencyMs = latencyMs;
    stats->lastResponseAt = now;
    stats->lastError = error;
}

BenchmarkProviderResult PriceService::runProviderBenchmark(PriceProviderKind provider, std::vector<BenchmarkSample>* samples) const {
    BenchmarkProviderResult result{};
    result.provider = provider;
    std::vector<int> latencies;
    std::vector<std::uint64_t> changeTimes;
    double lastSeenValue = 0.0;

    for (int sampleIndex = 0; sampleIndex < kBenchmarkSampleCount && !benchmarkStopRequested_.load(); ++sampleIndex) {
        BenchmarkSample sample{};
        sample.provider = provider;
        sample.sampleTime = unixNow();
        const auto startedAt = std::chrono::steady_clock::now();

        try {
            const auto snapshot = const_cast<PriceService*>(this)->fetchFromProvider(provider);
            sample.responseMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt).count());
            sample.price = snapshot.currentPrice;
            sample.sourceTimestamp = snapshot.sourceTimestamp;
            sample.changed = lastSeenValue <= 0.0 || std::abs(lastSeenValue - snapshot.currentPrice) > 0.0001;

            result.sampleCount += 1;
            result.successCount += 1;
            latencies.push_back(sample.responseMs);
            if (sample.changed) {
                result.changeCount += 1;
                changeTimes.push_back(sample.sampleTime);
            }
            lastSeenValue = snapshot.currentPrice;
            result.lastValue = snapshot.currentPrice;
        } catch (const std::exception& error) {
            sample.responseMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt).count());
            sample.error = widenUtf8(error.what());
            result.sampleCount += 1;
            result.lastError = sample.error;
        } catch (...) {
            sample.responseMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt).count());
            sample.error = L"unknown benchmark error";
            result.sampleCount += 1;
            result.lastError = sample.error;
        }

        if (samples) {
            samples->push_back(sample);
        }

        if (sampleIndex + 1 < kBenchmarkSampleCount && !benchmarkStopRequested_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        result.medianLatencyMs = latencies[latencies.size() / 2];
    }

    if (changeTimes.size() > 1) {
        double totalIntervals = 0.0;
        for (size_t index = 1; index < changeTimes.size(); ++index) {
            totalIntervals += static_cast<double>(changeTimes[index] - changeTimes[index - 1]);
        }
        result.averageChangeIntervalSec = totalIntervals / static_cast<double>(changeTimes.size() - 1);
    }

    return result;
}

void PriceService::runBenchmarkAsync() {
    const auto providers = allProviders();
    std::vector<BenchmarkProviderResult> results(providers.size());
    std::vector<std::vector<BenchmarkSample>> samples(providers.size());
    std::vector<std::thread> workers;
    workers.reserve(providers.size());

    for (size_t index = 0; index < providers.size(); ++index) {
        workers.emplace_back([&, index]() {
            results[index] = runProviderBenchmark(providers[index], &samples[index]);
        });
    }

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    std::vector<BenchmarkSample> mergedSamples;
    for (auto& sampleSet : samples) {
        mergedSamples.insert(mergedSamples.end(), sampleSet.begin(), sampleSet.end());
    }
    std::sort(mergedSamples.begin(), mergedSamples.end(), [](const BenchmarkSample& left, const BenchmarkSample& right) {
        if (left.provider != right.provider) {
            return static_cast<int>(left.provider) < static_cast<int>(right.provider);
        }
        return left.sampleTime < right.sampleTime;
    });

    std::lock_guard<std::mutex> lock(stateMutex_);
    benchmarkResults_ = std::move(results);
    benchmarkSamples_ = std::move(mergedSamples);
    benchmarkRunning_ = false;
}

PriceServiceStatus PriceService::currentStatus() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    PriceServiceStatus status{};
    status.requestIntervalMs = intervalMs_.load();
    status.activeProvider = activeProvider_;
    status.benchmarkRecommendedProvider = benchmarkRecommendedProvider();
    status.benchmarkRunning = benchmarkRunning_;
    status.lastSwitchAt = lastSwitchAt_;
    status.lastSwitchReason = lastSwitchReason_;
    status.providerStats = providerStats_;
    status.benchmarkResults = benchmarkResults_;

    for (const auto& stats : providerStats_) {
        status.lastSuccessfulAt = (std::max)(status.lastSuccessfulAt, stats.lastSuccessAt);
        status.lastChangedAt = (std::max)(status.lastChangedAt, stats.lastChangeAt);
    }
    return status;
}

std::vector<BenchmarkSample> PriceService::benchmarkSamples() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return benchmarkSamples_;
}

std::wstring PriceService::statusSummary() const {
    const auto status = currentStatus();
    std::wstringstream stream;
    stream << L"请求频率: 1 秒/次\n";
    stream << L"目标更新: 2-3 秒有效更新\n";
    stream << L"当前活跃源: " << priceProviderLabel(status.activeProvider) << L"\n";
    stream << L"Benchmark 推荐源: " << priceProviderLabel(status.benchmarkRecommendedProvider) << L"\n";
    stream << L"最近切源: " << (status.lastSwitchReason.empty() ? L"--" : status.lastSwitchReason) << L"\n";
    stream << L"最近成功: " << formatTimestamp(status.lastSuccessfulAt) << L"\n";
    stream << L"最近变价: " << formatTimestamp(status.lastChangedAt) << L"\n";
    stream << L"Benchmark: " << (status.benchmarkRunning ? L"运行中" : L"未运行/已完成") << L"\n";

    for (const auto& stats : status.providerStats) {
        const double rate = successRate(stats) * 100.0;
        stream << L"\n[" << priceProviderLabel(stats.provider) << L"] "
               << L"成功率=" << static_cast<int>(rate) << L"% "
               << L"延迟=" << stats.lastLatencyMs << L"ms "
               << L"失败=" << stats.errorCount << L" "
               << L"变价=" << stats.changeCount << L"\n"
               << L"最近成功=" << formatTimestamp(stats.lastSuccessAt)
               << L" 最近变价=" << formatTimestamp(stats.lastChangeAt) << L"\n";
        if (!stats.lastError.empty()) {
            stream << L"最近错误=" << stats.lastError << L"\n";
        }
    }

    if (!status.benchmarkResults.empty()) {
        stream << L"\nBenchmark 结果:\n";
        for (const auto& result : status.benchmarkResults) {
            stream << L"- " << priceProviderLabel(result.provider)
                   << L" 样本=" << result.sampleCount
                   << L" 成功=" << result.successCount
                   << L" 变价=" << result.changeCount
                   << L" 中位延迟=" << result.medianLatencyMs << L"ms"
                   << L" 平均变价间隔=" << result.averageChangeIntervalSec << L"s";
            if (!result.lastError.empty()) {
                stream << L" 错误=" << result.lastError;
            }
            stream << L"\n";
        }
    }

    return stream.str();
}

std::wstring PriceService::httpGet(const std::wstring& host, const std::wstring& path, bool secure) const {
    const auto content = httpGetUtf8(host, path, secure);
    return widenUtf8(content);
}

std::string PriceService::httpGetUtf8(const std::wstring& host, const std::wstring& path, bool secure,
    const std::wstring& extraHeaders) const {
    auto session = WinHttpOpen(L"GoldViewNative/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        throw std::runtime_error("WinHttpOpen failed");
    }

    const INTERNET_PORT port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    auto connection = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        throw std::runtime_error("WinHttpConnect failed");
    }

    auto request = WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        throw std::runtime_error("WinHttpOpenRequest failed");
    }

    if (!extraHeaders.empty()) {
        WinHttpAddRequestHeaders(request, extraHeaders.c_str(), static_cast<ULONG>(extraHeaders.size()),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    std::string result;
    if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(request, nullptr)) {
        DWORD availableBytes = 0;
        do {
            availableBytes = 0;
            if (!WinHttpQueryDataAvailable(request, &availableBytes) || availableBytes == 0) {
                break;
            }
            std::string buffer(availableBytes, '\0');
            DWORD downloadedBytes = 0;
            if (!WinHttpReadData(request, buffer.data(), availableBytes, &downloadedBytes)) {
                break;
            }
            buffer.resize(downloadedBytes);
            result += buffer;
        } while (availableBytes > 0);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (result.empty()) {
        throw std::runtime_error("empty response");
    }
    return result;
}

}  // namespace goldview
