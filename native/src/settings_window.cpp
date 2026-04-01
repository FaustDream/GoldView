#include "settings_window.h"

#include <algorithm>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <ctime>
#include <sstream>

#include "theme.h"

namespace goldview {

namespace {
constexpr wchar_t kSettingsClassName[] = L"GoldViewSettingsWindow";
constexpr int kWindowWidth = 760;
constexpr int kWindowHeight = 620;
constexpr int kIdAutoSource = 201;
constexpr int kIdPreferredProvider = 202;
constexpr int kIdBenchmarkButton = 203;
constexpr int kIdFontCombo = 301;
constexpr int kIdFontSizeCombo = 302;
constexpr int kIdTextColorButton = 303;
constexpr int kIdBackgroundColorButton = 304;
constexpr int kIdTransparent = 305;
constexpr int kIdAlignLeft = 306;
constexpr int kIdAlignCenter = 307;
constexpr int kIdHorizontalLayout = 308;
constexpr COLORREF kAppHeader = RGB(31, 41, 55);
constexpr COLORREF kAppCanvas = RGB(238, 242, 247);
constexpr COLORREF kCardBorder = RGB(203, 213, 225);
constexpr COLORREF kMutedText = RGB(71, 85, 105);
constexpr COLORREF kActiveTabFill = RGB(255, 247, 219);
constexpr COLORREF kActiveTabBorder = RGB(197, 139, 13);
constexpr COLORREF kActiveTabText = RGB(124, 84, 0);
constexpr COLORREF kInactiveTabFill = RGB(248, 250, 252);
constexpr COLORREF kInactiveTabText = RGB(51, 65, 85);
constexpr COLORREF kSuccessFill = RGB(238, 246, 242);
constexpr COLORREF kSuccessBorder = RGB(34, 160, 107);
constexpr COLORREF kSuccessText = RGB(22, 101, 52);

std::wstring formatTimestamp(std::uint64_t value) {
    if (value == 0) return L"--";
    std::time_t timeValue = static_cast<std::time_t>(value);
    std::tm localTime{};
    localtime_s(&localTime, &timeValue);
    wchar_t buffer[64]{};
    wcsftime(buffer, std::size(buffer), L"%H:%M:%S", &localTime);
    return buffer;
}

std::wstring colorToHex(COLORREF color) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buffer;
}

void fillRoundedRect(HDC dc, const RECT& rect, COLORREF fillColor, COLORREF borderColor, int radius = 12) {
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawTextLine(HDC dc, const std::wstring& text, RECT rect, UINT format, COLORREF color, HFONT font) {
    const auto oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), -1, &rect, format);
    SelectObject(dc, oldFont);
}

HWND createLabel(HWND parent, HINSTANCE instanceHandle, const wchar_t* text, DWORD style = SS_LEFT, int id = 0) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instanceHandle, nullptr);
}

HWND createButton(HWND parent, HINSTANCE instanceHandle, const wchar_t* text, int id, DWORD style = BS_PUSHBUTTON) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instanceHandle, nullptr);
}

HWND createCombo(HWND parent, HINSTANCE instanceHandle, int id) {
    return CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instanceHandle, nullptr);
}

HWND createReadOnlyEdit(HWND parent, HINSTANCE instanceHandle) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        0, 0, 0, 0, parent, nullptr, instanceHandle, nullptr);
}

PriceProviderKind providerFromComboIndex(int index) {
    switch (index) {
    case 1: return PriceProviderKind::Xwteam;
    case 2: return PriceProviderKind::Sina;
    default: return PriceProviderKind::GoldApi;
    }
}

int comboIndexFromProvider(PriceProviderKind provider) {
    switch (provider) {
    case PriceProviderKind::Xwteam: return 1;
    case PriceProviderKind::Sina: return 2;
    default: return 0;
    }
}
}  // namespace

bool SettingsWindow::create(HINSTANCE instanceHandle) {
    if (windowHandle_) return true;
    instanceHandle_ = instanceHandle;

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = SettingsWindow::windowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.lpszClassName = kSettingsClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(kAppCanvas);
    RegisterClassW(&windowClass);

    windowHandle_ = CreateWindowExW(WS_EX_TOOLWINDOW, kSettingsClassName, L"GoldView 设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 0, 0, kWindowWidth, kWindowHeight,
        nullptr, nullptr, instanceHandle, this);
    if (!windowHandle_) return false;

    createControls();
    applyFonts();
    syncControlsFromSettings();
    refreshStatusText();
    centerOnScreen();
    return true;
}

