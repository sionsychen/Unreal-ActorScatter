// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class URulePassAsset;
class UAreaSpawnBatch;
class AAreaSpawnerActor;

namespace AreaScatter
{
	struct FPickingResult;

	/**
	 * Spawn picked points using the rule's OutputMode (Actors / ISM / HISM / PointSet).
	 * If ExistingBatch is null, a fresh batch is created (clearing the previous when bAutoClearPreviousBatch).
	 * If ExistingBatch is provided, output is appended to it (used by the stack runner so all passes
	 * ship as one undoable unit). For PointSet mode an asset is written to /Game/AreaScatter/Generated/.
	 */
	AREASCATTEREDITOR_API UAreaSpawnBatch* SpawnBatch(
		AAreaSpawnerActor& Spawner,
		const URulePassAsset& Rule,
		const FPickingResult& Picks,
		UAreaSpawnBatch* ExistingBatch = nullptr);

	/** Destroy all actors referenced by a batch (transactional). */
	AREASCATTEREDITOR_API int32 ClearBatch(UAreaSpawnBatch* Batch);
}
