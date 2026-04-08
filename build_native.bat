@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "SCRIPT_DIR=%~dp0"
set "WEBVIEW2_SDK_DIR=%WEBVIEW2_SDK_DIR%"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "DIST_DIR=%SCRIPT_DIR%dist"
set "PUBLISH_DIR=%DIST_DIR%\GoldViewNative"
set "OUTPUT_EXE=%BUILD_DIR%\GoldViewNative.exe"

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
  for /f "delims=" %%D in ('dir /b /ad /o-n "%SCRIPT_DIR%packages\Microsoft.Web.WebView2.*" 2^>nul') do (
    if not defined WEBVIEW2_SDK_DIR if exist "%SCRIPT_DIR%packages\%%~D\build\native\include\WebView2.h" if exist "%SCRIPT_DIR%packages\%%~D\build\native\x64\WebView2LoaderStatic.lib" (
      set "WEBVIEW2_SDK_DIR=%SCRIPT_DIR%packages\%%~D"
    )
  )
)

if not defined WEBVIEW2_SDK_DIR (
  for /f "delims=" %%D in ('dir /b /ad /o-n "%USERPROFILE%\.nuget\packages\microsoft.web.webview2" 2^>nul') do (
    if not defined WEBVIEW2_SDK_DIR if exist "%USERPROFILE%\.nuget\packages\microsoft.web.webview2\%%~D\build\native\include\WebView2.h" if exist "%USERPROFILE%\.nuget\packages\microsoft.web.webview2\%%~D\build\native\x64\WebView2LoaderStatic.lib" (
      set "WEBVIEW2_SDK_DIR=%USERPROFILE%\.nuget\packages\microsoft.web.webview2\%%~D"
    )
  )
)

if not defined WEBVIEW2_SDK_DIR (
  for %%R in ("%ProgramFiles(x86)%\Microsoft SDKs\WebView2" "%ProgramFiles%\Microsoft SDKs\WebView2") do (
    if not defined WEBVIEW2_SDK_DIR if exist "%%~R" (
      for /f "delims=" %%D in ('dir /b /ad /o-n "%%~R" 2^>nul') do (
        if not defined WEBVIEW2_SDK_DIR if exist "%%~R\%%~D\build\native\include\WebView2.h" if exist "%%~R\%%~D\build\native\x64\WebView2LoaderStatic.lib" (
          set "WEBVIEW2_SDK_DIR=%%~R\%%~D"
        )
      )
    )
  )
)

if not defined WEBVIEW2_SDK_DIR (
  echo [build_native] WebView2 SDK not found.
  echo [build_native] Looked in:
  echo [build_native]   1. WEBVIEW2_SDK_DIR
  echo [build_native]   2. %SCRIPT_DIR%packages\Microsoft.Web.WebView2.*
  echo [build_native]   3. %USERPROFILE%\.nuget\packages\microsoft.web.webview2\*
  echo [build_native]   4. System WebView2 SDK folders under Program Files
  echo [build_native] Set WEBVIEW2_SDK_DIR to the extracted SDK root if you downloaded the minimal package manually.
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

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"
cl /nologo /utf-8 /std:c++17 /EHsc /DUNICODE /D_UNICODE /I "%SCRIPT_DIR%include" /I "%WEBVIEW2_INCLUDE_DIR%" "%SCRIPT_DIR%src\app\main.cpp" "%SCRIPT_DIR%src\app\app.cpp" "%SCRIPT_DIR%src\app\app_tray.cpp" "%SCRIPT_DIR%src\app\app_layout.cpp" "%SCRIPT_DIR%src\app\app_window.cpp" "%SCRIPT_DIR%src\utils\icon_utils.cpp" "%SCRIPT_DIR%src\ui\webui_window.cpp" "%SCRIPT_DIR%src\settings\settings_store.cpp" "%SCRIPT_DIR%src\pricing\price_service.cpp" "%SCRIPT_DIR%src\pricing\source_manager.cpp" "%SCRIPT_DIR%src\pricing\source_manager_tick.cpp" "%SCRIPT_DIR%src\pricing\source_manager_health.cpp" "%SCRIPT_DIR%src\pricing\source_manager_selection.cpp" "%SCRIPT_DIR%src\pricing\quote_source.cpp" "%SCRIPT_DIR%src\pricing\average_service.cpp" "%SCRIPT_DIR%src\ui\calculator_window.cpp" "%SCRIPT_DIR%src\settings\settings_window.cpp" "%SCRIPT_DIR%src\taskbar\taskbar_logger.cpp" "%SCRIPT_DIR%src\taskbar\taskbar_topology_detector.cpp" "%SCRIPT_DIR%src\taskbar\taskbar_tray_detector.cpp" "%SCRIPT_DIR%src\taskbar\taskbar_metrics.cpp" "%SCRIPT_DIR%src\taskbar\taskbar_anchor_resolver.cpp" "%SCRIPT_DIR%src\taskbar\taskbar_slot_controller.cpp" "%SCRIPT_DIR%src\taskbar\taskbar_host.cpp" /link /LIBPATH:"%WEBVIEW2_LIB_DIR%" winhttp.lib shell32.lib user32.lib userenv.lib gdi32.lib gdiplus.lib advapi32.lib shlwapi.lib comctl32.lib comdlg32.lib ole32.lib WebView2LoaderStatic.lib /SUBSYSTEM:WINDOWS /OUT:GoldViewNative.exe
set "BUILD_EXIT_CODE=%ERRORLEVEL%"
popd

if not "%BUILD_EXIT_CODE%"=="0" exit /b %BUILD_EXIT_CODE%
if not exist "%OUTPUT_EXE%" (
  echo [build_native] Build succeeded but output file was not generated: %OUTPUT_EXE%
  exit /b 1
)

if exist "%PUBLISH_DIR%" rmdir /s /q "%PUBLISH_DIR%"
mkdir "%PUBLISH_DIR%"
if errorlevel 1 exit /b 1

copy /y "%OUTPUT_EXE%" "%PUBLISH_DIR%\GoldViewNative.exe" >nul
if errorlevel 1 exit /b 1

xcopy "%SCRIPT_DIR%assets" "%PUBLISH_DIR%\assets\" /E /I /Y /Q >nul
if errorlevel 1 exit /b 1

xcopy "%SCRIPT_DIR%webui" "%PUBLISH_DIR%\webui\" /E /I /Y /Q >nul
if errorlevel 1 exit /b 1

for %%F in ("%BUILD_DIR%\*.obj" "%BUILD_DIR%\*.ilk" "%BUILD_DIR%\*.pdb" "%BUILD_DIR%\*.idb" "%BUILD_DIR%\*.res" "%BUILD_DIR%\*.exp" "%BUILD_DIR%\*.lib") do (
  if exist %%~F del /q %%~F >nul 2>&1
)

echo [build_native] Publish completed: %PUBLISH_DIR%
endlocal
exit /b 0