void SettingsWindow::show() {
    if (!windowHandle_ && !create(instanceHandle_)) return;
    centerOnScreen();
    ShowWindow(windowHandle_, SW_SHOWNORMAL);
    UpdateWindow(windowHandle_);
}

void SettingsWindow::focusOrShow() {
    if (!windowHandle_ && !create(instanceHandle_)) return;
    centerOnScreen();
    ShowWindow(windowHandle_, IsIconic(windowHandle_) ? SW_RESTORE : SW_SHOWNORMAL);
    SetForegroundWindow(windowHandle_);
}

void SettingsWindow::destroy() {
    if (windowHandle_) DestroyWindow(windowHandle_);
}

bool SettingsWindow::isCreated() const { return windowHandle_ != nullptr; }
HWND SettingsWindow::hwnd() const { return windowHandle_; }

void SettingsWindow::setSettings(const DisplaySettings& settings) {
    settings_ = settings;
    syncControlsFromSettings();
    if (windowHandle_) InvalidateRect(windowHandle_, nullptr, TRUE);
}

void SettingsWindow::updateRuntimeStatus(const PriceServiceStatus& status) {
    status_ = status;
    refreshStatusText();
}

void SettingsWindow::setSettingsChangedCallback(SettingsChangedCallback callback) { settingsChangedCallback_ = std::move(callback); }
void SettingsWindow::setBenchmarkRequestCallback(BenchmarkRequestCallback callback) { benchmarkRequestCallback_ = std::move(callback); }

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
    case WM_COMMAND:
        if (LOWORD(wParam) == kIdBenchmarkButton && HIWORD(wParam) == BN_CLICKED) {
            if (benchmarkRequestCallback_) benchmarkRequestCallback_();
            return 0;
        }
        if (!updatingControls_) {
            switch (LOWORD(wParam)) {
            case kIdAutoSource:
            case kIdPreferredProvider:
            case kIdFontCombo:
            case kIdFontSizeCombo:
            case kIdTransparent:
            case kIdAlignLeft:
            case kIdAlignCenter:
            case kIdHorizontalLayout:
                applyControlChanges();
                return 0;
            case kIdTextColorButton:
                if (chooseColorFor(settings_.textColor)) {
                    updateColorButton(textColorButton_, settings_.textColor);
                    applyControlChanges();
                }
                return 0;
            case kIdBackgroundColorButton:
                if (chooseColorFor(settings_.backgroundColor)) {
                    updateColorButton(backgroundColorButton_, settings_.backgroundColor);
                    applyControlChanges();
                }
                return 0;
            default:
                break;
            }
        }
        break;
    case WM_LBUTTONUP: {
        POINT point{static_cast<LONG>(GET_X_LPARAM(lParam)), static_cast<LONG>(GET_Y_LPARAM(lParam))};
        int nextTab = activeTab_;
        if (PtInRect(&dataTabRect_, point)) nextTab = 0;
        else if (PtInRect(&displayTabRect_, point)) nextTab = 1;
        if (nextTab != activeTab_) {
            activeTab_ = nextTab;
            syncControlsVisibility();
            InvalidateRect(windowHandle_, nullptr, TRUE);
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(windowHandle_);
        return 0;
    case WM_DESTROY:
        releaseHandles();
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, theme::kTextPrimary);
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }
    case WM_PAINT:
        paint();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(windowHandle_, message, wParam, lParam);
}

