#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace goldview {

enum class QuoteSourceKind {
    Sina,
    Xwteam,
    GoldApi,
};

enum class QuoteSourceTransport {
    Xhr,
    Api,
    Ws,
    Html,
};

enum class TextAlignment {
    Left,
    Center,
};

inline const wchar_t* quoteSourceLabel(QuoteSourceKind source) {
    switch (source) {
    case QuoteSourceKind::Sina:
        return L"新浪 hf_XAU";
    case QuoteSourceKind::Xwteam:
        return L"XWTeam GJ_Au";
    case QuoteSourceKind::GoldApi:
    default:
        return L"Gold API";
    }
}

struct PriceSnapshot {
    double currentPrice = 0.0;
    double openPrice = 0.0;
    double lowPrice = 0.0;
    std::wstring source;
    std::uint64_t updatedAt = 0;
    std::uint64_t sourceTimestamp = 0;
    bool delayed = false;
};

struct DisplaySettings {
    std::wstring fontName = L"Segoe UI";
    int fontSize = 18;
    COLORREF textColor = RGB(0, 0, 0);
    COLORREF backgroundColor = RGB(17, 24, 39);
    bool backgroundTransparent = true;
    TextAlignment textAlignment = TextAlignment::Center;
    bool horizontalLayout = true;
};

struct RuntimeSettings {
    bool launchAtStartup = false;
    bool launchOnWorkdaysOnly = true;
    int uiRefreshIntervalMs = 200;
    int activeRequestIntervalMs = 1000;
    int standbyRequestIntervalMs = 15000;
    int fallbackApiIntervalMs = 5000;
    int healthWindowSize = 20;
};

inline double normalizeDisplayedPrice(double price) {
    if (price <= 0.0) {
        return 0.0;
    }
    return std::round(price * 100.0) / 100.0;
}

struct QuoteSourceConfig {
    QuoteSourceKind kind = QuoteSourceKind::Sina;
    QuoteSourceTransport transport = QuoteSourceTransport::Xhr;
};

struct AppSettings {
    RuntimeSettings runtime;
    DisplaySettings display;
    std::vector<QuoteSourceConfig> sources;
};

inline AppSettings defaultAppSettings() {
    AppSettings settings{};
    settings.sources = {
        QuoteSourceConfig{QuoteSourceKind::Sina, QuoteSourceTransport::Xhr},
        QuoteSourceConfig{QuoteSourceKind::Xwteam, QuoteSourceTransport::Xhr},
        QuoteSourceConfig{QuoteSourceKind::GoldApi, QuoteSourceTransport::Api},
    };
    return settings;
}

struct SourceHealthSnapshot {
    QuoteSourceKind kind = QuoteSourceKind::Sina;
    QuoteSourceTransport transport = QuoteSourceTransport::Xhr;
    bool active = false;
    int requestCount = 0;
    int successCount = 0;
    int errorCount = 0;
    int averageLatencyMs = 0;
    int lastLatencyMs = 0;
    std::uint64_t lastRequestAt = 0;
    std::uint64_t lastSuccessAt = 0;
    double lastPrice = 0.0;
    std::wstring lastError;
};

struct PriceServiceStatus {
    bool delayed = false;
    int requestIntervalMs = 1000;
    QuoteSourceKind activeSource = QuoteSourceKind::Sina;
    std::uint64_t lastRequestedAt = 0;
    std::uint64_t lastSuccessfulAt = 0;
    std::uint64_t lastSwitchAt = 0;
    std::wstring lastSwitchReason;
    std::vector<SourceHealthSnapshot> sourceHealth;
};

struct TaskbarAnchor {
    enum class Mode {
        TaskListRight,
        TaskListLeft,
        PrimaryTrayFallback,
        ChevronLeft,
        TrayLeft,
        SafeBandLeft,
        Hidden,
    };

    Mode mode = Mode::Hidden;
    RECT anchorRect{};
    RECT safeRect{};
    RECT hostRect{};
    std::wstring reason;
};

struct TaskbarRefreshResult {
    bool shown{false};
    bool reparented{false};
    bool recoveryScheduled{false};
    std::wstring failureReason;
    std::wstring layoutMode;
    std::wstring taskbarContainerClass;
    std::wstring taskListClass;
    std::optional<RECT> taskbarRect;
    std::optional<RECT> taskListRect;
    std::optional<RECT> originalTaskListRect;
    std::optional<RECT> adjustedTaskListRect;
    std::optional<RECT> trayRect;
    std::optional<RECT> chevronRect;
    std::optional<RECT> hostRect;
    std::optional<TaskbarAnchor> anchor;
};

}  // namespace goldview
