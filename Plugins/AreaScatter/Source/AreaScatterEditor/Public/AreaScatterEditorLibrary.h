// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AreaScatterEditorLibrary.generated.h"

class AAreaSpawnerActor;

/**
 * BlueprintCallable wrappers for the orchestration ops. Lets users build their
 * own EditorUtilityWidget (or call from Python) on top of the same code path
 * the Slate panel uses.
 */
UCLASS()
class AREASCATTEREDITOR_API UAreaScatterEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AreaScatter|Editor")
	static void Preview(AAreaSpawnerActor* Spawner);

	UFUNCTION(BlueprintCallable, Category = "AreaScatter|Editor")
	static void Spawn(AAreaSpawnerActor* Spawner);

	UFUNCTION(BlueprintCallable, Category = "AreaScatter|Editor")
	static void ClearLastBatch(AAreaSpawnerActor* Spawner);

	/** First selected AAreaSpawnerActor in the level, or nullptr. */
	UFUNCTION(BlueprintCallable, Category = "AreaScatter|Editor")
	static AAreaSpawnerActor* GetSelectedSpawner();

	/** Open (or focus) the Area Scatter panel under the Window menu. */
	UFUNCTION(BlueprintCallable, Category = "AreaScatter|Editor")
	static void OpenPanel();
};
