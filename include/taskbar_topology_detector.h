#pragma once

#include <optional>
#include <string>

#include <windows.h>

namespace goldview {

struct TaskbarTopology {
    HWND shellTrayWnd{};
    HWND hostContainerWnd{};
    HWND taskListWnd{};
    HWND taskListParentWnd{};
    HWND trayNotifyWnd{};
    HWND chevronWnd{};
    RECT taskbarRect{};
    RECT hostContainerRect{};
    RECT taskListRect{};
    RECT trayRect{};
    RECT chevronRect{};
    bool horizontalTaskbar{true};
    bool centeredTaskbarButtons{false};
    bool taskListUsable{false};
    std::wstring hostContainerClassName;
    std::wstring taskListClassName;
};

class TaskbarTopologyDetector {
public:
    std::optional<TaskbarTopology> detect() const;
};

}  // namespace goldview
