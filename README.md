# GoldView

当前主线版本是原生 `Win32/C++` 实现，位于：

- `native/`

历史 `Python / PyQt` 参考实现已归档到：

- `legacy/pyqt-v2.3.0/`

当前版本说明：

- `native/` 对应 `v3.4.0`
- 应用启动后固定为任务栏左侧显示
- 不再提供显示模式设置
- 主显示只展示国际金价数值
- 刷新频率固定为 `0.5 秒`
- 数据源仅保留稳定且可公开接入的免费接口，当前接入源为 `gold-api`
- `exchangerate.host` 官方文档当前要求 `access_key`，因此当前不作为默认免配置来源
- 浙商银行未确认可公开接入的官方黄金行情 API，因此当前不接入
- `Gold API` 文档与条款对多次/秒请求存在表述差异，固定 `0.5 秒` 轮询需在实机阶段重点观察
- 当全部免费源请求失败时，继续显示最后一次成功价格
- 均价能力通过原生均价计算器提供，不再进入主显示链路

文档入口：

- `docs/GoldView任务栏技术实现说明.md`
- `docs/tasks/task.md`

运行方式：

```bat
native\build_native.bat
native\build\GoldViewNative.exe
```
