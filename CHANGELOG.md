# Changelog

All notable changes to AreaScatter. Format loosely follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning is [SemVer](https://semver.org/spec/v2.0.0.html).

## [0.1.0] — 2026-05-05 — Initial public release

First publishable cut. Phases 0-4 below trace how the code grew; this is the combined state at v0.1.0. Target engine: UE 5.4 (Windows).

### Added

**Phase 0 — Plugin skeleton**
- `AreaScatter.uplugin` registers two modules: `AreaScatter` (Runtime) and `AreaScatterEditor` (Editor).
- Placeholder `AAreaSpawnerActor` with a closed-loop `USplineComponent`.
- Depends on `EditorScriptingUtilities`.

**Phase 1 — MVP end-to-end**
- `URulePassAsset` DataAsset with sampling / slope-filter / AABB-avoid / spacing / output fields.
- `UAreaSpawnBatch` persistent batch UObject (GUID, timestamp, params snapshot, weak refs to spawned actors).
- Editor pipeline: polygon sampler → farthest-first picker → actor spawner with `FScopedTransaction` (Ctrl+Z reverts).
- Preview draws `DrawDebug` spheres + avoid AABBs + normal arrows.
- Outliner auto-grouping under `AreaScatter/<spawner>_<batchid>/`.

**Phase 2 — Rule composition + control panel**
- Cluster pick mode (`PickMode = Cluster`): farthest-first centers + per-cluster radius fill + global backfill.
- Attract rules (symmetric to avoid): name/class patterns + `[Min, Max]` distance band.
- `URuleStackAsset`: multi-pass orchestration, all passes land in one undoable batch.
- Slate nomad tab `Window -> Level Editor -> Area Scatter` with auto-binding to selected spawner.
- `UAreaScatterEditorLibrary`: BlueprintCallable wrappers (`Preview` / `Spawn` / `ClearLastBatch` / `GetSelectedSpawner` / `OpenPanel`).

**Phase 3 — Output modes + geometry features**
- `EAreaSpawnOutputMode`: Actors / ISM / HISM / PointSet asset.
- `AAreaScatterInstanceHost` owns per-batch ISMC/HISMC components; all stack passes share one host.
- `UAreaScatterPointSet` DataAsset persisted to `/Game/AreaScatter/Generated/` for PCG / BP handoff.
- `AAreaSpawnerActor.AdditionalAreas` (boolean OR) + `HoleAreas` (boolean SUBTRACT) — multi-spline regions and holes.
- `EAreaScatterAvoidShape`: `AABB` / `Capsule` (from longer-AABB-axis) — capsule fits fences / pillars / long props.
- Density driver: `FRuntimeFloatCurve DensityByAltitude` (Z → [0, 1]) + `UTexture2D DensityTexture` (BGRA8 / G8 source, UV maps to spline AABB).

**Phase 4 — Designer ergonomics + diagnostics + real-time + Mesh SDF**
- `EAreaScatterConfigMode`: Simple (~13 fields) / Detailed (full 30+ fields), with `EditConditionHides` gating.
- `UCLASS(meta = (PrioritizeCategories = "..."))` forces designer-friendly category order (General → Output → Filter → Spacing → Avoid → Attract → advanced).
- Rich per-property tooltips with units + concrete examples throughout `URulePassAsset`.
- `AAreaSpawnerActor.TargetCountOverride` + panel `SSpinBox<int32>` — iterate density without dirtying shared rule asset.
- `AAreaSpawnerActor.bAutoPreview` + panel checkbox: `OnObjectPropertyChanged` + 300ms `FActiveTimerHandle` debounce re-runs Preview on any relevant edit.
- `EAreaScatterAvoidShape::MeshSDF`: `FBodyInstance::GetSquaredDistanceToBody` against source actor's collision bodies, with AABB pre-reject for speed.
- Tool-in-code documentation:
  - **State-aware next-step text** at the top of the panel.
  - **Diagnose** button (`FDiagReport`) validates spline / rule / output / multi-area cycles / density texture format with per-issue fix suggestions.
  - **Generate Sample Rules** button writes three working `URulePassAsset` examples (`DA_Sample_Grass_Spread` / `DA_Sample_Trees_Cluster` / `DA_Sample_Rocks_AvoidBuildings`).
  - Log `TIP: ...` lines on zero-pick outcomes, naming the specific filter (cells_inside / cells_hit / slope / avoid / spacing) that ate the pool.
- `Docs/01_QuickStart.md`: 10-step smoke test, Phase 2/3 add-ons, Troubleshooting matrix.

### Known gaps (carry-over to future versions)

- Only verified on UE 5.4 + Windows.
- No `UPCGPointData` direct output — use the PointSet asset as a stepping stone.
- No Python hook for TA-authored passes.
- No automated tests for the pure-function picker/inside-test/distance math.
- Large-region performance (>1km²) not profiled.
- Real-time Auto Preview uses a global `OnObjectPropertyChanged` listener — noisy on huge projects; toggle off if the editor feels laggy.

---

## Versioning plan

- `0.1.x`: bugfixes only (no schema breaks). Safe to pull.
- `0.2.x`: new features, may break asset schema with a migration note.
- `1.0.0`: stable API contract + multi-engine-version support + automated tests.
