#include "taskbar_topology_detector.h"

#include <iterator>
#include <windows.h>

namespace goldview {

namespace {

bool isRectUsable(const RECT& rect, int minWidth, int minHeight) {
    return (rect.right - rect.left) >= minWidth && (rect.bottom - rect.top) >= minHeight;
}

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

std::wstring getWindowClassName(HWND hwnd) {
    wchar_t className[128]{};
    if (!hwnd) {
        return {};
    }

    GetClassNameW(hwnd, className, static_cast<int>(std::size(className)));
    return className;
}

}  // namespace

std::optional<TaskbarTopology> TaskbarTopologyDetector::detect() const {
    TaskbarTopology topology{};
    topology.shellTrayWnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!topology.shellTrayWnd) {
        return std::nullopt;
    }

    topology.hostContainerWnd = FindWindowExW(topology.shellTrayWnd, nullptr, L"ReBarWindow32", nullptr);
    if (!topology.hostContainerWnd) {
        topology.hostContainerWnd = FindWindowExW(topology.shellTrayWnd, nullptr, L"WorkerW", nullptr);
    }
    if (!topology.hostContainerWnd) {
        topology.hostContainerWnd = findDescendantByClass(topology.shellTrayWnd, L"ReBarWindow32");
    }
    if (!topology.hostContainerWnd) {
        topology.hostContainerWnd = findDescendantByClass(topology.shellTrayWnd, L"WorkerW");
    }
    if (!topology.hostContainerWnd) {
        return std::nullopt;
    }
    topology.hostContainerClassName = getWindowClassName(topology.hostContainerWnd);

    topology.taskListWnd = FindWindowExW(topology.hostContainerWnd, nullptr, L"MSTaskSwWClass", nullptr);
    if (!topology.taskListWnd) {
        topology.taskListWnd = FindWindowExW(topology.hostContainerWnd, nullptr, L"MSTaskListWClass", nullptr);
    }
    if (!topology.taskListWnd) {
        topology.taskListWnd = findDescendantByClass(topology.hostContainerWnd, L"MSTaskSwWClass");
    }
    if (!topology.taskListWnd) {
        topology.taskListWnd = findDescendantByClass(topology.hostContainerWnd, L"MSTaskListWClass");
    }
    if (!topology.taskListWnd) {
        return std::nullopt;
    }
    topology.taskListClassName = getWindowClassName(topology.taskListWnd);

    topology.taskListParentWnd = GetParent(topology.taskListWnd);
    if (!topology.taskListParentWnd) {
        topology.taskListParentWnd = topology.hostContainerWnd;
    }

    GetWindowRect(topology.shellTrayWnd, &topology.taskbarRect);
    GetWindowRect(topology.hostContainerWnd, &topology.hostContainerRect);
    GetWindowRect(topology.taskListWnd, &topology.taskListRect);
    topology.taskListUsable =
        IsWindowVisible(topology.taskListWnd) != FALSE &&
        isRectUsable(topology.taskListRect, 48, 18);
    topology.horizontalTaskbar =
        (topology.taskbarRect.right - topology.taskbarRect.left) >= (topology.taskbarRect.bottom - topology.taskbarRect.top);
    const int taskbarCenter = topology.taskbarRect.left + (topology.taskbarRect.right - topology.taskbarRect.left) / 2;
    const int taskListCenter = topology.taskListRect.left + (topology.taskListRect.right - topology.taskListRect.left) / 2;
    const int taskbarWidth = topology.taskbarRect.right - topology.taskbarRect.left;
    topology.centeredTaskbarButtons = abs(taskListCenter - taskbarCenter) < taskbarWidth / 5;
    return topology;
}

}  // namespace goldview
