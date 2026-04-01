#include "taskbar_metrics.h"

#include <algorithm>

namespace goldview {

namespace {

bool isRectValid(const RECT& rect) {
    return rect.right > rect.left && rect.bottom > rect.top;
}

}  // namespace

TaskbarLayoutMetrics TaskbarMetrics::calculate(const TaskbarTopology& topology, const DisplaySettings&) const {
    TaskbarLayoutMetrics metrics{};
    const int taskListWidth = topology.taskListRect.right - topology.taskListRect.left;
    const int taskListHeight = topology.taskListRect.bottom - topology.taskListRect.top;
    const int hostWidth = topology.hostContainerRect.right - topology.hostContainerRect.left;
    const int hostHeight = topology.hostContainerRect.bottom - topology.hostContainerRect.top;
    const int leftGap = topology.taskListRect.left - topology.hostContainerRect.left;
    const int rightGap = topology.hostContainerRect.right - topology.taskListRect.right;

    if (topology.horizontalTaskbar) {
        const int thickness = topology.taskbarRect.bottom - topology.taskbarRect.top;
        const int width = 144;

        if (!topology.taskListUsable) {
            if (!isRectValid(topology.trayRect)) {
                metrics.available = false;
                metrics.reason = L"Tray rect is unavailable for primary taskbar fallback";
                return metrics;
            }

            const int traySafeWidth = topology.trayRect.left - topology.taskbarRect.left - 12;
            if (traySafeWidth < 120) {
                metrics.available = false;
                metrics.reason = L"Primary taskbar tray fallback does not have enough width";
                return metrics;
            }

            metrics.prefersTaskbarReflow = false;
            metrics.desiredHeight = (std::clamp)(thickness - 12, 20, 36);
            metrics.desiredWidth = (std::clamp)(width, 120, (std::min)(traySafeWidth, 240));
            metrics.horizontalPadding = 8;
            metrics.verticalPadding = 4;
            metrics.reason = L"Calculated primary tray fallback metrics";
            return metrics;
        }

        if (taskListHeight < 18) {
            metrics.available = false;
            metrics.reason = L"Task list height is too small for taskbar slot";
            return metrics;
        }

        metrics.desiredHeight = (std::clamp)((std::min)(thickness - 10, taskListHeight - 4), 20, 36);

        const int maxWidth = (std::max)(120, (std::max)(taskListWidth * 2, hostWidth / 8));
        const int hostSafeWidth = (std::max)(120, hostWidth / 4);
        metrics.desiredWidth = (std::clamp)(width, 120, (std::min)(maxWidth, hostSafeWidth));
        metrics.horizontalPadding = rightGap < 12 && leftGap > rightGap ? 10 : 6;
        metrics.verticalPadding = 4;
        metrics.prefersTaskbarReflow = true;
        metrics.reason = L"Calculated horizontal taskbar metrics";
    } else {
        if (taskListHeight < 120 || taskListWidth < 24) {
            metrics.available = false;
            metrics.reason = L"Vertical taskbar area too small";
            return metrics;
        }

        const int thickness = topology.taskbarRect.right - topology.taskbarRect.left;
        metrics.desiredWidth = (std::clamp)(thickness - 8, 48, 96);
        metrics.desiredHeight = (std::clamp)(taskListHeight / 4, 48, (std::max)(64, hostHeight / 3));
        metrics.reason = L"Calculated vertical taskbar metrics";
    }

    return metrics;
}

}  // namespace goldview
