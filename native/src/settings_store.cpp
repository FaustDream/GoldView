#include "settings_store.h"

#include <shlwapi.h>

#include <filesystem>
#include <sstream>

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

std::wstring providerKey(PriceProviderKind provider) {
    switch (provider) {
    case PriceProviderKind::GoldApi:
        return L"gold-api";
    case PriceProviderKind::Sina:
        return L"sina";
    case PriceProviderKind::Xwteam:
        return L"xwteam";
    case PriceProviderKind::Auto:
    default:
        return L"auto";
    }
}

PriceProviderKind providerFromKey(const std::wstring& value) {
    if (value == L"gold-api") {
        return PriceProviderKind::GoldApi;
    }
    if (value == L"sina") {
        return PriceProviderKind::Sina;
    }
    if (value == L"xwteam") {
        return PriceProviderKind::Xwteam;
    }
    return PriceProviderKind::Auto;
}

std::wstring alignmentKey(TextAlignment alignment) {
    return alignment == TextAlignment::Left ? L"left" : L"center";
}

TextAlignment alignmentFromKey(const std::wstring& value) {
    return value == L"left" ? TextAlignment::Left : TextAlignment::Center;
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

DisplaySettings SettingsStore::load() const {
    DisplaySettings settings{};
    settings.autoSourceSelection = readString(L"update", L"autoSourceSelection", L"1") != L"0";
    settings.preferredProvider = providerFromKey(readString(L"update", L"preferredProvider", L"gold-api"));
    settings.fontName = readString(L"display", L"fontName", settings.fontName.c_str());
    settings.fontSize = (std::clamp)(readInt(readString(L"display", L"fontSize", L"20"), 20), 14, 36);
    settings.textColor = readColor(readString(L"display", L"textColor", L""), settings.textColor);
    settings.backgroundColor = readColor(readString(L"display", L"backgroundColor", L""), settings.backgroundColor);
    settings.backgroundTransparent = readString(L"display", L"backgroundTransparent", L"1") != L"0";
    settings.textAlignment = alignmentFromKey(readString(L"display", L"textAlignment", L"center"));
    settings.horizontalLayout = readString(L"display", L"horizontalLayout", L"1") != L"0";
    return settings;
}

void SettingsStore::save(const DisplaySettings& settings) const {
    const auto history = loadCalculatorHistory();
    DeleteFileW(iniPath_.c_str());

    wchar_t numberBuffer[32]{};
    WritePrivateProfileStringW(L"update", L"autoSourceSelection", boolValue(settings.autoSourceSelection), iniPath_.c_str());
    WritePrivateProfileStringW(L"update", L"preferredProvider", providerKey(settings.preferredProvider).c_str(), iniPath_.c_str());

    WritePrivateProfileStringW(L"display", L"fontName", settings.fontName.c_str(), iniPath_.c_str());
    swprintf_s(numberBuffer, L"%d", settings.fontSize);
    WritePrivateProfileStringW(L"display", L"fontSize", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%u", static_cast<unsigned>(settings.textColor));
    WritePrivateProfileStringW(L"display", L"textColor", numberBuffer, iniPath_.c_str());
    swprintf_s(numberBuffer, L"%u", static_cast<unsigned>(settings.backgroundColor));
    WritePrivateProfileStringW(L"display", L"backgroundColor", numberBuffer, iniPath_.c_str());
    WritePrivateProfileStringW(L"display", L"backgroundTransparent", boolValue(settings.backgroundTransparent), iniPath_.c_str());
    WritePrivateProfileStringW(L"display", L"textAlignment", alignmentKey(settings.textAlignment).c_str(), iniPath_.c_str());
    WritePrivateProfileStringW(L"display", L"horizontalLayout", boolValue(settings.horizontalLayout), iniPath_.c_str());

    saveCalculatorHistory(history);
}

std::vector<std::wstring> SettingsStore::loadCalculatorHistory() const {
    std::vector<std::wstring> history;
    const auto serialized = readString(L"calculator", L"history");
    if (serialized.empty()) {
        return history;
    }

    std::wstringstream stream(serialized);
    std::wstring item;
    while (std::getline(stream, item, L'|')) {
        if (!item.empty()) {
            history.push_back(item);
        }
    }
    return history;
}

void SettingsStore::saveCalculatorHistory(const std::vector<std::wstring>& history) const {
    std::wstring serialized;
    for (size_t index = 0; index < history.size(); ++index) {
        if (index > 0) {
            serialized += L"|";
        }
        serialized += history[index];
    }
    WritePrivateProfileStringW(L"calculator", L"history", serialized.c_str(), iniPath_.c_str());
}

}  // namespace goldview
