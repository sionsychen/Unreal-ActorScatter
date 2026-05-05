// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AreaScatterTypes.h"

class URulePassAsset;

namespace AreaScatter
{
	struct FSamplingResult;

	struct AREASCATTEREDITOR_API FPickingResult
	{
		TArray<FAreaScatterCandidate> Picked;
		int32 RequestedCount = 0;
		int32 CandidateCount = 0;
		float MinInterDist = -1.f;
		float MaxInterDist = -1.f;
		float AvgInterDist = -1.f;
	};

	/** Phase-1 picker: farthest-first with seeded RNG. Applies AABB-avoid post-sample.
	 *  TargetCountOverride > 0 wins over Rule.TargetCount (used by Spawner panel for fast iteration). */
	AREASCATTEREDITOR_API void RunPicker(
		const FSamplingResult& Sampling,
		const URulePassAsset& Rule,
		FPickingResult& Out,
		int32 TargetCountOverride = 0);
}
