// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AAreaSpawnerActor;

namespace AreaScatter
{
	enum class EDiagSeverity : uint8 { Info, Warn, Error };

	struct AREASCATTEREDITOR_API FDiagItem
	{
		EDiagSeverity Severity = EDiagSeverity::Info;
		FString Message;
		FString Suggestion;
	};

	struct AREASCATTEREDITOR_API FDiagReport
	{
		TArray<FDiagItem> Items;
		bool HasErrors() const;
		FString ToMultiLineString() const;
	};

	/**
	 * Inspect the spawner + rule(s) and return a human-readable report. Designed
	 * to surface every "your spawn would fail because X" reason at edit time.
	 */
	AREASCATTEREDITOR_API FDiagReport DiagnoseSpawner(AAreaSpawnerActor* Spawner);
}
