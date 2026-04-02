#include "settings_window.h"

#include <algorithm>
#include <array>
#include <commctrl.h>
#include <commdlg.h>
#include <ctime>
#include <sstream>

#include "theme.h"

namespace goldview {

namespace {

constexpr wchar_t kSettingsClassName[] = L"GoldViewSettingsWindow";
constexpr int kWindowWidth = 1120;
constexpr int kWindowHeight = 860;
constexpr int kIdTabControl = 180;
constexpr int kIdAutoRefresh = 201;
constexpr int kIdAutoSwitch = 202;
constexpr int kIdPreferredSource = 203;
constexpr int kIdSuccessThreshold = 204;
constexpr int kIdLatencyThreshold = 205;
constexpr int kIdRecentLimit = 206;
constexpr int kIdSourceEnabledBase = 300;
constexpr int kIdSourcePriorityBase = 320;
constexpr int kIdSourceWeightBase = 340;
constexpr int kIdFontCombo = 401;
constexpr int kIdFontSizeCombo = 402;
constexpr int kIdTextColor = 403;
constexpr int kIdBackgroundColor = 404;
constexpr int kIdTransparent = 405;
constexpr int kIdAlignLeft = 406;
constexpr int kIdAlignCenter = 407;
constexpr int kIdHorizontalLayout = 408;
constexpr int kIdSave = 501;
constexpr int kIdCancel = 502;
constexpr int kIdClearLogs = 503;
constexpr COLORREF kWindowFill = RGB(248, 250, 252);
constexpr COLORREF kInputFill = RGB(255, 255, 255);
constexpr COLORREF kAccentRed = RGB(220, 38, 38);

const std::array<QuoteSourceKind, 3> kSourceKinds{
    QuoteSourceKind::Sina,
    QuoteSourceKind::Xwteam,
    QuoteSourceKind::GoldApi,
};

std::wstring formatTimestamp(std::uint64_t value) {
    if (value == 0) {
        return L"--";
    }
    std::time_t timeValue = static_cast<std::time_t>(value);
    std::tm localTime{};
    localtime_s(&localTime, &timeValue);
    wchar_t buffer[64]{};
    wcsftime(buffer, static_cast<int>(std::size(buffer)), L"%H:%M:%S", &localTime);
    return buffer;
}

std::wstring colorToHex(COLORREF color) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buffer;
}

HWND createLabel(HWND parent, HINSTANCE instanceHandle, const wchar_t* text, DWORD style = SS_LEFT, int id = 0) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instanceHandle, nullptr);
}

HWND createGroupBox(HWND parent, HINSTANCE instanceHandle, const wchar_t* text) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 0, 0, parent, nullptr,
        instanceHandle, nullptr);
}

HWND createButton(HWND parent, HINSTANCE instanceHandle, const wchar_t* text, int id, DWORD style = BS_PUSHBUTTON) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, 0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instanceHandle, nullptr);
}

HWND createCombo(HWND parent, HINSTANCE instanceHandle, int id) {
    return CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instanceHandle, nullptr);
}

HWND createEdit(HWND parent, HINSTANCE instanceHandle, int id, bool readOnly = false, bool multiline = false) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
    if (readOnly) {
        style |= ES_READONLY;
    }
    if (multiline) {
        style = WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | (readOnly ? ES_READONLY : 0);
    }
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", style, 0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instanceHandle, nullptr);
}

HBRUSH panelBrush() {
    static HBRUSH brush = CreateSolidBrush(kWindowFill);
    return brush;
}

HBRUSH inputBrush() {
    static HBRUSH brush = CreateSolidBrush(kInputFill);
    return brush;
}

int comboIndexFromSource(QuoteSourceKind kind) {
    for (size_t index = 0; index < kSourceKinds.size(); ++index) {
        if (kSourceKinds[index] == kind) {
            return static_cast<int>(index);
        }
    }
    return 0;
}

QuoteSourceKind sourceFromComboIndex(int index) {
    if (index < 0 || index >= static_cast<int>(kSourceKinds.size())) {
        return QuoteSourceKind::Sina;
    }
    return kSourceKinds[static_cast<size_t>(index)];
}

QuoteSourceConfig* findSourceConfig(AppSettings& settings, QuoteSourceKind kind) {
    for (auto& source : settings.sources) {
        if (source.kind == kind) {
            return &source;
        }
    }
    return nullptr;
}

const QuoteSourceConfig* findSourceConfig(const AppSettings& settings, QuoteSourceKind kind) {
    for (const auto& source : settings.sources) {
        if (source.kind == kind) {
            return &source;
        }
    }
    return nullptr;
}

void setEditInt(HWND edit, int value) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%d", value);
    SetWindowTextW(edit, buffer);
}

