#include "taskbar_host.h"

#include <algorithm>

#include "theme.h"

namespace goldview {

namespace {

constexpr wchar_t kTaskbarHostClassName[] = L"GoldViewNativeTaskbarHost";
constexpr COLORREF kTransparentKey = RGB(255, 0, 255);

}  // namespace

bool TaskbarHost::create(HINSTANCE instanceHandle) {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = TaskbarHost::windowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.lpszClassName = kTaskbarHostClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&windowClass);

    windowHandle_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        kTaskbarHostClassName,
        L"GoldView",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        220,
        30,
        nullptr,
        nullptr,
        instanceHandle,
        this);
    if (!windowHandle_) {
        return false;
    }

    SetLayeredWindowAttributes(windowHandle_, kTransparentKey, 0, LWA_COLORKEY);
    return true;
}

void TaskbarHost::show() {
    ShowWindow(windowHandle_, SW_SHOWNOACTIVATE);
    SetWindowPos(windowHandle_, HWND_TOP, 0, 0, 0, 0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    RedrawWindow(windowHandle_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    UpdateWindow(windowHandle_);
}

void TaskbarHost::hide() {
    ShowWindow(windowHandle_, SW_HIDE);
}

void TaskbarHost::updateContent(const PriceSnapshot& snapshot, const DisplaySettings& settings) {
    settings_ = settings;
    if (snapshot.currentPrice > 0.0) {
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%.2f", snapshot.currentPrice);
        priceText_ = buffer;
    } else {
        priceText_ = L"--.--";
    }

    InvalidateRect(windowHandle_, nullptr, TRUE);
}

HWND TaskbarHost::hwnd() const {
    return windowHandle_;
}

void TaskbarHost::applyBounds(const RECT& rect) {
    bounds_ = rect;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    SetWindowPos(windowHandle_, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

bool TaskbarHost::attachToTaskbarContainer(HWND parent) {
    if (!windowHandle_ || !parent) {
        return false;
    }

    if (parentHandle_ == parent && embeddedInTaskbar_ && GetParent(windowHandle_) == parent) {
        return true;
    }

    LONG_PTR style = GetWindowLongPtrW(windowHandle_, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_POPUP);
    style |= WS_CHILD;
    SetWindowLongPtrW(windowHandle_, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtrW(windowHandle_, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_TOPMOST);
    exStyle |= WS_EX_LAYERED | WS_EX_NOACTIVATE;
    SetWindowLongPtrW(windowHandle_, GWL_EXSTYLE, exStyle);

    const HWND attachedParent = SetParent(windowHandle_, parent);
    if (!attachedParent && GetLastError() != 0) {
        return false;
    }

    parentHandle_ = parent;
    embeddedInTaskbar_ = true;
    SetWindowPos(windowHandle_, HWND_TOP, 0, 0, 0, 0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    return true;
}

void TaskbarHost::detachFromTaskbarContainer() {
    if (!windowHandle_ || !embeddedInTaskbar_) {
        return;
    }

    ShowWindow(windowHandle_, SW_HIDE);
    SetParent(windowHandle_, nullptr);

    LONG_PTR style = GetWindowLongPtrW(windowHandle_, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_CHILD);
    style |= WS_POPUP;
    SetWindowLongPtrW(windowHandle_, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtrW(windowHandle_, GWL_EXSTYLE);
    exStyle |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED;
    SetWindowLongPtrW(windowHandle_, GWL_EXSTYLE, exStyle);

    embeddedInTaskbar_ = false;
    parentHandle_ = nullptr;
    SetWindowPos(windowHandle_, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

bool TaskbarHost::isAttachedToTaskbarContainer(HWND expectedParent) const {
    if (!windowHandle_ || !embeddedInTaskbar_) {
        return false;
    }

    const HWND currentParent = GetParent(windowHandle_);
    if (expectedParent) {
        return currentParent == expectedParent;
    }
    return currentParent == parentHandle_;
}

HWND TaskbarHost::parentHwnd() const {
    return parentHandle_;
}

void TaskbarHost::applyBoundsInParent(const RECT& rect) {
    bounds_ = rect;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    SetWindowPos(windowHandle_, HWND_TOP, rect.left, rect.top, width, height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RedrawWindow(windowHandle_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

LRESULT CALLBACK TaskbarHost::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto self = static_cast<TaskbarHost*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    const auto self = reinterpret_cast<TaskbarHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT TaskbarHost::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    default:
        return DefWindowProcW(windowHandle_, message, wParam, lParam);
    }
}

HFONT TaskbarHost::createDisplayFont() const {
    const int clampedFontSize = (std::clamp)(settings_.fontSize, 14, 32);
    return CreateFontW(
        -clampedFontSize,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        settings_.fontName.empty() ? theme::kMonoFont : settings_.fontName.c_str());
}

void TaskbarHost::paint() {
    PAINTSTRUCT paintStruct{};
    const auto deviceContext = BeginPaint(windowHandle_, &paintStruct);
    RECT clientRect{};
    GetClientRect(windowHandle_, &clientRect);

    if (settings_.backgroundTransparent) {
        const auto transparentBrush = CreateSolidBrush(kTransparentKey);
        FillRect(deviceContext, &clientRect, transparentBrush);
        DeleteObject(transparentBrush);
    } else {
        const auto backgroundBrush = CreateSolidBrush(settings_.backgroundColor);
        FillRect(deviceContext, &clientRect, backgroundBrush);
        DeleteObject(backgroundBrush);
    }

    SetBkMode(deviceContext, TRANSPARENT);
    SetTextColor(deviceContext, settings_.textColor);

    HFONT largeFont = createDisplayFont();
    const auto oldFont = SelectObject(deviceContext, largeFont);

    RECT priceRect = clientRect;
    InflateRect(&priceRect, -6, -2);
    std::wstring renderedText = priceText_;
    if (!settings_.horizontalLayout) {
        const auto separator = renderedText.find(L'.');
        if (separator != std::wstring::npos) {
            renderedText.insert(separator, L"\n");
        }
    }

    UINT flags = DT_VCENTER | DT_NOPREFIX;
    if (settings_.horizontalLayout) {
        flags |= DT_SINGLELINE;
    } else {
        flags |= DT_WORDBREAK;
    }
    flags |= settings_.textAlignment == TextAlignment::Left ? DT_LEFT : DT_CENTER;
    DrawTextW(deviceContext, renderedText.c_str(), -1, &priceRect, flags);

    SelectObject(deviceContext, oldFont);
    DeleteObject(largeFont);
    EndPaint(windowHandle_, &paintStruct);
}

}  // namespace goldview
