@echo off
setlocal
set SCRIPT_DIR=%~dp0

if not defined VCINSTALLDIR (
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  ) else if exist "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  )
)

if not exist "%SCRIPT_DIR%build" mkdir "%SCRIPT_DIR%build"
pushd "%SCRIPT_DIR%build"
cl /nologo /utf-8 /std:c++17 /EHsc /DUNICODE /D_UNICODE /I "%SCRIPT_DIR%include" "%SCRIPT_DIR%src\main.cpp" "%SCRIPT_DIR%src\app.cpp" "%SCRIPT_DIR%src\settings_store.cpp" "%SCRIPT_DIR%src\price_service.cpp" "%SCRIPT_DIR%src\average_service.cpp" "%SCRIPT_DIR%src\calculator_window.cpp" "%SCRIPT_DIR%src\settings_window.cpp" "%SCRIPT_DIR%src\taskbar_logger.cpp" "%SCRIPT_DIR%src\taskbar_topology_detector.cpp" "%SCRIPT_DIR%src\taskbar_tray_detector.cpp" "%SCRIPT_DIR%src\taskbar_metrics.cpp" "%SCRIPT_DIR%src\taskbar_anchor_resolver.cpp" "%SCRIPT_DIR%src\taskbar_slot_controller.cpp" "%SCRIPT_DIR%src\taskbar_host.cpp" /link winhttp.lib shell32.lib user32.lib gdi32.lib advapi32.lib shlwapi.lib comctl32.lib comdlg32.lib /SUBSYSTEM:WINDOWS /OUT:GoldViewNative.exe
popd

endlocal
