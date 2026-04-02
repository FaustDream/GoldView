cmake_minimum_required(VERSION 3.20)
project(GoldViewNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(GoldViewNative WIN32
    src/main.cpp
    src/app.cpp
    src/settings_store.cpp
    src/price_service.cpp
    src/source_manager.cpp
    src/quote_source.cpp
    src/average_service.cpp
    src/calculator_window.cpp
    src/settings_window.cpp
    src/taskbar_logger.cpp
    src/taskbar_topology_detector.cpp
    src/taskbar_tray_detector.cpp
    src/taskbar_metrics.cpp
    src/taskbar_anchor_resolver.cpp
    src/taskbar_slot_controller.cpp
    src/taskbar_host.cpp
)

target_include_directories(GoldViewNative PRIVATE include)
target_link_libraries(GoldViewNative PRIVATE winhttp shell32 user32 gdi32 advapi32 comctl32 comdlg32)
