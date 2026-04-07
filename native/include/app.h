#pragma once

#include <memory>
#include <windows.h>

#include "average_service.h"
#include "calculator_window.h"
#include "models.h"
#include "price_service.h"
#include "settings_window.h"
#include "settings_store.h"
#include "taskbar_anchor_resolver.h"
#include "taskbar_host.h"
#include "taskbar_logger.h"
#include "taskbar_metrics.h"
#include "taskbar_slot_controller.h"
#include "taskbar_topology_detector.h"
#include "taskbar_tray_detector.h"

namespace goldview {

class App {
public:
    explicit App(HINSTANCE instanceHandle);
    ~App();

    bool initialize();
    int run();
    void shutdown();

private:
    void createTrayIcon();
    void destroyTrayIcon();
    void rebuildMode();
    TaskbarRefreshResult refreshTaskbarLayout();
    void enableTaskbarRecoveryTimer();
    void disableTaskbarRecoveryTimer();
    void enableUiRefreshTimer();
    void disableUiRefreshTimer();
    void resetTaskbarModeState();
    void applySnapshot(const PriceSnapshot& snapshot);
    void showCalculatorWindow();
    void showSettingsWindow();
    void applySettings(const AppSettings& settings);
    void refreshSettingsWindow();

    static LRESULT CALLBACK hiddenWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleTrayMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void showTrayMenu();

    HINSTANCE instanceHandle_;
    HWND hiddenWindow_ = nullptr;
    NOTIFYICONDATAW trayIconData_{};
    UINT trayMessageId_ = WM_APP + 1;
    AppSettings settings_{defaultAppSettings()};
    PriceSnapshot lastSnapshot_{};
    int taskbarRefreshFailureCount_ = 0;
    HWND lastTaskbarContainer_ = nullptr;

    SettingsStore settingsStore_;
    AverageService averageService_;
    PriceService priceService_;
    TaskbarLogger taskbarLogger_;
    TaskbarTopologyDetector topologyDetector_;
    TaskbarTrayDetector trayDetector_;
    TaskbarMetrics taskbarMetrics_;
    TaskbarAnchorResolver anchorResolver_;
    TaskbarSlotController slotController_;
    std::unique_ptr<TaskbarHost> taskbarHost_;
    std::unique_ptr<CalculatorWindow> calculatorWindow_;
    std::unique_ptr<SettingsWindow> settingsWindow_;
    HostWindow* activeHost_ = nullptr;
};

}  // namespace goldview
