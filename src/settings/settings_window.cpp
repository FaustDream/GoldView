#include "settings_window.h"

#include <algorithm>
#include <array>
#include <ctime>

namespace goldview {

namespace {

constexpr int kWindowWidth = 900;
constexpr int kWindowHeight = 760;

const std::array<QuoteSourceKind, 3> kSourceKinds{
    QuoteSourceKind::Sina,
    QuoteSourceKind::Xwteam,
    QuoteSourceKind::GoldApi,
};

std::wstring formatTimestamp(std::uint64_t value) {
    if (value == 0) {
        return L"--";
    }
    std::time_t timeValue = static_cast<std::time_t>(value);
    std::tm localTime{};
    localtime_s(&localTime, &timeValue);
    wchar_t buffer[64]{};
    wcsftime(buffer, static_cast<int>(std::size(buffer)), L"%H:%M:%S", &localTime);
    return buffer;
}

QuoteSourceConfig* findSourceConfig(AppSettings& settings, QuoteSourceKind kind) {
    for (auto& source : settings.sources) {
        if (source.kind == kind) {
            return &source;
        }
    }
    return nullptr;
}

const wchar_t* keyFromKind(QuoteSourceKind kind) {
    return quoteSourceKey(kind);
}

QuoteSourceKind kindFromKey(const std::wstring& key) {
    if (key == L"xwteam") {
        return QuoteSourceKind::Xwteam;
    }
    if (key == L"gold-api") {
        return QuoteSourceKind::GoldApi;
    }
    return QuoteSourceKind::Sina;
}

int parseBoundedInt(const std::map<std::wstring, std::wstring>& values, const std::wstring& key, int fallback, int minimum, int maximum) {
    const auto it = values.find(key);
    if (it == values.end()) {
        return fallback;
    }
    try {
        return (std::clamp)(std::stoi(it->second), minimum, maximum);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

SettingsWindow::SettingsWindow()
    : webUi_(L"GoldViewSourceControlWindow", L"数据源控制", L"source-control/index.html", kWindowWidth, kWindowHeight) {}

bool SettingsWindow::create(HINSTANCE instanceHandle) {
    return webUi_.create(instanceHandle,
        [this](const std::wstring& payload) { handleWebMessage(payload); },
        [this]() { pushStateToView(); });
}

void SettingsWindow::focusOrShow() {
    webUi_.focusOrShow();
}

void SettingsWindow::destroy() {
    webUi_.destroy();
}

bool SettingsWindow::isCreated() const {
    return webUi_.isCreated();
}

HWND SettingsWindow::hwnd() const {
    return webUi_.hwnd();
}

void SettingsWindow::setSettings(const AppSettings& settings) {
    settings_ = settings;
    pushStateToView();
}

void SettingsWindow::setStatus(const PriceServiceStatus& status) {
    status_ = status;
    pushStateToView();
}

void SettingsWindow::setSettingsSavedCallback(SettingsSavedCallback callback) {
    settingsSavedCallback_ = std::move(callback);
}

void SettingsWindow::handleWebMessage(const std::wstring& payload) {
    const auto values = parseWebUiMessage(payload);
    const auto actionIt = values.find(L"action");
    if (actionIt == values.end()) {
        return;
    }

    if (actionIt->second == L"saveSourceSettings") {
        settings_ = parseSettingsFromMessage(payload);
        if (settingsSavedCallback_) {
            settingsSavedCallback_(settings_);
        }
        pushStateToView();
    }
}

void SettingsWindow::pushStateToView() {
    if (!webUi_.isReady()) {
        return;
    }

    std::wstring json = L"{\"type\":\"sourceControlState\",\"data\":{";
    json += L"\"autoRefreshEnabled\":" + std::wstring(settings_.runtime.autoRefreshEnabled ? L"true" : L"false");
    json += L",\"autoSwitchSource\":" + std::wstring(settings_.runtime.autoSwitchSource ? L"true" : L"false");
    json += L",\"preferredSource\":\"" + std::wstring(quoteSourceKey(settings_.runtime.preferredSource)) + L"\"";
    json += L",\"successRateThreshold\":" + std::to_wstring(settings_.runtime.successRateThreshold);
    json += L",\"latencyThresholdMs\":" + std::to_wstring(settings_.runtime.latencyThresholdMs);
    json += L",\"lastSuccessfulAt\":\"" + escapeJsonString(formatTimestamp(status_.lastSuccessfulAt)) + L"\"";
    json += L",\"notice\":\"当前配置已生效\"";
    json += L",\"sources\":[";

    bool first = true;
    for (const auto kind : kSourceKinds) {
        const auto* source = findSourceConfig(settings_, kind);
        if (!source) {
            continue;
        }
        if (!first) {
            json += L",";
        }
        first = false;
        json += L"{\"key\":\"" + std::wstring(keyFromKind(kind)) + L"\"";
        json += L",\"label\":\"" + escapeJsonString(quoteSourceLabel(kind)) + L"\"";
        json += L",\"transport\":\"" + escapeJsonString(transportLabel(source->transport)) + L"\"";
        json += L",\"enabled\":" + std::wstring(source->enabled ? L"true" : L"false");
        json += L",\"priority\":" + std::to_wstring(source->priority);
        json += L",\"weight\":" + std::to_wstring(source->weight);
        json += L"}";
    }
    json += L"]}}";
    webUi_.postJson(json);
}

AppSettings SettingsWindow::parseSettingsFromMessage(const std::wstring& payload) const {
    AppSettings next = settings_;
    const auto values = parseWebUiMessage(payload);
    next.runtime.autoRefreshEnabled = parseWebUiBool(values.count(L"autoRefreshEnabled") ? values.at(L"autoRefreshEnabled") : L"1");
    next.runtime.autoSwitchSource = parseWebUiBool(values.count(L"autoSwitchSource") ? values.at(L"autoSwitchSource") : L"1");
    if (values.count(L"preferredSource")) {
        next.runtime.preferredSource = kindFromKey(values.at(L"preferredSource"));
    }
    next.runtime.successRateThreshold = parseBoundedInt(values, L"successRateThreshold", next.runtime.successRateThreshold, 50, 100);
    next.runtime.latencyThresholdMs = parseBoundedInt(values, L"latencyThresholdMs", next.runtime.latencyThresholdMs, 100, 5000);

    for (const auto kind : kSourceKinds) {
        auto* source = findSourceConfig(next, kind);
        if (!source) {
            continue;
        }
        const std::wstring key = keyFromKind(kind);
        const std::wstring enabledKey = L"enabled_" + key;
        const std::wstring priorityKey = L"priority_" + key;
        const std::wstring weightKey = L"weight_" + key;
        if (values.count(enabledKey)) {
            source->enabled = parseWebUiBool(values.at(enabledKey));
        }
        source->priority = parseBoundedInt(values, priorityKey, source->priority, 1, 99);
        source->weight = parseBoundedInt(values, weightKey, source->weight, 1, 100);
    }
    return next;
}

}  // namespace goldview
