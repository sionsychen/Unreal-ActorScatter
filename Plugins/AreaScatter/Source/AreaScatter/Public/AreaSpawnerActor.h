// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AreaSpawnerActor.generated.h"

class USplineComponent;
class URulePassAsset;
class URuleStackAsset;
class UAreaSpawnBatch;
class AAreaSpawnerActor;

#if WITH_EDITOR
DECLARE_DELEGATE_OneParam(FAreaSpawnerOpDelegate, AAreaSpawnerActor* /*Spawner*/);
#endif

/**
 * Designer-facing actor: paint a closed-loop spline, point at a RulePassAsset,
 * click Preview / Spawn / Clear in the details panel. The Editor module owns
 * the actual sampling / picking / spawning logic and binds the delegates below
 * at module startup.
 */
UCLASS(Blueprintable, ClassGroup = (AreaScatter), meta = (DisplayName = "Area Spawner"))
class AREASCATTER_API AAreaSpawnerActor : public AActor
{
	GENERATED_BODY()

public:
	AAreaSpawnerActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	TObjectPtr<USplineComponent> AreaSpline;

	/** Additional spawner actors whose splines join this region (boolean OR). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AreaScatter|Region")
	TArray<TObjectPtr<AAreaSpawnerActor>> AdditionalAreas;

	/** Spawner actors whose splines punch holes in this region (boolean SUBTRACT). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AreaScatter|Region")
	TArray<TObjectPtr<AAreaSpawnerActor>> HoleAreas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AreaScatter")
	TObjectPtr<URulePassAsset> Rule;

	/** Multi-pass stack. If set, takes precedence over Rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AreaScatter")
	TObjectPtr<URuleStackAsset> RuleStack;

	/** Latest committed batch. New Spawn replaces it (after auto-clearing previous). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter|Batch", Instanced)
	TObjectPtr<UAreaSpawnBatch> LastBatch;

	/** Auto-clear the previous batch's actors when Spawn() is called again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AreaScatter")
	bool bAutoClearPreviousBatch = true;

	/**
	 * When true, the panel re-runs Preview automatically (debounced ~300 ms) whenever this
	 * spawner's Rule / RuleStack / their passes / multi-area refs change. Useful for slider
	 * tuning. Off by default since each Preview re-samples the whole region.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AreaScatter|Override",
		meta = (ToolTip = "If on, panel re-runs Preview ~300ms after any Rule / spawner property edit. Off = manual Preview only."))
	bool bAutoPreview = false;

	/**
	 * If > 0, overrides every enabled pass's TargetCount for the next Spawn (Stack passes use the same value).
	 * 0 = use each rule's authored TargetCount.
	 * Designed for fast iteration without dirtying shared rule assets — edit it from the Slate panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AreaScatter|Override", meta = (ClampMin = "0",
		ToolTip = "If > 0, overrides every enabled pass's TargetCount for the next Spawn. 0 = use rule's value. Edit in panel for fast iteration."))
	int32 TargetCountOverride = 0;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "AreaScatter|Actions")
	void Preview();

	UFUNCTION(CallInEditor, Category = "AreaScatter|Actions")
	void Spawn();

	UFUNCTION(CallInEditor, Category = "AreaScatter|Actions")
	void ClearLastBatch();

	/** Editor module binds these to its orchestrator at module startup. */
	static FAreaSpawnerOpDelegate OnPreviewRequested;
	static FAreaSpawnerOpDelegate OnSpawnRequested;
	static FAreaSpawnerOpDelegate OnClearRequested;
#endif
};
