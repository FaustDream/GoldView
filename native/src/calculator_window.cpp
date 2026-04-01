#include "calculator_window.h"

#include <commctrl.h>

#include <stdexcept>

#include "theme.h"

namespace goldview {

namespace {

constexpr wchar_t kCalculatorClassName[] = L"GoldViewCalculatorWindow";
constexpr int kWindowWidth = 420;
constexpr int kWindowHeight = 560;
constexpr int kIdOldPrice = 100;
constexpr int kIdOldWeight = 101;
constexpr int kIdNewPrice = 102;
constexpr int kIdNewWeight = 103;
constexpr int kIdCalculate = 104;
constexpr int kIdClear = 105;
constexpr int kIdResultPrice = 106;
constexpr int kIdResultTotal = 107;
constexpr int kIdHistory = 108;

void setControlText(HWND control, const std::wstring& text) {
    if (control) {
        SetWindowTextW(control, text.c_str());
    }
}

HWND createLabel(HWND parent, HINSTANCE instanceHandle, const wchar_t* text, int id = 0) {
    return CreateWindowExW(
        0,
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instanceHandle,
        nullptr);
}

HWND createEdit(HWND parent, HINSTANCE instanceHandle, int id) {
    return CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instanceHandle,
        nullptr);
}

HWND createButton(HWND parent, HINSTANCE instanceHandle, const wchar_t* text, int id) {
    return CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instanceHandle,
        nullptr);
}

}  // namespace

CalculatorWindow::CalculatorWindow(SettingsStore& settingsStore, AverageService& averageService)
    : settingsStore_(settingsStore), averageService_(averageService) {}

bool CalculatorWindow::create(HINSTANCE instanceHandle) {
    if (windowHandle_) {
        return true;
    }

    instanceHandle_ = instanceHandle;

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = CalculatorWindow::windowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.lpszClassName = kCalculatorClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(theme::kPanel);
    RegisterClassW(&windowClass);

    windowHandle_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kCalculatorClassName,
        L"GoldView Average Calculator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        0,
        0,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instanceHandle,
        this);

    if (!windowHandle_) {
        return false;
    }

    createControls();
    applyFonts();
    loadHistory();
    centerOnScreen();
    return true;
}

void CalculatorWindow::show() {
    if (!windowHandle_ && !create(instanceHandle_)) {
        return;
    }

    centerOnScreen();
    ShowWindow(windowHandle_, SW_SHOWNORMAL);
    UpdateWindow(windowHandle_);
}

void CalculatorWindow::focusOrShow() {
    if (!windowHandle_ && !create(instanceHandle_)) {
        return;
    }

    centerOnScreen();
    ShowWindow(windowHandle_, IsIconic(windowHandle_) ? SW_RESTORE : SW_SHOWNORMAL);
    SetForegroundWindow(windowHandle_);
}

void CalculatorWindow::destroy() {
    if (windowHandle_) {
        DestroyWindow(windowHandle_);
    }
}

bool CalculatorWindow::isCreated() const {
    return windowHandle_ != nullptr;
}

HWND CalculatorWindow::hwnd() const {
    return windowHandle_;
}

LRESULT CALLBACK CalculatorWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto self = static_cast<CalculatorWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    const auto self = reinterpret_cast<CalculatorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CalculatorWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        layoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kIdCalculate:
            calculate();
            return 0;
        case kIdClear:
            clearInputs();
            return 0;
        default:
            break;
        }
        break;
    case WM_PAINT:
        paint();
        return 0;
    case WM_CLOSE:
        DestroyWindow(windowHandle_);
        return 0;
    case WM_DESTROY:
        releaseHandles();
        return 0;
    default:
        break;
    }

    return DefWindowProcW(windowHandle_, message, wParam, lParam);
}

