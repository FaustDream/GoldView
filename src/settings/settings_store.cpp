#include "settings_store.h"

#include <shlwapi.h>

#include <algorithm>
#include <filesystem>

#include "taskbar_text_style.h"

#pragma comment(lib, "Shlwapi.lib")

namespace goldview {

namespace {

std::wstring readDirectory(HINSTANCE instanceHandle) {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(instanceHandle, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);
    return modulePath;
}

int readInt(const std::wstring& text, int fallback) {
    if (text.empty()) {
        return fallback;
    }
    try {
        return std::stoi(text);
    } catch (...) {
        return fallback;
    }
}

COLORREF readColor(const std::wstring& text, COLORREF fallback) {
    if (text.empty()) {
        return fallback;
    }
    try {
        return static_cast<COLORREF>(std::stoul(text));
    } catch (...) {
        return fallback;
    }
}

const wchar_t* boolValue(bool value) {
    return value ? L"1" : L"0";
}

std::wstring alignmentKey(TextAlignment alignment) {
    return alignment == TextAlignment::Left ? L"left" : L"center";
}

TextAlignment alignmentFromKey(const std::wstring& value) {
    return value == L"left" ? TextAlignment::Left : TextAlignment::Center;
}

QuoteSourceKind sourceFromLegacyKey(const std::wstring& value) {
    if (value == L"sina") {
        return QuoteSourceKind::Sina;
    }
    if (value == L"xwteam") {
        return QuoteSourceKind::Xwteam;
    }
    return QuoteSourceKind::GoldApi;
}

std::wstring sourceSection(QuoteSourceKind kind) {
    return std::wstring(L"source.") + quoteSourceKey(kind);
}

QuoteSourceTransport inferTransport(QuoteSourceKind kind) {
    return kind == QuoteSourceKind::GoldApi ? QuoteSourceTransport::Api : QuoteSourceTransport::Xhr;
}

}  // namespace

SettingsStore::SettingsStore(HINSTANCE instanceHandle) {
    iniPath_ = readDirectory(instanceHandle) + L"\\goldview-native.ini";
}

std::wstring SettingsStore::readString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue) const {
    wchar_t buffer[1024]{};
    GetPrivateProfileStringW(section, key, defaultValue, buffer, static_cast<DWORD>(std::size(buffer)), iniPath_.c_str());
    return buffer;
}

AppSettings SettingsStore::load() const {
    AppSettings settings = defaultAppSettings();

    const std::wstring legacyAutoSwitch = readString(L"update", L"autoSourceSelection", L"");
    const std::wstring legacyPreferred = readString(L"update", L"preferredProvider", L"");

    settings.runtime.autoRefreshEnabled = readString(L"runtime", L"autoRefreshEnabled", L"1") != L"0";
    settings.runtime.launchAtStartup = readString(L"runtime", L"launchAtStartup", L"0") != L"0";
    settings.runtime.launchOnWorkdaysOnly = readString(L"runtime", L"launchOnWorkdaysOnly", L"1") != L"0";
    settings.runtime.autoSwitchSource = !legacyAutoSwitch.empty()
        ? legacyAutoSwitch != L"0"
        : readString(L"runtime", L"autoSwitchSource", L"1") != L"0";
    settings.runtime.preferredSource = !legacyPreferred.empty()
        ? sourceFromLegacyKey(legacyPreferred)
        : sourceFromLegacyKey(readString(L"runtime", L"preferredSource", quoteSourceKey(settings.runtime.preferredSource)));
    settings.runtime.successRateThreshold = (std::clamp)(readInt(readString(L"runtime", L"successRateThreshold", L"95"), 95), 50, 100);
    settings.runtime.latencyThresholdMs = (std::clamp)(readInt(readString(L"runtime", L"latencyThresholdMs", L"500"), 500), 100, 5000);
    settings.runtime.recentOutputLimit = (std::clamp)(readInt(readString(L"runtime", L"recentOutputLimit", L"50"), 50), 10, 200);
    settings.runtime.uiRefreshIntervalMs = (std::clamp)(readInt(readString(L"runtime", L"uiRefreshIntervalMs", L"200"), 200), 100, 1000);
    settings.runtime.activeRequestIntervalMs = (std::clamp)(readInt(readString(L"runtime", L"activeRequestIntervalMs", L"1000"), 1000), 500, 5000);
    settings.runtime.standbyRequestIntervalMs = (std::clamp)(readInt(readString(L"runtime", L"standbyRequestIntervalMs", L"15000"), 15000), 5000, 60000);
    settings.runtime.fallbackApiIntervalMs = (std::clamp)(readInt(readString(L"runtime", L"fallbackApiIntervalMs", L"5000"), 5000), 1000, 60000);
    settings.runtime.healthWindowSize = (std::clamp)(readInt(readString(L"runtime", L"healthWindowSize", L"20"), 20), 5, 100);
    settings.runtime.logLimit = (std::clamp)(readInt(readString(L"runtime", L"logLimit", L"300"), 300), 50, 1000);

    settings.display.fontName = readString(L"display", L"fontName", settings.display.fontName.c_str());
    settings.display.fontSize = (std::clamp)(readInt(readString(L"display", L"fontSize", L"20"), 20), 14, 36);
    settings.display.textColor = readColor(readString(L"display", L"textColor", L""), settings.display.textColor);
    settings.display.backgroundColor = readColor(readString(L"display", L"backgroundColor", L""), settings.display.backgroundColor);
    settings.display.backgroundTransparent = readString(L"display", L"backgroundTransparent", L"1") != L"0";
    settings.display.textAlignment = alignmentFromKey(readString(L"display", L"textAlignment", L"center"));
    settings.display.horizontalLayout = readString(L"display", L"horizontalLayout", L"1") != L"0";
    settings.display = taskbar_text_style::normalizeDisplaySettings(settings.display);

    for (auto& source : settings.sources) {
        const std::wstring section = sourceSection(source.kind);
        source.enabled = readString(section.c_str(), L"enabled", source.enabled ? L"1" : L"0") != L"0";
        source.priority = (std::clamp)(readInt(readString(section.c_str(), L"priority", std::to_wstring(source.priority).c_str()), source.priority), 1, 99);
        source.weight = (std::clamp)(readInt(readString(section.c_str(), L"weight", std::to_wstring(source.weight).c_str()), source.weight), 1, 100);
        source.apiKey = readString(section.c_str(), L"apiKey", L"");
        source.transport = inferTransport(source.kind);
    }

    return settings;
}

