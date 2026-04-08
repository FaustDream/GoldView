#pragma once

#include <windows.h>

namespace goldview::theme {

constexpr COLORREF kGold = RGB(241, 196, 15);
constexpr COLORREF kGoldDark = RGB(184, 134, 11);
constexpr COLORREF kInk = RGB(17, 24, 39);
constexpr COLORREF kPanel = RGB(248, 250, 252);
constexpr COLORREF kPanelBorder = RGB(226, 232, 240);
constexpr COLORREF kTextPrimary = RGB(15, 23, 42);
constexpr COLORREF kTextSecondary = RGB(71, 85, 105);
constexpr COLORREF kSuccess = RGB(34, 197, 94);

constexpr wchar_t kMonoFont[] = L"Consolas";
constexpr wchar_t kUiFont[] = L"Segoe UI";

inline HFONT createUiFont(int height, int weight = FW_NORMAL) {
    return CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        kUiFont);
}

inline HFONT createMonoFont(int height, int weight = FW_BOLD) {
    return CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        kMonoFont);
}

}  // namespace goldview::theme
