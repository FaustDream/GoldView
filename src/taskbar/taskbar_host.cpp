#include "taskbar_host.h"

#include <algorithm>
#include <gdiplus.h>
#include <mutex>

#include "taskbar_text_style.h"
#include "theme.h"

namespace goldview {

namespace {

constexpr wchar_t kTaskbarHostClassName[] = L"GoldViewNativeTaskbarHost";

class GdiplusSession {
public:
    static GdiplusSession& instance() {
        static GdiplusSession session;
        return session;
    }

    bool isReady() const {
        return token_ != 0;
    }

private:
    GdiplusSession() {
        Gdiplus::GdiplusStartupInput startupInput;
        if (Gdiplus::GdiplusStartup(&token_, &startupInput, nullptr) != Gdiplus::Ok) {
            token_ = 0;
        }
    }

    ~GdiplusSession() {
        if (token_ != 0) {
            Gdiplus::GdiplusShutdown(token_);
        }
    }

    ULONG_PTR token_ = 0;
};

bool isGdiplusReady() {
    static std::once_flag startupFlag;
    static bool startupReady = false;
    std::call_once(startupFlag, []() {
        startupReady = GdiplusSession::instance().isReady();
    });
    return startupReady;
}

std::wstring buildRenderedText(const std::wstring& priceText, bool horizontalLayout) {
    std::wstring renderedText = priceText;
    if (!horizontalLayout) {
        const auto separator = renderedText.find(L'.');
        if (separator != std::wstring::npos) {
            renderedText.insert(separator, L"\n");
        }
    }
    return renderedText;
}

UINT buildTextFlags(const DisplaySettings& settings) {
    UINT flags = DT_VCENTER | DT_NOPREFIX;
    if (settings.horizontalLayout) {
        flags |= DT_SINGLELINE;
    } else {
        flags |= DT_WORDBREAK;
    }
    flags |= settings.textAlignment == TextAlignment::Left ? DT_LEFT : DT_CENTER;
    return flags;
}

Gdiplus::Color toGdiplusColor(COLORREF color, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

Gdiplus::StringAlignment toStringAlignment(TextAlignment alignment) {
    return alignment == TextAlignment::Left ? Gdiplus::StringAlignmentNear : Gdiplus::StringAlignmentCenter;
}

}  // namespace

bool TaskbarHost::create(HINSTANCE instanceHandle) {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = TaskbarHost::windowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.lpszClassName = kTaskbarHostClassName;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
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
    return true;
}

void TaskbarHost::show() {
    ShowWindow(windowHandle_, SW_SHOWNOACTIVATE);
    SetWindowPos(windowHandle_, HWND_TOP, 0, 0, 0, 0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    paint();
}

void TaskbarHost::hide() {
    ShowWindow(windowHandle_, SW_HIDE);
}

void TaskbarHost::updateContent(const PriceSnapshot& snapshot, const DisplaySettings& settings) {
    settings_ = taskbar_text_style::normalizeDisplaySettings(settings);
    if (snapshot.currentPrice > 0.0) {
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%.2f", normalizeDisplayedPrice(snapshot.currentPrice));
        priceText_ = buffer;
    } else {
        priceText_ = L"--.--";
    }
    paint();
}

HWND TaskbarHost::hwnd() const {
    return windowHandle_;
}

void TaskbarHost::applyBounds(const RECT& rect) {
    bounds_ = rect;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    SetWindowPos(windowHandle_, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    paint();
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
    paint();
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
    paint();
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
    case WM_PAINT: {
        PAINTSTRUCT paintStruct{};
        BeginPaint(windowHandle_, &paintStruct);
        EndPaint(windowHandle_, &paintStruct);
        paint();
        return 0;
    }
    default:
        return DefWindowProcW(windowHandle_, message, wParam, lParam);
    }
}

HFONT TaskbarHost::createDisplayFont() const {
    const int clampedFontSize = taskbar_text_style::normalizeFontSize(settings_.fontSize);
    return CreateFontW(
        -clampedFontSize,
        0,
        0,
        0,
        taskbar_text_style::fontWeight(),
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        taskbar_text_style::fontQuality(),
        DEFAULT_PITCH | FF_DONTCARE,
        settings_.fontName.empty() ? theme::kUiFont : settings_.fontName.c_str());
}

void TaskbarHost::paint() {
    if (!windowHandle_ || !isGdiplusReady()) {
        return;
    }

    RECT clientRect{};
    GetClientRect(windowHandle_, &clientRect);
    const int width = clientRect.right - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    auto screenDc = GetDC(nullptr);
    if (!screenDc) {
        return;
    }

    auto memoryDc = CreateCompatibleDC(screenDc);
    if (!memoryDc) {
        ReleaseDC(nullptr, screenDc);
        return;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bitmapBits = nullptr;
    auto dibSection = CreateDIBSection(
        screenDc,
        &bitmapInfo,
        DIB_RGB_COLORS,
        &bitmapBits,
        nullptr,
        0);
    if (!dibSection || !bitmapBits) {
        if (dibSection) {
            DeleteObject(dibSection);
        }
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return;
    }

    auto oldBitmap = SelectObject(memoryDc, dibSection);
    Gdiplus::Bitmap layeredBitmap(
        width,
        height,
        width * 4,
        PixelFormat32bppPARGB,
        static_cast<BYTE*>(bitmapBits));
    Gdiplus::Graphics graphics(&layeredBitmap);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    if (!settings_.backgroundTransparent) {
        Gdiplus::SolidBrush backgroundBrush(toGdiplusColor(settings_.backgroundColor));
        graphics.FillRectangle(&backgroundBrush, 0, 0, width, height);
    }

    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    RECT priceRect = clientRect;
    InflateRect(&priceRect, -6, -2);
    const std::wstring renderedText = buildRenderedText(priceText_, settings_.horizontalLayout);
    const UINT flags = buildTextFlags(settings_);
    (void)flags;

    HFONT largeFont = createDisplayFont();
    if (largeFont) {
        Gdiplus::Font font(memoryDc, largeFont);
        Gdiplus::StringFormat format;
        format.SetAlignment(toStringAlignment(settings_.textAlignment));
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        if (settings_.horizontalLayout) {
            format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsNoWrap);
        }

        const Gdiplus::RectF layoutRect(
            static_cast<Gdiplus::REAL>(priceRect.left),
            static_cast<Gdiplus::REAL>(priceRect.top),
            static_cast<Gdiplus::REAL>(priceRect.right - priceRect.left),
            static_cast<Gdiplus::REAL>(priceRect.bottom - priceRect.top));
        Gdiplus::SolidBrush textBrush(toGdiplusColor(settings_.textColor));
        graphics.DrawString(renderedText.c_str(), -1, &font, layoutRect, &format, &textBrush);
        DeleteObject(largeFont);
    }

    BLENDFUNCTION blendFunction{};
    blendFunction.BlendOp = AC_SRC_OVER;
    blendFunction.SourceConstantAlpha = 255;
    blendFunction.AlphaFormat = AC_SRC_ALPHA;

    SIZE windowSize{width, height};
    POINT sourcePoint{0, 0};
    UpdateLayeredWindow(
        windowHandle_,
        screenDc,
        nullptr,
        &windowSize,
        memoryDc,
        &sourcePoint,
        0,
        &blendFunction,
        ULW_ALPHA);

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(dibSection);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
}

}  // namespace goldview
