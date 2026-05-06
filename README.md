# ActorScatter

> A designer-friendly area scatter tool for Unreal Engine 5.4 — Spline area + rule asset → spawned Actors / ISM / HISM / PCG-ready point set, with batch tracking and one-click undo.

[![UE](https://img.shields.io/badge/UE-5.4-blue)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Status](https://img.shields.io/badge/status-experimental-orange)](#status)
[![Version](https://img.shields.io/badge/version-0.1.0-lightgrey)](CHANGELOG.md)

🌐 **Languages:** English · [中文](README.zh-CN.md)

---

## What is this

ActorScatter sits in the gap between Unreal's foliage tool and PCG: more rule-driven than foliage, lower-friction than node graphs. Designers paint a closed-loop spline, drop a rule (or rule stack), click **Preview**, then **Spawn** — and get hundreds of placed actors / instanced meshes / point-set assets, all packaged as a single undoable batch.

Built around two ideas:

1. **The tool is the documentation.** A status panel guides you through each step; a Diagnose button validates the setup with concrete fix suggestions; a Generate Sample Rules button bootstraps a working configuration in one click.
2. **Designer ergonomics.** Rules ship in a Simple mode (~13 core fields) by default; advanced controls (cluster, density curves, mesh SDF) are one toggle away.

## Highlights

- **Two pick modes** — Spread (farthest-first, even coverage) and Cluster (centers + radius fill, e.g. tree stands).
- **Avoid + Attract** — name / class pattern matching with three distance metrics: AABB, Capsule (long-thin shapes), Mesh SDF (true collision-shape distance).
- **Density driver** — runtime float curve over altitude + grayscale texture mask (UV-projected to spline AABB).
- **Multi-spline + holes** — boolean OR for unioned regions, boolean SUBTRACT for cutouts.
- **Output modes** — Actors / ISM / HISM / PointSet asset (PCG-friendly handoff).
- **Rule stack** — multi-pass rules execute as one undoable batch; per-pass enable toggles.
- **Auto Preview** — debounced re-sample on property edits when you want it; off by default.
- **Spawn Count Override** — iterate density (50 / 200 / 1000) on the spawner without dirtying the shared rule asset.
- **Self-documenting** — state-aware next-step text, contextual diagnostics, log `TIP:` messages on zero-pick outcomes.

## Quick start

1. Drop the plugin into your project's `Plugins/` folder, regenerate VS files, build the editor target.
2. Open `Window → Level Editor → Area Scatter`.
3. Click **Generate Sample Rules** in the panel — you now have three working `URulePassAsset` examples in `/Game/AreaScatter/Samples/`.
4. Place an `Area Spawner`, draw a closed-loop spline, assign a sample rule, click **Preview** → **Spawn**.

Full walkthrough: **[Plugins/AreaScatter/Docs/01_QuickStart.md](Plugins/AreaScatter/Docs/01_QuickStart.md)**

Plugin-level reference: **[Plugins/AreaScatter/README.md](Plugins/AreaScatter/README.md)**

## Status

**v0.1.0 — Experimental.** Tested on UE 5.4 (Windows). 5.3 / 5.5 untested. API and asset schema may change in minor releases. Not recommended for ship-blocking work yet — designed for individual designer / TA use and feedback.

See [CHANGELOG.md](CHANGELOG.md) for the per-phase development log and known gaps.

## Project layout

```
ActorScatter/
├── Source/                          # Sample game module (Engine boilerplate)
├── Plugins/
│   └── AreaScatter/                 # The plugin itself
│       ├── AreaScatter.uplugin
│       ├── Source/
│       │   ├── AreaScatter/         # Runtime — data assets + spawner actor
│       │   └── AreaScatterEditor/   # Editor — sampling / picking / spawning / Slate panel
│       ├── Docs/01_QuickStart.md    # 10-step walkthrough
│       └── README.md                # Plugin reference
├── CHANGELOG.md
└── LICENSE
```

The plugin is self-contained — copy `Plugins/AreaScatter/` into any UE 5.4 project and it works.

## Contributing

Issues and PRs welcome at https://github.com/sionsychen/Unreal-ActorScatter/issues. Anything triggers a fix: bug reports, feature requests, "this UI is confusing" notes.

If you're considering a non-trivial PR, please open an issue first to align on direction — the plugin is still small enough that a refactor by one party can collide with another's WIP.

## License

[MIT](LICENSE) — free for commercial / non-commercial use.

## Acknowledgements

- Algorithm inheritance from a personal Python EUW prototype: farthest-first / cluster fill / spline polygon sampling.
- Shape-level avoidance via Chaos `FBodyInstance::GetSquaredDistanceToBody`.
- Designed in the spirit of *Houdini Scatter, but native to UE and approachable to designers.*
- Built collaboratively with [Claude Code](https://claude.com/claude-code) (Anthropic).
