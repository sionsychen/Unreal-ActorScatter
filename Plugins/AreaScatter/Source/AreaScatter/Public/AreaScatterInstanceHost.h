// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AreaScatterInstanceHost.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Lightweight host actor owning one ISMC/HISMC per pass spawned via OutputMode = ISM/HISM.
 * Stored in the batch's SpawnedActors so Clear destroys it (with all instances).
 */
UCLASS(NotPlaceable, ClassGroup = (AreaScatter), meta = (DisplayName = "Area Scatter Instance Host"))
class AREASCATTER_API AAreaScatterInstanceHost : public AActor
{
	GENERATED_BODY()

public:
	AAreaScatterInstanceHost();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ScatterInstanceComponents;
};
