@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
set "BUILD_DIR=%SCRIPT_DIR%\build"
set "DIST_DIR=%SCRIPT_DIR%\dist"
set "PUBLISH_DIR=%DIST_DIR%\GoldViewNative"
set "OUTPUT_EXE=%BUILD_DIR%\GoldViewNative.exe"
set "SLINT_DLL=%BUILD_DIR%\_deps\slint-build\slint_cpp.dll"
set "CMAKE_EXE="

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
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
  set "CMAKE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\17\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

if "%CMAKE_EXE%"=="" (
  echo [build_native] 未找到 cmake.exe
  exit /b 1
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

if not exist "%SLINT_DLL%" (
  echo [build_native] Missing runtime dependency: %SLINT_DLL%
  exit /b 1
)

copy /y "%SLINT_DLL%" "%PUBLISH_DIR%\slint_cpp.dll" >nul
if errorlevel 1 exit /b 1

xcopy "%SCRIPT_DIR%\assets" "%PUBLISH_DIR%\assets\" /E /I /Y /Q >nul
if errorlevel 1 exit /b 1

for %%F in ("%BUILD_DIR%\*.obj" "%BUILD_DIR%\*.ilk" "%BUILD_DIR%\*.pdb" "%BUILD_DIR%\*.idb" "%BUILD_DIR%\*.exp" "%BUILD_DIR%\*.lib") do (
  if exist %%~F del /q %%~F >nul 2>&1
)

echo [build_native] Publish completed: %PUBLISH_DIR%
endlocal
exit /b 0
