cmake_minimum_required(VERSION 3.21)
project(GoldViewNative LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SOFTWARE ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG release/1.16
    SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)

add_executable(
    GoldViewNative
    WIN32
    src/app/main.cpp
    src/app/app.cpp
    src/app/app_tray.cpp
    src/app/app_layout.cpp
    src/app/app_window.cpp
    src/utils/icon_utils.cpp
    src/pricing/price_service.cpp
    src/pricing/source_manager.cpp
    src/pricing/source_manager_tick.cpp
    src/pricing/source_manager_health.cpp
    src/pricing/source_manager_selection.cpp
    src/pricing/quote_source.cpp
    src/taskbar/taskbar_topology_detector.cpp
    src/taskbar/taskbar_tray_detector.cpp
    src/taskbar/taskbar_metrics.cpp
    src/taskbar/taskbar_anchor_resolver.cpp
    src/taskbar/taskbar_slot_controller.cpp
    src/taskbar/taskbar_host.cpp
)

target_include_directories(GoldViewNative PRIVATE include)

target_compile_definitions(
    GoldViewNative
    PRIVATE
    NOMINMAX
)

target_compile_options(
    GoldViewNative
    PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_link_libraries(
    GoldViewNative
    PRIVATE
    Slint::Slint
    winhttp
    shell32
    user32
    userenv
    gdi32
    gdiplus
    advapi32
    comctl32
    comdlg32
    shlwapi
    ole32
)
