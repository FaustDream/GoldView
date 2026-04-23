#include "app.h"

#include <slint.h>

#include <commctrl.h>

#include <array>
#include <ctime>
#include <filesystem>
#include <shellapi.h>
#include <system_error>

#include "icon_utils.h"

namespace goldview {

namespace {

constexpr wchar_t kHiddenClassName[] = L"GoldViewNativeHiddenWindow";
constexpr wchar_t kStartupRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kStartupValueName[] = L"GoldView";
constexpr wchar_t kAutoStartArg[] = L"--autostart";

bool isWorkday() {
    std::tm localTime{};
    std::time_t now = std::time(nullptr);
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localTime = *std::localtime(&now);
#endif
    // 0 表示周日，1 到 5 表示周一到周五，6 表示周六
    return localTime.tm_wday >= 1 && localTime.tm_wday <= 5;
}

bool parseCommandLineForAutoStart(LPWSTR* argv, int argc) {
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], kAutoStartArg) == 0) {
            return true;
        }
    }
    return false;
}

std::filesystem::path executableDirectory(HINSTANCE instanceHandle) {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(instanceHandle, modulePath, MAX_PATH);
    return std::filesystem::path(modulePath).parent_path();
}

std::wstring canonicalPath(const std::filesystem::path& path) {
    std::error_code error;
    const auto normalized = std::filesystem::weakly_canonical(path, error);
    return (error ? path : normalized).wstring();
}

std::wstring resolveCalculatorPagePath(HINSTANCE instanceHandle) {
    const auto moduleDir = executableDirectory(instanceHandle);
    const std::array<std::filesystem::path, 3> candidates{
        moduleDir / L"assets" / L"calculator" / L"index.html",
        moduleDir / L".." / L"assets" / L"calculator" / L"index.html",
        std::filesystem::current_path() / L"assets" / L"calculator" / L"index.html",
    };

    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::exists(candidate, error) && !error) {
            return canonicalPath(candidate);
        }
    }

    return canonicalPath(candidates.front());
}

}  // namespace

App::App(HINSTANCE instanceHandle)
    : instanceHandle_(instanceHandle) {}

App::~App() {
    shutdown();
}

bool App::initialize(int argc, wchar_t* argv[]) {
    // 通过命令行参数识别是否为自启动拉起
    autoStartLaunch_ = parseCommandLineForAutoStart(argv, argc);
    settings_.runtime.launchAtStartup = isLaunchAtStartupEnabled();

    // 仅在系统自启动时检查工作日限制，手动启动不拦截
    if (autoStartLaunch_ && settings_.runtime.launchAtStartup && settings_.runtime.launchOnWorkdaysOnly && !isWorkday()) {
        return false;
    }

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&commonControls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = App::hiddenWindowProc;
    windowClass.hInstance = instanceHandle_;
    windowClass.lpszClassName = kHiddenClassName;
    windowClass.hIcon = loadAppIcon(instanceHandle_, 32);
    windowClass.hIconSm = loadAppIcon(instanceHandle_, 16);
    RegisterClassExW(&windowClass);

    hiddenWindow_ = CreateWindowExW(
        0,
        kHiddenClassName,
        L"GoldView Hidden Window",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instanceHandle_,
        this);
    if (!hiddenWindow_) {
        return false;
    }

    taskbarHost_ = std::make_unique<TaskbarHost>();
    if (!taskbarHost_->create(instanceHandle_)) {
        return false;
    }

    createTrayIcon();
    rebuildMode();
    priceService_.updateSettings(settings_);
    enableUiRefreshTimer();

    priceService_.start([this](const PriceSnapshot& snapshot) {
        if (hiddenWindow_) {
            constexpr UINT kSnapshotMessage = WM_APP + 2;
            auto payload = new PriceSnapshot(snapshot);
            PostMessageW(hiddenWindow_, kSnapshotMessage, 0, reinterpret_cast<LPARAM>(payload));
        }
    });
    return true;
}

int App::run() {
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    return 0;
}