void SettingsWindow::createControls() {
    dataPageControls_.clear();
    displayPageControls_.clear();

    dataPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"自动切换"));
    autoSourceCheck_ = createButton(windowHandle_, instanceHandle_, L"启用自动切换数据源", kIdAutoSource, BS_AUTOCHECKBOX);
    dataPageControls_.push_back(autoSourceCheck_);
    dataPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"首选数据源"));
    preferredProviderCombo_ = createCombo(windowHandle_, instanceHandle_, kIdPreferredProvider);
    SendMessageW(preferredProviderCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Gold API"));
    SendMessageW(preferredProviderCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"XWTeam GJ_Au"));
    SendMessageW(preferredProviderCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sina hf_XAU"));
    dataPageControls_.push_back(preferredProviderCombo_);
    dataPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"刷新目标"));
    requestRateLabel_ = createLabel(windowHandle_, instanceHandle_, L"1 秒请求一次，目标 2-3 秒有效更新", SS_LEFTNOWORDWRAP);
    dataPageControls_.push_back(requestRateLabel_);
    dataPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"当前数据源"));
    activeProviderValue_ = createLabel(windowHandle_, instanceHandle_, L"--");
    dataPageControls_.push_back(activeProviderValue_);
    dataPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"最近成功"));
    lastSuccessValue_ = createLabel(windowHandle_, instanceHandle_, L"--");
    dataPageControls_.push_back(lastSuccessValue_);
    dataPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"最近变价"));
    lastChangeValue_ = createLabel(windowHandle_, instanceHandle_, L"--");
    dataPageControls_.push_back(lastChangeValue_);
    dataPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"切换原因"));
    switchReasonValue_ = createLabel(windowHandle_, instanceHandle_, L"--", SS_LEFT);
    dataPageControls_.push_back(switchReasonValue_);
    dataPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"数据统计 / Benchmark"));
    statsEdit_ = createReadOnlyEdit(windowHandle_, instanceHandle_);
    dataPageControls_.push_back(statsEdit_);
    benchmarkButton_ = createButton(windowHandle_, instanceHandle_, L"执行 Benchmark", kIdBenchmarkButton);
    dataPageControls_.push_back(benchmarkButton_);

    displayPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"字体"));
    fontCombo_ = createCombo(windowHandle_, instanceHandle_, kIdFontCombo);
    SendMessageW(fontCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Consolas"));
    SendMessageW(fontCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Segoe UI"));
    SendMessageW(fontCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Microsoft YaHei UI"));
    displayPageControls_.push_back(fontCombo_);
    displayPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"字号"));
    fontSizeCombo_ = createCombo(windowHandle_, instanceHandle_, kIdFontSizeCombo);
    for (const wchar_t* size : {L"14", L"16", L"18", L"20", L"22", L"24", L"28", L"32"}) {
        SendMessageW(fontSizeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(size));
    }
    displayPageControls_.push_back(fontSizeCombo_);
    displayPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"文字颜色"));
    textColorButton_ = createButton(windowHandle_, instanceHandle_, L"", kIdTextColorButton);
    displayPageControls_.push_back(textColorButton_);
    displayPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"背景颜色"));
    backgroundColorButton_ = createButton(windowHandle_, instanceHandle_, L"", kIdBackgroundColorButton);
    displayPageControls_.push_back(backgroundColorButton_);
    transparentCheck_ = createButton(windowHandle_, instanceHandle_, L"透明背景", kIdTransparent, BS_AUTOCHECKBOX);
    displayPageControls_.push_back(transparentCheck_);
    displayPageControls_.push_back(createLabel(windowHandle_, instanceHandle_, L"文字对齐"));
    alignLeftRadio_ = createButton(windowHandle_, instanceHandle_, L"左对齐", kIdAlignLeft, BS_AUTORADIOBUTTON);
    alignCenterRadio_ = createButton(windowHandle_, instanceHandle_, L"居中对齐", kIdAlignCenter, BS_AUTORADIOBUTTON);
    displayPageControls_.push_back(alignLeftRadio_);
    displayPageControls_.push_back(alignCenterRadio_);
    horizontalLayoutCheck_ = createButton(windowHandle_, instanceHandle_, L"水平排列", kIdHorizontalLayout, BS_AUTOCHECKBOX);
    displayPageControls_.push_back(horizontalLayoutCheck_);

    layoutControls(kWindowWidth, kWindowHeight);
    syncControlsVisibility();
}

