# GoldView

GoldView 是一个基于 `Win32/C++` 的 Windows 桌面工具，用于任务栏黄金价格显示、价格源切换，
并提供网页版均价计算器入口。任务栏显示链路保持原生样式。

## 项目信息

- 作者：凌致
- 版本：`v5.1.1`

## 当前目录结构

- `src/`
  - C++ 源码实现，按 `app`、`pricing`、`taskbar`、`utils` 分模块组织
- `include/`
  - 头文件声明
- `assets/`
  - 图标和网页计算器资源
- `build_native.bat`
  - 本地构建与发布脚本
- `CMakeLists.txt`
  - CMake 构建入口

## 文件树与删除说明

```text
GoldView/
├─ src/                  # 必须保留：C++ 源码实现
├─ include/              # 必须保留：头文件声明
├─ assets/               # 必须保留：图标和网页计算器资源
│  ├─ gold.png           # 必须保留：程序图标资源
│  └─ calculator/        # 必须保留：均价计算器网页
├─ build/                # 可删除：CMake 构建缓存和中间产物，重新构建会生成
├─ dist/                 # 可删除：发布目录，执行 build_native.bat 后会重新生成
├─ CMakeLists.txt        # 必须保留：CMake 构建配置
├─ build_native.bat      # 必须保留：本地构建与发布脚本
├─ README.md             # 必须保留：项目说明文档
├─ LICENSE               # 必须保留：许可证
├─ .gitignore            # 必须保留：Git 忽略规则
└─ build_log.txt         # 可删除但不建议：历史构建日志，删除会产生 Git 变更
```

可安全删除并重新生成：`build/`、`dist/`。不能删除：`src/`、`include/`、`assets/`、`CMakeLists.txt`、`build_native.bat`；删除后会导致构建失败或运行缺资源。

## 主要功能

- 原生托盘入口
- 任务栏黄金价格显示
- 多数据源价格获取与回退
- 网页版均价计算器（浏览器打开）

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

- 项目通过 `CMake` 构建，事件循环依赖 `Slint` 运行时
- 首次构建 Slint 依赖时，需要本机可用的 Rust 工具链
- 项目已不再依赖 `WebView2` 和内置网页容器
