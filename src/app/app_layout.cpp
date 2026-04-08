#include "app.h"

#include <cstdint>
#include <sstream>

namespace goldview {

namespace {

constexpr int kTaskbarRecoveryThreshold = 3;

std::wstring rectToString(const RECT& rect) {
    std::wstringstream stream;
    stream << L"(" << rect.left << L"," << rect.top << L"," << rect.right << L"," << rect.bottom << L")";
    return stream.str();
}

std::wstring rectSizeToString(const RECT& rect) {
    std::wstringstream stream;
    stream << L"(" << (rect.right - rect.left) << L"," << (rect.bottom - rect.top) << L")";
    return stream.str();
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
        result.failureReason = L"Taskbar host window is not available";
        result.layoutMode = L"hidden";
        taskbarLogger_.error(result.failureReason);
        return result;
    }

    auto topology = topologyDetector_.detect();
    if (!topology) {
        taskbarRefreshFailureCount_ += 1;
        result.recoveryScheduled = true;
        result.failureReason = L"Failed to detect taskbar topology";
        result.layoutMode = L"hidden";
        taskbarLogger_.warn(result.failureReason + L", layoutMode=hidden, retry count=" + std::to_wstring(taskbarRefreshFailureCount_));
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

    taskbarLogger_.info(
        L"Topology detected taskbar=" + rectToString(topology->taskbarRect) +
        L" taskList=" + rectToString(topology->taskListRect) +
        L" taskListSize=" + rectSizeToString(topology->taskListRect) +
        L" taskListUsable=" + std::wstring(topology->taskListUsable ? L"true" : L"false") +
        L" hostClass=" + topology->hostContainerClassName +
        L" taskClass=" + topology->taskListClassName +
        L" tray=" + (topology->trayNotifyWnd ? rectToString(topology->trayRect) : std::wstring(L"(missing)")) +
        L" chevron=" + (topology->chevronWnd ? rectToString(topology->chevronRect) : std::wstring(L"(missing)")));

    const auto metrics = taskbarMetrics_.calculate(*topology, settings_.display);
    if (!metrics.available) {
        taskbarRefreshFailureCount_ += 1;
        result.recoveryScheduled = true;
        result.failureReason = metrics.reason;
        result.layoutMode = L"hidden";
        taskbarLogger_.warn(L"Taskbar metrics unavailable: " + metrics.reason + L", layoutMode=hidden, retry count=" + std::to_wstring(taskbarRefreshFailureCount_));
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
        taskbarLogger_.warn(L"Taskbar anchor unavailable: " + anchor.reason + L", layoutMode=hidden, retry count=" + std::to_wstring(taskbarRefreshFailureCount_));
        if (taskbarRefreshFailureCount_ >= kTaskbarRecoveryThreshold) {
            hideTaskbarHost();
        } else if (taskbarHost_->isAttachedToTaskbarContainer()) {
            result.shown = IsWindowVisible(taskbarHost_->hwnd()) != FALSE;
        }
        return result;
    }

    taskbarLogger_.info(
        L"Anchor resolved mode=" + std::to_wstring(static_cast<int>(anchor.mode)) +
        L" host=" + rectToString(anchor.hostRect) +
        L" safe=" + rectToString(anchor.safeRect) +
        L" reason=" + anchor.reason);

    bool useTaskbarReflow = topology->taskListUsable && metrics.prefersTaskbarReflow;
    RECT finalHostRect = anchor.hostRect;
    if (useTaskbarReflow) {
        const RECT candidateRectInParent = screenRectToParentClient(topology->hostContainerWnd, finalHostRect);
        if (!rectFitsInsideParentClient(topology->hostContainerWnd, candidateRectInParent)) {
            useTaskbarReflow = false;
            taskbarLogger_.warn(
                L"Taskbar reflow downgraded to absolute-left-fallback because host rect mapped outside parent client: " +
                rectToString(candidateRectInParent));
        }
    }

    const HWND hostParent = useTaskbarReflow ? topology->hostContainerWnd : topology->shellTrayWnd;
    const std::wstring layoutMode = useTaskbarReflow ? L"reflow" : L"absolute-left-fallback";
    result.layoutMode = layoutMode;

    const bool wasReparented = !taskbarHost_->isAttachedToTaskbarContainer(hostParent);
    if (!taskbarHost_->attachToTaskbarContainer(hostParent)) {
        taskbarRefreshFailureCount_ += 1;
        result.recoveryScheduled = true;
        result.failureReason = L"SetParent to taskbar container failed";
        result.layoutMode = L"hidden";
        taskbarLogger_.error(result.failureReason + L", layoutMode=hidden, retry count=" + std::to_wstring(taskbarRefreshFailureCount_) + L", lastError=" + std::to_wstring(GetLastError()));
        if (taskbarRefreshFailureCount_ >= kTaskbarRecoveryThreshold) {
            hideTaskbarHost();
        }
        return result;
    }

    result.reparented = wasReparented;
    lastTaskbarContainer_ = hostParent;
    taskbarLogger_.info(
        L"Taskbar host parent attached class=" +
        (useTaskbarReflow ? topology->hostContainerClassName : std::wstring(L"Shell_TrayWnd")) +
        L" hwnd=" + std::to_wstring(reinterpret_cast<std::uintptr_t>(hostParent)) +
        L" layoutMode=" + layoutMode);

    if (useTaskbarReflow) {
        if (slotController_.isAttachedTo(*topology)) {
            taskbarLogger_.info(L"slotController=update layoutMode=" + layoutMode);
            slotController_.updateLayout(*topology, anchor.hostRect);
        } else {
            taskbarLogger_.info(L"slotController=attach layoutMode=" + layoutMode);
            slotController_.attach(*topology, anchor.hostRect);
        }
    } else if (slotController_.state()) {
        taskbarLogger_.info(L"slotController=restore layoutMode=" + layoutMode);
        slotController_.restore();
    } else {
        taskbarLogger_.info(L"slotController=skipped layoutMode=" + layoutMode);
    }

    if (useTaskbarReflow) {
        const auto& state = slotController_.state();
        if (!state) {
            taskbarRefreshFailureCount_ += 1;
            result.recoveryScheduled = true;
            result.failureReason = L"Failed to attach taskbar slot controller";
            result.layoutMode = L"hidden";
            taskbarLogger_.error(result.failureReason + L", layoutMode=hidden, retry count=" + std::to_wstring(taskbarRefreshFailureCount_));
            if (taskbarRefreshFailureCount_ >= kTaskbarRecoveryThreshold) {
                taskbarHost_->detachFromTaskbarContainer();
                taskbarHost_->hide();
            }
            return result;
        }

        taskbarLogger_.info(
            L"Task list original=" + rectToString(state->originalTaskListRect) +
            L" adjusted=" + rectToString(state->currentTaskListRect));
        result.originalTaskListRect = state->originalTaskListRect;
        result.adjustedTaskListRect = state->currentTaskListRect;
        finalHostRect = state->goldViewRect;
    }

    const RECT hostRectInParent = screenRectToParentClient(hostParent, finalHostRect);
    taskbarHost_->applyBoundsInParent(hostRectInParent);
    taskbarHost_->show();
    taskbarRefreshFailureCount_ = 0;
    taskbarLogger_.info(
        L"Taskbar host bounds applied screen=" + rectToString(finalHostRect) +
        L" parent=" + rectToString(hostRectInParent) +
        L" layoutMode=" + layoutMode);
    result.hostRect = finalHostRect;
    result.shown = true;
    return result;
}

}  // namespace goldview
