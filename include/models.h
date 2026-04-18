#pragma once

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

enum class RuntimeLogLevel {
    Info,
    Warn,
    Error,
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

inline const wchar_t* quoteSourceKey(QuoteSourceKind source) {
    switch (source) {
    case QuoteSourceKind::Sina:
        return L"sina";
    case QuoteSourceKind::Xwteam:
        return L"xwteam";
    case QuoteSourceKind::GoldApi:
    default:
        return L"gold-api";
    }
}

inline const wchar_t* transportLabel(QuoteSourceTransport transport) {
    switch (transport) {
    case QuoteSourceTransport::Xhr:
        return L"XHR";
    case QuoteSourceTransport::Ws:
        return L"WebSocket";
    case QuoteSourceTransport::Html:
        return L"HTML";
    case QuoteSourceTransport::Api:
    default:
        return L"API";
    }
}

inline const wchar_t* runtimeLogLevelLabel(RuntimeLogLevel level) {
    switch (level) {
    case RuntimeLogLevel::Warn:
        return L"WARN";
    case RuntimeLogLevel::Error:
        return L"ERROR";
    case RuntimeLogLevel::Info:
    default:
        return L"INFO";
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
    bool autoRefreshEnabled = true;
    bool launchAtStartup = false;
    bool launchOnWorkdaysOnly = true;
    bool autoSwitchSource = true;
    QuoteSourceKind preferredSource = QuoteSourceKind::Sina;
    int successRateThreshold = 95;
    int latencyThresholdMs = 500;
    int recentOutputLimit = 50;
    int uiRefreshIntervalMs = 200;
    int activeRequestIntervalMs = 1000;
    int standbyRequestIntervalMs = 15000;
    int fallbackApiIntervalMs = 5000;
    int healthWindowSize = 20;
    int logLimit = 300;
};

struct QuoteSourceConfig {
    QuoteSourceKind kind = QuoteSourceKind::Sina;
    QuoteSourceTransport transport = QuoteSourceTransport::Xhr;
    bool enabled = true;
    int priority = 1;
    int weight = 100;
    std::wstring apiKey;
};

struct AppSettings {
    RuntimeSettings runtime;
    DisplaySettings display;
    std::vector<QuoteSourceConfig> sources;
};

inline AppSettings defaultAppSettings() {
    AppSettings settings{};
    settings.sources = {
        QuoteSourceConfig{QuoteSourceKind::Sina, QuoteSourceTransport::Xhr, true, 1, 100, L""},
        QuoteSourceConfig{QuoteSourceKind::Xwteam, QuoteSourceTransport::Xhr, true, 2, 90, L""},
        QuoteSourceConfig{QuoteSourceKind::GoldApi, QuoteSourceTransport::Api, true, 3, 80, L""},
    };
    return settings;
}

struct SourceHealthSnapshot {
    QuoteSourceKind kind = QuoteSourceKind::Sina;
    QuoteSourceTransport transport = QuoteSourceTransport::Xhr;
    bool enabled = true;
    bool authReady = true;
    bool active = false;
    int priority = 1;
    int weight = 100;
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

struct RuntimeLogEntry {
    RuntimeLogLevel level = RuntimeLogLevel::Info;
    std::uint64_t timestamp = 0;
    std::wstring message;
};

struct RecentOutputEntry {
    std::wstring text;
    std::wstring source;
    double price = 0.0;
    std::uint64_t timestamp = 0;
    bool delayed = false;
};

struct PriceServiceStatus {
    bool autoRefreshEnabled = true;
    bool delayed = false;
    int requestIntervalMs = 1000;
    QuoteSourceKind activeSource = QuoteSourceKind::Sina;
    std::uint64_t lastRequestedAt = 0;
    std::uint64_t lastSuccessfulAt = 0;
    std::uint64_t lastSwitchAt = 0;
    std::wstring lastSwitchReason;
    std::vector<SourceHealthSnapshot> sourceHealth;
    std::vector<RuntimeLogEntry> logs;
};

struct RuntimeViewState {
    PriceServiceStatus status;
    std::vector<RecentOutputEntry> recentOutputs;
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
