# GoldView Native

这个目录包含 `v3.2.0` 的原生 `Win32/C++` 实现。

当前能力：

- 原生托盘入口与模式切换
- `任务栏左侧模式`
- `桌面悬浮模式`
- 多源国际金价获取与回退
- 原生均价计算器
- 本地设置与历史记录持久化

当前产品约束：

- 主显示永远只显示黄金价格数值
- 不再提供“显示数值”开关
- 不再提供“显示均价信息”开关
- 刷新频率统一为 `0.5 秒 / 1 秒 / 2 秒`

构建：

```bat
native\build_native.bat
```

输出：

- `native\build\GoldViewNative.exe`

关键文档：

- `docs/GoldView任务栏技术实现说明.md`
- `docs/tasks/任务记录.md`
