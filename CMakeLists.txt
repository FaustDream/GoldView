cmake_minimum_required(VERSION 3.20)
project(GoldViewNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WEBVIEW2_SDK_DIR "$ENV{WEBVIEW2_SDK_DIR}" CACHE PATH "Path to WebView2 SDK root")
if(NOT WEBVIEW2_SDK_DIR)
    file(GLOB _repo_sdk_candidates LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2.*")
    list(SORT _repo_sdk_candidates COMPARE NATURAL ORDER DESCENDING)
    foreach(_candidate IN LISTS _repo_sdk_candidates)
        if(EXISTS "${_candidate}/build/native/include/WebView2.h" AND EXISTS "${_candidate}/build/native/x64/WebView2LoaderStatic.lib")
            set(WEBVIEW2_SDK_DIR "${_candidate}")
            break()
        endif()
    endforeach()
endif()

if(NOT WEBVIEW2_SDK_DIR)
    file(GLOB _nuget_sdk_candidates LIST_DIRECTORIES true "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/*")
    list(SORT _nuget_sdk_candidates COMPARE NATURAL ORDER DESCENDING)
    foreach(_candidate IN LISTS _nuget_sdk_candidates)
        if(EXISTS "${_candidate}/build/native/include/WebView2.h" AND EXISTS "${_candidate}/build/native/x64/WebView2LoaderStatic.lib")
            set(WEBVIEW2_SDK_DIR "${_candidate}")
            break()
        endif()
    endforeach()
endif()

if(NOT WEBVIEW2_SDK_DIR)
    set(_webview2_search_roots
        "C:/Program Files (x86)/Microsoft SDKs/WebView2"
        "C:/Program Files/Microsoft SDKs/WebView2"
    )
    foreach(_root IN LISTS _webview2_search_roots)
        if(EXISTS "${_root}")
            file(GLOB _sdk_candidates LIST_DIRECTORIES true "${_root}/*")
            list(SORT _sdk_candidates COMPARE NATURAL ORDER DESCENDING)
            foreach(_candidate IN LISTS _sdk_candidates)
                if(EXISTS "${_candidate}/build/native/include/WebView2.h" AND EXISTS "${_candidate}/build/native/x64/WebView2LoaderStatic.lib")
                    set(WEBVIEW2_SDK_DIR "${_candidate}")
                    break()
                endif()
            endforeach()
        endif()
        if(WEBVIEW2_SDK_DIR)
            break()
        endif()
    endforeach()
endif()

if(NOT WEBVIEW2_SDK_DIR OR NOT EXISTS "${WEBVIEW2_SDK_DIR}/build/native/include/WebView2.h")
    message(FATAL_ERROR "WebView2 SDK not found. Looked in WEBVIEW2_SDK_DIR, ${CMAKE_SOURCE_DIR}/packages, the user NuGet cache, and the system SDK folders. Set WEBVIEW2_SDK_DIR to the extracted SDK root, e.g. C:/Program Files (x86)/Microsoft SDKs/WebView2/1.0.xxxxx.x")
endif()

set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_SDK_DIR}/build/native/include")
set(WEBVIEW2_LIB_DIR "${WEBVIEW2_SDK_DIR}/build/native/x64")
if(NOT EXISTS "${WEBVIEW2_LIB_DIR}/WebView2LoaderStatic.lib")
    message(FATAL_ERROR "WebView2 loader library not found: ${WEBVIEW2_LIB_DIR}/WebView2LoaderStatic.lib")
endif()

add_executable(GoldViewNative WIN32
    src/app/main.cpp
    src/app/app.cpp
    src/app/app_tray.cpp
    src/app/app_layout.cpp
    src/app/app_window.cpp
    src/utils/icon_utils.cpp
    src/ui/webui_window.cpp
    src/settings/settings_store.cpp
    src/pricing/price_service.cpp
    src/pricing/source_manager.cpp
    src/pricing/source_manager_tick.cpp
    src/pricing/source_manager_health.cpp
    src/pricing/source_manager_selection.cpp
    src/pricing/quote_source.cpp
    src/pricing/average_service.cpp
    src/ui/calculator_window.cpp
    src/settings/settings_window.cpp
    src/taskbar/taskbar_logger.cpp
    src/taskbar/taskbar_topology_detector.cpp
    src/taskbar/taskbar_tray_detector.cpp
    src/taskbar/taskbar_metrics.cpp
    src/taskbar/taskbar_anchor_resolver.cpp
    src/taskbar/taskbar_slot_controller.cpp
    src/taskbar/taskbar_host.cpp
)

target_include_directories(GoldViewNative PRIVATE include "${WEBVIEW2_INCLUDE_DIR}")
target_link_directories(GoldViewNative PRIVATE "${WEBVIEW2_LIB_DIR}")
target_link_libraries(GoldViewNative PRIVATE winhttp shell32 user32 userenv gdi32 gdiplus advapi32 comctl32 comdlg32 shlwapi ole32 WebView2LoaderStatic)
