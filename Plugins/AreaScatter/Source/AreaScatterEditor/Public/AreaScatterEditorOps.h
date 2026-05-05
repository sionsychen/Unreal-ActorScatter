// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AAreaSpawnerActor;

namespace AreaScatter
{
	/** Run the full pipeline: validate -> sample -> pick -> draw debug points (does not spawn). */
	void HandlePreview(AAreaSpawnerActor* Spawner);

	/** Run the full pipeline: validate -> sample -> pick -> spawn -> create batch (transactional). */
	void HandleSpawn(AAreaSpawnerActor* Spawner);

	/** Clear last batch (transactional). */
	void HandleClear(AAreaSpawnerActor* Spawner);
}
