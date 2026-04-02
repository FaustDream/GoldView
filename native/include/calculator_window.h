#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "average_service.h"
#include "settings_store.h"

namespace goldview {

class CalculatorWindow {
public:
    CalculatorWindow(SettingsStore& settingsStore, AverageService& averageService);

    bool create(HINSTANCE instanceHandle);
    void show();
    void focusOrShow();
    void destroy();
    bool isCreated() const;
    HWND hwnd() const;

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void createControls();
    void applyFonts();
    void layoutControls(int width, int height);
    void paint();
    void centerOnScreen() const;
    void releaseHandles();
    void releaseFonts();
    void calculate();
    void clearInputs();
    void loadHistory();
    void renderHistory(const std::vector<std::wstring>& history);
    double parseEditDouble(HWND edit, bool& ok) const;
    void setResultText(const std::wstring& priceText, const std::wstring& totalText);

    SettingsStore& settingsStore_;
    AverageService& averageService_;
    HINSTANCE instanceHandle_ = nullptr;
    HWND windowHandle_ = nullptr;
    HWND oldPriceLabel_ = nullptr;
    HWND oldPriceEdit_ = nullptr;
    HWND oldWeightLabel_ = nullptr;
    HWND oldWeightEdit_ = nullptr;
    HWND newPriceLabel_ = nullptr;
    HWND newPriceEdit_ = nullptr;
    HWND newWeightLabel_ = nullptr;
    HWND newWeightEdit_ = nullptr;
    HWND calculateButton_ = nullptr;
    HWND clearButton_ = nullptr;
    HWND resultPriceLabel_ = nullptr;
    HWND resultTotalLabel_ = nullptr;
    HWND historyTitleLabel_ = nullptr;
    HWND historyList_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT monoFont_ = nullptr;
    std::vector<std::wstring> history_;
};

}  // namespace goldview
