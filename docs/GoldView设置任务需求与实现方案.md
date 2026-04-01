# GoldView 设置任务需求与实现方案

## 1. 目标

`v3.5.0` 重新把设置功能加回主线，但不恢复旧的模式切换产品。

本轮设置窗口的职责是：

- 为当前固定任务栏显示产品提供数据更新设置
- 为当前任务栏文本显示提供有限且稳定的显示设置
- 作为内部 API 验证入口展示 benchmark 和调度状态

## 2. 页面结构

设置窗口为独立原生窗口，页签分为：

1. `数据更新设置`
2. `显示设置`

托盘菜单入口为：

- `设置`
- `均价计算器`
- `退出`

## 3. 数据更新设置

数据更新页至少包含：

- 自动调度开关
- 首选源选择
- 固定提示：`1 秒请求 / 目标 2-3 秒有效更新`
- 当前活跃源
- 最近成功时间
- 最近变价时间
- 最近切源原因
- 每个源的成功率 / 延迟 / 最近错误 / 变价次数
- 手动执行内部 API 验证按钮

本轮生产调度源固定为：

- `gold-api`
- `xwteam GJ_Au`
- `sina hf_XAU`

## 4. 显示设置

显示设置页服务当前任务栏文本样式，固定支持：

- 字体选择
- 字体大小
- 文字颜色
- 背景颜色
- 背景透明开关
- 左对齐 / 居中对齐
- 水平排列开关
- 预览区域

约束：

- 不恢复桌面悬浮模式
- 不恢复显示模式切换
- 如果某项设置可能破坏任务栏稳定性，则限制取值

## 5. 实现落点

- 应用入口与托盘菜单：
  [app.cpp](/E:/gitHub/GoldView/native/src/app.cpp)
- 设置窗口：
  [settings_window.cpp](/E:/gitHub/GoldView/native/src/settings_window.cpp)
- 价格状态与 benchmark：
  [price_service.cpp](/E:/gitHub/GoldView/native/src/price_service.cpp)
- 任务栏显示：
  [taskbar_host.cpp](/E:/gitHub/GoldView/native/src/taskbar_host.cpp)
- 设置持久化：
  [settings_store.cpp](/E:/gitHub/GoldView/native/src/settings_store.cpp)

## 6. 验收结果

满足以下条件即可视为本轮设置功能达成：

- 托盘菜单能打开独立设置窗口
- 修改显示设置后任务栏立即刷新并持久化
- 修改自动调度和首选源后价格服务即时生效
- 数据页能看到活跃源、最近成功时间、最近变价时间和每源统计
- 内部 API 验证按钮能触发 benchmark 并更新面板
