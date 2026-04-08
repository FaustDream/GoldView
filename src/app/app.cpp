#include "app.h"

#include <commctrl.h>

#include "icon_utils.h"

namespace goldview {

namespace {

constexpr wchar_t kHiddenClassName[] = L"GoldViewNativeHiddenWindow";

}

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

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = App::hiddenWindowProc;
    windowClass.hInstance = instanceHandle_;
    windowClass.lpszClassName = kHiddenClassName;
    windowClass.hIcon = loadAppIcon(instanceHandle_, 32);
    windowClass.hIconSm = loadAppIcon(instanceHandle_, 16);
    RegisterClassExW(&windowClass);

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
    calculatorWindow_ = std::make_unique<CalculatorWindow>(averageService_);
    settingsWindow_ = std::make_unique<SettingsWindow>();
    if (!taskbarHost_->create(instanceHandle_)) {
        taskbarLogger_.error(L"Failed to create window hosts");
        return false;
    }

    settingsWindow_->setSettings(settings_);
    settingsWindow_->setStatus(priceService_.currentStatus());
    settingsWindow_->setSettingsSavedCallback([this](const AppSettings& settings) {
        applySettings(settings);
    });

    createTrayIcon();
    rebuildMode();
    priceService_.updateSettings(settings_);
    enableUiRefreshTimer();
    refreshSettingsWindow();

    priceService_.start([this](const PriceSnapshot& snapshot) {
        if (hiddenWindow_) {
            constexpr UINT kSnapshotMessage = WM_APP + 2;
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

void App::resetTaskbarModeState() {
    taskbarRefreshFailureCount_ = 0;
    lastTaskbarContainer_ = nullptr;
}

void App::applySnapshot(const PriceSnapshot& snapshot) {
    lastSnapshot_ = snapshot;
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
        taskbarLogger_.info(L"Opening source control window");
        if (!settingsWindow_->isCreated()) {
            taskbarLogger_.info(L"Creating source control window on demand");
            settingsWindow_->create(instanceHandle_);
            settingsWindow_->setSettings(settings_);
            settingsWindow_->setStatus(priceService_.currentStatus());
            settingsWindow_->setSettingsSavedCallback([this](const AppSettings& settings) {
                applySettings(settings);
            });
        }
        taskbarLogger_.info(L"Focusing source control window");
        settingsWindow_->focusOrShow();
    }
}

void App::applySettings(const AppSettings& settings) {
    settings_ = settings;
    settingsStore_.save(settings_);
    priceService_.updateSettings(settings_);
    enableUiRefreshTimer();

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
        settingsWindow_->setStatus(priceService_.currentStatus());
    }
}

}  // namespace goldview