void SettingsWindow::layoutControls(int width, int height) {
    const int outerMargin = 24;
    cardRect_ = RECT{outerMargin, outerMargin, width - outerMargin, height - outerMargin};
    const int headerHeight = 58;
    dataTabRect_ = RECT{cardRect_.left + 28, cardRect_.top + headerHeight + 24, cardRect_.left + 182, cardRect_.top + headerHeight + 64};
    displayTabRect_ = RECT{dataTabRect_.right + 18, dataTabRect_.top, dataTabRect_.right + 164, dataTabRect_.bottom};

    const int contentLeft = cardRect_.left + 36;
    const int contentTop = dataTabRect_.bottom + 28;
    const int labelWidth = 170;
    const int fieldLeft = contentLeft + labelWidth + 18;
    const int fieldWidth = cardRect_.right - fieldLeft - 36;
    const int rowHeight = 30;
    const int rowGap = 18;

    int top = contentTop;
    auto placeRow = [&](HWND label, HWND control, int controlHeight) {
        MoveWindow(label, contentLeft, top + 6, labelWidth, 22, TRUE);
        MoveWindow(control, fieldLeft, top, fieldWidth, controlHeight, TRUE);
        top += controlHeight + rowGap;
    };
    placeRow(dataPageControls_[0], autoSourceCheck_, 34);
    placeRow(dataPageControls_[2], preferredProviderCombo_, 240);
    placeRow(dataPageControls_[4], requestRateLabel_, 32);
    placeRow(dataPageControls_[6], activeProviderValue_, 28);
    placeRow(dataPageControls_[8], lastSuccessValue_, 28);
    placeRow(dataPageControls_[10], lastChangeValue_, 28);
    placeRow(dataPageControls_[12], switchReasonValue_, 48);
    MoveWindow(dataPageControls_[14], contentLeft, top + 6, labelWidth, 22, TRUE);
    MoveWindow(statsEdit_, fieldLeft, top, fieldWidth, 170, TRUE);
    top += 188;
    MoveWindow(benchmarkButton_, fieldLeft, top, 210, 34, TRUE);

    top = contentTop;
    const int displayFieldWidth = 220;
    auto placeDisplayRow = [&](HWND label, HWND control, int controlHeight) {
        MoveWindow(label, contentLeft, top + 6, labelWidth, 22, TRUE);
        MoveWindow(control, fieldLeft, top, displayFieldWidth, controlHeight, TRUE);
        top += controlHeight + rowGap;
    };
    placeDisplayRow(displayPageControls_[0], fontCombo_, 240);
    placeDisplayRow(displayPageControls_[2], fontSizeCombo_, 240);
    placeDisplayRow(displayPageControls_[4], textColorButton_, 32);
    placeDisplayRow(displayPageControls_[6], backgroundColorButton_, 32);
    MoveWindow(transparentCheck_, fieldLeft, top, 180, 24, TRUE);
    top += 40;
    MoveWindow(displayPageControls_[9], contentLeft, top + 3, labelWidth, 22, TRUE);
    MoveWindow(alignLeftRadio_, fieldLeft, top, 110, 24, TRUE);
    MoveWindow(alignCenterRadio_, fieldLeft + 124, top, 110, 24, TRUE);
    top += 40;
    MoveWindow(horizontalLayoutCheck_, fieldLeft, top, 180, 24, TRUE);

    previewRect_ = RECT{fieldLeft + displayFieldWidth + 34, contentTop + 26, cardRect_.right - 42, contentTop + 110};
    previewSummaryRect_ = RECT{previewRect_.left, previewRect_.bottom + 24, previewRect_.right, previewRect_.bottom + 210};
    syncControlsVisibility();
}

