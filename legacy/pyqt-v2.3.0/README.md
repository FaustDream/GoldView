# GoldView PyQt Legacy

这个目录保存 GoldView 在 `v2.3.0` 阶段的 Python / PyQt 参考实现。

## 目录用途

- 作为旧版双模式重构尝试的代码归档
- 作为后续原生 C++ 版本迁移时的业务逻辑参考
- 作为价格服务、均价逻辑、设置结构和交互流程的对照样机

## 包含内容

- `gold_view.py`
- `calculator.py`
- `build_app.py`
- `package.bat`
- `pyproject.toml`
- `requirements.txt`
- `core/`
- `shell/`

## 说明

- 这里的“任务栏模式”是旧版拟态或过渡实现，不等于当前 `v3.0.0` 真任务栏原生方案
- 当前主线开发请看仓库根目录下的 `native/`
