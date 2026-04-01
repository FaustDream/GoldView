#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "models.h"

namespace goldview {

class SettingsStore {
public:
    explicit SettingsStore(HINSTANCE instanceHandle);

    DisplaySettings load() const;
    void save(const DisplaySettings& settings) const;
    std::vector<std::wstring> loadCalculatorHistory() const;
    void saveCalculatorHistory(const std::vector<std::wstring>& history) const;

private:
    std::wstring readString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue = L"") const;
    std::wstring iniPath_;
};

}  // namespace goldview
