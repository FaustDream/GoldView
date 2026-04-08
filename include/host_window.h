#pragma once

#include <windows.h>

#include "models.h"

namespace goldview {

class HostWindow {
public:
    virtual ~HostWindow() = default;

    virtual bool create(HINSTANCE instanceHandle) = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void updateContent(const PriceSnapshot& snapshot, const DisplaySettings& settings) = 0;
    virtual HWND hwnd() const = 0;
    virtual void applyBounds(const RECT& rect) = 0;
};

}  // namespace goldview
