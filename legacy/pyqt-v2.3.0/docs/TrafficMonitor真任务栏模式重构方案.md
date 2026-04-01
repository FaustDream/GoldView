# GoldView TrafficMonitor 真任务栏模式重构方案

![TrafficMonitor 真任务栏模式示意图](./assets/TrafficMonitor真任务栏模式示意图.svg)

## 1. 方案结论

本轮方案将 GoldView 的“任务栏贴边拟态模式”升级为“真任务栏语义模式”。

这里的“真任务栏模式”含义非常明确：

- 显示区域属于任务栏承载体系，而不是桌面上的独立顶层窗口
- 跟随任务栏布局、DPI、自动隐藏和主屏任务栏位置变化
- 不再把“固定在右下角的无边框窗口”称为任务栏模式

因此，旧的两条路线被正式废弃：

- 伪嵌入任务栏数字条
- 桌面贴边拟态任务栏模式

新方案保留双模式，但两种模式的定义发生变化：

- **模式 A：真任务栏模式**
  - 由原生 Win32/Shell 宿主层承载
  - 语义属于任务栏区域
  - 是 GoldView 的主显示模式
- **模式 B：悬浮模式**
  - 由原生独立窗口承载
  - 是备用显示模式
  - 不再与任务栏模式共享同一种窗口实现

## 2. 为什么必须改成真任务栏模式

当前仓库里已经验证过一个事实：把无边框窗口固定在桌面右下角，只能做出“像任务栏”的效果，做不出“属于任务栏”的语义。

这会带来几个根本问题：

- 用户认知错误：看起来像任务栏组件，实际上是桌面独立窗口
- 兼容性错误归因：闪烁、遮挡、位置偏移并不是简单 UI bug，而是承载层选错了
- 后续优化空间有限：DPI、多屏、全屏、自动隐藏都需要不断打补丁

因此，本轮不是继续优化拟态窗口，而是直接切换显示承载技术路线。

## 3. 承载机制选择

### 3.1 最终结论

GoldView 真任务栏模式的最终运行态只保留**纯 C++ 原生应用**。

- 任务栏模式：由 `Win32/Shell` 原生宿主层负责
- 悬浮模式：由原生窗口层负责
- 托盘、设置、价格服务、均价服务：全部进入原生程序

### 3.2 为什么不是继续用 PyQt

PyQt 仍然适合快速做业务样机，但不适合作为真任务栏承载层：

- 无法自然获得任务栏语义
- 很难稳定处理 Win11 任务栏可用区域和系统区避让
- 需要不断用顶层窗口技巧模拟任务栏存在感

因此，现有 PyQt 代码在新方案中的定位只剩 3 个：

- 业务逻辑迁移参照
- 交互样机参考
- 迁移阶段行为对照

它不再是终态运行依赖。

### 3.3 真任务栏模式的现实边界

需要明确，TrafficMonitor 类方案在 Windows 11 下也不是“零难点”：

- 任务栏区域的可用空间并不稳定
- 图标重叠和局部偏移在同类软件中也是已知问题
- 某些全屏或系统状态下，任务栏本身就会改变可见性和布局

所以新方案追求的不是“理论完美”，而是：

- 原生承载
- 明确避让策略
- 可验证的退化行为

参考资料：

