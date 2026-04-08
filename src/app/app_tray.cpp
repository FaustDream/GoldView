#include "app.h"

#include <shellapi.h>

#include "icon_utils.h"

namespace goldview {

namespace {

constexpr UINT kTrayIconId = 1001;
constexpr UINT kMenuSettings = 2001;
constexpr UINT kMenuCalculator = 2003;
constexpr UINT kMenuExit = 2099;
constexpr int kTrayIconSize = 32;

}

void App::createTrayIcon() {
    trayIconData_.cbSize = sizeof(trayIconData_);
    trayIconData_.hWnd = hiddenWindow_;
    trayIconData_.uID = kTrayIconId;
    trayIconData_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayIconData_.uCallbackMessage = trayMessageId_;
    trayIconData_.hIcon = loadAppIcon(instanceHandle_, kTrayIconSize);
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

void App::showTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuCalculator, L"Calculator");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

    POINT cursorPoint{};
    GetCursorPos(&cursorPoint);
    SetForegroundWindow(hiddenWindow_);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursorPoint.x, cursorPoint.y, 0, hiddenWindow_, nullptr);
    DestroyMenu(menu);
}

}  // namespace goldview
