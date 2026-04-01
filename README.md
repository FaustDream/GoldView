# GoldView

当前主线版本是原生 `Win32/C++` 实现，位于：

- `native/`

历史 `Python / PyQt` 参考实现已归档到：

- `legacy/pyqt-v2.3.0/`

当前版本说明：

- `native/` 对应 `v3.2.0`
- 主显示模式为：
  - `任务栏左侧模式`
  - `桌面悬浮模式`
- 主显示只展示国际金价数值
- 均价能力通过原生均价计算器提供，不再进入主显示链路

文档入口：

- `docs/GoldView任务栏技术实现说明.md`
- `docs/tasks/任务记录.md`

运行方式：

```bat
native\build_native.bat
native\build\GoldViewNative.exe
```