void SettingsWindow::paint() {
    PAINTSTRUCT paintStruct{};
    HDC dc = BeginPaint(windowHandle_, &paintStruct);
    RECT clientRect{};
    GetClientRect(windowHandle_, &clientRect);
    HBRUSH canvasBrush = CreateSolidBrush(kAppCanvas);
    FillRect(dc, &clientRect, canvasBrush);
    DeleteObject(canvasBrush);

    fillRoundedRect(dc, cardRect_, theme::kPanel, kCardBorder, 20);
    RECT headerRect{cardRect_.left, cardRect_.top, cardRect_.right, cardRect_.top + 56};
    fillRoundedRect(dc, headerRect, kAppHeader, kAppHeader, 20);
    fillRoundedRect(dc, dataTabRect_, activeTab_ == 0 ? kActiveTabFill : kInactiveTabFill, activeTab_ == 0 ? kActiveTabBorder : kCardBorder);
    fillRoundedRect(dc, displayTabRect_, activeTab_ == 1 ? kActiveTabFill : kInactiveTabFill, activeTab_ == 1 ? kActiveTabBorder : kCardBorder);
    drawTextLine(dc, L"数据设置", dataTabRect_, DT_CENTER | DT_VCENTER | DT_SINGLELINE, activeTab_ == 0 ? kActiveTabText : kInactiveTabText, bodyFont_);
    drawTextLine(dc, L"显示设置", displayTabRect_, DT_CENTER | DT_VCENTER | DT_SINGLELINE, activeTab_ == 1 ? kActiveTabText : kInactiveTabText, bodyFont_);

    if (activeTab_ == 1) {
        RECT previewTitle{previewRect_.left, previewRect_.top - 28, previewRect_.right, previewRect_.top - 2};
        drawTextLine(dc, L"任务栏预览", previewTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE, theme::kTextPrimary, sectionFont_);
        const COLORREF previewFill = settings_.backgroundTransparent ? RGB(245, 245, 245) : settings_.backgroundColor;
        fillRoundedRect(dc, previewRect_, previewFill, kCardBorder, 18);

        HFONT previewFont = CreateFontW(-(std::clamp)(settings_.fontSize + 4, 18, 34), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            settings_.fontName.empty() ? theme::kMonoFont : settings_.fontName.c_str());
        RECT textRect = previewRect_;
        InflateRect(&textRect, -18, -12);
        std::wstring previewText = settings_.horizontalLayout ? L"2345.67" : L"2345\n.67";
        UINT flags = DT_VCENTER | DT_NOPREFIX | (settings_.textAlignment == TextAlignment::Left ? DT_LEFT : DT_CENTER);
        flags |= settings_.horizontalLayout ? DT_SINGLELINE : DT_WORDBREAK;
        drawTextLine(dc, previewText, textRect, flags, settings_.textColor, previewFont);
        DeleteObject(previewFont);

        fillRoundedRect(dc, previewSummaryRect_, RGB(255, 255, 255), kCardBorder, 18);
        RECT summaryTitle{previewSummaryRect_.left + 20, previewSummaryRect_.top + 16, previewSummaryRect_.right - 20, previewSummaryRect_.top + 42};
        drawTextLine(dc, L"效果摘要", summaryTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE, theme::kTextPrimary, sectionFont_);
        RECT summaryLine = summaryTitle;
        summaryLine.top += 42;
        summaryLine.bottom = summaryLine.top + 24;
        for (const std::wstring& line : {std::wstring(L"1 秒请求一次"), std::wstring(L"目标 2-3 秒可见更新"),
                 std::wstring(L"设置修改立即保存"), std::wstring(L"任务栏样式实时刷新")}) {
            drawTextLine(dc, line, summaryLine, DT_LEFT | DT_VCENTER | DT_SINGLELINE, kMutedText, bodyFont_);
            OffsetRect(&summaryLine, 0, 32);
        }
    } else {
        RECT badgeRect{cardRect_.right - 274, dataTabRect_.top, cardRect_.right - 32, dataTabRect_.top + 36};
        fillRoundedRect(dc, badgeRect, kSuccessFill, kSuccessBorder, 10);
        drawTextLine(dc, statusSummaryText(), badgeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, kSuccessText, bodyFont_);
    }
    EndPaint(windowHandle_, &paintStruct);
}