void CalculatorWindow::createControls() {
    oldPriceLabel_ = createLabel(windowHandle_, instanceHandle_, L"Current price");
    oldPriceEdit_ = createEdit(windowHandle_, instanceHandle_, kIdOldPrice);
    oldWeightLabel_ = createLabel(windowHandle_, instanceHandle_, L"Current weight");
    oldWeightEdit_ = createEdit(windowHandle_, instanceHandle_, kIdOldWeight);

    newPriceLabel_ = createLabel(windowHandle_, instanceHandle_, L"Buy price");
    newPriceEdit_ = createEdit(windowHandle_, instanceHandle_, kIdNewPrice);
    newWeightLabel_ = createLabel(windowHandle_, instanceHandle_, L"Buy weight");
    newWeightEdit_ = createEdit(windowHandle_, instanceHandle_, kIdNewWeight);

    calculateButton_ = createButton(windowHandle_, instanceHandle_, L"Calculate", kIdCalculate);
    clearButton_ = createButton(windowHandle_, instanceHandle_, L"Clear", kIdClear);
    resultPriceLabel_ = createLabel(windowHandle_, instanceHandle_, L"Average: --", kIdResultPrice);
    resultTotalLabel_ = createLabel(windowHandle_, instanceHandle_, L"Total weight: --", kIdResultTotal);
    historyList_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        nullptr,
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
        0,
        0,
        0,
        0,
        windowHandle_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHistory)),
        instanceHandle_,
        nullptr);

    layoutControls(kWindowWidth, kWindowHeight);
}

