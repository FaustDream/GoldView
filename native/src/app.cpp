#include "app.h"

#include <commctrl.h>
#include <cstdint>
#include <shellapi.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <vector>

#include "theme.h"

namespace goldview {

namespace {

constexpr UINT kTrayIconId = 1001;
constexpr UINT kMenuSettings = 2001;
constexpr UINT kMenuCalculator = 2003;
constexpr UINT kMenuExit = 2099;
constexpr wchar_t kHiddenClassName[] = L"GoldViewNativeHiddenWindow";
constexpr UINT_PTR kTaskbarRecoveryTimer = 3001;
constexpr UINT_PTR kUiRefreshTimer = 3002;
constexpr int kTaskbarRecoveryThreshold = 3;
constexpr UINT kSnapshotMessage = WM_APP + 2;

HICON createAppIcon() {
    constexpr int size = 64;
    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP colorBitmap = CreateCompatibleBitmap(screenDc, size, size);
    HBITMAP maskBitmap = CreateBitmap(size, size, 1, 1, nullptr);
    auto oldBitmap = SelectObject(memoryDc, colorBitmap);

    RECT rect{0, 0, size, size};
    HBRUSH background = CreateSolidBrush(theme::kInk);
    FillRect(memoryDc, &rect, background);
    DeleteObject(background);

    HBRUSH outerBrush = CreateSolidBrush(theme::kInk);
    HBRUSH innerBrush = CreateSolidBrush(theme::kGold);
    HPEN pen = CreatePen(PS_SOLID, 2, theme::kGoldDark);
    auto oldBrush = SelectObject(memoryDc, outerBrush);
    auto oldPen = SelectObject(memoryDc, pen);
    Ellipse(memoryDc, 4, 4, 60, 60);
    SelectObject(memoryDc, innerBrush);
    Ellipse(memoryDc, 10, 10, 54, 54);
    SetBkMode(memoryDc, TRANSPARENT);
    SetTextColor(memoryDc, theme::kInk);
    HFONT font = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    const auto oldFont = SelectObject(memoryDc, font);
    RECT textRect{0, 14, size, size};
    DrawTextW(memoryDc, L"GV", -1, &textRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

    SelectObject(memoryDc, oldFont);
    SelectObject(memoryDc, oldBrush);
    SelectObject(memoryDc, oldPen);
    DeleteObject(font);
    DeleteObject(outerBrush);
    DeleteObject(innerBrush);
    DeleteObject(pen);
    SelectObject(memoryDc, oldBitmap);

    ICONINFO iconInfo{};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = colorBitmap;
    iconInfo.hbmMask = maskBitmap;
    HICON icon = CreateIconIndirect(&iconInfo);

    DeleteObject(colorBitmap);
    DeleteObject(maskBitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return icon;
}

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

RecentOutputEntry buildRecentOutput(const PriceSnapshot& snapshot) {
    std::wstringstream stream;
    if (snapshot.updatedAt != 0) {
        std::time_t timeValue = static_cast<std::time_t>(snapshot.updatedAt);
        std::tm localTime{};
        localtime_s(&localTime, &timeValue);
        stream << std::put_time(&localTime, L"%H:%M:%S") << L"  ";
    }
    stream << std::fixed << std::setprecision(2) << snapshot.currentPrice;
    if (!snapshot.source.empty()) {
        stream << L"  " << snapshot.source;
    }
    return RecentOutputEntry{stream.str(), snapshot.source, snapshot.currentPrice, snapshot.updatedAt, snapshot.delayed};
}

}  // namespace

App::App(HINSTANCE instanceHandle)
    : instanceHandle_(instanceHandle),
      settingsStore_(instanceHandle) {}

App::~App() {
    shutdown();
}

bool App::initialize() {
    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&commonControls);

    settings_ = settingsStore_.load();
    settingsStore_.save(settings_);
    taskbarLogger_.info(L"Initializing GoldView native runtime");

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = App::hiddenWindowProc;
    windowClass.hInstance = instanceHandle_;
    windowClass.lpszClassName = kHiddenClassName;
    RegisterClassW(&windowClass);

    hiddenWindow_ = CreateWindowExW(
        0,
        kHiddenClassName,
        L"GoldView Hidden Window",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instanceHandle_,
        this);
    if (!hiddenWindow_) {
        taskbarLogger_.error(L"Failed to create hidden window");
        return false;
    }

    taskbarHost_ = std::make_unique<TaskbarHost>();
    calculatorWindow_ = std::make_unique<CalculatorWindow>(settingsStore_, averageService_);
    settingsWindow_ = std::make_unique<SettingsWindow>();
    if (!taskbarHost_->create(instanceHandle_)) {
        taskbarLogger_.error(L"Failed to create window hosts");
        return false;
    }

    settingsWindow_->setSettings(settings_);
    settingsWindow_->setSettingsSavedCallback([this](const AppSettings& settings) {
        applySettings(settings);
    });
    settingsWindow_->setClearLogsCallback([this]() {
        clearRuntimePanels();
    });

    createTrayIcon();
    rebuildMode();
    priceService_.updateSettings(settings_);
    enableUiRefreshTimer();
    refreshSettingsWindow();

    priceService_.start([this](const PriceSnapshot& snapshot) {
        if (hiddenWindow_) {
            auto payload = new PriceSnapshot(snapshot);
            PostMessageW(hiddenWindow_, kSnapshotMessage, 0, reinterpret_cast<LPARAM>(payload));
        }
    });
    return true;
}

int App::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void App::shutdown() {
    taskbarLogger_.info(L"Shutting down GoldView native runtime");
    disableTaskbarRecoveryTimer();
    disableUiRefreshTimer();
    priceService_.stop();
    slotController_.restore();
    destroyTrayIcon();
    if (taskbarHost_) {
        taskbarHost_->detachFromTaskbarContainer();
        taskbarHost_->hide();
    }
    if (calculatorWindow_) {
        calculatorWindow_->destroy();
    }
    if (settingsWindow_) {
        settingsWindow_->destroy();
    }
    if (hiddenWindow_) {
        DestroyWindow(hiddenWindow_);
        hiddenWindow_ = nullptr;
    }
}

void App::createTrayIcon() {
    trayIconData_.cbSize = sizeof(trayIconData_);
    trayIconData_.hWnd = hiddenWindow_;
    trayIconData_.uID = kTrayIconId;
    trayIconData_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayIconData_.uCallbackMessage = trayMessageId_;
    trayIconData_.hIcon = createAppIcon();
    wcscpy_s(trayIconData_.szTip, L"GoldView");
    Shell_NotifyIconW(NIM_ADD, &trayIconData_);
}

void App::destroyTrayIcon() {
    if (trayIconData_.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &trayIconData_);
        if (trayIconData_.hIcon) {
            DestroyIcon(trayIconData_.hIcon);
        }
        trayIconData_ = {};
    }
}

void App::rebuildMode() {
    taskbarHost_->hide();

    taskbarLogger_.info(L"Rebuilding fixed taskbar display");
    enableTaskbarRecoveryTimer();
    activeHost_ = static_cast<HostWindow*>(taskbarHost_.get());
    const auto refresh = refreshTaskbarLayout();
    if (refresh.shown && activeHost_) {
        activeHost_->show();
        activeHost_->updateContent(lastSnapshot_, settings_.display);
    } else {
        taskbarLogger_.warn(L"Fixed taskbar display hidden: " + refresh.failureReason);
        activeHost_ = nullptr;
    }
}

TaskbarRefreshResult App::refreshTaskbarLayout() {
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
            slotController_.restore();
            taskbarHost_->detachFromTaskbarContainer();
            taskbarHost_->hide();
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
            slotController_.restore();
            taskbarHost_->detachFromTaskbarContainer();
            taskbarHost_->hide();
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
            slotController_.restore();
            taskbarHost_->detachFromTaskbarContainer();
            taskbarHost_->hide();
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
            slotController_.restore();
            taskbarHost_->detachFromTaskbarContainer();
            taskbarHost_->hide();
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
    } else {
        if (slotController_.state()) {
            taskbarLogger_.info(L"slotController=restore layoutMode=" + layoutMode);
            slotController_.restore();
        } else {
            taskbarLogger_.info(L"slotController=skipped layoutMode=" + layoutMode);
        }
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

void App::resetTaskbarModeState() {
    taskbarRefreshFailureCount_ = 0;
    lastTaskbarContainer_ = nullptr;
}

void App::trimRecentOutputs() {
    const size_t limit = static_cast<size_t>((std::max)(10, settings_.runtime.recentOutputLimit));
    while (recentOutputs_.size() > limit) {
        recentOutputs_.pop_back();
    }
}

RuntimeViewState App::buildRuntimeViewState() const {
    RuntimeViewState state{};
    state.status = priceService_.currentStatus();
    state.recentOutputs.assign(recentOutputs_.begin(), recentOutputs_.end());
    return state;
}

void App::clearRuntimePanels() {
    recentOutputs_.clear();
    priceService_.clearRuntimeLogs();
    refreshSettingsWindow();
}

void App::applySnapshot(const PriceSnapshot& snapshot) {
    lastSnapshot_ = snapshot;
    if (snapshot.currentPrice > 0.0) {
        recentOutputs_.push_front(buildRecentOutput(snapshot));
        trimRecentOutputs();
    }
    if (activeHost_) {
        activeHost_->updateContent(snapshot, settings_.display);
    }
    refreshSettingsWindow();
}

void App::showCalculatorWindow() {
    if (calculatorWindow_) {
        if (!calculatorWindow_->isCreated()) {
            calculatorWindow_->create(instanceHandle_);
        }
        calculatorWindow_->focusOrShow();
    }
}

void App::showSettingsWindow() {
    if (settingsWindow_) {
        taskbarLogger_.info(L"Opening settings window");
        if (!settingsWindow_->isCreated()) {
            taskbarLogger_.info(L"Creating settings window on demand");
            settingsWindow_->create(instanceHandle_);
            settingsWindow_->setSettings(settings_);
            settingsWindow_->setSettingsSavedCallback([this](const AppSettings& settings) {
                applySettings(settings);
            });
            settingsWindow_->setClearLogsCallback([this]() {
                clearRuntimePanels();
            });
            refreshSettingsWindow();
        }
        taskbarLogger_.info(L"Focusing settings window");
        settingsWindow_->focusOrShow();
    }
}

void App::applySettings(const AppSettings& settings) {
    settings_ = settings;
    settingsStore_.save(settings_);
    priceService_.updateSettings(settings_);
    enableUiRefreshTimer();
    trimRecentOutputs();

    if (taskbarHost_) {
        taskbarHost_->updateContent(lastSnapshot_, settings_.display);
    }

    const auto refresh = refreshTaskbarLayout();
    if (refresh.shown && activeHost_) {
        activeHost_->show();
        activeHost_->updateContent(lastSnapshot_, settings_.display);
    }
    refreshSettingsWindow();
}

void App::refreshSettingsWindow() {
    if (settingsWindow_) {
        settingsWindow_->setSettings(settings_);
        settingsWindow_->updateRuntimeState(buildRuntimeViewState());
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

void App::showTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"设置");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuCalculator, L"均价计算");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出");

    POINT cursorPoint{};
    GetCursorPos(&cursorPoint);
    SetForegroundWindow(hiddenWindow_);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursorPoint.x, cursorPoint.y, 0, hiddenWindow_, nullptr);
    DestroyMenu(menu);
}

}  // namespace goldview