- [Microsoft Learn: Taskbar Extensions - Win32 apps](https://learn.microsoft.com/en-us/windows/win32/shell/taskbar-extensions)
- [TrafficMonitor Wiki: Option Settings](https://github.com/zhongyang219/TrafficMonitor/wiki/Option-Settings)
- [TrafficMonitor Issue #1309](https://github.com/zhongyang219/TrafficMonitor/issues/1309)
- [TrafficMonitor Issue #1958](https://github.com/zhongyang219/TrafficMonitor/issues/1958)

## 4. 语言路线与技术栈

### 4.1 语言路线

锁定为：**纯 C++ 终局**

原因：

- 更适合 Win32/Shell/COM 任务栏承载
- 最终体积更可控
- 运行依赖更纯净
- 不需要长期维持 Python 与原生双栈

### 4.2 推荐模块边界

原生主程序建议拆为以下模块：

- `TaskbarHost`
- `FloatingHost`
- `PriceService`
- `AverageService`
- `SettingsStore`
- `TrayController`
- `ModeController`

### 4.3 关键数据接口

建议统一为以下结构：

```cpp
struct PriceSnapshot {
    double currentPrice;
    double openPrice;
    double lowPrice;
    std::wstring source;
    std::uint64_t updatedAt;
};

struct AverageSummary {
    double averagePrice;
    double totalWeight;
    std::wstring summaryText;
};

struct DisplaySettings {
    DisplayMode mode;
    bool showGoldPrice;
    bool showAverageInfo;
    double refreshInterval;
};
```

### 4.4 模式控制规则

- 任一时刻仅允许一个主显示模式活跃
- 模式切换只切宿主层，不重建业务状态
- 价格轮询、均价摘要、设置状态在切换时持续有效

## 5. 与现有 PyQt 业务层的边界

现有仓库中的 Python 代码应按“迁移参考层”处理，而不是继续扩展：

- `gold_view.py`
  - 当前只是托盘和拟态模式总控参考
  - 不再继续承担真任务栏宿主职责
- `core/price_service.py`
  - 可作为原生价格服务的接口和容错逻辑参考
- `core/average_service.py`
  - 可作为均价计算迁移参考
- `core/settings_service.py`
  - 可作为设置项结构参考
- `calculator.py`
  - 可作为均价输入流程和结果展示参考

迁移原则：

- 迁移逻辑，不迁移 PyQt 窗口模型
- 保留字段语义，不保留技术栈依赖
- 先完成原生任务栏宿主原型，再迁移剩余业务逻辑

## 6. 闪烁、遮挡、DPI、多屏、全屏问题治理

这部分是新方案落地后的最高优先级治理项。

### 6.1 闪烁控制

- 禁止高频 `show/hide`
- 禁止重复 `SetWindowPos` 抖动式定位
- 使用文本缓存，仅在值变化时重绘
- 模式切换只更换宿主，不重建价格与设置状态

### 6.2 遮挡规避

- 必须检测 Win11 主屏任务栏可用区域
- 必须避让系统托盘区和小组件等系统占用区
- 当空间不足时优先退化显示字段
  - 先隐藏均价信息
  - 再必要时只显示价格
- 不允许与任务栏图标争抢同一像素区

### 6.3 多屏策略

- 首轮仅绑定系统主任务栏
- 副屏任务栏不作为首轮阻塞项
- 后续扩展再单列副屏任务栏支持

### 6.4 DPI 一致性

- 全链路采用 DPI 感知坐标
- 禁止逻辑坐标与物理坐标混用
- 固定验证断点：
  - `100%`
  - `125%`
  - `150%`
  - `200%`

### 6.5 全屏行为

- 全屏程序激活时不抢焦点
- 不主动盖住系统前景层
- 若系统语义下任务栏区域本身不可见，则允许任务栏模式暂时不可见
- 禁止用悬浮窗偷偷补位伪装“仍在任务栏里”

## 7. 建议实施顺序

### 第一阶段：文档与路线冻结

- 输出真任务栏模式 SVG
- 输出真任务栏重构方案
- 在任务记录中关闭旧锚点并建立 `v3.0.0`

### 第二阶段：原生任务栏宿主原型

- 验证真任务栏承载机制
- 验证文本刷新、区域占用、位置跟随、托盘控制
- 明确 Win11 下的退化行为

### 第三阶段：原生业务层迁移

- 迁移价格服务
- 迁移均价服务
- 迁移设置服务
- 迁移托盘控制和模式管理

### 第四阶段：稳定性治理

- 闪烁
- 遮挡
- DPI
- 主屏任务栏兼容
- 全屏与自动隐藏行为

## 8. 验收标准

满足以下条件，才算“真任务栏模式”成立：

- 显示区域不再表现为独立桌面顶层窗口
- 行为跟随任务栏，而不是仅跟随桌面右下角
- 双模式切换后，任务栏模式仍保持任务栏宿主语义
- 连续刷新时无明显闪烁
- Win11 主屏任务栏在常见场景下行为稳定且可解释
- 文档、示意图、任务记录三者对“任务栏模式”的定义一致

