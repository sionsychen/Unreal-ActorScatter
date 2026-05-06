# ActorScatter

> 面向策划的 Unreal Engine 5.4 区域撒点工具 —— Spline 区域 + 规则资产 → Actor / ISM / HISM / PCG 兼容点集，自带批次管理与一键撤销。

[![UE](https://img.shields.io/badge/UE-5.4-blue)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Status](https://img.shields.io/badge/status-experimental-orange)](#状态)
[![Version](https://img.shields.io/badge/version-0.1.0-lightgrey)](CHANGELOG.md)

🌐 **语言：** [English](README.md) · 中文

---

## 这是什么

ActorScatter 处于 UE 自带 Foliage 工具和 PCG 之间的空白地带：比 Foliage 多了规则驱动，比 PCG 节点图低门槛。策划画一条闭合 spline，挂一个规则（或规则栈），点 **Preview** 看候选点，**Spawn** 提交——几百个 actor / ISM 实例 / 点集资产瞬间到位，整批可一键撤销。

围绕两个核心理念构建：

1. **工具即文档。** 状态面板逐步引导操作；Diagnose 按钮校验配置并给出具体修复建议；Generate Sample Rules 一键生成可运行示例。
2. **策划友好。** 规则默认 Simple 模式只露 ~13 个核心字段；高级功能（簇、密度曲线、Mesh SDF 等）切一个开关即可。

## 主要功能

- **两种选点模式** —— Spread（farthest-first，均匀覆盖）与 Cluster（中心 + 半径填充，如树丛矿脉）。
- **避让 + 吸附** —— 名字/类名 pattern 匹配，三种距离度量：AABB、Capsule（长条形）、Mesh SDF（真碰撞形状距离）。
- **密度驱动** —— 高度 float curve + 灰度贴图（UV 映射到 spline AABB）。
- **多 Spline + 孔洞** —— 多 spline 区域并集，孔洞作 boolean 减法。
- **输出模式** —— Actor / ISM / HISM / PointSet 资产（与 PCG 衔接）。
- **规则栈** —— 多 Pass 串行执行打包成单一可撤销批次；每个 Pass 独立开关。
- **Auto Preview** —— 编辑参数后自动 debounced 重采样，默认关闭。
- **Spawn Count Override** —— spawner 自带数量覆盖字段，调密度不污染共享规则资产。
- **自文档** —— 状态感知的下一步提示、上下文 Diagnose 报告、零结果时 Log 的 `TIP:` 排错指引。

## 快速开始

1. 将插件放入项目的 `Plugins/` 目录，重新生成 VS 项目文件，编译 Editor target。
2. 打开 `Window → Level Editor → Area Scatter`。
3. 面板里点 **Generate Sample Rules** —— `/Game/AreaScatter/Samples/` 会出现 3 个可用 `URulePassAsset` 示例。
4. 放一个 `Area Spawner`，画闭合 spline，挂上示例规则，点 **Preview** → **Spawn**。

完整教程：**[Plugins/AreaScatter/Docs/01_QuickStart.md](Plugins/AreaScatter/Docs/01_QuickStart.md)**

插件级参考文档：**[Plugins/AreaScatter/README.md](Plugins/AreaScatter/README.md)**

## 状态

**v0.1.0 — 实验性。** 已在 UE 5.4 (Windows) 上验证。5.3 / 5.5 未测。次要版本可能改动 API 与资产 schema。**暂不建议用于上线项目**，目前定位为个人/小团队 designer & TA 的工作流加速器，欢迎试用反馈。

阶段开发记录、已知缺口见 [CHANGELOG.md](CHANGELOG.md)。

## 目录结构

```
ActorScatter/
├── Source/                          # 示例 game module（UE 默认模板）
├── Plugins/
│   └── AreaScatter/                 # 插件主体
│       ├── AreaScatter.uplugin
│       ├── Source/
│       │   ├── AreaScatter/         # Runtime —— 数据资产 + 撒点 Actor
│       │   └── AreaScatterEditor/   # Editor —— 采样 / 选点 / 实例化 / Slate 面板
│       ├── Docs/01_QuickStart.md    # 10 步上手教程
│       └── README.md                # 插件参考文档
├── CHANGELOG.md
└── LICENSE
```

插件自包含 —— 把 `Plugins/AreaScatter/` 整目录拷进任意 UE 5.4 工程都能直接用。

## 参与开发

issues 和 PR 都欢迎：https://github.com/sionsychen/Unreal-ActorScatter/issues —— bug 反馈、新功能提议、"这个 UI 看不懂"的吐槽都算。

如果想提非琐碎的 PR，建议先开 issue 对齐方向 —— 目前代码量还小，避免双方各自重构后撞车。

## 许可

[MIT 许可证](LICENSE) —— 商业/非商业自由使用。

## 致谢

- 核心算法源自一个 Python EUW 原型：farthest-first / 簇填充 / spline 多边形采样。
- Mesh SDF 距离查询基于 Chaos 物理 `FBodyInstance::GetSquaredDistanceToBody`。
- 设计原则上参考 *Houdini Scatter，但深度集成 UE，且对策划友好。*
- 与 [Claude Code](https://claude.com/claude-code)（Anthropic）协作开发。