void SettingsWindow::centerOnScreen() const {
    if (!windowHandle_) return;
    RECT windowRect{};
    GetWindowRect(windowHandle_, &windowRect);
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(MonitorFromWindow(windowHandle_, MONITOR_DEFAULTTONEAREST), &monitorInfo);
    const RECT& workRect = monitorInfo.rcWork;
    const int x = workRect.left + ((workRect.right - workRect.left) - width) / 2;
    const int y = workRect.top + ((workRect.bottom - workRect.top) - height) / 2;
    SetWindowPos(windowHandle_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void SettingsWindow::releaseHandles() {
    auto deleteFont = [](HFONT& font) { if (font) { DeleteObject(font); font = nullptr; } };
    deleteFont(titleFont_);
    deleteFont(sectionFont_);
    deleteFont(bodyFont_);
    deleteFont(monoFont_);
    windowHandle_ = nullptr;
    autoSourceCheck_ = nullptr;
    preferredProviderCombo_ = nullptr;
    requestRateLabel_ = nullptr;
    activeProviderValue_ = nullptr;
    lastSuccessValue_ = nullptr;
    lastChangeValue_ = nullptr;
    switchReasonValue_ = nullptr;
    statsEdit_ = nullptr;
    benchmarkButton_ = nullptr;
    fontCombo_ = nullptr;
    fontSizeCombo_ = nullptr;
    textColorButton_ = nullptr;
    backgroundColorButton_ = nullptr;
    transparentCheck_ = nullptr;
    alignLeftRadio_ = nullptr;
    alignCenterRadio_ = nullptr;
    horizontalLayoutCheck_ = nullptr;
    dataPageControls_.clear();
    displayPageControls_.clear();
}

void SettingsWindow::applyFonts() {
    titleFont_ = theme::createUiFont(22, FW_BOLD);
    sectionFont_ = theme::createUiFont(18, FW_BOLD);
    bodyFont_ = theme::createUiFont(16, FW_NORMAL);
    monoFont_ = theme::createMonoFont(18, FW_BOLD);
    EnumChildWindows(windowHandle_, [](HWND hwnd, LPARAM lParam) -> BOOL {
        const auto self = reinterpret_cast<SettingsWindow*>(lParam);
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(self->bodyFont_), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
}

void SettingsWindow::syncControlsFromSettings() {
    if (!windowHandle_) return;
    updatingControls_ = true;
    SendMessageW(autoSourceCheck_, BM_SETCHECK, settings_.autoSourceSelection ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(preferredProviderCombo_, CB_SETCURSEL, comboIndexFromProvider(settings_.preferredProvider), 0);
    const int fontIndex = static_cast<int>(SendMessageW(fontCombo_, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(settings_.fontName.c_str())));
    SendMessageW(fontCombo_, CB_SETCURSEL, fontIndex >= 0 ? fontIndex : 0, 0);
    wchar_t fontSizeBuffer[16]{};
    swprintf_s(fontSizeBuffer, L"%d", settings_.fontSize);
    const int fontSizeIndex = static_cast<int>(SendMessageW(fontSizeCombo_, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(fontSizeBuffer)));
    SendMessageW(fontSizeCombo_, CB_SETCURSEL, fontSizeIndex >= 0 ? fontSizeIndex : 3, 0);
    updateColorButton(textColorButton_, settings_.textColor);
    updateColorButton(backgroundColorButton_, settings_.backgroundColor);
    SendMessageW(transparentCheck_, BM_SETCHECK, settings_.backgroundTransparent ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(alignLeftRadio_, BM_SETCHECK, settings_.textAlignment == TextAlignment::Left ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(alignCenterRadio_, BM_SETCHECK, settings_.textAlignment == TextAlignment::Center ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(horizontalLayoutCheck_, BM_SETCHECK, settings_.horizontalLayout ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(backgroundColorButton_, settings_.backgroundTransparent ? FALSE : TRUE);
    updatingControls_ = false;
}

void SettingsWindow::syncControlsVisibility() {
    for (HWND control : dataPageControls_) ShowWindow(control, activeTab_ == 0 ? SW_SHOW : SW_HIDE);
    for (HWND control : displayPageControls_) ShowWindow(control, activeTab_ == 1 ? SW_SHOW : SW_HIDE);
}

void SettingsWindow::applyControlChanges() {
    settings_.autoSourceSelection = SendMessageW(autoSourceCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings_.preferredProvider = providerFromComboIndex(static_cast<int>(SendMessageW(preferredProviderCombo_, CB_GETCURSEL, 0, 0)));
    const int fontIndex = static_cast<int>(SendMessageW(fontCombo_, CB_GETCURSEL, 0, 0));
    wchar_t textBuffer[128]{};
    if (fontIndex >= 0) {
        SendMessageW(fontCombo_, CB_GETLBTEXT, fontIndex, reinterpret_cast<LPARAM>(textBuffer));
        settings_.fontName = textBuffer;
    }
    const int sizeIndex = static_cast<int>(SendMessageW(fontSizeCombo_, CB_GETCURSEL, 0, 0));
    if (sizeIndex >= 0) {
        wchar_t sizeBuffer[32]{};
        SendMessageW(fontSizeCombo_, CB_GETLBTEXT, sizeIndex, reinterpret_cast<LPARAM>(sizeBuffer));
        settings_.fontSize = (std::clamp)(_wtoi(sizeBuffer), 14, 32);
    }
    settings_.backgroundTransparent = SendMessageW(transparentCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings_.textAlignment = SendMessageW(alignLeftRadio_, BM_GETCHECK, 0, 0) == BST_CHECKED ? TextAlignment::Left : TextAlignment::Center;
    settings_.horizontalLayout = SendMessageW(horizontalLayoutCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(backgroundColorButton_, settings_.backgroundTransparent ? FALSE : TRUE);
    if (settingsChangedCallback_) settingsChangedCallback_(settings_);
    InvalidateRect(windowHandle_, nullptr, TRUE);
}

void SettingsWindow::refreshStatusText() {
    if (!windowHandle_) return;
    SetWindowTextW(activeProviderValue_, priceProviderLabel(status_.activeProvider));
    SetWindowTextW(lastSuccessValue_, formatTimestamp(status_.lastSuccessfulAt).c_str());
    SetWindowTextW(lastChangeValue_, formatTimestamp(status_.lastChangedAt).c_str());
    SetWindowTextW(switchReasonValue_, status_.lastSwitchReason.empty() ? L"--" : status_.lastSwitchReason.c_str());
    SetWindowTextW(statsEdit_, statsTableText().c_str());
    SetWindowTextW(requestRateLabel_, statusSummaryText().c_str());
    EnableWindow(benchmarkButton_, status_.benchmarkRunning ? FALSE : TRUE);
    InvalidateRect(windowHandle_, nullptr, TRUE);
}

void SettingsWindow::updateColorButton(HWND button, COLORREF color) const { SetWindowTextW(button, colorToHex(color).c_str()); }

bool SettingsWindow::chooseColorFor(COLORREF& color) {
    static COLORREF customColors[16]{};
    CHOOSECOLORW chooseColor{};
    chooseColor.lStructSize = sizeof(chooseColor);
    chooseColor.hwndOwner = windowHandle_;
    chooseColor.rgbResult = color;
    chooseColor.lpCustColors = customColors;
    chooseColor.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&chooseColor)) return false;
    color = chooseColor.rgbResult;
    return true;
}

std::wstring SettingsWindow::statusSummaryText() const {
    std::wstringstream stream;
    stream << L"1 秒请求一次，目标 2-3 秒有效更新";
    if (status_.benchmarkRunning) stream << L" / Benchmark 运行中";
    else if (!status_.benchmarkResults.empty()) stream << L" / 推荐 " << priceProviderLabel(status_.benchmarkRecommendedProvider);
    return stream.str();
}

std::wstring SettingsWindow::statsTableText() const {
    std::wstringstream stream;
    for (const auto& provider : status_.providerStats) {
        const int total = provider.successCount + provider.errorCount;
        const int successPercent = total > 0 ? static_cast<int>((provider.successCount * 100.0) / total) : 0;
        stream << L"[" << priceProviderLabel(provider.provider) << L"] 成功率=" << successPercent << L"% "
               << L"延迟=" << provider.lastLatencyMs << L"ms "
               << L"最近错误=" << (provider.lastError.empty() ? L"--" : provider.lastError) << L"\r\n"
               << L"最近成功=" << formatTimestamp(provider.lastSuccessAt)
               << L" 最近变价=" << formatTimestamp(provider.lastChangeAt)
               << L" 变价次数=" << provider.changeCount
               << L" 连续失败=" << provider.consecutiveFailures << L"\r\n\r\n";
    }
    if (!status_.benchmarkResults.empty()) {
        stream << L"Benchmark 摘要\r\n";
        for (const auto& result : status_.benchmarkResults) {
            stream << L"- " << priceProviderLabel(result.provider)
                   << L" 样本=" << result.sampleCount
                   << L" 成功=" << result.successCount
                   << L" 变价=" << result.changeCount
                   << L" 中位延迟=" << result.medianLatencyMs << L"ms"
                   << L" 平均变价间隔=" << result.averageChangeIntervalSec << L"s";
            if (!result.lastError.empty()) stream << L" 错误=" << result.lastError;
            stream << L"\r\n";
        }
    }
    return stream.str();
}

}  // namespace goldview
