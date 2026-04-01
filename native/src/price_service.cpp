#include "price_service.h"

#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <regex>
#include <stdexcept>
#include <vector>

namespace goldview {

namespace {

std::uint64_t unixNow() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

double parseJsonDouble(const std::wstring& content, const std::wstring& key) {
    const auto pattern = std::wregex(key + L"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)");
    std::wsmatch match;
    if (std::regex_search(content, match, pattern) && match.size() > 1) {
        return std::stod(match[1].str());
    }
    return 0.0;
}

std::wstring urlEncode(const std::wstring& value) {
    std::wstring encoded;
    wchar_t buffer[8]{};
    for (const auto ch : value) {
        if ((ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || ch == L'-' || ch == L'_' || ch == L'.') {
            encoded.push_back(ch);
        } else {
            swprintf_s(buffer, L"%%%02X", static_cast<unsigned>(ch));
            encoded.append(buffer);
        }
    }
    return encoded;
}

}  // namespace

PriceService::PriceService() = default;

PriceService::~PriceService() {
    stop();
}

void PriceService::start(const DisplaySettings& settings, SnapshotCallback callback) {
    stop();
    intervalMs_.store(settings.refreshIntervalMs);
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback_ = std::move(callback);
    }
    {
        std::lock_guard<std::mutex> lock(settingsMutex_);
        settings_ = settings;
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

void PriceService::updateInterval(int intervalMs) {
    intervalMs_.store(intervalMs);
}

void PriceService::updateSettings(const DisplaySettings& settings) {
    intervalMs_.store(settings.refreshIntervalMs);
    std::lock_guard<std::mutex> lock(settingsMutex_);
    settings_ = settings;
}

DisplaySettings PriceService::settingsCopy() const {
    std::lock_guard<std::mutex> lock(settingsMutex_);
    return settings_;
}

void PriceService::run() {
    while (running_.load()) {
        const auto snapshot = fetchSnapshot();
        SnapshotCallback callbackCopy;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callbackCopy = callback_;
        }
        if (callbackCopy) {
            callbackCopy(snapshot);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs_.load()));
    }
}

PriceSnapshot PriceService::fetchSnapshot() const {
    const auto settings = settingsCopy();
    const std::vector<std::function<PriceSnapshot()>> providers{
        [this]() { return fetchFromGoldApi(); },
        [this]() { return fetchFromExchangeRateHost(); },
        [this, settings]() {
            if (settings.metalsDevApiKey.empty()) {
                throw std::runtime_error("metals.dev key missing");
            }
            return fetchFromMetalsDev();
        },
        [this, settings]() {
            if (settings.metalPriceApiKey.empty()) {
                throw std::runtime_error("metalpriceapi key missing");
            }
            return fetchFromMetalPriceApi();
        },
    };

    for (const auto& provider : providers) {
        try {
            const auto snapshot = provider();
            if (snapshot.currentPrice > 0.0) {
                return snapshot;
            }
        } catch (...) {
        }
    }

    PriceSnapshot fallback{};
    fallback.source = L"offline";
    fallback.updatedAt = unixNow();
    return fallback;
}

PriceSnapshot PriceService::fetchFromGoldApi() const {
    const auto content = httpGet(L"api.gold-api.com", L"/price/XAU");
    PriceSnapshot snapshot{};
    snapshot.currentPrice = parseJsonDouble(content, L"\"price\"");
    snapshot.openPrice = snapshot.currentPrice;
    snapshot.lowPrice = snapshot.currentPrice;
    snapshot.source = L"gold-api";
    snapshot.updatedAt = unixNow();
    return snapshot;
}

PriceSnapshot PriceService::fetchFromExchangeRateHost() const {
    const auto content = httpGet(L"api.exchangerate.host", L"/live?source=USD&currencies=XAU");
    const auto xauRate = parseJsonDouble(content, L"\"USDXAU\"");
    if (xauRate <= 0.0) {
        throw std::runtime_error("exchangerate.host missing USDXAU");
    }

    PriceSnapshot snapshot{};
    snapshot.currentPrice = 1.0 / xauRate * 31.1035;
    snapshot.openPrice = snapshot.currentPrice;
    snapshot.lowPrice = snapshot.currentPrice;
    snapshot.source = L"exchangerate.host";
    snapshot.updatedAt = unixNow();
    return snapshot;
}

PriceSnapshot PriceService::fetchFromMetalsDev() const {
    const auto settings = settingsCopy();
    const auto path = L"/api/latest?api_key=" + urlEncode(settings.metalsDevApiKey) + L"&currency=USD&unit=toz&metals=gold";
    const auto content = httpGet(L"metals.dev", path);
    PriceSnapshot snapshot{};
    snapshot.currentPrice = parseJsonDouble(content, L"\"gold\"");
    snapshot.openPrice = snapshot.currentPrice;
    snapshot.lowPrice = snapshot.currentPrice;
    snapshot.source = L"metals.dev";
    snapshot.updatedAt = unixNow();
    return snapshot;
}

PriceSnapshot PriceService::fetchFromMetalPriceApi() const {
    const auto settings = settingsCopy();
    const auto path = L"/api/latest?api_key=" + urlEncode(settings.metalPriceApiKey) + L"&base=USD&currencies=XAU";
    const auto content = httpGet(L"api.metalpriceapi.com", path);
    const auto xauRate = parseJsonDouble(content, L"\"XAU\"");
    if (xauRate <= 0.0) {
        throw std::runtime_error("metalpriceapi missing XAU");
    }

    PriceSnapshot snapshot{};
    snapshot.currentPrice = 1.0 / xauRate * 31.1035;
    snapshot.openPrice = snapshot.currentPrice;
    snapshot.lowPrice = snapshot.currentPrice;
    snapshot.source = L"metalpriceapi";
    snapshot.updatedAt = unixNow();
    return snapshot;
}

std::wstring PriceService::httpGet(const std::wstring& host, const std::wstring& path, bool secure) const {
    auto session = WinHttpOpen(L"GoldViewNative/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        throw std::runtime_error("WinHttpOpen failed");
    }

    auto connection = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
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

    std::wstring result;
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
            result.append(buffer.begin(), buffer.end());
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
