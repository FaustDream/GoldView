#include "taskbar_tray_detector.h"

#include <iterator>

namespace goldview {

namespace {

HWND findDescendantByClass(HWND parent, const wchar_t* className) {
    if (!parent) {
        return nullptr;
    }

    for (HWND child = FindWindowExW(parent, nullptr, nullptr, nullptr); child; child = FindWindowExW(parent, child, nullptr, nullptr)) {
        wchar_t currentClass[128]{};
        GetClassNameW(child, currentClass, static_cast<int>(std::size(currentClass)));
        if (wcscmp(currentClass, className) == 0) {
            return child;
        }

        if (const HWND nested = findDescendantByClass(child, className)) {
            return nested;
        }
    }

    return nullptr;
}

void findLeftMostChevronCandidate(HWND parent, HWND& bestHwnd, RECT& bestRect) {
    if (!parent) {
        return;
    }

    for (HWND child = FindWindowExW(parent, nullptr, nullptr, nullptr); child; child = FindWindowExW(parent, child, nullptr, nullptr)) {
        wchar_t currentClass[128]{};
        GetClassNameW(child, currentClass, static_cast<int>(std::size(currentClass)));

        RECT rect{};
        if (GetWindowRect(child, &rect)) {
            const int width = rect.right - rect.left;
            const int height = rect.bottom - rect.top;
            if ((wcscmp(currentClass, L"Button") == 0 || wcscmp(currentClass, L"ToolbarWindow32") == 0) &&
                width > 8 && width < 64 && height > 8 && height < 64) {
                if (!bestHwnd || rect.left < bestRect.left) {
                    bestHwnd = child;
                    bestRect = rect;
                }
            }
        }

        findLeftMostChevronCandidate(child, bestHwnd, bestRect);
    }
}

}  // namespace

void TaskbarTrayDetector::enrich(TaskbarTopology& topology) const {
    topology.trayNotifyWnd = findDescendantByClass(topology.shellTrayWnd, L"TrayNotifyWnd");
    if (!topology.trayNotifyWnd) {
        topology.trayNotifyWnd = findDescendantByClass(topology.shellTrayWnd, L"NotifyIconOverflowWindow");
    }

    if (topology.trayNotifyWnd) {
        GetWindowRect(topology.trayNotifyWnd, &topology.trayRect);
    }

    HWND bestChevron = nullptr;
    RECT bestChevronRect{};
    findLeftMostChevronCandidate(topology.trayNotifyWnd, bestChevron, bestChevronRect);
    if (bestChevron) {
        topology.chevronWnd = bestChevron;
        topology.chevronRect = bestChevronRect;
    }
}

}  // namespace goldview
