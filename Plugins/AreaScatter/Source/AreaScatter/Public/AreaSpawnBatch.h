// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AreaSpawnBatch.generated.h"

class AActor;
class URulePassAsset;
class AAreaSpawnerActor;

USTRUCT()
struct AREASCATTER_API FAreaSpawnBatchParams
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	int32 TargetCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	int32 PickedCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	float MaxSlopeDegrees = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	float AvoidDistance = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	float MinPointDistance = 0.f;
};

/**
 * Persistent record of a single Spawn invocation. Owned by the level (or the
 * spawner actor) so Undo / re-run / clear all have a canonical batch identity.
 */
UCLASS(BlueprintType, EditInlineNew)
class AREASCATTER_API UAreaSpawnBatch : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Batch")
	FGuid BatchId;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	FDateTime CreatedAt;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	TWeakObjectPtr<AAreaSpawnerActor> SourceSpawner;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	TSoftObjectPtr<URulePassAsset> RuleAsset;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	FAreaSpawnBatchParams Params;

	UPROPERTY(VisibleAnywhere, Category = "Batch")
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;
};
