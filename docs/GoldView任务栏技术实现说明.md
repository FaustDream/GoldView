# GoldView 任务栏技术实现说明

## 1. 当前结论

`v3.2.0` 的 GoldView 不再把目标定义为“伪任务栏贴边窗口”，而是采用两级实现：

1. `taskListUsable=true`
   - 走 TrafficMonitor 类路径
   - 识别任务栏容器与任务按钮区
   - 压缩任务按钮区
   - 将 GoldView 宿主窗口挂到任务栏容器下
2. `taskListUsable=false`
   - 走主屏 fallback 路径
   - 直接挂到 `Shell_TrayWnd`
   - 仍然在主屏任务栏内显示，不再因为零尺寸 `taskList` 直接隐藏

这让 GoldView 在当前机器上已经可以稳定显示在主屏任务栏中，并把显示位置统一调整为任务栏最左侧。

## 2. TrafficMonitor 类机制

结合 TrafficMonitor 源码，可以确认其核心机制不是普通顶层窗口，而是：

- 找到 `Shell_TrayWnd`
- 找到 `ReBarWindow32` 或 `WorkerW`
- 找到 `MSTaskSwWClass` 或 `MSTaskListWClass`
- 用 `MoveWindow(...)` 压缩任务按钮区
- 用 `SetParent(...)` 把自己的任务栏窗口挂进任务栏容器
- 在任务栏刷新、结构变化、按钮区异常时继续重试和恢复

GoldView 当前实现沿用的是同一条思路，只是增加了一个更现实的 fallback：

- 当 Win11 当前窗口树给出的 `taskListRect` 是零尺寸时，不再持续隐藏
- 直接退化到主屏任务栏左侧显示，保证主路径失败时主屏仍可见

## 3. 当前实现结构

当前原生工程核心模块如下：

- `App`
  - 托盘入口
  - 模式切换
  - 刷新频率设置
  - 价格快照分发
  - 均价计算器入口
- `TaskbarTopologyDetector`
  - 识别 `Shell_TrayWnd`
  - 识别任务栏容器、任务按钮区、托盘区
  - 输出 `taskListUsable`
- `TaskbarSlotController`
  - 仅在按钮区可用时介入
  - 压缩任务按钮区并保留恢复路径
- `TaskbarHost`
  - 任务栏左侧模式宿主
  - 只显示黄金价格数值
- `FloatingHost`
  - 桌面悬浮模式宿主
  - 只显示黄金价格数值
- `PriceService`
  - 多 API 顺序回退
  - 默认走系统代理
- `AverageService`
  - 均价计算逻辑
- `CalculatorWindow`
  - 原生均价计算器
- `SettingsStore`
  - 模式
  - 刷新频率
  - 可选 API key
  - 计算器历史

## 4. 任务栏左侧实现

### 4.1 左侧锚点规则

GoldView `v3.2.0` 已将任务栏模式统一定义为“任务栏左侧模式”：

- 如果 `taskListUsable=true`
  - GoldView 锚定到主屏任务栏绝对最左侧
  - 同时保留任务按钮区重排能力
- 如果 `taskListUsable=false`
  - GoldView 直接挂到 `Shell_TrayWnd`
  - 宿主矩形从 `taskbarRect.left` 起算
  - 不再靠托盘区左边界决定主显示位置

### 4.2 当前日志判断方式

日志文件路径：

- `%LOCALAPPDATA%\GoldView\logs\taskbar.log`

关键字段：

- `taskListUsable=true/false`
- `layoutMode=reflow|absolute-left-fallback|hidden`
- `slotController=attach|update|restore|skipped`
- `Taskbar host parent attached ...`
- `Taskbar host bounds applied ...`

如果再次不可见，先看：

1. 是否识别到 `Shell_TrayWnd`
2. `taskListUsable` 是否为 `false`
3. 是否已经进入 `absolute-left-fallback`
4. `SetParent` 是否成功
5. 最终宿主矩形是否已应用

## 5. 国际金价 API 策略

GoldView `v3.2.0` 的价格服务采用顺序回退：

1. `gold-api`
2. `exchangerate.host`
3. `metals.dev`（可选 key）
4. `metalpriceapi`（可选 key）

设计原则：

- 默认安装即可工作，不要求先填写 key
- 如果用户手动写入 key，可启用更多备用源
- 国外 API 默认继承系统代理
- 如果只有现价可用，则 `openPrice` 和 `lowPrice` 使用最小可用降级

当前配置保存在：

- `goldview-native.ini`

可选字段：

- `api/metalsDevApiKey`
- `api/metalPriceApiKey`

## 6. 产品收口

`v3.2.0` 对产品形态做了明确收口：

- 软件统一命名为 `GoldView`
- 模式文案统一为：
  - `任务栏左侧模式`
  - `桌面悬浮模式`
- 任务栏与悬浮模式都只显示黄金价格数值
- 去掉“显示数值”选项
- 去掉“显示均价信息”选项
- 刷新频率统一为：
  - `0.5 秒`
  - `1 秒`
  - `2 秒`
- 原生均价计算器通过托盘入口打开

## 7. 原生均价计算器

均价计算器参照旧版 Python 逻辑迁移：

- 输入原持仓价格/重量
- 输入新买入价格/重量
- 计算均价和总持有量
- 保留最近 5 条历史

迁移目标不是复刻 PyQt 样式细节，而是保持风格方向一致：

- 同样的主色调
- 同样的字体和视觉密度
- 同样的简单直接交互结构

## 8. 图标与品牌资源

`v3.2.0` 新增品牌源文件：

- `native/assets/goldview-icon.svg`

运行时托盘图标仍使用原生绘制路径，但视觉方向与 SVG 保持一致：

- 深色底
- 金色圆形徽章
- GoldView 简写图形

## 9. 验收标准

满足以下条件即可视为 `v3.2.0` 达成：

- GoldView 启动后显示在主屏任务栏最左侧
- 任务栏模式与桌面悬浮模式命名统一
- 托盘菜单只保留有效产品功能
- 去掉均价显示链路，只保留均价计算器
- 国际金价支持多源回退，国外 API 走系统代理
- 原生均价计算器可打开、可计算、可保存历史
- 仓库中只保留这一份任务栏技术说明文档
