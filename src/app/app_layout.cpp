#include "app.h"

namespace goldview {

namespace {

constexpr int kTaskbarRecoveryThreshold = 3;

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

bool rectFitsInsideParentClient(HWND parent, const RECT& rectInParent) {
    RECT parentClientRect{};
    if (!GetClientRect(parent, &parentClientRect)) {
        return false;
    }

    return rectInParent.left >= parentClientRect.left &&
           rectInParent.top >= parentClientRect.top &&
           rectInParent.right <= parentClientRect.right &&
           rectInParent.bottom <= parentClientRect.bottom;
}

}

TaskbarRefreshResult App::refreshTaskbarLayout() {
    const auto hideTaskbarHost = [this]() {
        slotController_.restore();
        taskbarHost_->detachFromTaskbarContainer();
        taskbarHost_->hide();
    };

    TaskbarRefreshResult result{};
    if (!taskbarHost_) {
        result.failureReason = L"任务栏宿主窗口不可用";
        result.layoutMode = L"hidden";
        return result;
    }

    auto topology = topologyDetector_.detect();
    if (!topology) {
        taskbarRefreshFailureCount_ += 1;
        result.recoveryScheduled = true;
        result.failureReason = L"任务栏拓扑检测失败";
        result.layoutMode = L"hidden";
        if (taskbarRefreshFailureCount_ >= kTaskbarRecoveryThreshold) {
            hideTaskbarHost();
        } else if (taskbarHost_->isAttachedToTaskbarContainer()) {
            result.shown = IsWindowVisible(taskbarHost_->hwnd()) != FALSE;
        }
        return result;
    }

    trayDetector_.enrich(*topology);
    result.taskbarRect = topology->taskbarRect;
    result.taskListRect = topology->taskListRect;
    if (topology->trayNotifyWnd) {
        result.trayRect = topology->trayRect;
    }
    if (topology->chevronWnd) {
        result.chevronRect = topology->chevronRect;
    }
    result.taskbarContainerClass = topology->hostContainerClassName;
    result.taskListClass = topology->taskListClassName;

    const auto metrics = taskbarMetrics_.calculate(*topology, settings_.display);
    if (!metrics.available) {
        taskbarRefreshFailureCount_ += 1;
        result.recoveryScheduled = true;
        result.failureReason = metrics.reason;
        result.layoutMode = L"hidden";
        if (taskbarRefreshFailureCount_ >= kTaskbarRecoveryThreshold) {
            hideTaskbarHost();
        } else if (taskbarHost_->isAttachedToTaskbarContainer()) {
            result.shown = IsWindowVisible(taskbarHost_->hwnd()) != FALSE;
        }
        return result;
    }

    TaskbarTopology anchorTopology = *topology;
    if (slotController_.isAttachedTo(*topology)) {
        const auto& existingState = slotController_.state();
        if (existingState) {
            anchorTopology.taskListRect = existingState->originalTaskListRect;
            result.originalTaskListRect = existingState->originalTaskListRect;
        }
    }

    const auto anchor = anchorResolver_.resolve(anchorTopology, metrics);
    result.anchor = anchor;
    if (anchor.mode == TaskbarAnchor::Mode::Hidden) {
        taskbarRefreshFailureCount_ += 1;
        result.recoveryScheduled = true;
        result.failureReason = anchor.reason;
        result.layoutMode = L"hidden";
        if (taskbarRefreshFailureCount_ >= kTaskbarRecoveryThreshold) {
            hideTaskbarHost();
        } else if (taskbarHost_->isAttachedToTaskbarContainer()) {
            result.shown = IsWindowVisible(taskbarHost_->hwnd()) != FALSE;
        }
        return result;
    }

    bool useTaskbarReflow = topology->taskListUsable && metrics.prefersTaskbarReflow;
    RECT finalHostRect = anchor.hostRect;
    if (useTaskbarReflow) {
        const RECT candidateRectInParent = screenRectToParentClient(topology->hostContainerWnd, finalHostRect);
        if (!rectFitsInsideParentClient(topology->hostContainerWnd, candidateRectInParent)) {
            useTaskbarReflow = false;
        }
    }

    const HWND hostParent = useTaskbarReflow ? topology->hostContainerWnd : topology->shellTrayWnd;
    const std::wstring layoutMode = useTaskbarReflow ? L"reflow" : L"absolute-left-fallback";
    result.layoutMode = layoutMode;

    const bool wasReparented = !taskbarHost_->isAttachedToTaskbarContainer(hostParent);
    if (!taskbarHost_->attachToTaskbarContainer(hostParent)) {
        taskbarRefreshFailureCount_ += 1;
        result.recoveryScheduled = true;
        result.failureReason = L"挂载到任务栏容器失败";
        result.layoutMode = L"hidden";
        if (taskbarRefreshFailureCount_ >= kTaskbarRecoveryThreshold) {
            hideTaskbarHost();
        }
        return result;
    }

    result.reparented = wasReparented;
    lastTaskbarContainer_ = hostParent;

    if (useTaskbarReflow) {
        if (slotController_.isAttachedTo(*topology)) {
            slotController_.updateLayout(*topology, anchor.hostRect);
        } else {
            slotController_.attach(*topology, anchor.hostRect);
        }
    } else if (slotController_.state()) {
        slotController_.restore();
    }

    if (useTaskbarReflow) {
        const auto& state = slotController_.state();
        if (!state) {
            taskbarRefreshFailureCount_ += 1;
            result.recoveryScheduled = true;
            result.failureReason = L"任务栏槽位控制器挂载失败";
            result.layoutMode = L"hidden";
            if (taskbarRefreshFailureCount_ >= kTaskbarRecoveryThreshold) {
                taskbarHost_->detachFromTaskbarContainer();
                taskbarHost_->hide();
            }
            return result;
        }

        result.originalTaskListRect = state->originalTaskListRect;
        result.adjustedTaskListRect = state->currentTaskListRect;
        finalHostRect = state->goldViewRect;
    }

    const RECT hostRectInParent = screenRectToParentClient(hostParent, finalHostRect);
    taskbarHost_->applyBoundsInParent(hostRectInParent);
    taskbarHost_->show();
    taskbarRefreshFailureCount_ = 0;
    result.hostRect = finalHostRect;
    result.shown = true;
    return result;
}

}  // namespace goldview
