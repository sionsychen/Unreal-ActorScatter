# Quick Start — AreaScatter

Get from "fresh project" to "spawned actors on a landscape" in 10 minutes.

## Prereqs

- UE 5.4
- Project compiles (`ActorScatterEditor Win64 Development` target)
- Plugin enabled: `ActorScatter.uproject` lists `AreaScatter` under Plugins

## 10-step smoke test

### 1. Open the project
Double-click `ActorScatter.uproject`. Wait for editor to finish loading.

**Verify:** `Window -> Developer Tools -> Output Log`. Search for `LogPluginManager`. You should see `AreaScatter` listed without any `ERROR` lines.

### 2. Open the panel
`Window -> Level Editor -> Area Scatter`.

**Verify:** A panel docks somewhere with the title "Area Scatter" and a help line saying "No Area Spawner selected...". If the menu entry is missing, the editor module didn't load — check `LogAreaScatter` in Output Log.

### 3. Make a level with a Landscape
Easiest path: `File -> New Level -> Open World`. That gives you a Landscape covering the origin.

Save to `/Content/TestMap.umap` (Ctrl+S).

### 4. First-time bootstrap (the panel does this for you)
Click the panel's **"Generate Sample Rules"** button.

**What it does:** writes three working `URulePassAsset` examples to `/Game/AreaScatter/Samples/`:

| Asset | Mode | What it shows |
|---|---|---|
| `DA_Sample_Grass_Spread` | Spread, Actors output | Even coverage of a target point per pick |
| `DA_Sample_Trees_Cluster` | Cluster, ISM output | 4-6 stands of static-mesh instances |
| `DA_Sample_Rocks_AvoidBuildings` | Spread, AABB avoid | Avoids actors with "Bldg/House/Wall" in their name |

You only need this once per project.

### 5. Drop a spawner
`Place Actors` panel -> search "Area Spawner" -> drag into the level near the origin.

Select it. In the viewport, edit the `AreaSpline` component:
- The default has 2 points. Hold **Alt** and drag a spline tangent to add points.
- Make a roughly square loop ~30m on a side (3000 units in UE).

**Verify:** the panel auto-binds. The header text should change to `Bound to: AreaSpawnerActor_X`.

### 6. Assign a rule
Select the spawner. In Details, set `Rule = DA_Sample_Grass_Spread`.

**Verify:** the panel's "Rule" section shows `Rule: DA_Sample_Grass_Spread [Spread -> Actors, target=200, slope<=45°, seed=42]`.

> **Note on Simple vs Detailed:** Open `DA_Sample_Grass_Spread` and you'll see ~12 fields under General/Sampling/Filter/Avoid/Spacing/Mode/Output. That's because `ConfigMode = Simple` is set at the top. Switch `ConfigMode` to `Detailed` and the asset reveals 22 more fields (cluster params, density curve/texture, attract bands, normal blend, scale ranges, sampling tuning). Values are preserved when you switch back; Simple just hides them. `DA_Sample_Trees_Cluster` ships with `Detailed` because cluster mode is a Detailed-only feature.

### 7. Diagnose
Click **Diagnose** in the panel.

**Verify:** the Diagnostics box shows a list of green checks plus a `TargetCountOverride: 0 (each pass uses its rule's authored TargetCount)` info line. If it shows red lines, fix them and click again.

Common red lines and fixes:
- `Spline has fewer than 3 points` -> add more spline points
- `Rule has no SpawnActorClass (Actors mode)` -> the rule is missing its actor class
- `Density texture source format is not BGRA8/G8` -> editor can't read the source bytes; re-import without sRGB compression or use grayscale

### 8. Preview
Click **Preview**.

**Expect:** ~200 small green spheres scatter inside the spline, with cyan arrows showing surface normals. Red boxes show avoid AABBs (none, in this sample). Blue boxes would show attract AABBs.

If you see zero spheres, look at Output Log. The picker logs `TIP:` messages telling you which filter ate everything.

### 9. Spawn
Click **Spawn**.

**Expect:**
- ~200 `TargetPoint` actors materialize
- World Outliner shows a new folder `AreaScatter/<spawner>_<batchid>/`
- Panel "Last Batch" section shows the GUID and actor count

### 9b. Quickly iterate count
Look at the panel's **Spawn Count Override** SpinBox (between Rule and Last Batch).

- Default `0` = use the rule's authored count (200).
- Set it to `50`. Click Spawn → only 50 actors spawn (the rule asset is **not** modified — the override lives on the spawner instance).
- Set it back to `0`. Click Spawn → 200 actors again.
- Drag the slider to live-tune; the value commits when you release. Ctrl+Z reverts.

