#pragma once

#include <string>
#include <vector>
#include <windows.h>

#include "average_service.h"
#include "webui_window.h"

namespace goldview {

class CalculatorWindow {
public:
    explicit CalculatorWindow(AverageService& averageService);

    bool create(HINSTANCE instanceHandle);
    void show();
    void focusOrShow();
    void destroy();
    bool isCreated() const;
    HWND hwnd() const;

private:
    void handleWebMessage(const std::wstring& payload);
    void pushStateToView();
    double parseNumber(const std::wstring& value, bool& ok) const;

    AverageService& averageService_;
    WebUiWindow webUi_;
    std::vector<std::wstring> history_;
    std::wstring resultPriceText_{L"--"};
    std::wstring resultTotalText_{L"--"};
    std::wstring errorText_;
};

}  // namespace goldview
