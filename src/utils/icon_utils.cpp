#include "icon_utils.h"

#include <gdiplus.h>
#include <objidl.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

namespace goldview {

namespace {

class GdiplusSession {
public:
    static GdiplusSession& instance() {
        static GdiplusSession session;
        return session;
    }

    bool isReady() const {
        return token_ != 0;
    }

private:
    GdiplusSession() {
        Gdiplus::GdiplusStartupInput startupInput;
        if (Gdiplus::GdiplusStartup(&token_, &startupInput, nullptr) != Gdiplus::Ok) {
            token_ = 0;
        }
    }

    ~GdiplusSession() {
        if (token_ != 0) {
            Gdiplus::GdiplusShutdown(token_);
        }
    }

    ULONG_PTR token_ = 0;
};

std::filesystem::path moduleDirectory(HINSTANCE instanceHandle) {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(instanceHandle, modulePath, MAX_PATH);
    return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path resolveIconPath(HINSTANCE instanceHandle) {
    const auto baseDirectory = moduleDirectory(instanceHandle);
    const std::filesystem::path candidates[] = {
        baseDirectory / L"assets" / L"gold.png",
        baseDirectory.parent_path() / L"assets" / L"gold.png",
        baseDirectory.parent_path().parent_path() / L"assets" / L"gold.png",
    };

    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

}  // namespace

HICON loadAppIcon(HINSTANCE instanceHandle, int iconSize) {
    static std::once_flag startupFlag;
    static bool startupReady = false;
    std::call_once(startupFlag, []() {
        startupReady = GdiplusSession::instance().isReady();
    });
    if (!startupReady) {
        return nullptr;
    }

    const auto iconPath = resolveIconPath(instanceHandle);
    if (iconPath.empty()) {
        return nullptr;
    }

    std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromFile(iconPath.c_str(), FALSE));
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }

    const auto width = static_cast<INT>(bitmap->GetWidth());
    const auto height = static_cast<INT>(bitmap->GetHeight());
    const auto targetSize = iconSize > 0 ? iconSize : (width > 0 ? width : height);
    if (targetSize <= 0) {
        return nullptr;
    }

    std::unique_ptr<Gdiplus::Bitmap> scaledBitmap;
    Gdiplus::Bitmap* iconBitmap = bitmap.get();
    if (width != targetSize || height != targetSize) {
        scaledBitmap = std::make_unique<Gdiplus::Bitmap>(targetSize, targetSize, PixelFormat32bppARGB);
        Gdiplus::Graphics graphics(scaledBitmap.get());
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.DrawImage(bitmap.get(), Gdiplus::Rect(0, 0, targetSize, targetSize));
        if (scaledBitmap->GetLastStatus() != Gdiplus::Ok) {
            return nullptr;
        }
        iconBitmap = scaledBitmap.get();
    }

    HICON iconHandle = nullptr;
    if (iconBitmap->GetHICON(&iconHandle) != Gdiplus::Ok) {
        return nullptr;
    }
    return iconHandle;
}

}