int readEditInt(HWND edit, int fallback, int minimum, int maximum) {
    wchar_t buffer[32]{};
    GetWindowTextW(edit, buffer, static_cast<int>(std::size(buffer)));
    try {
        return (std::clamp)(std::stoi(buffer), minimum, maximum);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

bool SettingsWindow::create(HINSTANCE instanceHandle) {
    if (windowHandle_) {
        return true;
    }

    instanceHandle_ = instanceHandle;
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = SettingsWindow::windowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.lpszClassName = kSettingsClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = panelBrush();
    RegisterClassW(&windowClass);

    windowHandle_ = CreateWindowExW(0, kSettingsClassName, L"设置",
        (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_MINIMIZEBOX) | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
        nullptr, nullptr, instanceHandle, this);
    if (!windowHandle_) {
        return false;
    }

    createControls();
    applyFonts();
    syncControlsFromSettings();
    refreshRuntimeText();
    updateWindowTitle();
    centerOnScreen();
    return true;
}

void SettingsWindow::focusOrShow() {
    if (!windowHandle_ && !create(instanceHandle_)) {
        return;
    }
    ShowWindow(windowHandle_, SW_SHOWNORMAL);
    SetForegroundWindow(windowHandle_);
}

void SettingsWindow::destroy() {
    if (windowHandle_) {
        DestroyWindow(windowHandle_);
    }
}

bool SettingsWindow::isCreated() const {
    return windowHandle_ != nullptr;
}

HWND SettingsWindow::hwnd() const {
    return windowHandle_;
}

void SettingsWindow::setSettings(const AppSettings& settings) {
    savedSettings_ = settings;
    if (!dirty_) {
        draftSettings_ = settings;
        if (windowHandle_) {
            syncControlsFromSettings();
        }
    }
}

void SettingsWindow::updateRuntimeState(const RuntimeViewState& runtimeState) {
    runtimeState_ = runtimeState;
    refreshRuntimeText();
}

void SettingsWindow::setSettingsSavedCallback(SettingsSavedCallback callback) {
    settingsSavedCallback_ = std::move(callback);
}

void SettingsWindow::setClearLogsCallback(ClearLogsCallback callback) {
    clearLogsCallback_ = std::move(callback);
}

LRESULT CALLBACK SettingsWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto self = static_cast<SettingsWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    const auto self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT SettingsWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        layoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            saveDraft();
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (confirmDiscardIfNeeded()) {
                DestroyWindow(windowHandle_);
            }
            return 0;
        }
        break;
    case WM_NOTIFY:
        if (reinterpret_cast<LPNMHDR>(lParam)->hwndFrom == tabControl_ &&
            reinterpret_cast<LPNMHDR>(lParam)->code == TCN_SELCHANGE) {
            activeTab_ = TabCtrl_GetCurSel(tabControl_);
            showActiveTab();
            layoutControls(0, 0);
            return 0;
        }
        break;
    case WM_COMMAND: {
        const int controlId = LOWORD(wParam);
        const int notifyCode = HIWORD(wParam);
        if (controlId == kIdSave && notifyCode == BN_CLICKED) {
            saveDraft();
            return 0;
        }
        if (controlId == kIdCancel && notifyCode == BN_CLICKED) {
            cancelDraft();
            return 0;
        }
        if (controlId == kIdClearLogs && notifyCode == BN_CLICKED) {
            if (clearLogsCallback_) {
                clearLogsCallback_();
            }
            return 0;
        }
        if (controlId == kIdTextColor && notifyCode == BN_CLICKED) {
            if (chooseColorFor(draftSettings_.display.textColor)) {
                updateColorButton(textColorButton_, draftSettings_.display.textColor);
                refreshPreviewStyle();
                markDirty(true);
            }
            return 0;
        }
        if (controlId == kIdBackgroundColor && notifyCode == BN_CLICKED) {
            if (chooseColorFor(draftSettings_.display.backgroundColor)) {
                updateColorButton(backgroundColorButton_, draftSettings_.display.backgroundColor);
                refreshPreviewStyle();
                markDirty(true);
            }
            return 0;
        }

        if (!updatingControls_) {
            const bool sourceControlChange =
                (controlId >= kIdSourceEnabledBase && controlId < kIdSourceEnabledBase + 3) ||
                (controlId >= kIdSourcePriorityBase && controlId < kIdSourcePriorityBase + 3) ||
                (controlId >= kIdSourceWeightBase && controlId < kIdSourceWeightBase + 3);
            const bool generalChange =
                controlId == kIdAutoRefresh || controlId == kIdAutoSwitch || controlId == kIdPreferredSource ||
                controlId == kIdSuccessThreshold || controlId == kIdLatencyThreshold || controlId == kIdRecentLimit ||
                controlId == kIdFontCombo || controlId == kIdFontSizeCombo || controlId == kIdTransparent ||
                controlId == kIdAlignLeft || controlId == kIdAlignCenter || controlId == kIdHorizontalLayout;
            if (sourceControlChange || generalChange) {
                pullDraftFromControls();
                refreshPreviewStyle();
                markDirty(true);
            }
        }
        return 0;
    }
    case WM_CLOSE:
        if (confirmDiscardIfNeeded()) {
            DestroyWindow(windowHandle_);
        }
        return 0;
    case WM_DESTROY:
        releaseHandles();
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        if (control == latestOutputValue_) {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, kAccentRed);
            return reinterpret_cast<LRESULT>(panelBrush());
        }
        if (control == previewBox_) {
            SetBkColor(dc, draftSettings_.display.backgroundTransparent ? kWindowFill : draftSettings_.display.backgroundColor);
            SetTextColor(dc, draftSettings_.display.textColor);
            return reinterpret_cast<LRESULT>(previewBrush());
        }
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, theme::kTextPrimary);
        return reinterpret_cast<LRESULT>(panelBrush());
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, kInputFill);
        SetTextColor(dc, theme::kTextPrimary);
        return reinterpret_cast<LRESULT>(inputBrush());
    }
    default:
        break;
    }
    return DefWindowProcW(windowHandle_, message, wParam, lParam);
}

