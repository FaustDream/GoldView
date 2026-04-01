#pragma once

#include <windows.h>

#include <string>

#include "host_window.h"

namespace goldview {

class FloatingHost final : public HostWindow {
public:
    bool create(HINSTANCE instanceHandle) override;
    void show() override;
    void hide() override;
    void updateContent(const PriceSnapshot& snapshot, const DisplaySettings& settings) override;
    HWND hwnd() const override;
    void applyBounds(const RECT& rect) override;

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void positionNearPrimaryTaskbar() const;
    void paint();

    HWND windowHandle_ = nullptr;
    std::wstring priceText_ = L"--.--";
};

}  // namespace goldview
