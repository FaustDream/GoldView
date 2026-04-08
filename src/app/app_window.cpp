#include "app.h"

namespace goldview {

namespace {

constexpr UINT kMenuSettings = 2001;
constexpr UINT kMenuCalculator = 2003;
constexpr UINT kMenuExit = 2099;
constexpr UINT_PTR kTaskbarRecoveryTimer = 3001;
constexpr UINT_PTR kUiRefreshTimer = 3002;
constexpr UINT kSnapshotMessage = WM_APP + 2;

}

void App::enableTaskbarRecoveryTimer() {
    if (hiddenWindow_) {
        SetTimer(hiddenWindow_, kTaskbarRecoveryTimer, 1000, nullptr);
    }
}

void App::disableTaskbarRecoveryTimer() {
    if (hiddenWindow_) {
        KillTimer(hiddenWindow_, kTaskbarRecoveryTimer);
    }
}

void App::enableUiRefreshTimer() {
    if (hiddenWindow_) {
        SetTimer(hiddenWindow_, kUiRefreshTimer, settings_.runtime.uiRefreshIntervalMs, nullptr);
    }
}

void App::disableUiRefreshTimer() {
    if (hiddenWindow_) {
        KillTimer(hiddenWindow_, kUiRefreshTimer);
    }
}

LRESULT CALLBACK App::hiddenWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto self = static_cast<App*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    const auto self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handleTrayMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT App::handleTrayMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == trayMessageId_) {
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            showTrayMenu();
        }
        if (lParam == WM_LBUTTONDBLCLK) {
            showCalculatorWindow();
        }
        return 0;
    }

    if (message == WM_COMMAND) {
        switch (LOWORD(wParam)) {
        case kMenuSettings:
            showSettingsWindow();
            return 0;
        case kMenuCalculator:
            showCalculatorWindow();
            return 0;
        case kMenuExit:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
    }

    if (message == kSnapshotMessage) {
        const auto snapshot = reinterpret_cast<PriceSnapshot*>(lParam);
        if (snapshot) {
            applySnapshot(*snapshot);
            delete snapshot;
        }
        return 0;
    }

    if (message == WM_TIMER && (wParam == kTaskbarRecoveryTimer || wParam == kUiRefreshTimer)) {
        if (wParam == kTaskbarRecoveryTimer) {
            const bool needRecovery = taskbarRefreshFailureCount_ > 0 ||
                !taskbarHost_->isAttachedToTaskbarContainer(lastTaskbarContainer_);
            if (needRecovery) {
                const auto refresh = refreshTaskbarLayout();
                if (refresh.shown && activeHost_) {
                    activeHost_->show();
                    activeHost_->updateContent(lastSnapshot_, settings_.display);
                }
            }
        }
        refreshSettingsWindow();
        return 0;
    }

    if (message == WM_DISPLAYCHANGE || message == WM_SETTINGCHANGE || message == WM_DPICHANGED) {
        const auto refresh = refreshTaskbarLayout();
        if (refresh.shown && activeHost_) {
            activeHost_->show();
            activeHost_->updateContent(lastSnapshot_, settings_.display);
        } else if (!refresh.shown) {
            taskbarLogger_.warn(L"Taskbar refresh did not show host: " + refresh.failureReason);
        }
        refreshSettingsWindow();
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace goldview
