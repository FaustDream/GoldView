#pragma once

#include <memory>
#include <windows.h>
#include <shellapi.h>

#include "models.h"
#include "price_service.h"
#include "taskbar_anchor_resolver.h"
#include "taskbar_host.h"
#include "taskbar_metrics.h"
#include "taskbar_slot_controller.h"
#include "taskbar_topology_detector.h"
#include "taskbar_tray_detector.h"

namespace goldview {

class App {
public:
    explicit App(HINSTANCE instanceHandle);
    ~App();

    bool initialize(int argc, wchar_t* argv[]);
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
    bool shouldSyncTaskbarFromSnapshot(const PriceSnapshot& snapshot) const;
    void syncTaskbarDisplay(bool forceLayoutRefresh);
    void applySnapshot(const PriceSnapshot& snapshot);
    void openCalculatorWebPage();
    bool setLaunchAtStartupEnabled(bool enabled);
    bool isLaunchAtStartupEnabled() const;

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
    bool autoStartLaunch_ = false;

    PriceService priceService_;
    TaskbarTopologyDetector topologyDetector_;
    TaskbarTrayDetector trayDetector_;
    TaskbarMetrics taskbarMetrics_;
    TaskbarAnchorResolver anchorResolver_;
    TaskbarSlotController slotController_;
    std::unique_ptr<TaskbarHost> taskbarHost_;
    HostWindow* activeHost_ = nullptr;
};

}  // namespace goldview