This is how designers iterate density without polluting shared rule presets. Multiple spawners can share the same rule but each carry its own override.

### 10. Undo / Redo / Clear
- `Ctrl+Z` -> all 200 actors disappear
- `Ctrl+Y` -> all back
- Panel **Clear Last** -> destroys the batch (spawner + spline + rule remain)

## Phase 2 / 3 add-ons to try

After the 10 steps work:

**Cluster mode:**
- Open `DA_Sample_Trees_Cluster`. Notice `PickMode = Cluster`, `NumClusters = 5`, `ClusterRadius = 800`.
- Spawn it. You should see 5 distinct stands instead of even coverage.

**ISM output:**
- Same asset uses `OutputMode = ISM`. After Spawn, Outliner has one `ISMHost_<batchid>` actor — open it, you'll see a single ISMC component holding all instances.
- Performance: ~10x cheaper draw calls vs. Actors mode for identical meshes.

**Multi-spline + holes:**
- Drop a second `Area Spawner`. Paint a smaller spline (~10m loop) overlapping the first.
- On the **first** spawner, drag the second spawner into `HoleAreas`.
- Preview again. The smaller area is excluded.

**RuleStack (multi-pass):**
- Right-click in Content Browser -> Data Asset -> `URuleStackAsset` -> name it `DA_Stack_Forest`.
- Drop both `DA_Sample_Trees_Cluster` and `DA_Sample_Grass_Spread` into `Passes`.
- On the spawner, leave `Rule` blank, set `RuleStack = DA_Stack_Forest`.
- Spawn. Both passes share one batch — one `Ctrl+Z` reverts everything.

**Density (texture):**
- Import a 256×256 grayscale PNG (must NOT be sRGB; check Texture Editor: source format should read `G8` or `BGRA8`).
- Open `DA_Sample_Grass_Spread`, set `DensityTexture` to your texture, leave `DensityTextureChannel = 0` and `DensityTextureScale = 1`.
- Spawn. Black areas of the texture have no points; gray areas have ~half density.

**PointSet (PCG handoff):**
- Duplicate `DA_Sample_Grass_Spread`, change `OutputMode = PointSet`.
- Spawn. Nothing visible in the viewport, but `/Game/AreaScatter/Generated/` now has a `PS_<spawner>_<batch>.uasset`.
- Open the asset — it stores `Points` as `TArray<FAreaScatterPoint>`. Wire into PCG via a `Get Asset` element or a small BP that iterates and spawns whatever you want.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Panel menu missing | Editor module didn't load | Rebuild `ActorScatterEditor`. Check `Plugins/AreaScatter/Binaries/Win64` has both DLLs |
| Preview shows nothing, log says `cells_in_spline=0` | Spline isn't above the Landscape | Move spline so it overlaps the Landscape in XY |
| `cells_hit=0` despite `cells_in_spline>0` | Trace not reaching ground | Increase `TraceHalfHeight` (default 50000 = 500m) |
| `candidates=0` despite `cells_hit>0` | Slope filter or `RequireComponentClassContains` cuts everything | Set `RequireComponentClassContains = ""` (empty = any surface), raise `MaxSlopeDegrees` |
| 0 picks despite `candidates>200` | `MinPointDistance` too large for the area | Halve `MinPointDistance` |
| `SpawnActorClass not set` warning | Rule's `OutputMode = Actors` but no class assigned | Pick e.g. `TargetPoint` or your own BP |
| Spawn works but Ctrl+Z does nothing | Transaction wasn't captured | Re-build the editor target; verify `FScopedTransaction` is in `AreaScatterSpawner.cpp` |
| ISM mode: no instances visible | `SpawnStaticMesh` empty, or mesh with bad collision | Pick `/Engine/BasicShapes/Cube` to verify, then swap |
| PointSet mode: asset save failed | `/Game/AreaScatter/Generated/` not writable / source-controlled | Check Output Log for the exact error; usually permissions or P4 |

## Where to look next

- All available fields and their meaning: hover any property in the Details panel to see its tooltip
- Algorithm details: `Plugins/AreaScatter/Source/AreaScatterEditor/Private/Picking/AreaScatterPicker.cpp`
- Sampler logic: `Plugins/AreaScatter/Source/AreaScatterEditor/Private/Sampling/AreaScatterSampler.cpp`
- Output dispatch: `Plugins/AreaScatter/Source/AreaScatterEditor/Private/Spawning/AreaScatterSpawner.cpp`
