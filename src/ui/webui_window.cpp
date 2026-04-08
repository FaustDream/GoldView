#include "webui_window.h"

#include <objbase.h>
#include <shlwapi.h>
#include <wrl.h>

#include <filesystem>
#include <memory>
#include <string_view>

#include <WebView2.h>

#include "icon_utils.h"

#pragma comment(lib, "Shlwapi.lib")

namespace goldview {

namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

constexpr wchar_t kWebUiHostName[] = L"goldview.local";

std::wstring urlDecode(const std::wstring& value) {
    std::wstring result;
    result.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        const wchar_t ch = value[index];
        if (ch == L'+' ) {
            result.push_back(L' ');
            continue;
        }
        if (ch == L'%' && index + 2 < value.size()) {
            const auto hex = value.substr(index + 1, 2);
            wchar_t* end = nullptr;
            const long code = wcstol(hex.c_str(), &end, 16);
            if (end && *end == 0) {
                result.push_back(static_cast<wchar_t>(code));
                index += 2;
                continue;
            }
        }
        result.push_back(ch);
    }
    return result;
}

std::filesystem::path moduleDirectory(HINSTANCE instanceHandle) {
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(instanceHandle, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path locateWebUiRoot(HINSTANCE instanceHandle) {
    auto current = moduleDirectory(instanceHandle);
    for (int depth = 0; depth < 6; ++depth) {
        const auto candidate = current / L"webui";
        if (std::filesystem::exists(candidate / L"common" / L"base.css")) {
            return candidate;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return {};
}

std::wstring buildInitialUrl(const std::wstring& routePath) {
    return std::wstring(L"https://") + kWebUiHostName + L"/" + routePath;
}

}  // namespace

std::map<std::wstring, std::wstring> parseWebUiMessage(const std::wstring& message) {
    std::map<std::wstring, std::wstring> values;
    size_t start = 0;
    while (start <= message.size()) {
        const size_t end = message.find(L'&', start);
        const std::wstring token = message.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!token.empty()) {
            const size_t separator = token.find(L'=');
            const std::wstring key = urlDecode(token.substr(0, separator));
            const std::wstring value = separator == std::wstring::npos ? L"" : urlDecode(token.substr(separator + 1));
            values[key] = value;
        }
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
    }
    return values;
}

bool parseWebUiBool(const std::wstring& value) {
    return value == L"1" || value == L"true" || value == L"on" || value == L"yes";
}

std::wstring escapeJsonString(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 8);
    for (const wchar_t ch : value) {
        switch (ch) {
        case L'\\':
            escaped += L"\\\\";
            break;
        case L'"':
            escaped += L"\\\"";
            break;
        case L'\n':
            escaped += L"\\n";
            break;
        case L'\r':
            escaped += L"\\r";
            break;
        case L'\t':
            escaped += L"\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

struct WebUiWindow::Impl {
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webView;
    std::filesystem::path webRoot;
};

WebUiWindow::WebUiWindow(std::wstring className, std::wstring title, std::wstring routePath, int width, int height)
    : className_(std::move(className)),
      title_(std::move(title)),
      routePath_(std::move(routePath)),
      width_(width),
      height_(height),
      impl_(new Impl()) {}

WebUiWindow::~WebUiWindow() {
    destroy();
    delete impl_;
    impl_ = nullptr;
}

bool WebUiWindow::create(HINSTANCE instanceHandle, MessageHandler messageHandler, ReadyHandler readyHandler) {
    if (windowHandle_) {
        return true;
    }

    instanceHandle_ = instanceHandle;
    messageHandler_ = std::move(messageHandler);
    readyHandler_ = std::move(readyHandler);

    if (!initializeCom()) {
        MessageBoxW(nullptr, L"初始化 WebView2 所需 COM 环境失败。", title_.c_str(), MB_ICONERROR);
        return false;
    }

    impl_->webRoot = locateWebUiRoot(instanceHandle_);
    if (impl_->webRoot.empty()) {
        MessageBoxW(nullptr, L"未找到 webui 前端资源目录。", title_.c_str(), MB_ICONERROR);
        return false;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WebUiWindow::windowProc;
    windowClass.hInstance = instanceHandle_;
    windowClass.lpszClassName = className_.c_str();
    windowClass.hIcon = loadAppIcon(instanceHandle_, 32);
    windowClass.hIconSm = loadAppIcon(instanceHandle_, 16);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&windowClass);

    windowHandle_ = CreateWindowExW(0, className_.c_str(), title_.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, width_, height_, nullptr, nullptr, instanceHandle_, this);
    if (!windowHandle_) {
        return false;
    }

    LONG_PTR style = GetWindowLongPtrW(windowHandle_, GWL_STYLE);
    style |= WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    style &= ~static_cast<LONG_PTR>(WS_CHILD);
    SetWindowLongPtrW(windowHandle_, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtrW(windowHandle_, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_TOOLWINDOW);
    exStyle |= WS_EX_APPWINDOW;
    SetWindowLongPtrW(windowHandle_, GWL_EXSTYLE, exStyle);

    SetWindowPos(windowHandle_, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    initializeWebView();
    return true;
}

void WebUiWindow::focusOrShow() {
    if (!windowHandle_) {
        return;
    }
    ShowWindow(windowHandle_, SW_SHOWNORMAL);
    SetForegroundWindow(windowHandle_);
}

void WebUiWindow::destroy() {
    if (windowHandle_) {
        DestroyWindow(windowHandle_);
    }
    releaseWebView();
    if (comInitialized_) {
        CoUninitialize();
        comInitialized_ = false;
    }
}

bool WebUiWindow::isCreated() const {
    return windowHandle_ != nullptr;
}

bool WebUiWindow::isReady() const {
    return ready_ && impl_ && impl_->webView;
}

HWND WebUiWindow::hwnd() const {
    return windowHandle_;
}

void WebUiWindow::postJson(const std::wstring& json) {
    if (impl_ && impl_->webView) {
        impl_->webView->PostWebMessageAsJson(json.c_str());
    }
}

LRESULT CALLBACK WebUiWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto self = static_cast<WebUiWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    const auto self = reinterpret_cast<WebUiWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT WebUiWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        resizeWebView();
        return 0;
    case WM_CLOSE:
        DestroyWindow(windowHandle_);
        return 0;
    case WM_DESTROY:
        releaseWebView();
        windowHandle_ = nullptr;
        ready_ = false;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(windowHandle_, message, wParam, lParam);
}

bool WebUiWindow::initializeCom() {
    if (comInitialized_) {
        return true;
    }
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    comInitialized_ = hr != RPC_E_CHANGED_MODE;
    return true;
}

void WebUiWindow::releaseWebView() {
    if (!impl_) {
        return;
    }
    impl_->webView.Reset();
    impl_->controller.Reset();
    impl_->environment.Reset();
}

void WebUiWindow::initializeWebView() {
    if (!impl_ || !windowHandle_) {
        return;
    }

    CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || !environment || !windowHandle_) {
                    MessageBoxW(windowHandle_, L"创建 WebView2 环境失败。", title_.c_str(), MB_ICONERROR);
                    return result;
                }
                impl_->environment = environment;
                return environment->CreateCoreWebView2Controller(windowHandle_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || !controller || !windowHandle_) {
                                MessageBoxW(windowHandle_, L"创建 WebView2 控制器失败。", title_.c_str(), MB_ICONERROR);
                                return controllerResult;
                            }
                            impl_->controller = controller;
                            controller->get_CoreWebView2(&impl_->webView);
                            if (!impl_->webView) {
                                MessageBoxW(windowHandle_, L"获取 WebView2 实例失败。", title_.c_str(), MB_ICONERROR);
                                return E_FAIL;
                            }

                            impl_->webView->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR rawMessage = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&rawMessage)) && rawMessage) {
                                            const std::wstring payload(rawMessage);
                                            CoTaskMemFree(rawMessage);
                                            onWebMessage(payload);
                                        }
                                        return S_OK;
                                    }).Get(), nullptr);

                            ComPtr<ICoreWebView2_3> webView3;
                            if (FAILED(impl_->webView.As(&webView3)) || !webView3) {
                                MessageBoxW(windowHandle_, L"当前 WebView2 SDK 不支持虚拟主机目录映射。", title_.c_str(), MB_ICONERROR);
                                return E_NOINTERFACE;
                            }

                            webView3->SetVirtualHostNameToFolderMapping(
                                kWebUiHostName,
                                impl_->webRoot.c_str(),
                                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                            resizeWebView();
                            impl_->webView->Navigate(buildInitialUrl(routePath_).c_str());
                            return S_OK;
                        }).Get());
            }).Get());
}

void WebUiWindow::resizeWebView() {
    if (!impl_ || !impl_->controller || !windowHandle_) {
        return;
    }
    RECT bounds{};
    GetClientRect(windowHandle_, &bounds);
    impl_->controller->put_Bounds(bounds);
}

void WebUiWindow::onWebMessage(const std::wstring& payload) {
    const auto values = parseWebUiMessage(payload);
    const auto actionIt = values.find(L"action");
    if (actionIt != values.end() && actionIt->second == L"ready") {
        ready_ = true;
        if (readyHandler_) {
            readyHandler_();
        }
        return;
    }
    if (actionIt != values.end() && actionIt->second == L"closeWindow") {
        destroy();
        return;
    }
    if (messageHandler_) {
        messageHandler_(payload);
    }
}

}  // namespace goldview
