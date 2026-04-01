#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace goldview {

enum class PriceProviderKind {
    Auto,
    GoldApi,
    Sina,
    Xwteam,
};

enum class TextAlignment {
    Left,
    Center,
};

inline const wchar_t* priceProviderLabel(PriceProviderKind provider) {
    switch (provider) {
    case PriceProviderKind::Auto:
        return L"自动调度";
    case PriceProviderKind::GoldApi:
        return L"Gold API";
    case PriceProviderKind::Sina:
        return L"新浪 hf_XAU";
    case PriceProviderKind::Xwteam:
        return L"XWTeam GJ_Au";
    default:
        return L"未知来源";
    }
}

struct PriceSnapshot {
    double currentPrice = 0.0;
    double openPrice = 0.0;
    double lowPrice = 0.0;
    std::wstring source;
    std::uint64_t updatedAt = 0;
    std::uint64_t sourceTimestamp = 0;
};

struct DisplaySettings {
    bool autoSourceSelection = true;
    PriceProviderKind preferredProvider = PriceProviderKind::GoldApi;
    std::wstring fontName = L"Consolas";
    int fontSize = 20;
    COLORREF textColor = RGB(241, 196, 15);
    COLORREF backgroundColor = RGB(17, 24, 39);
    bool backgroundTransparent = true;
    TextAlignment textAlignment = TextAlignment::Center;
    bool horizontalLayout = true;
};

struct ProviderStats {
    PriceProviderKind provider{PriceProviderKind::GoldApi};
    int successCount{0};
    int errorCount{0};
    int changeCount{0};
    int consecutiveFailures{0};
    int lastLatencyMs{0};
    std::uint64_t lastResponseAt{0};
    std::uint64_t lastSuccessAt{0};
    std::uint64_t lastChangeAt{0};
    double lastValue{0.0};
    std::wstring lastError;
};

struct BenchmarkSample {
    PriceProviderKind provider{PriceProviderKind::GoldApi};
    std::uint64_t sampleTime{0};
    int responseMs{0};
    double price{0.0};
    bool changed{false};
    std::wstring error;
    std::uint64_t sourceTimestamp{0};
};

struct BenchmarkProviderResult {
    PriceProviderKind provider{PriceProviderKind::GoldApi};
    int sampleCount{0};
    int successCount{0};
    int changeCount{0};
    int medianLatencyMs{0};
    double averageChangeIntervalSec{0.0};
    double lastValue{0.0};
    std::wstring lastError;
};

struct PriceServiceStatus {
    int requestIntervalMs{1000};
    PriceProviderKind activeProvider{PriceProviderKind::GoldApi};
    PriceProviderKind benchmarkRecommendedProvider{PriceProviderKind::GoldApi};
    bool benchmarkRunning{false};
    std::uint64_t lastSuccessfulAt{0};
    std::uint64_t lastChangedAt{0};
    std::uint64_t lastSwitchAt{0};
    std::wstring lastSwitchReason;
    std::vector<ProviderStats> providerStats;
    std::vector<BenchmarkProviderResult> benchmarkResults;
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
