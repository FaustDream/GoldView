@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
set "BUILD_DIR=%SCRIPT_DIR%\build"
set "DIST_DIR=%SCRIPT_DIR%\dist"
set "PUBLISH_DIR=%DIST_DIR%\GoldViewNative"
set "OUTPUT_EXE=%BUILD_DIR%\GoldViewNative.exe"
set "CMAKE_EXE="
set "VC_REDIST_BASE="
set "CRT_REDIST_DIR="
set "RUNTIME_DLL_SOURCE="
set "CACHE_DIR=%BUILD_DIR%\downloads\vc_redist_x64"
set "CACHED_REDIST_EXE=%CACHE_DIR%\vc_redist.x64.exe"
set "DOWNLOADED_REDIST_URL=%VC_REDIST_DOWNLOAD_URL%"
set "SEVEN_ZIP_EXE=C:\Program Files\7-Zip\7z.exe"

if not defined DOWNLOADED_REDIST_URL (
  set "DOWNLOADED_REDIST_URL=https://aka.ms/vc14/vc_redist.x64.exe"
)

if not defined VCINSTALLDIR (
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  ) else if exist "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  )
)

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
  set "CMAKE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  set "VC_REDIST_BASE=%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Redist\MSVC"
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
  set "CMAKE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  set "VC_REDIST_BASE=%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\VC\Redist\MSVC"
) else if exist "C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
  set "CMAKE_EXE=C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  set "VC_REDIST_BASE=C:\BuildTools\VC\Redist\MSVC"
)

if "%CMAKE_EXE%"=="" (
  echo [build_native] Unable to find cmake.exe
  exit /b 1
)

if exist "%VC_REDIST_BASE%" (
  for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "$base = $env:VC_REDIST_BASE; if (Test-Path $base) { Get-ChildItem -Path $base -Directory | Sort-Object Name -Descending | ForEach-Object { $crt = Get-ChildItem -Path (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue | Select-Object -First 1; if ($crt) { $crt.FullName; break } } }"`) do (
    if not defined CRT_REDIST_DIR set "CRT_REDIST_DIR=%%D"
  )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

"%CMAKE_EXE%" -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

"%CMAKE_EXE%" --build "%BUILD_DIR%"
set "BUILD_EXIT_CODE=%ERRORLEVEL%"

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

if not exist "%BUILD_DIR%\slint_cpp.dll" (
  echo [build_native] Missing runtime dependency collection in build directory.
  exit /b 1
)

copy /y "%BUILD_DIR%\*.dll" "%PUBLISH_DIR%\" >nul
if errorlevel 1 exit /b 1

call :resolve_runtime_dll_source
if errorlevel 1 exit /b 1

call :copy_runtime_dlls "%RUNTIME_DLL_SOURCE%" "%PUBLISH_DIR%"
if errorlevel 1 exit /b 1

xcopy "%SCRIPT_DIR%\assets" "%PUBLISH_DIR%\assets\" /E /I /Y /Q >nul
if errorlevel 1 exit /b 1

for %%F in ("%BUILD_DIR%\*.obj" "%BUILD_DIR%\*.ilk" "%BUILD_DIR%\*.pdb" "%BUILD_DIR%\*.idb" "%BUILD_DIR%\*.exp" "%BUILD_DIR%\*.lib") do (
  if exist %%~F del /q %%~F >nul 2>&1
)

echo [build_native] Publish completed: %PUBLISH_DIR%
endlocal
exit /b 0

:resolve_runtime_dll_source
set "RUNTIME_DLL_SOURCE="
if defined CRT_REDIST_DIR (
  call :directory_has_runtime_dlls "%CRT_REDIST_DIR%"
  if not errorlevel 1 set "RUNTIME_DLL_SOURCE=%CRT_REDIST_DIR%"
)

if not defined RUNTIME_DLL_SOURCE (
  call :directory_has_runtime_dlls "%SystemRoot%\System32"
  if not errorlevel 1 set "RUNTIME_DLL_SOURCE=%SystemRoot%\System32"
)

if not defined RUNTIME_DLL_SOURCE (
  call :download_redist
  if errorlevel 1 exit /b 1
  call :install_downloaded_redist
  if errorlevel 1 exit /b 1
  call :directory_has_runtime_dlls "%SystemRoot%\System32"
  if not errorlevel 1 set "RUNTIME_DLL_SOURCE=%SystemRoot%\System32"
)

if not defined RUNTIME_DLL_SOURCE (
  echo [build_native] Unable to locate VC runtime DLLs after checking local redist, System32, and downloaded installer.
  exit /b 1
)

exit /b 0

:directory_has_runtime_dlls
set "CHECK_DIR=%~1"
if "%CHECK_DIR%"=="" exit /b 1
for %%F in (
  concrt140.dll
  msvcp140.dll
  msvcp140_1.dll
  msvcp140_2.dll
  msvcp140_atomic_wait.dll
  msvcp140_codecvt_ids.dll
  vccorlib140.dll
  vcruntime140.dll
  vcruntime140_1.dll
  vcruntime140_threads.dll
) do (
  if not exist "%CHECK_DIR%\%%F" exit /b 1
)
exit /b 0

:copy_runtime_dlls
set "RUNTIME_SRC=%~1"
set "RUNTIME_DST=%~2"
if "%RUNTIME_SRC%"=="" exit /b 1
if "%RUNTIME_DST%"=="" exit /b 1

for %%F in (
  concrt140.dll
  msvcp140.dll
  msvcp140_1.dll
  msvcp140_2.dll
  msvcp140_atomic_wait.dll
  msvcp140_codecvt_ids.dll
  vccorlib140.dll
  vcruntime140.dll
  vcruntime140_1.dll
  vcruntime140_threads.dll
) do (
  copy /y "%RUNTIME_SRC%\%%F" "%RUNTIME_DST%\%%F" >nul
  if errorlevel 1 exit /b 1
)

exit /b 0

:download_redist
if exist "%CACHED_REDIST_EXE%" exit /b 0

if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"
if errorlevel 1 exit /b 1

echo [build_native] Downloading VC runtime from %DOWNLOADED_REDIST_URL%
curl.exe -L --fail --retry 3 --retry-delay 2 -o "%CACHED_REDIST_EXE%" "%DOWNLOADED_REDIST_URL%"
if errorlevel 1 (
  powershell -NoProfile -Command "Invoke-WebRequest -Uri $env:DOWNLOADED_REDIST_URL -OutFile $env:CACHED_REDIST_EXE"
  if errorlevel 1 exit /b 1
)

if not exist "%CACHED_REDIST_EXE%" exit /b 1
exit /b 0

:install_downloaded_redist
call :directory_has_runtime_dlls "%SystemRoot%\System32"
if not errorlevel 1 exit /b 0

if not exist "%CACHED_REDIST_EXE%" exit /b 1

echo [build_native] Installing downloaded VC runtime on build machine.
"%CACHED_REDIST_EXE%" /install /quiet /norestart
if errorlevel 1 exit /b 1

call :directory_has_runtime_dlls "%SystemRoot%\System32"
if errorlevel 1 exit /b 1
exit /b 0
