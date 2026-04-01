#include "floating_host.h"

#include "theme.h"

namespace goldview {

namespace {

constexpr wchar_t kFloatingHostClassName[] = L"GoldViewNativeFloatingHost";

RECT queryPrimaryWorkArea() {
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    return workArea;
}

}  // namespace

bool FloatingHost::create(HINSTANCE instanceHandle) {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = FloatingHost::windowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.lpszClassName = kFloatingHostClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&windowClass);

    windowHandle_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kFloatingHostClassName,
        L"GoldView 桌面悬浮模式",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        160,
        60,
        nullptr,
        nullptr,
        instanceHandle,
        this);
    return windowHandle_ != nullptr;
}

void FloatingHost::show() {
    positionNearPrimaryTaskbar();
    ShowWindow(windowHandle_, SW_SHOWNOACTIVATE);
    UpdateWindow(windowHandle_);
}

void FloatingHost::hide() {
    ShowWindow(windowHandle_, SW_HIDE);
}

void FloatingHost::updateContent(const PriceSnapshot& snapshot, const DisplaySettings&) {
    if (snapshot.currentPrice > 0.0) {
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%.2f", snapshot.currentPrice);
        priceText_ = buffer;
    } else {
        priceText_ = L"--.--";
    }

    InvalidateRect(windowHandle_, nullptr, TRUE);
}

HWND FloatingHost::hwnd() const {
    return windowHandle_;
}

void FloatingHost::applyBounds(const RECT& rect) {
    SetWindowPos(windowHandle_, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

LRESULT CALLBACK FloatingHost::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto self = static_cast<FloatingHost*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    const auto self = reinterpret_cast<FloatingHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT FloatingHost::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    default:
        return DefWindowProcW(windowHandle_, message, wParam, lParam);
    }
}

void FloatingHost::positionNearPrimaryTaskbar() const {
    const RECT workArea = queryPrimaryWorkArea();
    const int width = 176;
    const int height = 52;
    SetWindowPos(windowHandle_, HWND_TOPMOST, workArea.right - width - 16, workArea.bottom - height - 20,
        width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void FloatingHost::paint() {
    PAINTSTRUCT paintStruct{};
    const auto deviceContext = BeginPaint(windowHandle_, &paintStruct);
    RECT clientRect{};
    GetClientRect(windowHandle_, &clientRect);

    const auto background = CreateSolidBrush(theme::kPanel);
    FillRect(deviceContext, &clientRect, background);
    DeleteObject(background);

    const auto border = CreateSolidBrush(theme::kPanelBorder);
    FrameRect(deviceContext, &clientRect, border);
    DeleteObject(border);

    SetBkMode(deviceContext, TRANSPARENT);
    SetTextColor(deviceContext, theme::kTextPrimary);

    HFONT largeFont = theme::createMonoFont(22, FW_BOLD);
    const auto oldFont = SelectObject(deviceContext, largeFont);
    RECT priceRect = clientRect;
    priceRect.top += 10;
    DrawTextW(deviceContext, priceText_.c_str(), -1, &priceRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
    SelectObject(deviceContext, oldFont);
    DeleteObject(largeFont);

    EndPaint(windowHandle_, &paintStruct);
}

}  // namespace goldview
