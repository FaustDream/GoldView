#include "taskbar_logger.h"

#include <ShlObj.h>
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace goldview {

namespace {

std::wstring getTimestamp() {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t buffer[64]{};
    swprintf_s(
        buffer,
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds);
    return buffer;
}

std::filesystem::path getLogPath() {
    wchar_t localAppData[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData))) {
        auto logDir = std::filesystem::path(localAppData) / "GoldView" / "logs";
        std::filesystem::create_directories(logDir);
        return logDir / "taskbar.log";
    }

    auto fallbackDir = std::filesystem::temp_directory_path() / "GoldView";
    std::filesystem::create_directories(fallbackDir);
    return fallbackDir / "taskbar.log";
}

}  // namespace

void TaskbarLogger::info(const std::wstring& message) const {
    write(L"INFO", message);
}

void TaskbarLogger::warn(const std::wstring& message) const {
    write(L"WARN", message);
}

void TaskbarLogger::error(const std::wstring& message) const {
    write(L"ERROR", message);
}

void TaskbarLogger::write(const wchar_t* level, const std::wstring& message) const {
    std::wstringstream stream;
    stream << L"[GoldView][" << level << L"][" << getTimestamp() << L"] " << message << L"\n";
    const std::wstring line = stream.str();
    OutputDebugStringW(line.c_str());

    const auto logPath = getLogPath();
    std::wofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << line;
    }
}

}  // namespace goldview
