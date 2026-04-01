#pragma once

#include <optional>

#include "taskbar_topology_detector.h"

namespace goldview {

struct TaskbarSlotState {
    RECT originalTaskListRect{};
    RECT currentTaskListRect{};
    RECT goldViewRect{};
    int reservedWidth{};
    bool attached{false};
};

class TaskbarSlotController {
public:
    bool attach(const TaskbarTopology& topology, const RECT& desiredHostRect);
    void updateLayout(const TaskbarTopology& topology, const RECT& desiredHostRect);
    void restore();
    const std::optional<TaskbarSlotState>& state() const;
    bool isAttachedTo(const TaskbarTopology& topology) const;

private:
    std::optional<TaskbarTopology> topology_;
    std::optional<TaskbarSlotState> state_;
};

}  // namespace goldview
