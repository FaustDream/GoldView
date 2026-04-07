#pragma once

#include <functional>
#include <map>
#include <string>
#include <windows.h>

namespace goldview {

std::map<std::wstring, std::wstring> parseWebUiMessage(const std::wstring& message);
bool parseWebUiBool(const std::wstring& value);
std::wstring escapeJsonString(const std::wstring& value);

class WebUiWindow {
public:
    using MessageHandler = std::function<void(const std::wstring&)>;
    using ReadyHandler = std::function<void()>;

    WebUiWindow(std::wstring className, std::wstring title, std::wstring routePath, int width, int height);
    ~WebUiWindow();

    bool create(HINSTANCE instanceHandle, MessageHandler messageHandler, ReadyHandler readyHandler = {});
    void focusOrShow();
    void destroy();
    bool isCreated() const;
    bool isReady() const;
    HWND hwnd() const;
    void postJson(const std::wstring& json);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool initializeCom();
    void releaseWebView();
    void initializeWebView();
    void resizeWebView();
    void onWebMessage(const std::wstring& payload);

    std::wstring className_;
    std::wstring title_;
    std::wstring routePath_;
    int width_;
    int height_;
    HINSTANCE instanceHandle_{nullptr};
    HWND windowHandle_{nullptr};
    bool comInitialized_{false};
    bool ready_{false};
    MessageHandler messageHandler_;
    ReadyHandler readyHandler_;

    struct Impl;
    Impl* impl_{nullptr};
};

}  // namespace goldview
