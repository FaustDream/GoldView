#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <windows.h>

namespace goldview {

enum class DisplayMode {
    Taskbar,
    Floating,
};

struct PriceSnapshot {
    double currentPrice = 0.0;
    double openPrice = 0.0;
    double lowPrice = 0.0;
    std::wstring source;
    std::uint64_t updatedAt = 0;
};

struct DisplaySettings {
    DisplayMode mode = DisplayMode::Taskbar;
    int refreshIntervalMs = 1000;
    std::wstring metalsDevApiKey;
    std::wstring metalPriceApiKey;
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
