# AreaScatter

Editor-time area scatter plugin for Unreal Engine 5.4. Spline area + rule asset (or rule stack) -> Actors / ISM / HISM / PointSet asset, with batch tracking.

> ⚠️ **Experimental — v0.1.0.** Tested on UE 5.4 (Windows). 5.3 / 5.5 untested. API and asset schema may change in minor releases. Not recommended for ship-blocking work yet — designed for individual designer/TA use and feedback.
>
> License: [MIT](../../LICENSE) — free for any use including commercial. Issues / PRs welcome at [GitHub](https://github.com/sion-sychen/ActorScatter/issues).

## Self-documenting tooling

The plugin is built so the tool itself answers "what do I do next?":

- **State-aware next-step text** at the top of the panel updates as you select a spawner / draw the spline / assign a rule. It always tells you what's missing.
- **Diagnose** button validates everything (spline, rule output mode, multi-area cycles, density texture format) and lists each issue with a one-line fix suggestion in the panel's Diagnostics box.
- **Generate Sample Rules** button creates 3 working examples in `/Game/AreaScatter/Samples/` so a fresh project has something to copy from.
- **Spawn Count Override** SpinBox lets designers iterate density (50 vs 200 vs 1000) without modifying the shared rule asset. 0 = use rule's authored value.
- **Auto Preview** checkbox: when on, edits to the spawner / its rule / stack passes re-run Preview ~300ms after the last change (debounced). Off by default — Preview re-samples the whole region.
- **Simple vs Detailed mode** on every `URulePassAsset` (`ConfigMode` field at the top): Simple shows ~13 core fields (incl. avoid + attract pattern/distance); Detailed reveals all 30+ knobs.
- Every `URulePassAsset` field has a rich tooltip with units and examples — hover any property in the Details panel.
- When picking yields zero points, the Output Log emits a `TIP: ...` line naming the specific filter that ate everything (slope / avoid / spacing / etc.).

## Status

**Phase 3 — output modes, multi-spline, capsule avoid, density.** Compiles clean. Awaiting in-editor validation.

| Layer | Item | Phase |
|---|---|---|
| Runtime | `URulePassAsset` (slope, avoid, attract, cluster, density, output) | 1+2+3 |
| Runtime | `URuleStackAsset` (multi-pass) | 2 |
| Runtime | `UAreaSpawnBatch` | 1 |
| Runtime | `AAreaSpawnerActor` + spline + 3 CallInEditor + **AdditionalAreas / HoleAreas** | 1+2+3 |
| Runtime | **`AAreaScatterInstanceHost`** (ISMC/HISMC owner) | 3 |
| Runtime | **`UAreaScatterPointSet`** (DataAsset; Phase-3 PCG handoff) | 3 |
| Editor  | Sampler — multi-positive + hole splines, polygon, ray grid | 3 |
| Editor  | Picker — Spread + Cluster + **AABB / Capsule avoid + density curve / texture** | 1+2+3 |
| Editor  | Spawner — Actors / **ISM / HISM / PointSet** with one-batch undo | 1+2+3 |
| Editor  | Slate panel — Window -> Level Editor -> Area Scatter | 2 |
| Editor  | `UAreaScatterEditorLibrary` — BP/Python callable | 2 |

## Quick start

**See [Docs/01_QuickStart.md](Docs/01_QuickStart.md) for a 10-step walkthrough.** Short version:

1. Open `ActorScatter.uproject`. Plugin auto-loads.
2. **Window -> Level Editor -> Area Scatter** opens the panel.
3. **Click "Generate Sample Rules"** in the panel — writes 3 working rules to `/Game/AreaScatter/Samples/`.
4. Place an `Area Spawner`, draw a closed-loop spline, assign a sample rule.
5. **Click "Diagnose"** to verify, **"Preview"** to see candidate points, **"Spawn"** to commit.
6. Ctrl+Z reverts. Click "Clear Last" to remove a batch without affecting setup.

The panel shows **state-aware next-step guidance** at the top — it always tells you what to do next based on what's missing.

## Output modes

| Mode | Use case | What gets created |
|---|---|---|
| Actors | A few hundred bespoke BP actors | `AActor`s, parented to `AreaScatter/<batch>/` Outliner folder |
| ISM | Mid-volume props (rocks, props sharing a mesh) | One `AAreaScatterInstanceHost` actor per batch with one ISMC per pass |
| HISM | Vegetation, large counts needing LOD | Same, but `UHierarchicalInstancedStaticMeshComponent` |
| PointSet | "Hand off to PCG" — designer authors picks here, PCG/BP consumes them | `UAreaScatterPointSet` asset at `/Game/AreaScatter/Generated/PS_<spawner>_<batchid>.uasset` |

PointSet rationale: a full PCG plugin coupling adds a lot of surface area for a feature most users will wire via a small BP/PCG element anyway. The asset stores `TArray<FAreaScatterPoint>` (transform + normal + slope) — drop into a PCG graph via `Get Asset` or read from BP.

## Density

Both density inputs multiply into a per-candidate accept probability, then the picker rolls one RNG draw per candidate. Same `Seed` -> same picks.

- **`bUseDensityByAltitude`** + `DensityByAltitude` curve: float curve maps **Z (cm) -> [0, 1]**. Useful for "coastline grass dies above 80m".
- **`DensityTexture`** (Texture2D, source format BGRA8 or G8): UV maps to spline AABB. Use channel 0..3 (R/G/B/A). `DensityTextureScale` multiplies the sampled value before clamping.

## Capsule / MeshSDF avoid

`AvoidShapeMode` picks the metric used by `AvoidDistance`:

| Mode | Distance metric | Use case | Cost |
|---|---|---|---|
| AABB | XY distance to actor's axis-aligned box | Default; rectangular props | 🟢 fastest |
| Capsule | XY distance to capsule derived from longer AABB extent | Fences, pillars, rock columns | 🟡 cheap |
| MeshSDF | 3D distance to actor's collision shapes (`FBodyInstance::GetSquaredDistanceToBody`) | Tree canopies / cliff overhangs / cooked convex hulls | 🔴 expensive — gets an AABB pre-reject |

MeshSDF requires the avoid actors to have collision (any body). Actors without a body silently fall back to no contribution.

`AvoidShapeMode = Capsule` derives a capsule from each avoid actor's AABB:
- Capsule axis = longer of X/Y extents
- Radius = shorter half-extent
- Half-length = (longer extent - shorter extent)

Fence segments and rock columns now reject candidates aligned along their length, not in a wasteful square halo.

## Multi-spline + holes

Each `AdditionalAreas` entry contributes its `AreaSpline` to the positive region (boolean OR). Each `HoleAreas` entry punches a subtractive hole. Inside-test:

```
in_region(P) = (any positive contains P) AND (no hole contains P)
```

Avoid/attract collection runs against all positive polygons, deduped by actor name. Bounds union across all positives drives the sampling grid.

## Cluster vs Spread

| Mode | Use case | Key params |
|---|---|---|
| Spread | Even coverage (grass, debris) | `MinPointDistance` |
| Cluster | Stands of trees, ore deposits | `NumClusters`, `CenterMinDistance`, `ClusterRadius`, `ClusterSizeMin/Max` |

Cluster: farthest-first centers -> per-cluster radius fill respecting `MinPointDistance` -> global farthest-first backfill.

## Stack semantics

- Passes execute in order; each independently samples / filters / picks.
- All passes' output lands in **one** `UAreaSpawnBatch`. One Ctrl+Z undoes the lot.
- For ISM/HISM mode, all passes share the same `AAreaScatterInstanceHost` actor (one ISMC/HISMC per pass).
- A single pass's `bEnabled = false` skips it without removing it from the stack.

## Layout

```
Plugins/AreaScatter/
  AreaScatter.uplugin
  Source/
    AreaScatter/
      Public/
        AreaScatterTypes.h       # Candidate / AvoidShape / log
        RulePassAsset.h          # all rule fields
        RuleStackAsset.h
        AreaSpawnBatch.h
        AreaSpawnerActor.h       # spline + multi-area + delegates
        AreaScatterInstanceHost.h  # ISM/HISM owner
        AreaScatterPointSet.h    # PointSet asset
    AreaScatterEditor/
      Public/
        Sampling/AreaScatterSampler.h    # multi-poly + hole inside-test
        Picking/AreaScatterPicker.h      # filters + cluster/spread
        Spawning/AreaScatterSpawner.h    # output dispatch
        Slate/SAreaScatterPanel.h
        AreaScatterEditorOps.h
        AreaScatterEditorLibrary.h
```

## Phase 4 (later, "as needed")

- PCG plugin direct coupling (UPCGPointData asset writeback) if PointSet asset isn't enough
- Mesh SDF avoid (`PhysX-cooked-mesh distance` for true shape avoidance)
- Real-time preview slider (debounced sampler on parameter edits)
- Built-in preset library (.uasset files; can't author from text — author once in editor and ship)
- Python TA hook pass (PythonScriptPlugin dependency)
- Performance tuning: parallel sampling chunks for >1km regions

## Non-goals

Runtime streaming, gameplay systems, save serialization. Editor-only.

## Reference

Predecessor (Python EUW prototype): `D:/_Cursor/UE_AreaSpawn`. Algorithm parity in C++:

| Python | C++ |
|---|---|
| `_farthest_first` | `AreaScatter::PickSpread` |
| `_pick_cluster` | `AreaScatter::PickCluster` |
| `_sample_spline_outline` | `AreaScatter::SampleSplineOutline2D` |
| `_point_in_polygon` | `AreaScatter::PointInPolygon2D` |
| `_aabb_dist_2d` | `AreaScatter::AABBDistanceXY` |
| (new) Capsule | `AreaScatter::CapsuleDistanceXY` |
| `_build_avoid_list` / `_build_attract_list` | `AreaScatter::CollectShapes` |
| `BP_PlacementRule` field set | `URulePassAsset` UPROPERTYs |
