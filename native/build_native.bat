@echo off
setlocal EnableExtensions EnableDelayedExpansion
set SCRIPT_DIR=%~dp0
set WEBVIEW2_SDK_DIR=%WEBVIEW2_SDK_DIR%

if not defined VCINSTALLDIR (
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  ) else if exist "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  )
)

if not defined WEBVIEW2_SDK_DIR (
  for %%R in ("%ProgramFiles(x86)%\Microsoft SDKs\WebView2" "%ProgramFiles%\Microsoft SDKs\WebView2") do (
    if not defined WEBVIEW2_SDK_DIR if exist "%%~R" (
      for /f "delims=" %%D in ('dir /b /ad /o-n "%%~R" 2^>nul') do (
        if not defined WEBVIEW2_SDK_DIR if exist "%%~R\%%~D\build\native\include\WebView2.h" (
          set "WEBVIEW2_SDK_DIR=%%~R\%%~D"
        )
      )
    )
  )
)

if not defined WEBVIEW2_SDK_DIR (
  echo [build_native] WebView2 SDK not found. Please set WEBVIEW2_SDK_DIR to the SDK root.
  exit /b 1
)

set "WEBVIEW2_INCLUDE_DIR=%WEBVIEW2_SDK_DIR%\build\native\include"
set "WEBVIEW2_LIB_DIR=%WEBVIEW2_SDK_DIR%\build\native\x64"

if not exist "%WEBVIEW2_INCLUDE_DIR%\WebView2.h" (
  echo [build_native] Missing WebView2 header: %WEBVIEW2_INCLUDE_DIR%\WebView2.h
  exit /b 1
)

if not exist "%WEBVIEW2_LIB_DIR%\WebView2LoaderStatic.lib" (
  echo [build_native] Missing WebView2 loader library: %WEBVIEW2_LIB_DIR%\WebView2LoaderStatic.lib
  exit /b 1
)

if not exist "%SCRIPT_DIR%build" mkdir "%SCRIPT_DIR%build"
pushd "%SCRIPT_DIR%build"
cl /nologo /utf-8 /std:c++17 /EHsc /DUNICODE /D_UNICODE /I "%SCRIPT_DIR%include" /I "%WEBVIEW2_INCLUDE_DIR%" "%SCRIPT_DIR%src\main.cpp" "%SCRIPT_DIR%src\app.cpp" "%SCRIPT_DIR%src\icon_utils.cpp" "%SCRIPT_DIR%src\webui_window.cpp" "%SCRIPT_DIR%src\settings_store.cpp" "%SCRIPT_DIR%src\price_service.cpp" "%SCRIPT_DIR%src\source_manager.cpp" "%SCRIPT_DIR%src\quote_source.cpp" "%SCRIPT_DIR%src\average_service.cpp" "%SCRIPT_DIR%src\calculator_window.cpp" "%SCRIPT_DIR%src\settings_window.cpp" "%SCRIPT_DIR%src\taskbar_logger.cpp" "%SCRIPT_DIR%src\taskbar_topology_detector.cpp" "%SCRIPT_DIR%src\taskbar_tray_detector.cpp" "%SCRIPT_DIR%src\taskbar_metrics.cpp" "%SCRIPT_DIR%src\taskbar_anchor_resolver.cpp" "%SCRIPT_DIR%src\taskbar_slot_controller.cpp" "%SCRIPT_DIR%src\taskbar_host.cpp" /link /LIBPATH:"%WEBVIEW2_LIB_DIR%" winhttp.lib shell32.lib user32.lib userenv.lib gdi32.lib gdiplus.lib advapi32.lib shlwapi.lib comctl32.lib comdlg32.lib ole32.lib WebView2LoaderStatic.lib /SUBSYSTEM:WINDOWS /OUT:GoldViewNative.exe
popd

endlocal
