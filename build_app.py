import PyInstaller.__main__
import os

# 1. 基础打包参数
params = [
    'gold_view.py',             # 主入口
    '--onefile',                # 打包成单个可执行文件 (免安装绿色版)
    '--noconsole',              # 不显示控制台窗口
    '--clean',                  # 打包前清理临时文件
    '--name=GoldView',          # 可执行文件名
    
    # 2. 极致排除无关模块 (PyInstaller 默认会把环境中所有模块都尝试分析一遍)
    '--exclude-module=numpy',
    '--exclude-module=pandas',
    '--exclude-module=matplotlib',
    '--exclude-module=PIL',
    '--exclude-module=openpyxl',
    '--exclude-module=pyexcel',
    '--exclude-module=xlsxwriter',
    '--exclude-module=xlrd',
    '--exclude-module=xlwt',
    '--exclude-module=xlutils',
    '--exclude-module=pytest',
    '--exclude-module=unittest',
    '--exclude-module=tkinter',
    '--exclude-module=PyQt6.QtWebEngineCore',
    '--exclude-module=PyQt6.QtWebEngineWidgets',
    '--exclude-module=PyQt6.QtSql',
    '--exclude-module=PyQt6.QtBluetooth',
    '--exclude-module=PyQt6.QtNfc',
    '--exclude-module=PyQt6.QtMultimedia',
    '--exclude-module=PyQt6.QtMultimediaWidgets',
    '--exclude-module=PyQt6.QtQuick',
    '--exclude-module=PyQt6.QtQml',
    '--exclude-module=PyQt6.QtTest',
]

# 3. 执行打包
if __name__ == "__main__":
    print("正在开始打包 GoldView...")
    PyInstaller.__main__.run(params)
    print("打包完成！请在 dist 目录下查找 GoldView.exe")