void CalculatorWindow::applyFonts() {
    releaseFonts();

    titleFont_ = theme::createUiFont(22, FW_BOLD);
    bodyFont_ = theme::createUiFont(18, FW_NORMAL);
    monoFont_ = theme::createMonoFont(20, FW_BOLD);

    EnumChildWindows(windowHandle_, [](HWND hwnd, LPARAM lParam) -> BOOL {
        const auto self = reinterpret_cast<CalculatorWindow*>(lParam);
        const int id = GetDlgCtrlID(hwnd);
        HFONT font = self->bodyFont_;
        if (id == kIdResultPrice) {
            font = self->monoFont_;
        }
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
}

void CalculatorWindow::layoutControls(int width, int) {
    const int margin = 20;
    const int labelWidth = width - margin * 2;
    const int editWidth = labelWidth;
    const int editHeight = 30;
    int top = 8;

    auto placePair = [&](HWND label, HWND edit) {
        MoveWindow(label, margin, top, labelWidth, 22, TRUE);
        top += 24;
        MoveWindow(edit, margin, top, editWidth, editHeight, TRUE);
        top += 42;
    };

    placePair(oldPriceLabel_, oldPriceEdit_);
    placePair(oldWeightLabel_, oldWeightEdit_);
    placePair(newPriceLabel_, newPriceEdit_);
    placePair(newWeightLabel_, newWeightEdit_);

    MoveWindow(clearButton_, margin, top, 100, 34, TRUE);
    MoveWindow(calculateButton_, width - margin - 140, top, 140, 34, TRUE);
    top += 54;

    MoveWindow(resultPriceLabel_, margin, top, labelWidth, 28, TRUE);
    top += 30;
    MoveWindow(resultTotalLabel_, margin, top, labelWidth, 22, TRUE);
    top += 34;

    MoveWindow(historyList_, margin, top, labelWidth, 150, TRUE);
}

void CalculatorWindow::paint() {
    PAINTSTRUCT paintStruct{};
    const auto deviceContext = BeginPaint(windowHandle_, &paintStruct);
    RECT clientRect{};
    GetClientRect(windowHandle_, &clientRect);

    const auto brush = CreateSolidBrush(theme::kPanel);
    FillRect(deviceContext, &clientRect, brush);
    DeleteObject(brush);

    EndPaint(windowHandle_, &paintStruct);
}

void CalculatorWindow::centerOnScreen() const {
    if (!windowHandle_) {
        return;
    }

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

void CalculatorWindow::releaseHandles() {
    releaseFonts();
    windowHandle_ = nullptr;
    oldPriceLabel_ = nullptr;
    oldPriceEdit_ = nullptr;
    oldWeightLabel_ = nullptr;
    oldWeightEdit_ = nullptr;
    newPriceLabel_ = nullptr;
    newPriceEdit_ = nullptr;
    newWeightLabel_ = nullptr;
    newWeightEdit_ = nullptr;
    calculateButton_ = nullptr;
    clearButton_ = nullptr;
    resultPriceLabel_ = nullptr;
    resultTotalLabel_ = nullptr;
    historyList_ = nullptr;
}

void CalculatorWindow::releaseFonts() {
    if (titleFont_) {
        DeleteObject(titleFont_);
        titleFont_ = nullptr;
    }
    if (bodyFont_) {
        DeleteObject(bodyFont_);
        bodyFont_ = nullptr;
    }
    if (monoFont_) {
        DeleteObject(monoFont_);
        monoFont_ = nullptr;
    }
}

double CalculatorWindow::parseEditDouble(HWND edit, bool& ok) const {
    wchar_t buffer[64]{};
    GetWindowTextW(edit, buffer, static_cast<int>(std::size(buffer)));
    wchar_t* endPtr = nullptr;
    const double value = wcstod(buffer, &endPtr);
    ok = endPtr != buffer && *endPtr == L'\0';
    return value;
}

void CalculatorWindow::calculate() {
    bool okOldPrice = false;
    bool okOldWeight = false;
    bool okNewPrice = false;
    bool okNewWeight = false;
    const double oldPrice = parseEditDouble(oldPriceEdit_, okOldPrice);
    const double oldWeight = parseEditDouble(oldWeightEdit_, okOldWeight);
    const double newPrice = parseEditDouble(newPriceEdit_, okNewPrice);
    const double newWeight = parseEditDouble(newWeightEdit_, okNewWeight);

    if (!(okOldPrice && okOldWeight && okNewPrice && okNewWeight)) {
        setResultText(L"Invalid input", L"Please enter valid numbers");
        return;
    }

    try {
        const auto calculation = averageService_.calculate(oldPrice, oldWeight, newPrice, newWeight);

        wchar_t priceBuffer[64]{};
        wchar_t totalBuffer[64]{};
        swprintf_s(priceBuffer, L"Average: %.2f", calculation.averagePrice);
        swprintf_s(totalBuffer, L"Total weight: %.2f", calculation.totalWeight);
        setResultText(priceBuffer, totalBuffer);

        auto history = settingsStore_.loadCalculatorHistory();
        history.insert(history.begin(), averageService_.formatHistoryRecord(oldPrice, oldWeight, newPrice, newWeight, calculation));
        if (history.size() > 5) {
            history.resize(5);
        }
        settingsStore_.saveCalculatorHistory(history);
        renderHistory(history);
    } catch (const std::exception&) {
        setResultText(L"Calculation failed", L"Total weight must be greater than 0");
    }
}

void CalculatorWindow::clearInputs() {
    setControlText(oldPriceEdit_, L"");
    setControlText(oldWeightEdit_, L"");
    setControlText(newPriceEdit_, L"");
    setControlText(newWeightEdit_, L"");
    setResultText(L"Average: --", L"Total weight: --");
}

void CalculatorWindow::loadHistory() {
    renderHistory(settingsStore_.loadCalculatorHistory());
}

void CalculatorWindow::renderHistory(const std::vector<std::wstring>& history) {
    if (!historyList_) {
        return;
    }

    SendMessageW(historyList_, LB_RESETCONTENT, 0, 0);
    for (const auto& entry : history) {
        SendMessageW(historyList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.c_str()));
    }
}

void CalculatorWindow::setResultText(const std::wstring& priceText, const std::wstring& totalText) {
    setControlText(resultPriceLabel_, priceText);
    setControlText(resultTotalLabel_, totalText);
}

}  // namespace goldview
