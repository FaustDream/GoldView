#include "taskbar_text_style.h"

#include <cassert>

int wmain() {
    using namespace goldview;
    using namespace goldview::taskbar_text_style;

    DisplaySettings settings{};

    assert(useOutline() == false);
    assert(usesPerPixelAlpha() == true);
    assert(usesColorKeyTransparency() == false);
    assert(fontWeight() == FW_SEMIBOLD);
    assert(fontQuality() == ANTIALIASED_QUALITY);
    assert(normalizeFontName(settings.fontName) == L"Segoe UI");
    assert(normalizeFontSize(settings.fontSize) == 18);
    assert(normalizeTextColor(settings.textColor) == RGB(0, 0, 0));

    settings.fontName = L"Microsoft YaHei UI";
    settings.fontSize = 16;
    settings.textColor = RGB(20, 20, 20);
    settings = normalizeDisplaySettings(settings);

    assert(settings.fontName == L"Microsoft YaHei UI");
    assert(settings.fontSize == 16);
    assert(settings.textColor == RGB(20, 20, 20));
    return 0;
}
