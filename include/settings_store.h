#pragma once

#include <windows.h>

#include <string>

#include "models.h"

namespace goldview {

class SettingsStore {
public:
    explicit SettingsStore(HINSTANCE instanceHandle);

    AppSettings load() const;
    void save(const AppSettings& settings) const;

private:
    std::wstring readString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue = L"") const;
    std::wstring iniPath_;
};

}  // namespace goldview
