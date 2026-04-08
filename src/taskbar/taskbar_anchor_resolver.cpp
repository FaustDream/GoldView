#include "taskbar_anchor_resolver.h"

#include <algorithm>

namespace goldview {

namespace {

RECT makeRect(int left, int top, int width, int height) {
    RECT rect{};
    rect.left = left;
    rect.top = top;
    rect.right = left + width;
    rect.bottom = top + height;
    return rect;
}

bool isRectValid(const RECT& rect) {
    return rect.right > rect.left && rect.bottom > rect.top;
}

}  // namespace

TaskbarAnchor TaskbarAnchorResolver::resolve(const TaskbarTopology& topology, const TaskbarLayoutMetrics& metrics) const {
    TaskbarAnchor anchor{};
    anchor.reason = metrics.reason;

    if (!topology.horizontalTaskbar) {
        anchor.reason = L"Only horizontal bottom taskbar is supported in this round";
        return anchor;
    }

    const int height = metrics.desiredHeight;
    const int width = metrics.desiredWidth;
    const int spacing = metrics.horizontalPadding;
    const int hostTop = topology.taskbarRect.top +
                        ((topology.taskbarRect.bottom - topology.taskbarRect.top) - height) / 2;

    if (topology.taskListUsable && isRectValid(topology.taskListRect)) {
        const int safeLeft = topology.taskbarRect.left + spacing;
        const int safeRight = topology.taskListRect.right - spacing;
        anchor.anchorRect = topology.taskbarRect;
        anchor.safeRect = makeRect(safeLeft, hostTop, (std::max)(width, safeRight - safeLeft), height);
        anchor.hostRect = makeRect(safeLeft, hostTop, width, height);
        anchor.mode = TaskbarAnchor::Mode::TaskListLeft;
        anchor.reason = L"Anchored to absolute left edge of primary taskbar";
        return anchor;
    }

    if (!topology.taskListUsable && isRectValid(topology.trayRect)) {
        const int safeLeft = topology.taskbarRect.left + spacing;
        const int safeRight = topology.trayRect.left - spacing;
        if (safeRight - safeLeft >= width) {
            anchor.mode = TaskbarAnchor::Mode::PrimaryTrayFallback;
            anchor.anchorRect = topology.trayRect;
            anchor.safeRect = makeRect(safeLeft, hostTop, safeRight - safeLeft, height);
            anchor.hostRect = makeRect(safeLeft, hostTop, width, height);
            anchor.reason = L"Anchored to absolute left edge because task list is unusable";
            return anchor;
        }
    }

    if (isRectValid(topology.chevronRect)) {
        const int safeRight = topology.chevronRect.left - spacing;
        const int safeLeft = topology.taskListUsable ? topology.taskListRect.left + spacing : topology.taskbarRect.left + spacing;
        if (safeRight - safeLeft >= width) {
            anchor.mode = TaskbarAnchor::Mode::ChevronLeft;
            anchor.anchorRect = topology.chevronRect;
            anchor.safeRect = makeRect(safeLeft, hostTop, safeRight - safeLeft, height);
            anchor.hostRect = makeRect(safeRight - width, hostTop, width, height);
            anchor.reason = L"Anchored to chevron left";
            return anchor;
        }
    }

    if (isRectValid(topology.trayRect) && !topology.centeredTaskbarButtons) {
        const int safeRight = topology.trayRect.left - spacing;
        const int safeLeft = topology.taskListUsable ? topology.taskListRect.left + spacing : topology.taskbarRect.left + spacing;
        if (safeRight - safeLeft >= width) {
            anchor.mode = TaskbarAnchor::Mode::TrayLeft;
            anchor.anchorRect = topology.trayRect;
            anchor.safeRect = makeRect(safeLeft, hostTop, safeRight - safeLeft, height);
            anchor.hostRect = makeRect(safeRight - width, hostTop, width, height);
            anchor.reason = L"Anchored to tray left edge";
            return anchor;
        }
    }

    if (topology.centeredTaskbarButtons) {
        const int taskbarWidth = topology.taskbarRect.right - topology.taskbarRect.left;
        const int reservedSystemBand = (std::clamp)(taskbarWidth / 6, 180, 320);
        const int safeLeft = topology.taskbarRect.left + reservedSystemBand;
        const int safeRight = topology.taskListRect.left - spacing;
        if (safeRight - safeLeft >= width) {
            anchor.mode = TaskbarAnchor::Mode::SafeBandLeft;
            anchor.anchorRect = makeRect(safeLeft, topology.taskbarRect.top, safeRight - safeLeft, topology.taskbarRect.bottom - topology.taskbarRect.top);
            anchor.safeRect = makeRect(safeLeft, hostTop, safeRight - safeLeft, height);
            anchor.hostRect = makeRect(safeLeft, hostTop, width, height);
            anchor.reason = L"Anchored to left safe band for centered taskbar";
            return anchor;
        }
    }

    anchor.reason = L"No safe anchor slot available on bottom primary taskbar";
    return anchor;
}

}  // namespace goldview
