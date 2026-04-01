#include "settings_store.h"

#include <shlwapi.h>

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
    const auto modeValue = GetPrivateProfileIntW(L"display", L"mode", 0, iniPath_.c_str());
    settings.mode = modeValue == 1 ? DisplayMode::Floating : DisplayMode::Taskbar;
    settings.refreshIntervalMs = GetPrivateProfileIntW(L"display", L"refreshIntervalMs", 1000, iniPath_.c_str());
    if (settings.refreshIntervalMs < 500) {
        settings.refreshIntervalMs = 500;
    }
    settings.metalsDevApiKey = readString(L"api", L"metalsDevApiKey");
    settings.metalPriceApiKey = readString(L"api", L"metalPriceApiKey");
    return settings;
}

void SettingsStore::save(const DisplaySettings& settings) const {
    WritePrivateProfileStringW(L"display", L"mode", settings.mode == DisplayMode::Floating ? L"1" : L"0", iniPath_.c_str());

    wchar_t intervalBuffer[32]{};
    wsprintfW(intervalBuffer, L"%d", settings.refreshIntervalMs);
    WritePrivateProfileStringW(L"display", L"refreshIntervalMs", intervalBuffer, iniPath_.c_str());
    WritePrivateProfileStringW(L"api", L"metalsDevApiKey", settings.metalsDevApiKey.c_str(), iniPath_.c_str());
    WritePrivateProfileStringW(L"api", L"metalPriceApiKey", settings.metalPriceApiKey.c_str(), iniPath_.c_str());
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
