#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

#include "models.h"

namespace goldview {

class SettingsWindow {
public:
    using SettingsChangedCallback = std::function<void(const DisplaySettings&)>;
    using BenchmarkRequestCallback = std::function<void()>;

    SettingsWindow() = default;

    bool create(HINSTANCE instanceHandle);
    void show();
    void focusOrShow();
    void destroy();
    bool isCreated() const;
    HWND hwnd() const;

    void setSettings(const DisplaySettings& settings);
    void updateRuntimeStatus(const PriceServiceStatus& status);
    void setSettingsChangedCallback(SettingsChangedCallback callback);
    void setBenchmarkRequestCallback(BenchmarkRequestCallback callback);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void createControls();
    void layoutControls(int width, int height);
    void paint();
    void centerOnScreen() const;
    void releaseHandles();
    void applyFonts();
    void syncControlsFromSettings();
    void syncControlsVisibility();
    void applyControlChanges();
    void refreshStatusText();
    void updateColorButton(HWND button, COLORREF color) const;
    bool chooseColorFor(COLORREF& color);
    std::wstring statusSummaryText() const;
    std::wstring statsTableText() const;

    HINSTANCE instanceHandle_ = nullptr;
    HWND windowHandle_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT sectionFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT monoFont_ = nullptr;

    HWND autoSourceCheck_ = nullptr;
    HWND preferredProviderCombo_ = nullptr;
    HWND requestRateLabel_ = nullptr;
    HWND activeProviderValue_ = nullptr;
    HWND lastSuccessValue_ = nullptr;
    HWND lastChangeValue_ = nullptr;
    HWND switchReasonValue_ = nullptr;
    HWND statsEdit_ = nullptr;
    HWND benchmarkButton_ = nullptr;

    HWND fontCombo_ = nullptr;
    HWND fontSizeCombo_ = nullptr;
    HWND textColorButton_ = nullptr;
    HWND backgroundColorButton_ = nullptr;
    HWND transparentCheck_ = nullptr;
    HWND alignLeftRadio_ = nullptr;
    HWND alignCenterRadio_ = nullptr;
    HWND horizontalLayoutCheck_ = nullptr;

    RECT dataTabRect_{};
    RECT displayTabRect_{};
    RECT cardRect_{};
    RECT previewSummaryRect_{};
    RECT previewRect_{};
    DisplaySettings settings_{};
    PriceServiceStatus status_{};
    SettingsChangedCallback settingsChangedCallback_;
    BenchmarkRequestCallback benchmarkRequestCallback_;
    bool updatingControls_{false};
    int activeTab_{0};
    std::vector<HWND> dataPageControls_;
    std::vector<HWND> displayPageControls_;
};

}  // namespace goldview
