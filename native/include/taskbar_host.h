#pragma once

#include <windows.h>

#include <string>

#include "host_window.h"

namespace goldview {

class TaskbarHost final : public HostWindow {
public:
    bool create(HINSTANCE instanceHandle) override;
    void show() override;
    void hide() override;
    void updateContent(const PriceSnapshot& snapshot, const DisplaySettings& settings) override;
    HWND hwnd() const override;
    void applyBounds(const RECT& rect) override;
    bool attachToTaskbarContainer(HWND parent);
    void detachFromTaskbarContainer();
    bool isAttachedToTaskbarContainer(HWND expectedParent = nullptr) const;
    HWND parentHwnd() const;
    void applyBoundsInParent(const RECT& rect);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void paint();

    HWND windowHandle_ = nullptr;
    HWND parentHandle_ = nullptr;
    bool embeddedInTaskbar_ = false;
    RECT bounds_{};
    std::wstring priceText_ = L"--.--";
};

}  // namespace goldview
