#pragma once

#include <functional>
#include <string>
#include <windows.h>

#include "models.h"
#include "webui_window.h"

namespace goldview {

class SettingsWindow {
public:
    using SettingsSavedCallback = std::function<void(const AppSettings&)>;

    SettingsWindow();

    bool create(HINSTANCE instanceHandle);
    void focusOrShow();
    void destroy();
    bool isCreated() const;
    HWND hwnd() const;

    void setSettings(const AppSettings& settings);
    void setStatus(const PriceServiceStatus& status);
    void setSettingsSavedCallback(SettingsSavedCallback callback);

private:
    void handleWebMessage(const std::wstring& payload);
    void pushStateToView();
    AppSettings parseSettingsFromMessage(const std::wstring& payload) const;

    WebUiWindow webUi_;
    AppSettings settings_{defaultAppSettings()};
    PriceServiceStatus status_{};
    SettingsSavedCallback settingsSavedCallback_;
};

}  // namespace goldview
