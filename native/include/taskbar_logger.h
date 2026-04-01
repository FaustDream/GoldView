#pragma once

#include <string>

namespace goldview {

class TaskbarLogger {
public:
    void info(const std::wstring& message) const;
    void warn(const std::wstring& message) const;
    void error(const std::wstring& message) const;

private:
    void write(const wchar_t* level, const std::wstring& message) const;
};

}  // namespace goldview