void SettingsWindow::createControls() {
    tabControl_ = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        0, 0, 0, 0, windowHandle_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTabControl)), instanceHandle_, nullptr);
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    const std::array<const wchar_t*, 5> tabTitles{L"实施状态", L"数据源控制", L"数据流输出", L"显示设置", L"常规设置"};
    for (int index = 0; index < static_cast<int>(tabTitles.size()); ++index) {
        item.pszText = const_cast<LPWSTR>(tabTitles[static_cast<size_t>(index)]);
        TabCtrl_InsertItem(tabControl_, index, &item);
    }

    statusGroup_ = createGroupBox(windowHandle_, instanceHandle_, L"实施状态");
    lastSuccessLabel_ = createLabel(windowHandle_, instanceHandle_, L"最近成功时间");
    lastSuccessValue_ = createLabel(windowHandle_, instanceHandle_, L"--");
    lastRequestLabel_ = createLabel(windowHandle_, instanceHandle_, L"最近请求时间");
    lastRequestValue_ = createLabel(windowHandle_, instanceHandle_, L"--");
    currentSourceLabel_ = createLabel(windowHandle_, instanceHandle_, L"当前数据源状态");
    currentSourceValue_ = createLabel(windowHandle_, instanceHandle_, L"--");
    switchReasonLabel_ = createLabel(windowHandle_, instanceHandle_, L"切换原因");
    switchReasonValue_ = createLabel(windowHandle_, instanceHandle_, L"--");
    statsLabel_ = createLabel(windowHandle_, instanceHandle_, L"状态统计");
    statsEdit_ = createEdit(windowHandle_, instanceHandle_, 0, true, true);
    for (HWND control : {statusGroup_, lastSuccessLabel_, lastSuccessValue_, lastRequestLabel_, lastRequestValue_, currentSourceLabel_,
             currentSourceValue_, switchReasonLabel_, switchReasonValue_, statsLabel_, statsEdit_}) {
        registerTabControl(TabPage::Status, control);
    }

    sourceGroup_ = createGroupBox(windowHandle_, instanceHandle_, L"数据源控制");
    autoRefreshCheck_ = createButton(windowHandle_, instanceHandle_, L"启用自动功能", kIdAutoRefresh, BS_AUTOCHECKBOX);
    autoSwitchCheck_ = createButton(windowHandle_, instanceHandle_, L"自动切换数据源", kIdAutoSwitch, BS_AUTOCHECKBOX);
    preferredSourceLabel_ = createLabel(windowHandle_, instanceHandle_, L"首选数据源");
    preferredSourceCombo_ = createCombo(windowHandle_, instanceHandle_, kIdPreferredSource);
    successThresholdLabel_ = createLabel(windowHandle_, instanceHandle_, L"成功率阈值 (%)");
    successThresholdEdit_ = createEdit(windowHandle_, instanceHandle_, kIdSuccessThreshold);
    latencyThresholdLabel_ = createLabel(windowHandle_, instanceHandle_, L"延迟阈值 (ms)");
    latencyThresholdEdit_ = createEdit(windowHandle_, instanceHandle_, kIdLatencyThreshold);
    recentLimitLabel_ = createLabel(windowHandle_, instanceHandle_, L"最近输出条数");
    recentLimitEdit_ = createEdit(windowHandle_, instanceHandle_, kIdRecentLimit);
    for (auto kind : kSourceKinds) {
        SendMessageW(preferredSourceCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(quoteSourceLabel(kind)));
    }
    for (HWND control : {sourceGroup_, autoRefreshCheck_, autoSwitchCheck_, preferredSourceLabel_, preferredSourceCombo_,
             successThresholdLabel_, successThresholdEdit_, latencyThresholdLabel_, latencyThresholdEdit_, recentLimitLabel_,
             recentLimitEdit_}) {
        registerTabControl(TabPage::Source, control);
    }
    for (size_t index = 0; index < sourceRows_.size(); ++index) {
        auto& row = sourceRows_[index];
        row.kind = kSourceKinds[index];
        row.enabledCheck = createButton(windowHandle_, instanceHandle_, L"", kIdSourceEnabledBase + static_cast<int>(index), BS_AUTOCHECKBOX);
        row.label = createLabel(windowHandle_, instanceHandle_, quoteSourceLabel(row.kind));
        row.transportValue = createLabel(windowHandle_, instanceHandle_,
            transportLabel(row.kind == QuoteSourceKind::GoldApi ? QuoteSourceTransport::Api : QuoteSourceTransport::Xhr));
        row.priorityLabel = createLabel(windowHandle_, instanceHandle_, L"优先级");
        row.priorityEdit = createEdit(windowHandle_, instanceHandle_, kIdSourcePriorityBase + static_cast<int>(index));
        row.weightLabel = createLabel(windowHandle_, instanceHandle_, L"权重");
        row.weightEdit = createEdit(windowHandle_, instanceHandle_, kIdSourceWeightBase + static_cast<int>(index));
        for (HWND control : {row.enabledCheck, row.label, row.transportValue, row.priorityLabel, row.priorityEdit, row.weightLabel, row.weightEdit}) {
            registerTabControl(TabPage::Source, control);
        }
    }

    outputGroup_ = createGroupBox(windowHandle_, instanceHandle_, L"数据流输出");
    latestOutputLabel_ = createLabel(windowHandle_, instanceHandle_, L"当前最新一条");
    latestOutputValue_ = createLabel(windowHandle_, instanceHandle_, L"--");
    recentOutputsLabel_ = createLabel(windowHandle_, instanceHandle_, L"最近输出列表");
    recentOutputsEdit_ = createEdit(windowHandle_, instanceHandle_, 0, true, true);
    for (HWND control : {outputGroup_, latestOutputLabel_, latestOutputValue_, recentOutputsLabel_, recentOutputsEdit_}) {
        registerTabControl(TabPage::Output, control);
    }

    displayGroup_ = createGroupBox(windowHandle_, instanceHandle_, L"显示设置");
    fontLabel_ = createLabel(windowHandle_, instanceHandle_, L"字体");
    fontCombo_ = createCombo(windowHandle_, instanceHandle_, kIdFontCombo);
    fontSizeLabel_ = createLabel(windowHandle_, instanceHandle_, L"字号");
    fontSizeCombo_ = createCombo(windowHandle_, instanceHandle_, kIdFontSizeCombo);
    textColorLabel_ = createLabel(windowHandle_, instanceHandle_, L"文字颜色");
    textColorButton_ = createButton(windowHandle_, instanceHandle_, L"", kIdTextColor);
    backgroundColorLabel_ = createLabel(windowHandle_, instanceHandle_, L"背景颜色");
    backgroundColorButton_ = createButton(windowHandle_, instanceHandle_, L"", kIdBackgroundColor);
    transparentCheck_ = createButton(windowHandle_, instanceHandle_, L"透明背景", kIdTransparent, BS_AUTOCHECKBOX);
    alignLabel_ = createLabel(windowHandle_, instanceHandle_, L"对齐方式");
    alignLeftRadio_ = createButton(windowHandle_, instanceHandle_, L"左对齐", kIdAlignLeft, BS_AUTORADIOBUTTON);
    alignCenterRadio_ = createButton(windowHandle_, instanceHandle_, L"居中对齐", kIdAlignCenter, BS_AUTORADIOBUTTON);
    horizontalLayoutCheck_ = createButton(windowHandle_, instanceHandle_, L"水平排布", kIdHorizontalLayout, BS_AUTOCHECKBOX);
    previewLabel_ = createLabel(windowHandle_, instanceHandle_, L"任务栏预览");
    previewBox_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        0, 0, 0, 0, windowHandle_, nullptr, instanceHandle_, nullptr);
    for (const wchar_t* size : {L"14", L"16", L"18", L"20", L"22", L"24", L"28", L"32"}) {
        SendMessageW(fontSizeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(size));
    }
    for (const wchar_t* font : {L"Consolas", L"Segoe UI", L"Microsoft YaHei UI"}) {
        SendMessageW(fontCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(font));
    }
    for (HWND control : {displayGroup_, fontLabel_, fontCombo_, fontSizeLabel_, fontSizeCombo_, textColorLabel_, textColorButton_,
             backgroundColorLabel_, backgroundColorButton_, transparentCheck_, alignLabel_, alignLeftRadio_,
             alignCenterRadio_, horizontalLayoutCheck_, previewLabel_, previewBox_}) {
        registerTabControl(TabPage::Display, control);
    }

    generalGroup_ = createGroupBox(windowHandle_, instanceHandle_, L"常规设置");
    generalPlaceholder_ = createLabel(windowHandle_, instanceHandle_, L"暂未提供设置项。", SS_CENTER);
    registerTabControl(TabPage::General, generalGroup_);
    registerTabControl(TabPage::General, generalPlaceholder_);

    saveButton_ = createButton(windowHandle_, instanceHandle_, L"保存设置", kIdSave);
    cancelButton_ = createButton(windowHandle_, instanceHandle_, L"取消设置", kIdCancel);
    clearLogsButton_ = createButton(windowHandle_, instanceHandle_, L"清空日志", kIdClearLogs);

    showActiveTab();
    layoutControls(kWindowWidth, kWindowHeight);
}

