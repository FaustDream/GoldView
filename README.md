# GoldView

GoldView 是一个基于 `Win32/C++` 的 Windows 桌面工具，用于任务栏黄金价格显示、价格源切换、设置管理和均价计算。

## 项目信息

- 作者：凌致
- 版本：`5.1.0`

## 当前目录结构

- `src/`
  - C++ 源码实现，按 `app`、`pricing`、`settings`、`taskbar`、`ui`、`utils` 分模块组织
- `include/`
  - 头文件声明
- `webui/`
  - 设置页、计算器页和通用前端静态资源
- `assets/`
  - 图标等应用资源
- `packages/`
  - 提交到仓库中的 WebView2 SDK 依赖目录，支持放置最小版 `Microsoft.Web.WebView2.*`
- `build_native.bat`
  - 本地构建与发布脚本
- `CMakeLists.txt`
  - CMake 构建入口

## 主要功能

- 原生托盘入口
- 任务栏黄金价格显示
- 多数据源价格获取与回退
- 本地设置持久化
- 均价计算器

## 当前说明

- 仓库已经去掉 `native/` 目录层级，主工程直接位于仓库根目录
- `src/` 负责实现代码，`include/` 负责头文件声明，两者不是重复目录
- `packages/` 保留最小版 WebView2 SDK，便于仓库克隆后直接构建
- `build/` 和 `dist/` 属于构建产物，已在 Git 中忽略

## 构建方式

```bat
build_native.bat
```

构建完成后，主要输出目录为：

```text
dist\GoldViewNative
```

可执行文件位置：

```text
dist\GoldViewNative\GoldViewNative.exe
```

## 开发备注

- 如果系统已安装 WebView2 SDK，也可以通过 `WEBVIEW2_SDK_DIR` 使用外部 SDK
- 当前仓库会按以下顺序查找 SDK：`WEBVIEW2_SDK_DIR`、仓库内 `packages/`、用户 NuGet 缓存 `%USERPROFILE%\.nuget\packages\microsoft.web.webview2`、系统 SDK 目录
- 如果你下载的是最小版 WebView2 SDK，请把解压后的根目录放到 `packages/Microsoft.Web.WebView2.<version>/`，或者直接把 `WEBVIEW2_SDK_DIR` 指向该目录
