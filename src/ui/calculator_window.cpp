#include "calculator_window.h"

#include <stdexcept>

namespace goldview {

namespace {

constexpr int kWindowWidth = 760;
constexpr int kWindowHeight = 760;

}  // namespace

CalculatorWindow::CalculatorWindow(AverageService& averageService)
    : averageService_(averageService),
      webUi_(L"GoldViewCalculatorWindow", L"均价计算", L"calculator/index.html", kWindowWidth, kWindowHeight) {}

bool CalculatorWindow::create(HINSTANCE instanceHandle) {
    return webUi_.create(instanceHandle,
        [this](const std::wstring& payload) { handleWebMessage(payload); },
        [this]() { pushStateToView(); });
}

void CalculatorWindow::show() {
    focusOrShow();
}

void CalculatorWindow::focusOrShow() {
    webUi_.focusOrShow();
}

void CalculatorWindow::destroy() {
    webUi_.destroy();
}

bool CalculatorWindow::isCreated() const {
    return webUi_.isCreated();
}

HWND CalculatorWindow::hwnd() const {
    return webUi_.hwnd();
}

void CalculatorWindow::handleWebMessage(const std::wstring& payload) {
    const auto values = parseWebUiMessage(payload);
    const auto actionIt = values.find(L"action");
    if (actionIt == values.end()) {
        return;
    }

    if (actionIt->second == L"calculateAverage") {
        bool okOldPrice = false;
        bool okOldWeight = false;
        bool okNewPrice = false;
        bool okNewWeight = false;
        const double oldPrice = parseNumber(values.count(L"oldPrice") ? values.at(L"oldPrice") : L"", okOldPrice);
        const double oldWeight = parseNumber(values.count(L"oldWeight") ? values.at(L"oldWeight") : L"", okOldWeight);
        const double newPrice = parseNumber(values.count(L"newPrice") ? values.at(L"newPrice") : L"", okNewPrice);
        const double newWeight = parseNumber(values.count(L"newWeight") ? values.at(L"newWeight") : L"", okNewWeight);

        if (!okOldPrice || !okOldWeight || !okNewPrice || !okNewWeight) {
            errorText_ = L"请输入有效数字";
            pushStateToView();
            return;
        }

        try {
            const auto calculation = averageService_.calculate(oldPrice, oldWeight, newPrice, newWeight);
            wchar_t priceBuffer[64]{};
            wchar_t totalBuffer[64]{};
            swprintf_s(priceBuffer, L"%.2f", calculation.averagePrice);
            swprintf_s(totalBuffer, L"%.2f", calculation.totalWeight);
            resultPriceText_ = priceBuffer;
            resultTotalText_ = totalBuffer;
            errorText_.clear();
            history_.insert(history_.begin(), averageService_.formatHistoryRecord(oldPrice, oldWeight, newPrice, newWeight, calculation));
            if (history_.size() > 20) {
                history_.resize(20);
            }
        } catch (const std::exception&) {
            errorText_ = L"总克重必须大于 0";
        }
        pushStateToView();
    }
}

void CalculatorWindow::pushStateToView() {
    if (!webUi_.isReady()) {
        return;
    }
    std::wstring json = L"{\"type\":\"calculatorState\",\"data\":{";
    json += L"\"resultPrice\":\"" + escapeJsonString(resultPriceText_) + L"\"";
    json += L",\"resultTotal\":\"" + escapeJsonString(resultTotalText_) + L"\"";
    json += L",\"error\":\"" + escapeJsonString(errorText_) + L"\"";
    json += L",\"history\":[";
    for (size_t index = 0; index < history_.size(); ++index) {
        if (index > 0) {
            json += L",";
        }
        json += L"\"" + escapeJsonString(history_[index]) + L"\"";
    }
    json += L"]}}";
    webUi_.postJson(json);
}

double CalculatorWindow::parseNumber(const std::wstring& value, bool& ok) const {
    if (value.empty()) {
        ok = false;
        return 0.0;
    }
    try {
        ok = true;
        return std::stod(value);
    } catch (...) {
        ok = false;
        return 0.0;
    }
}

}  // namespace goldview