void SettingsWindow::layoutControls(int width, int height) {
    if (!windowHandle_) {
        return;
    }
    if (width == 0 || height == 0) {
        RECT clientRect{};
        GetClientRect(windowHandle_, &clientRect);
        width = clientRect.right - clientRect.left;
        height = clientRect.bottom - clientRect.top;
    }

    const int outer = 16;
    const int actionHeight = 56;
    tabRect_ = RECT{outer, outer, width - outer, height - outer * 2 - actionHeight};
    actionBarRect_ = RECT{outer, height - outer - actionHeight, width - outer, height - outer};
    MoveWindow(tabControl_, tabRect_.left, tabRect_.top, tabRect_.right - tabRect_.left, tabRect_.bottom - tabRect_.top, TRUE);

    RECT pageRect{};
    GetClientRect(tabControl_, &pageRect);
    TabCtrl_AdjustRect(tabControl_, FALSE, &pageRect);
    MapWindowPoints(tabControl_, windowHandle_, reinterpret_cast<POINT*>(&pageRect), 2);
    contentRect_ = pageRect;

    const int x = contentRect_.left + 16;
    const int y = contentRect_.top + 28;
    const int contentWidth = contentRect_.right - contentRect_.left;
    const int contentHeight = contentRect_.bottom - contentRect_.top;

    MoveWindow(statusGroup_, contentRect_.left, contentRect_.top, contentWidth, contentHeight, TRUE);
    MoveWindow(lastSuccessLabel_, x, y, 110, 22, TRUE);
    MoveWindow(lastSuccessValue_, x + 118, y, 180, 22, TRUE);
    MoveWindow(lastRequestLabel_, x + 340, y, 110, 22, TRUE);
    MoveWindow(lastRequestValue_, x + 458, y, 180, 22, TRUE);
    MoveWindow(currentSourceLabel_, x, y + 40, 110, 22, TRUE);
    MoveWindow(currentSourceValue_, x + 118, y + 40, 220, 22, TRUE);
    MoveWindow(switchReasonLabel_, x, y + 78, 110, 22, TRUE);
    MoveWindow(switchReasonValue_, x + 118, y + 78, contentRect_.right - x - 134, 40, TRUE);
    MoveWindow(statsLabel_, x, y + 132, 100, 22, TRUE);
    MoveWindow(statsEdit_, x, y + 160, contentRect_.right - x - 16, contentRect_.bottom - y - 176, TRUE);

    MoveWindow(sourceGroup_, contentRect_.left, contentRect_.top, contentWidth, contentHeight, TRUE);
    MoveWindow(autoRefreshCheck_, x, y, 160, 24, TRUE);
    MoveWindow(autoSwitchCheck_, x + 180, y, 180, 24, TRUE);
    MoveWindow(preferredSourceLabel_, x, y + 40, 100, 22, TRUE);
    MoveWindow(preferredSourceCombo_, x + 110, y + 38, 180, 220, TRUE);
    MoveWindow(successThresholdLabel_, x + 330, y + 40, 120, 22, TRUE);
    MoveWindow(successThresholdEdit_, x + 458, y + 38, 70, 26, TRUE);
    MoveWindow(latencyThresholdLabel_, x, y + 80, 120, 22, TRUE);
    MoveWindow(latencyThresholdEdit_, x + 110, y + 78, 70, 26, TRUE);
    MoveWindow(recentLimitLabel_, x + 220, y + 80, 110, 22, TRUE);
    MoveWindow(recentLimitEdit_, x + 338, y + 78, 70, 26, TRUE);
    for (size_t index = 0; index < sourceRows_.size(); ++index) {
        auto& row = sourceRows_[index];
        const int rowTop = y + 132 + static_cast<int>(index) * 34;
        MoveWindow(row.enabledCheck, x, rowTop, 22, 22, TRUE);
        MoveWindow(row.label, x + 30, rowTop, 140, 22, TRUE);
        MoveWindow(row.transportValue, x + 180, rowTop, 110, 22, TRUE);
        MoveWindow(row.priorityLabel, x + 320, rowTop, 52, 22, TRUE);
        MoveWindow(row.priorityEdit, x + 378, rowTop - 2, 50, 26, TRUE);
        MoveWindow(row.weightLabel, x + 460, rowTop, 40, 22, TRUE);
        MoveWindow(row.weightEdit, x + 506, rowTop - 2, 50, 26, TRUE);
    }

    MoveWindow(outputGroup_, contentRect_.left, contentRect_.top, contentWidth, contentHeight, TRUE);
    MoveWindow(latestOutputLabel_, x, y, 120, 22, TRUE);
    MoveWindow(latestOutputValue_, x, y + 30, contentRect_.right - x - 16, 30, TRUE);
    MoveWindow(recentOutputsLabel_, x, y + 76, 120, 22, TRUE);
    MoveWindow(recentOutputsEdit_, x, y + 104, contentRect_.right - x - 16, contentRect_.bottom - y - 120, TRUE);

    MoveWindow(displayGroup_, contentRect_.left, contentRect_.top, contentWidth, contentHeight, TRUE);
    MoveWindow(fontLabel_, x, y, 72, 22, TRUE);
    MoveWindow(fontCombo_, x + 90, y - 2, 220, 220, TRUE);
    MoveWindow(fontSizeLabel_, x, y + 38, 72, 22, TRUE);
    MoveWindow(fontSizeCombo_, x + 90, y + 36, 80, 220, TRUE);
    MoveWindow(textColorLabel_, x, y + 76, 72, 22, TRUE);
    MoveWindow(textColorButton_, x + 90, y + 72, 110, 30, TRUE);
    MoveWindow(backgroundColorLabel_, x, y + 116, 72, 22, TRUE);
    MoveWindow(backgroundColorButton_, x + 90, y + 112, 110, 30, TRUE);
    MoveWindow(transparentCheck_, x + 90, y + 154, 120, 24, TRUE);
    MoveWindow(alignLabel_, x, y + 190, 72, 22, TRUE);
    MoveWindow(alignLeftRadio_, x + 90, y + 188, 80, 24, TRUE);
    MoveWindow(alignCenterRadio_, x + 176, y + 188, 90, 24, TRUE);
    MoveWindow(horizontalLayoutCheck_, x + 90, y + 222, 120, 24, TRUE);
    MoveWindow(previewLabel_, x + 380, y, 120, 22, TRUE);
    MoveWindow(previewBox_, x + 380, y + 28, contentRect_.right - x - 396, 140, TRUE);

    MoveWindow(generalGroup_, contentRect_.left, contentRect_.top, contentWidth, contentHeight, TRUE);
    MoveWindow(generalPlaceholder_, contentRect_.left + 24, contentRect_.top + 56, contentWidth - 48, 28, TRUE);

    MoveWindow(clearLogsButton_, actionBarRect_.left, actionBarRect_.top + 10, 120, 36, TRUE);
    MoveWindow(cancelButton_, actionBarRect_.right - 264, actionBarRect_.top + 10, 120, 36, TRUE);
    MoveWindow(saveButton_, actionBarRect_.right - 132, actionBarRect_.top + 10, 120, 36, TRUE);
}

