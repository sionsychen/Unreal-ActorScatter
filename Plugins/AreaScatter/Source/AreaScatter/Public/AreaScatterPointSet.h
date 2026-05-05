// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AreaScatterPointSet.generated.h"

USTRUCT(BlueprintType)
struct AREASCATTER_API FAreaScatterPoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Point")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Point")
	FVector Normal = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Point")
	float SlopeDegrees = 0.f;
};

/**
 * Persistent picks asset. Saved to /Game/AreaScatter/Generated/ when OutputMode == PointSet.
 * Use it as a static input for PCG graphs (via a small BP element) or any custom system that
 * needs to consume the spawn picks without spawning actors.
 */
UCLASS(BlueprintType)
class AREASCATTER_API UAreaScatterPointSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FString SourceSpawnerLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FString SourceRuleName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FGuid BatchId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FDateTime CreatedAt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	TArray<FAreaScatterPoint> Points;
};
