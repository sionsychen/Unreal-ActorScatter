// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"

namespace AreaScatter
{
	/**
	 * Write three working URulePassAssets to /Game/AreaScatter/Samples/. Idempotent —
	 * existing assets are left alone unless bOverwrite is true. Returns the number of
	 * assets actually written.
	 */
	AREASCATTEREDITOR_API int32 GenerateSampleRules(bool bOverwrite = false);
}
