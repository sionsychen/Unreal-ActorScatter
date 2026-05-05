// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "RulePassAsset.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class EAreaScatterConfigMode : uint8
{
	Simple   UMETA(DisplayName = "Simple (core fields only)"),
	Detailed UMETA(DisplayName = "Detailed (all knobs)"),
};

UENUM(BlueprintType)
enum class EAreaScatterPickMode : uint8
{
	Spread  UMETA(DisplayName = "Spread (farthest-first)"),
	Cluster UMETA(DisplayName = "Cluster (centers + radius)"),
};

UENUM(BlueprintType)
enum class EAreaSpawnOutputMode : uint8
{
	Actors    UMETA(DisplayName = "Actors (spawn an Actor per pick)"),
	ISM       UMETA(DisplayName = "Instanced Static Mesh"),
	HISM      UMETA(DisplayName = "Hierarchical ISM (LOD)"),
	PointSet  UMETA(DisplayName = "Point Set asset (PCG handoff)"),
};

UENUM(BlueprintType)
enum class EAreaScatterAvoidShape : uint8
{
	AABB    UMETA(DisplayName = "AABB (axis-aligned box, fast)"),
	Capsule UMETA(DisplayName = "Capsule (derived from bounds, better for long shapes)"),
	MeshSDF UMETA(DisplayName = "Mesh SDF (true distance to collision shape; requires collision)"),
};

/**
 * One atomic placement rule. A spawner can hold a single Rule for simple cases,
 * or a URuleStackAsset wrapping multiple of these for layered placements.
 *
 * Units: positions in cm (UE world units). Slope in degrees. Probabilities 0..1.
 *
 * Category order is forced via PrioritizeCategories so the most-edited groups
 * (General -> Output -> Filter -> Spacing -> Avoid) appear first; advanced groups
 * (Mode -> Cluster -> Density -> Attract -> Sampling) sink to the bottom.
 */
UCLASS(BlueprintType,
	meta = (PrioritizeCategories = "General Output Filter Spacing Avoid Attract Mode Cluster Density Sampling"))
