#include "quote_source.h"

#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace goldview {

namespace {

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

std::vector<std::string> split(const std::string& content, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(content);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

std::string httpGetUtf8(const std::wstring& host, const std::wstring& path, bool secure,
    const std::wstring& extraHeaders = L"") {
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

class SinaQuoteSource final : public IQuoteSource {
public:
    QuoteSourceKind kind() const override { return QuoteSourceKind::Sina; }
    QuoteSourceTransport transport() const override { return QuoteSourceTransport::Xhr; }

    PriceSnapshot fetch(const QuoteSourceConfig&) override {
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
        snapshot.source = quoteSourceLabel(kind());
        snapshot.updatedAt = unixNow();
        snapshot.sourceTimestamp = snapshot.updatedAt;
        if (snapshot.currentPrice <= 0.0) {
            throw std::runtime_error("sina returned invalid price");
        }
        return snapshot;
    }
};

class XwteamQuoteSource final : public IQuoteSource {
public:
    QuoteSourceKind kind() const override { return QuoteSourceKind::Xwteam; }
    QuoteSourceTransport transport() const override { return QuoteSourceTransport::Xhr; }

    PriceSnapshot fetch(const QuoteSourceConfig&) override {
        const auto content = httpGetUtf8(L"free.xwteam.cn", L"/api/gold/trade", true);
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

        const double buyPrice = std::stod(match[1].str());
        const double sellPrice = std::stod(match[2].str());

        PriceSnapshot snapshot{};
        snapshot.currentPrice = sellPrice > 0.0 ? sellPrice : buyPrice;
        snapshot.openPrice = snapshot.currentPrice;
        snapshot.lowPrice = buyPrice > 0.0 ? buyPrice : snapshot.currentPrice;
        snapshot.source = quoteSourceLabel(kind());
        snapshot.updatedAt = unixNow();
        snapshot.sourceTimestamp = snapshot.updatedAt;
        if (snapshot.currentPrice <= 0.0) {
            throw std::runtime_error("xwteam returned invalid price");
        }
        return snapshot;
    }
};

class GoldApiQuoteSource final : public IQuoteSource {
public:
    QuoteSourceKind kind() const override { return QuoteSourceKind::GoldApi; }
    QuoteSourceTransport transport() const override { return QuoteSourceTransport::Api; }

    PriceSnapshot fetch(const QuoteSourceConfig&) override {
        const auto content = httpGetUtf8(L"api.gold-api.com", L"/price/XAU", true);

        PriceSnapshot snapshot{};
        snapshot.currentPrice = parseJsonDouble(content, "price");
        snapshot.openPrice = snapshot.currentPrice;
        snapshot.lowPrice = snapshot.currentPrice;
        snapshot.source = quoteSourceLabel(kind());
        snapshot.updatedAt = unixNow();
        snapshot.sourceTimestamp = snapshot.updatedAt;
        if (snapshot.currentPrice <= 0.0) {
            throw std::runtime_error("gold-api returned invalid price");
        }
        return snapshot;
    }
};

}  // namespace

std::unique_ptr<IQuoteSource> createQuoteSource(QuoteSourceKind kind) {
    switch (kind) {
    case QuoteSourceKind::Sina:
        return std::make_unique<SinaQuoteSource>();
    case QuoteSourceKind::Xwteam:
        return std::make_unique<XwteamQuoteSource>();
    case QuoteSourceKind::GoldApi:
    default:
        return std::make_unique<GoldApiQuoteSource>();
    }
}

}  // namespace goldview