void SettingsWindow::centerOnScreen() const {
    if (!windowHandle_) {
        return;
    }
    RECT windowRect{};
    GetWindowRect(windowHandle_, &windowRect);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(MonitorFromWindow(windowHandle_, MONITOR_DEFAULTTONEAREST), &monitorInfo);
    const RECT& workRect = monitorInfo.rcWork;
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    const int x = workRect.left + ((workRect.right - workRect.left) - width) / 2;
    const int y = workRect.top + ((workRect.bottom - workRect.top) - height) / 2;
    SetWindowPos(windowHandle_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void SettingsWindow::releaseHandles() {
    for (HFONT* font : {&sectionFont_, &bodyFont_, &monoFont_, &previewFont_}) {
        if (*font) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
    if (previewBrush_) {
        DeleteObject(previewBrush_);
        previewBrush_ = nullptr;
    }
    for (auto& controls : tabControls_) {
        controls.clear();
    }
    windowHandle_ = nullptr;
}

void SettingsWindow::applyFonts() {
    sectionFont_ = theme::createUiFont(18, FW_BOLD);
    bodyFont_ = theme::createUiFont(16, FW_NORMAL);
    monoFont_ = theme::createMonoFont(18, FW_BOLD);
    SendMessageW(tabControl_, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), TRUE);
    EnumChildWindows(windowHandle_, [](HWND hwnd, LPARAM lParam) -> BOOL {
        const auto self = reinterpret_cast<SettingsWindow*>(lParam);
        HFONT font = self->bodyFont_;
        if (hwnd == self->statsEdit_ || hwnd == self->recentOutputsEdit_ || hwnd == self->latestOutputValue_) {
            font = self->monoFont_;
        }
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
    for (HWND group : {statusGroup_, sourceGroup_, outputGroup_, displayGroup_, generalGroup_}) {
        SendMessageW(group, WM_SETFONT, reinterpret_cast<WPARAM>(sectionFont_), TRUE);
    }
    refreshPreviewStyle();
}

void SettingsWindow::syncControlsFromSettings() {
    if (!windowHandle_) {
        return;
    }
    updatingControls_ = true;
    SendMessageW(autoRefreshCheck_, BM_SETCHECK, draftSettings_.runtime.autoRefreshEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(autoSwitchCheck_, BM_SETCHECK, draftSettings_.runtime.autoSwitchSource ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(preferredSourceCombo_, CB_SETCURSEL, comboIndexFromSource(draftSettings_.runtime.preferredSource), 0);
    setEditInt(successThresholdEdit_, draftSettings_.runtime.successRateThreshold);
    setEditInt(latencyThresholdEdit_, draftSettings_.runtime.latencyThresholdMs);
    setEditInt(recentLimitEdit_, draftSettings_.runtime.recentOutputLimit);
    for (auto& row : sourceRows_) {
        const QuoteSourceConfig* config = findSourceConfig(draftSettings_, row.kind);
        SendMessageW(row.enabledCheck, BM_SETCHECK, (config && config->enabled) ? BST_CHECKED : BST_UNCHECKED, 0);
        setEditInt(row.priorityEdit, config ? config->priority : 1);
        setEditInt(row.weightEdit, config ? config->weight : 100);
        SetWindowTextW(row.transportValue, transportLabel(config ? config->transport : QuoteSourceTransport::Xhr));
    }
    const int fontIndex = static_cast<int>(SendMessageW(fontCombo_, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
        reinterpret_cast<LPARAM>(draftSettings_.display.fontName.c_str())));
    SendMessageW(fontCombo_, CB_SETCURSEL, fontIndex >= 0 ? fontIndex : 0, 0);
    wchar_t sizeBuffer[16]{};
    swprintf_s(sizeBuffer, L"%d", draftSettings_.display.fontSize);
    const int fontSizeIndex = static_cast<int>(SendMessageW(fontSizeCombo_, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
        reinterpret_cast<LPARAM>(sizeBuffer)));
    SendMessageW(fontSizeCombo_, CB_SETCURSEL, fontSizeIndex >= 0 ? fontSizeIndex : 3, 0);
    updateColorButton(textColorButton_, draftSettings_.display.textColor);
    updateColorButton(backgroundColorButton_, draftSettings_.display.backgroundColor);
    SendMessageW(transparentCheck_, BM_SETCHECK, draftSettings_.display.backgroundTransparent ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(alignLeftRadio_, BM_SETCHECK, draftSettings_.display.textAlignment == TextAlignment::Left ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(alignCenterRadio_, BM_SETCHECK, draftSettings_.display.textAlignment == TextAlignment::Center ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(horizontalLayoutCheck_, BM_SETCHECK, draftSettings_.display.horizontalLayout ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(backgroundColorButton_, draftSettings_.display.backgroundTransparent ? FALSE : TRUE);
    updatingControls_ = false;
    refreshPreviewStyle();
    refreshRuntimeText();
}

void SettingsWindow::pullDraftFromControls() {
    draftSettings_.runtime.autoRefreshEnabled = SendMessageW(autoRefreshCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    draftSettings_.runtime.autoSwitchSource = SendMessageW(autoSwitchCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    draftSettings_.runtime.preferredSource = sourceFromComboIndex(static_cast<int>(SendMessageW(preferredSourceCombo_, CB_GETCURSEL, 0, 0)));
    draftSettings_.runtime.successRateThreshold = readEditInt(successThresholdEdit_, draftSettings_.runtime.successRateThreshold, 50, 100);
    draftSettings_.runtime.latencyThresholdMs = readEditInt(latencyThresholdEdit_, draftSettings_.runtime.latencyThresholdMs, 100, 5000);
    draftSettings_.runtime.recentOutputLimit = readEditInt(recentLimitEdit_, draftSettings_.runtime.recentOutputLimit, 10, 200);
    for (auto& row : sourceRows_) {
        if (QuoteSourceConfig* config = findSourceConfig(draftSettings_, row.kind)) {
            config->enabled = SendMessageW(row.enabledCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            config->priority = readEditInt(row.priorityEdit, config->priority, 1, 99);
            config->weight = readEditInt(row.weightEdit, config->weight, 1, 100);
        }
    }
    const int fontIndex = static_cast<int>(SendMessageW(fontCombo_, CB_GETCURSEL, 0, 0));
    if (fontIndex >= 0) {
        wchar_t textBuffer[128]{};
        SendMessageW(fontCombo_, CB_GETLBTEXT, fontIndex, reinterpret_cast<LPARAM>(textBuffer));
        draftSettings_.display.fontName = textBuffer;
    }
    const int sizeIndex = static_cast<int>(SendMessageW(fontSizeCombo_, CB_GETCURSEL, 0, 0));
    if (sizeIndex >= 0) {
        wchar_t textBuffer[32]{};
        SendMessageW(fontSizeCombo_, CB_GETLBTEXT, sizeIndex, reinterpret_cast<LPARAM>(textBuffer));
        draftSettings_.display.fontSize = (std::clamp)(_wtoi(textBuffer), 14, 32);
    }
    draftSettings_.display.backgroundTransparent = SendMessageW(transparentCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    draftSettings_.display.textAlignment =
        SendMessageW(alignLeftRadio_, BM_GETCHECK, 0, 0) == BST_CHECKED ? TextAlignment::Left : TextAlignment::Center;
    draftSettings_.display.horizontalLayout = SendMessageW(horizontalLayoutCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(backgroundColorButton_, draftSettings_.display.backgroundTransparent ? FALSE : TRUE);
}

void SettingsWindow::refreshRuntimeText() {
    if (!windowHandle_) {
        return;
    }
    SetWindowTextW(lastSuccessValue_, formatTimestamp(runtimeState_.status.lastSuccessfulAt).c_str());
    SetWindowTextW(lastRequestValue_, formatTimestamp(runtimeState_.status.lastRequestedAt).c_str());
    SetWindowTextW(currentSourceValue_, quoteSourceLabel(runtimeState_.status.activeSource));
    SetWindowTextW(switchReasonValue_, runtimeState_.status.lastSwitchReason.empty() ? L"--" : runtimeState_.status.lastSwitchReason.c_str());
    SetWindowTextW(statsEdit_, formatHealthSummary().c_str());
    if (runtimeState_.recentOutputs.empty()) {
        SetWindowTextW(latestOutputValue_, L"--");
        SetWindowTextW(recentOutputsEdit_, L"--");
    } else {
        std::wstring latest = runtimeState_.recentOutputs.front().text;
        if (runtimeState_.status.delayed) {
            latest += L"  [数据延迟]";
        }
        SetWindowTextW(latestOutputValue_, latest.c_str());
        SetWindowTextW(recentOutputsEdit_, formatRecentOutputs().c_str());
    }
}

void SettingsWindow::markDirty(bool dirty) {
    dirty_ = dirty;
    updateWindowTitle();
}

bool SettingsWindow::confirmDiscardIfNeeded() {
    if (!dirty_) {
        return true;
    }
    const int result = MessageBoxW(windowHandle_, L"当前设置尚未保存。是否先保存更改？", L"未保存的设置", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (result == IDCANCEL) {
        return false;
    }
    if (result == IDYES) {
        saveDraft();
        return !dirty_;
    }
    draftSettings_ = savedSettings_;
    syncControlsFromSettings();
    markDirty(false);
    return true;
}

void SettingsWindow::saveDraft() {
    pullDraftFromControls();
    savedSettings_ = draftSettings_;
    if (settingsSavedCallback_) {
        settingsSavedCallback_(savedSettings_);
    }
    markDirty(false);
}

void SettingsWindow::cancelDraft() {
    draftSettings_ = savedSettings_;
    syncControlsFromSettings();
    markDirty(false);
}

void SettingsWindow::updateColorButton(HWND button, COLORREF color) const {
    SetWindowTextW(button, colorToHex(color).c_str());
}

bool SettingsWindow::chooseColorFor(COLORREF& color) {
    static COLORREF customColors[16]{};
    CHOOSECOLORW chooseColor{};
    chooseColor.lStructSize = sizeof(chooseColor);
    chooseColor.hwndOwner = windowHandle_;
    chooseColor.rgbResult = color;
    chooseColor.lpCustColors = customColors;
    chooseColor.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&chooseColor)) {
        return false;
    }
    color = chooseColor.rgbResult;
    return true;
}

std::wstring SettingsWindow::formatHealthSummary() const {
    std::wstringstream stream;
    for (const auto& source : runtimeState_.status.sourceHealth) {
        const int total = source.successCount + source.errorCount;
        const int successRate = total > 0 ? static_cast<int>((source.successCount * 100.0) / total) : 100;
        stream << L"["
               << quoteSourceLabel(source.kind)
               << (source.active ? L" *" : L"")
               << L"] 成功率=" << successRate << L"% 平均延迟=" << source.averageLatencyMs << L"ms 最近错误="
               << (source.lastError.empty() ? L"--" : source.lastError) << L"\r\n";
    }
    if (!runtimeState_.status.logs.empty()) {
        stream << L"\r\n运行日志\r\n";
        const size_t startIndex = runtimeState_.status.logs.size() > 12 ? runtimeState_.status.logs.size() - 12 : 0;
        for (size_t index = startIndex; index < runtimeState_.status.logs.size(); ++index) {
            const auto& log = runtimeState_.status.logs[index];
            stream << L"[" << runtimeLogLevelLabel(log.level) << L"][" << formatTimestamp(log.timestamp) << L"] " << log.message << L"\r\n";
        }
    }
    const std::wstring text = stream.str();
    return text.empty() ? L"--" : text;
}

std::wstring SettingsWindow::formatRecentOutputs() const {
    if (runtimeState_.recentOutputs.size() <= 1) {
        return L"--";
    }
    std::wstringstream stream;
    for (size_t index = 1; index < runtimeState_.recentOutputs.size(); ++index) {
        if (index > 1) {
            stream << L"\r\n";
        }
        stream << runtimeState_.recentOutputs[index].text;
    }
    return stream.str();
}

void SettingsWindow::registerTabControl(TabPage page, HWND control) {
    tabControls_[static_cast<size_t>(page)].push_back(control);
}

void SettingsWindow::showActiveTab() {
    for (size_t pageIndex = 0; pageIndex < tabControls_.size(); ++pageIndex) {
        const int showState = static_cast<int>(pageIndex) == activeTab_ ? SW_SHOW : SW_HIDE;
        for (HWND control : tabControls_[pageIndex]) {
            ShowWindow(control, showState);
        }
    }
}

void SettingsWindow::updateWindowTitle() const {
    if (windowHandle_) {
        SetWindowTextW(windowHandle_, dirty_ ? L"设置 *" : L"设置");
    }
}

void SettingsWindow::refreshPreviewStyle() {
    if (!windowHandle_ || !previewBox_) {
        return;
    }
    if (previewBrush_) {
        DeleteObject(previewBrush_);
    }
    previewBrush_ = CreateSolidBrush(draftSettings_.display.backgroundTransparent ? kWindowFill : draftSettings_.display.backgroundColor);
    if (previewFont_) {
        DeleteObject(previewFont_);
    }
    previewFont_ = CreateFontW(-(std::clamp)(draftSettings_.display.fontSize + 4, 18, 34), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        draftSettings_.display.fontName.empty() ? theme::kMonoFont : draftSettings_.display.fontName.c_str());
    SendMessageW(previewBox_, WM_SETFONT, reinterpret_cast<WPARAM>(previewFont_), TRUE);
    SetWindowTextW(previewBox_, draftSettings_.display.horizontalLayout ? L"2345.67" : L"2345\r\n.67");
    InvalidateRect(previewBox_, nullptr, TRUE);
}

HBRUSH SettingsWindow::previewBrush() const {
    return previewBrush_ ? previewBrush_ : panelBrush();
}

}  // namespace goldview