class AREASCATTER_API URulePassAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// =====================================================================
	// General — always visible. Identity + visibility toggle + RNG seed.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (ToolTip = "Whether this pass runs. Disabled passes inside a RuleStack are skipped without removing them."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (ToolTip = "Optional human-readable label shown in logs and the panel. Example: \"Trees\", \"Grass\"."))
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (ToolTip = "Simple = ~12 core fields visible. Detailed = full 30+ knobs (cluster, density, attract, sampling tuning). Switch any time; values are preserved when hidden."))
	EAreaScatterConfigMode ConfigMode = EAreaScatterConfigMode::Simple;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (ToolTip = "RNG seed. Same seed + same inputs = same picks. Bump to re-roll layout deterministically."))
	int32 Seed = 42;

	// =====================================================================
	// Output — common knobs first (mode, target, count), Detailed transforms after.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (ToolTip = "Actors = spawn an Actor per pick. ISM/HISM = pack into one ISMC/HISMC component (cheap). PointSet = save picks as an asset for PCG/BP."))
	EAreaSpawnOutputMode OutputMode = EAreaSpawnOutputMode::Actors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (EditCondition = "OutputMode == EAreaSpawnOutputMode::Actors",
		ToolTip = "Actor class to spawn per pick. TargetPoint = simplest test class. Use your own BP for game props."))
	TSoftClassPtr<AActor> SpawnActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (EditCondition = "OutputMode == EAreaSpawnOutputMode::ISM || OutputMode == EAreaSpawnOutputMode::HISM",
		ToolTip = "Static mesh used by ISMC/HISMC. /Engine/BasicShapes/Cube to test, then swap to your asset."))
	TSoftObjectPtr<UStaticMesh> SpawnStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (ClampMin = "1",
		ToolTip = "How many points to pick. Real count may be lower if filters / density / spacing reject candidates. The Spawner panel can override this without dirtying the rule."))
	int32 TargetCount = 200;

	// ----- Detailed-only output micro-knobs --------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (EditCondition = "(OutputMode == EAreaSpawnOutputMode::ISM || OutputMode == EAreaSpawnOutputMode::HISM) && ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Optional per-slot material override for ISM/HISM. Leave empty to use the static mesh's defaults."))
	TArray<TSoftObjectPtr<UMaterialInterface>> InstanceMaterialOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Rotate the spawn so its Up axis follows the surface normal. Off = vertical regardless of slope."))
	bool bAlignToNormal = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (ClampMin = "0.0", ClampMax = "1.0",
		EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Lerp between vertical (0) and pure surface-normal (1). 0.5-0.7 looks natural for vegetation."))
	float NormalBlend = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Random yaw (degrees) applied per pick. (0, 360) = full random; (0, 0) = always face +X."))
	FVector2D RandomYawRange = FVector2D(0.f, 360.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Random tilt (degrees) on top of normal alignment. Small range (0, 5) breaks 'planted-in-rows' look."))
	FVector2D RandomTiltRange = FVector2D(0.f, 5.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Random Z offset (cm) per pick. Use to push slightly into the ground or float above (e.g. (-5, 5) jitters ±5cm)."))
	FVector2D ZOffsetRange = FVector2D(0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Uniform scale range per pick. (0.8, 1.5) = 0.8x..1.5x for natural variety. (1, 1) = no variation."))
	FVector2D UniformScaleRange = FVector2D(1.f, 1.f);

	// =====================================================================
	// Filter — slope is everyday; component-class filter is Detailed.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (ClampMin = "0.0", ClampMax = "89.0",
		ToolTip = "Discard candidates whose surface slope (deg) exceeds this. 30=gentle, 45=walkable, 60=steep cliffs."))
	float MaxSlopeDegrees = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Substring (case-insensitive) the hit component's class name must contain. \"Landscape\" = only on terrain. Empty = any surface."))
	FString RequireComponentClassContains = TEXT("Landscape");

	// =====================================================================
	// Spacing — single most-tuned knob after count.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spacing", meta = (ClampMin = "0.0",
		ToolTip = "Minimum distance (cm) between any two picked points. Larger = sparser. Example: grass=100, rocks=300, trees=500."))
	float MinPointDistance = 300.f;

	// =====================================================================
	// Avoid — pattern + distance are everyday; class/exclude/shape Detailed.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoid",
		meta = (ToolTip = "Substrings (case-insensitive) — actor label matches any => avoid. Example: [\"Bldg\",\"House\",\"Wall\"]."))
	TArray<FString> AvoidNamePatterns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoid", meta = (ClampMin = "0.0",
		ToolTip = "Minimum distance (cm) from a candidate to the nearest avoid shape. 0 = no avoid filter. Example: 500 = 5 m clearance."))
	float AvoidDistance = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoid",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Distance metric for the avoid test. Capsule fits long-thin actors (fences/pillars) better than AABB; AABB is fastest."))
	EAreaScatterAvoidShape AvoidShapeMode = EAreaScatterAvoidShape::AABB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoid",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Class short names — exact match => avoid. Use this when names are inconsistent but classes are stable."))
	TArray<FName> AvoidClassNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoid",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Whitelist substrings — actor label match here cancels avoidance. Use for ground-cover actors that shouldn't count (e.g. \"PCG_GrassLand\")."))
	TArray<FString> ExcludeNamePatterns;

	// =====================================================================
	// Mode — Detailed only (Simple defaults to Spread).
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Spread = even coverage (farthest-first). Cluster = stands / groups. Cluster mode is hidden in Simple — switch to Detailed to access it."))
	EAreaScatterPickMode PickMode = EAreaScatterPickMode::Spread;

	// =====================================================================
	// Cluster — Detailed AND PickMode == Cluster.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cluster",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed && PickMode == EAreaScatterPickMode::Cluster", EditConditionHides, ClampMin = "1",
		ToolTip = "How many cluster centers to seed. Centers picked via farthest-first; each then fills inwards within ClusterRadius."))
	int32 NumClusters = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cluster",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed && PickMode == EAreaScatterPickMode::Cluster", EditConditionHides, ClampMin = "0.0",
		ToolTip = "Radius (cm) around each cluster center within which the cluster fills. Example: 600 = 6m stand of trees."))
	float ClusterRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cluster",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed && PickMode == EAreaScatterPickMode::Cluster", EditConditionHides, ClampMin = "0.0",
		ToolTip = "Minimum distance (cm) between two cluster centers. Example: 1500 = stands at least 15m apart."))
	float CenterMinDistance = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cluster",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed && PickMode == EAreaScatterPickMode::Cluster", EditConditionHides, ClampMin = "0",
		ToolTip = "Per-cluster size lower bound. Both Min and Max > 0 = randomized per-cluster size in [Min, Max]; both 0 = even split (TargetCount / NumClusters)."))
	int32 ClusterSizeMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cluster",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed && PickMode == EAreaScatterPickMode::Cluster", EditConditionHides, ClampMin = "0",
		ToolTip = "Per-cluster size upper bound. See ClusterSizeMin."))
	int32 ClusterSizeMax = 0;

	// =====================================================================
	// Density — Detailed only. Toggle + curve, then texture sub-knobs.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Toggle to actually use DensityByAltitude. The curve struct has no clean empty sentinel."))
	bool bUseDensityByAltitude = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Optional accept-probability curve mapping altitude Z (cm) -> [0, 1]. Useful for biome bands."))
	FRuntimeFloatCurve DensityByAltitude;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Optional accept-probability multiplier sampled from a texture (UV maps to spline AABB). Source format must be BGRA8 or G8 — re-import without sRGB if needed."))
	TSoftObjectPtr<UTexture2D> DensityTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density", meta = (ClampMin = "0", ClampMax = "3",
		EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Channel sampled from DensityTexture. 0=R, 1=G, 2=B, 3=A. For grayscale (G8) any value reads the gray byte."))
	int32 DensityTextureChannel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density", meta = (ClampMin = "0.0",
		EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Multiplier on density texture sample before clamp. 1.0 = pure mask; 1.5 = brighten; 0.5 = halve density."))
	float DensityTextureScale = 1.f;

	// =====================================================================
	// Attract — symmetrical to Avoid. Patterns + distance band visible in Simple.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attract",
		meta = (ToolTip = "Substrings — actor label match nominates that actor as an attractor. Picks must lie inside [Min, Max] distance from at least one attractor. Empty + Min=Max=0 = no attract filter."))
	TArray<FString> AttractNamePatterns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attract", meta = (ClampMin = "0.0",
		ToolTip = "Min XY distance (cm) from a candidate to nearest attractor. 0 = no lower bound (touching is fine)."))
	float AttractDistanceMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attract", meta = (ClampMin = "0.0",
		ToolTip = "Max XY distance (cm) from a candidate to nearest attractor. 0 = no upper bound. Example: 500 = within 5m of water actors."))
	float AttractDistanceMax = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attract",
		meta = (EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Class short names that count as attractors. Same band rule as AttractNamePatterns."))
	TArray<FName> AttractClassNames;

	// =====================================================================
	// Sampling — debug / large-region tuning. Detailed only.
	// =====================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "8", ClampMax = "1024",
		EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "XY grid resolution along the longer side of the spline AABB. 128 = good default; 256 for fine detail; 64 for huge regions."))
	int32 GridResolution = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "8", ClampMax = "512",
		EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Number of samples used to approximate the spline as a 2D polygon. 64 is plenty for typical splines; raise for very curvy ones."))
	int32 OutlineSampleCount = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "100.0",
		EditCondition = "ConfigMode == EAreaScatterConfigMode::Detailed", EditConditionHides,
		ToolTip = "Vertical half-distance of the down-trace, in cm. Default 50000 = 500m up and 500m down from the spawner Z. Increase for floating regions."))
	float TraceHalfHeight = 50000.f;
};
