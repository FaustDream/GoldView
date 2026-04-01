# GoldView 任务栏技术实现说明

## 1. 当前结论

`v3.5.0` 的 GoldView 仍然是固定任务栏显示产品，但相比 `v3.4.0` 有两处关键升级：

- 价格链路从单源收口升级为三源调度
- 任务栏宿主不再只吃硬编码主题值，而是优先消费 `DisplaySettings`

## 2. 主体结构

- `App`
  - 托盘入口
  - 任务栏宿主刷新
  - 设置窗口与计算器窗口管理
  - 价格快照分发
- `TaskbarHost`
  - 原生任务栏内容宿主
  - 按 `DisplaySettings` 绘制价格文本
- `TaskbarMetrics`
  - 按字体大小与横向/纵向排布推导宿主尺寸
- `PriceService`
  - 固定 `1 秒` 请求
  - `gold-api / xwteam / sina` 三源调度
  - benchmark 与统计输出
- `SettingsWindow`
  - 数据更新页
  - 显示设置页
- `SettingsStore`
  - 显示设置与数据更新配置持久化
  - 计算器历史持久化

## 3. 任务栏宿主渲染

`TaskbarHost` 当前的绘制规则：

- 字体名、字体大小来自 `DisplaySettings`
- 文字颜色来自 `DisplaySettings`
- 背景支持透明或实色
- 对齐方式支持左对齐 / 居中
- 水平排列关闭时，价格按多行方式排布

默认主题值仍由 [theme.h](/E:/gitHub/GoldView/native/include/theme.h) 提供，但只作为默认值来源，不再是唯一渲染来源。

## 4. 调度与显示的关系

价格服务与任务栏宿主的衔接方式：

1. `PriceService` 每秒向活跃源发起一次请求
2. 成功后将快照派发给 `App`
3. `App` 把最新快照同步给 `TaskbarHost`
4. 如果设置窗口处于打开状态，`App` 同步刷新状态面板

全部源失败时：

- 任务栏继续显示最后一次成功价格
- 设置窗口保留错误统计
- 调度器为下一个周期准备切源

## 5. 布局与稳定性原则

- 优先保持任务栏嵌入稳定
- 任务栏可用空间不足时允许退化到绝对左侧 fallback
- 显示设置只开放安全范围内的字号和渲染参数
- 不再恢复桌面悬浮模式和模式切换入口

## 6. 日志与定位

任务栏链路日志仍写入：

- `%LOCALAPPDATA%\\GoldView\\logs\\taskbar.log`

结合本轮升级，建议关注：

- `layoutMode=reflow|absolute-left-fallback|hidden`
- 当前活跃源
- 最近切源原因
- 设置变更后的宿主尺寸与父容器坐标
