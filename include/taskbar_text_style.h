#pragma once

#include <algorithm>
#include <string>

#include "models.h"
#include "theme.h"

namespace goldview::taskbar_text_style {

inline constexpr COLORREF kLegacyGoldTextColor = RGB(241, 196, 15);
inline constexpr COLORREF kLegacyNeonGreenTextColor = RGB(32, 255, 96);
inline constexpr COLORREF kLegacyComfortGreenTextColor = RGB(34, 197, 94);
inline constexpr COLORREF kPreferredTextColor = RGB(0, 0, 0);
inline constexpr int kLegacyFontSize = 20;
inline constexpr int kLegacyComfortFontSize = 17;
inline constexpr int kPreferredFontSize = 18;

inline std::wstring normalizeFontName(const std::wstring& fontName) {
    if (fontName.empty() || fontName == theme::kMonoFont) {
        return theme::kUiFont;
    }
    return fontName;
}

inline int normalizeFontSize(int fontSize) {
    if (fontSize == kLegacyFontSize || fontSize == kLegacyComfortFontSize) {
        return kPreferredFontSize;
    }
    return (std::clamp)(fontSize, 13, 32);
}

inline COLORREF normalizeTextColor(COLORREF textColor) {
    if (textColor == kLegacyGoldTextColor
        || textColor == kLegacyNeonGreenTextColor
        || textColor == kLegacyComfortGreenTextColor) {
        return kPreferredTextColor;
    }
    return textColor;
}

inline DisplaySettings normalizeDisplaySettings(DisplaySettings settings) {
    settings.fontName = normalizeFontName(settings.fontName);
    settings.fontSize = normalizeFontSize(settings.fontSize);
    settings.textColor = normalizeTextColor(settings.textColor);
    return settings;
}

inline int fontWeight() {
    return FW_SEMIBOLD;
}

inline UINT fontQuality() {
    return ANTIALIASED_QUALITY;
}

inline bool usesPerPixelAlpha() {
    return true;
}

inline bool usesColorKeyTransparency() {
    return false;
}

inline bool useOutline() {
    return false;
}

}  // namespace goldview::taskbar_text_style
