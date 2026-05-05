// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RuleStackAsset.generated.h"

class URulePassAsset;

/**
 * Ordered list of URulePassAsset. Each pass executes independently and writes
 * its spawned actors into the same UAreaSpawnBatch on the spawner — one Spawn
 * click can place trees + bushes + flowers as a single undoable unit.
 */
UCLASS(BlueprintType)
class AREASCATTER_API URuleStackAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuleStack")
	TArray<TObjectPtr<URulePassAsset>> Passes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuleStack")
	FString Description;
};