void App::shutdown() {
    disableTaskbarRecoveryTimer();
    disableUiRefreshTimer();
    priceService_.stop();
    slotController_.restore();
    destroyTrayIcon();
    if (taskbarHost_) {
        taskbarHost_->detachFromTaskbarContainer();
        taskbarHost_->hide();
    }
    if (hiddenWindow_) {
        DestroyWindow(hiddenWindow_);
        hiddenWindow_ = nullptr;
    }
}

void App::rebuildMode() {
    taskbarHost_->hide();
    enableTaskbarRecoveryTimer();
    activeHost_ = static_cast<HostWindow*>(taskbarHost_.get());
    syncTaskbarDisplay(true);
}

void App::resetTaskbarModeState() {
    taskbarRefreshFailureCount_ = 0;
    lastTaskbarContainer_ = nullptr;
}

bool App::shouldSyncTaskbarFromSnapshot(const PriceSnapshot& snapshot) const {
    const double nextDisplayedPrice = normalizeDisplayedPrice(snapshot.currentPrice);
    const double currentDisplayedPrice = normalizeDisplayedPrice(lastSnapshot_.currentPrice);
    return nextDisplayedPrice != currentDisplayedPrice;
}

void App::syncTaskbarDisplay(bool forceLayoutRefresh) {
    if (!taskbarHost_) {
        return;
    }

    const bool needLayoutRefresh = forceLayoutRefresh ||
        taskbarRefreshFailureCount_ > 0 ||
        !taskbarHost_->isAttachedToTaskbarContainer(lastTaskbarContainer_);
    if (needLayoutRefresh) {
        const auto refresh = refreshTaskbarLayout();
        if (!refresh.shown) {
            return;
        }
    }

    if (activeHost_) {
        activeHost_->show();
        activeHost_->updateContent(lastSnapshot_, settings_.display);
    }
}

void App::applySnapshot(const PriceSnapshot& snapshot) {
    lastSnapshot_ = snapshot;
    syncTaskbarDisplay(false);
}

void App::openCalculatorWebPage() {
    const std::wstring pagePath = resolveCalculatorPagePath(instanceHandle_);
    const HINSTANCE launchResult = ShellExecuteW(hiddenWindow_, L"open", pagePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(launchResult) <= 32) {
        MessageBoxW(hiddenWindow_, L"打开均价计算网页失败，请检查 assets/calculator/index.html 是否存在。", L"GoldView", MB_OK | MB_ICONWARNING);
    }
}

bool App::setLaunchAtStartupEnabled(bool enabled) {
    HKEY runKey = nullptr;
    const LONG openResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kStartupRunKeyPath,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_QUERY_VALUE | KEY_SET_VALUE,
        nullptr,
        &runKey,
        nullptr);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }

    bool success = false;
    if (enabled) {
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(instanceHandle_, modulePath, MAX_PATH);
        std::wstring command = L"\"";
        command += modulePath;
        command += L"\" ";
        command += kAutoStartArg;
        const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
        success =
            RegSetValueExW(runKey, kStartupValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), bytes) ==
            ERROR_SUCCESS;
    } else {
        const LONG deleteResult = RegDeleteValueW(runKey, kStartupValueName);
        success = deleteResult == ERROR_SUCCESS || deleteResult == ERROR_FILE_NOT_FOUND;
    }

    RegCloseKey(runKey);

    if (!success) {
        return false;
    }

    settings_.runtime.launchAtStartup = enabled;
    return true;
}

bool App::isLaunchAtStartupEnabled() const {
    HKEY runKey = nullptr;
    const LONG openResult = RegOpenKeyExW(HKEY_CURRENT_USER, kStartupRunKeyPath, 0, KEY_QUERY_VALUE, &runKey);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    const LONG queryResult = RegQueryValueExW(runKey, kStartupValueName, nullptr, &type, nullptr, nullptr);
    RegCloseKey(runKey);
    return queryResult == ERROR_SUCCESS && type == REG_SZ;
}

}  // namespace goldview
