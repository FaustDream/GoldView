@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   GoldView 快捷打包程序 (绿色版)
echo ========================================
echo.

:: 检查 Python 环境
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 找不到 Python，请确保已安装并添加到环境变量。
    pause
    exit /b 1
)

:: 检查 PyInstaller
pip show pyinstaller >nul 2>&1
if %errorlevel% neq 0 (
    echo [状态] 正在安装打包依赖 pyinstaller...
    pip install pyinstaller -i https://pypi.tuna.tsinghua.edu.cn/simple
)

:: 清理旧的打包文件
if exist dist (
    echo [状态] 正在清理旧的打包文件...
    rd /s /q dist
)
if exist build (
    rd /s /q build
)

:: 开始打包
echo [状态] 正在启动打包程序，请稍候...
echo.
python build_app.py

if %errorlevel% eq 0 (
    echo.
    echo ========================================
    echo   [成功] 打包完成！
    echo   文件位置: %~dp0dist\GoldView.exe
    echo ========================================
) else (
    echo.
    echo [错误] 打包失败，请检查错误提示。
)

echo.
pause
