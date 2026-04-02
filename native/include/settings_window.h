#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>
#include <windows.h>

#include "models.h"

namespace goldview {

class SettingsWindow {
public:
    using SettingsSavedCallback = std::function<void(const AppSettings&)>;
    using ClearLogsCallback = std::function<void()>;

    SettingsWindow() = default;

    bool create(HINSTANCE instanceHandle);
    void focusOrShow();
    void destroy();
    bool isCreated() const;
    HWND hwnd() const;

    void setSettings(const AppSettings& settings);
    void updateRuntimeState(const RuntimeViewState& runtimeState);
    void setSettingsSavedCallback(SettingsSavedCallback callback);
    void setClearLogsCallback(ClearLogsCallback callback);

private:
    enum class TabPage : int {
        Status = 0,
        Source = 1,
        Output = 2,
        Display = 3,
        General = 4,
        Count = 5
    };

    struct SourceRowControls {
        QuoteSourceKind kind{QuoteSourceKind::Sina};
        HWND enabledCheck{nullptr};
        HWND label{nullptr};
        HWND transportValue{nullptr};
        HWND priorityLabel{nullptr};
        HWND priorityEdit{nullptr};
        HWND weightLabel{nullptr};
        HWND weightEdit{nullptr};
    };

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void createControls();
    void layoutControls(int width, int height);
    void centerOnScreen() const;
    void releaseHandles();
    void applyFonts();
    void syncControlsFromSettings();
    void pullDraftFromControls();
    void refreshRuntimeText();
    void markDirty(bool dirty);
    bool confirmDiscardIfNeeded();
    void saveDraft();
    void cancelDraft();
    void updateColorButton(HWND button, COLORREF color) const;
    bool chooseColorFor(COLORREF& color);
    std::wstring formatHealthSummary() const;
    std::wstring formatRecentOutputs() const;
    void registerTabControl(TabPage page, HWND control);
    void showActiveTab();
    void updateWindowTitle() const;
    void refreshPreviewStyle();
    HBRUSH previewBrush() const;

    HINSTANCE instanceHandle_{nullptr};
    HWND windowHandle_{nullptr};
    HFONT sectionFont_{nullptr};
    HFONT bodyFont_{nullptr};
    HFONT monoFont_{nullptr};
    HFONT previewFont_{nullptr};
    mutable HBRUSH previewBrush_{nullptr};

    RECT tabRect_{};
    RECT contentRect_{};
    RECT actionBarRect_{};

    HWND tabControl_{nullptr};

    HWND statusGroup_{nullptr};
    HWND lastSuccessLabel_{nullptr};
    HWND lastSuccessValue_{nullptr};
    HWND lastRequestLabel_{nullptr};
    HWND lastRequestValue_{nullptr};
    HWND currentSourceLabel_{nullptr};
    HWND currentSourceValue_{nullptr};
    HWND switchReasonLabel_{nullptr};
    HWND switchReasonValue_{nullptr};
    HWND statsLabel_{nullptr};
    HWND statsEdit_{nullptr};

    HWND sourceGroup_{nullptr};
    HWND autoRefreshCheck_{nullptr};
    HWND autoSwitchCheck_{nullptr};
    HWND preferredSourceLabel_{nullptr};
    HWND preferredSourceCombo_{nullptr};
    HWND successThresholdLabel_{nullptr};
    HWND successThresholdEdit_{nullptr};
    HWND latencyThresholdLabel_{nullptr};
    HWND latencyThresholdEdit_{nullptr};
    HWND recentLimitLabel_{nullptr};
    HWND recentLimitEdit_{nullptr};

    std::array<SourceRowControls, 3> sourceRows_{};

    HWND outputGroup_{nullptr};
    HWND latestOutputLabel_{nullptr};
    HWND latestOutputValue_{nullptr};
    HWND recentOutputsLabel_{nullptr};
    HWND recentOutputsEdit_{nullptr};

    HWND displayGroup_{nullptr};
    HWND fontLabel_{nullptr};
    HWND fontCombo_{nullptr};
    HWND fontSizeLabel_{nullptr};
    HWND fontSizeCombo_{nullptr};
    HWND textColorLabel_{nullptr};
    HWND textColorButton_{nullptr};
    HWND backgroundColorLabel_{nullptr};
    HWND backgroundColorButton_{nullptr};
    HWND transparentCheck_{nullptr};
    HWND alignLabel_{nullptr};
    HWND alignLeftRadio_{nullptr};
    HWND alignCenterRadio_{nullptr};
    HWND horizontalLayoutCheck_{nullptr};
    HWND previewLabel_{nullptr};
    HWND previewBox_{nullptr};

    HWND generalGroup_{nullptr};
    HWND generalPlaceholder_{nullptr};

    HWND saveButton_{nullptr};
    HWND cancelButton_{nullptr};
    HWND clearLogsButton_{nullptr};

    std::array<std::vector<HWND>, static_cast<size_t>(TabPage::Count)> tabControls_{};
    int activeTab_{0};

    AppSettings savedSettings_{defaultAppSettings()};
    AppSettings draftSettings_{defaultAppSettings()};
    RuntimeViewState runtimeState_{};
    SettingsSavedCallback settingsSavedCallback_;
    ClearLogsCallback clearLogsCallback_;
    bool updatingControls_{false};
    bool dirty_{false};
};

}  // namespace goldview