void SettingsStore::save(const AppSettings& settings) const {
    DeleteFileW(iniPath_.c_str());

    wchar_t numberBuffer[32]{};

    WritePrivateProfileStringW(L"runtime", L"autoRefreshEnabled", boolValue(settings.runtime.autoRefreshEnabled), iniPath_.c_str());
    WritePrivateProfileStringW(L"runtime", L"launchAtStartup", boolValue(settings.runtime.launchAtStartup), iniPath_.c_str());
    WritePrivateProfileStringW(L"runtime", L"launchOnWorkdaysOnly", boolValue(settings.runtime.launchOnWorkdaysOnly), iniPath_.c_str());
    WritePrivateProfileStringW(L"runtime", L"autoSwitchSource", boolValue(settings.runtime.autoSwitchSource), iniPath_.c_str());
    WritePrivateProfileStringW(L"runtime", L"preferredSource", quoteSourceKey(settings.runtime.preferredSource), iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.successRateThreshold);
    WritePrivateProfileStringW(L"runtime", L"successRateThreshold", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.latencyThresholdMs);
    WritePrivateProfileStringW(L"runtime", L"latencyThresholdMs", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.recentOutputLimit);
    WritePrivateProfileStringW(L"runtime", L"recentOutputLimit", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.uiRefreshIntervalMs);
    WritePrivateProfileStringW(L"runtime", L"uiRefreshIntervalMs", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.activeRequestIntervalMs);
    WritePrivateProfileStringW(L"runtime", L"activeRequestIntervalMs", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.standbyRequestIntervalMs);
    WritePrivateProfileStringW(L"runtime", L"standbyRequestIntervalMs", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.fallbackApiIntervalMs);
    WritePrivateProfileStringW(L"runtime", L"fallbackApiIntervalMs", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.healthWindowSize);
    WritePrivateProfileStringW(L"runtime", L"healthWindowSize", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.runtime.logLimit);
    WritePrivateProfileStringW(L"runtime", L"logLimit", numberBuffer, iniPath_.c_str());

    WritePrivateProfileStringW(L"display", L"fontName", settings.display.fontName.c_str(), iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.display.fontSize);
    WritePrivateProfileStringW(L"display", L"fontSize", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%u", static_cast<unsigned>(settings.display.textColor));
    WritePrivateProfileStringW(L"display", L"textColor", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%u", static_cast<unsigned>(settings.display.backgroundColor));
    WritePrivateProfileStringW(L"display", L"backgroundColor", numberBuffer, iniPath_.c_str());
    WritePrivateProfileStringW(L"display", L"backgroundTransparent", boolValue(settings.display.backgroundTransparent), iniPath_.c_str());
    WritePrivateProfileStringW(L"display", L"textAlignment", alignmentKey(settings.display.textAlignment).c_str(), iniPath_.c_str());
    WritePrivateProfileStringW(L"display", L"horizontalLayout", boolValue(settings.display.horizontalLayout), iniPath_.c_str());

    for (const auto& source : settings.sources) {
        const std::wstring section = sourceSection(source.kind);
        WritePrivateProfileStringW(section.c_str(), L"enabled", boolValue(source.enabled), iniPath_.c_str());
        swprintf_s(numberBuffer, L"%d", source.priority);
        WritePrivateProfileStringW(section.c_str(), L"priority", numberBuffer, iniPath_.c_str());
        swprintf_s(numberBuffer, L"%d", source.weight);
        WritePrivateProfileStringW(section.c_str(), L"weight", numberBuffer, iniPath_.c_str());
        WritePrivateProfileStringW(section.c_str(), L"transport", transportLabel(source.transport), iniPath_.c_str());
        WritePrivateProfileStringW(section.c_str(), L"apiKey", source.apiKey.c_str(), iniPath_.c_str());
    }
}

}  // namespace goldview
