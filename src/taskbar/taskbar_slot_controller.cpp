#include "taskbar_slot_controller.h"

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

RECT screenRectToParentClient(HWND parent, const RECT& screenRect) {
    RECT clientRect = screenRect;
    POINT points[2]{
        {clientRect.left, clientRect.top},
        {clientRect.right, clientRect.bottom},
    };
    MapWindowPoints(HWND_DESKTOP, parent, points, 2);
    clientRect.left = points[0].x;
    clientRect.top = points[0].y;
    clientRect.right = points[1].x;
    clientRect.bottom = points[1].y;
    return clientRect;
}

}  // namespace

bool TaskbarSlotController::attach(const TaskbarTopology& topology, const RECT& desiredHostRect) {
    if (state_ && !isAttachedTo(topology)) {
        restore();
    }

    topology_ = topology;
    TaskbarSlotState slotState{};
    slotState.originalTaskListRect = topology.taskListRect;
    slotState.reservedWidth = desiredHostRect.right - desiredHostRect.left;
    state_ = slotState;
    updateLayout(topology, desiredHostRect);
    return state_.has_value();
}

void TaskbarSlotController::updateLayout(const TaskbarTopology& topology, const RECT& desiredHostRect) {
    if (!state_) {
        attach(topology, desiredHostRect);
        return;
    }

    if (!isAttachedTo(topology)) {
        attach(topology, desiredHostRect);
        return;
    }

    topology_ = topology;
    auto& slotState = *state_;
    const RECT original = slotState.originalTaskListRect;
    const RECT originalClient = screenRectToParentClient(topology.taskListParentWnd, original);
    slotState.reservedWidth = desiredHostRect.right - desiredHostRect.left;

    if (topology.horizontalTaskbar) {
        const int originalWidth = original.right - original.left;
        const int safeWidth = (std::clamp)(slotState.reservedWidth, 96, (std::max)(96, originalWidth / 2));
        const int taskTop = originalClient.top;
        const int taskHeight = original.bottom - original.top;
        const int leftOffset = originalClient.left;
        const int hostLeft = desiredHostRect.left;
        const bool hostBeforeTaskList = hostLeft <= original.left;

        if (hostBeforeTaskList) {
            const int newLeft = leftOffset + safeWidth;
            const int newWidth = (std::max)(80, originalWidth - safeWidth);
            MoveWindow(topology.taskListWnd, newLeft, taskTop, newWidth, taskHeight, TRUE);
        } else {
            const int newWidth = (std::max)(80, static_cast<int>(hostLeft - original.left - 2));
            MoveWindow(topology.taskListWnd, leftOffset, taskTop, newWidth, taskHeight, TRUE);
        }
    } else {
        const int originalHeight = original.bottom - original.top;
        const int desiredHeight = desiredHostRect.bottom - desiredHostRect.top;
        const int safeHeight = (std::clamp)(desiredHeight, 48, (std::max)(48, originalHeight / 2));
        const int newHeight = (std::max)(80, originalHeight - safeHeight);
        const int topOffset = originalClient.top;
        const int taskWidth = original.right - original.left;
        const int leftOffset = originalClient.left;
        const int width = desiredHostRect.right - desiredHostRect.left;
        const int left = original.left + (taskWidth - width) / 2;
        if (desiredHostRect.top <= original.top) {
            MoveWindow(topology.taskListWnd, leftOffset, topOffset + safeHeight, taskWidth, newHeight, TRUE);
            slotState.goldViewRect = makeRect(left, original.top, width, safeHeight);
        } else {
            MoveWindow(topology.taskListWnd, leftOffset, topOffset, taskWidth, newHeight, TRUE);
            slotState.goldViewRect = makeRect(left, original.top + newHeight + 2, width, safeHeight);
        }
    }

    slotState.goldViewRect = desiredHostRect;
    GetWindowRect(topology.taskListWnd, &slotState.currentTaskListRect);
    slotState.attached = true;
}

void TaskbarSlotController::restore() {
    if (!topology_ || !state_) {
        return;
    }

    const auto& topology = *topology_;
    const auto& slotState = *state_;
    if (!IsWindow(topology.taskListWnd)) {
        state_.reset();
        topology_.reset();
        return;
    }

    const RECT original = slotState.originalTaskListRect;
    const RECT originalClient = screenRectToParentClient(topology.taskListParentWnd, original);
    const int left = originalClient.left;
    const int top = originalClient.top;
    const int width = originalClient.right - originalClient.left;
    const int height = originalClient.bottom - originalClient.top;
    MoveWindow(topology.taskListWnd, left, top, width, height, TRUE);
    state_.reset();
    topology_.reset();
}

const std::optional<TaskbarSlotState>& TaskbarSlotController::state() const {
    return state_;
}

bool TaskbarSlotController::isAttachedTo(const TaskbarTopology& topology) const {
    if (!topology_ || !state_) {
        return false;
    }

    return topology_->shellTrayWnd == topology.shellTrayWnd &&
           topology_->hostContainerWnd == topology.hostContainerWnd &&
           topology_->taskListWnd == topology.taskListWnd;
}

}  // namespace goldview
