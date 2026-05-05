// Copyright (c) ActorScatter authors. All rights reserved.

#include "AreaScatterDiagnostics.h"

#include "AreaScatterTypes.h"
#include "AreaSpawnerActor.h"
#include "RulePassAsset.h"
#include "RuleStackAsset.h"

#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

namespace AreaScatter
{
	bool FDiagReport::HasErrors() const
	{
		for (const FDiagItem& I : Items)
		{
			if (I.Severity == EDiagSeverity::Error) { return true; }
		}
		return false;
	}

	FString FDiagReport::ToMultiLineString() const
	{
		FString Out;
		for (const FDiagItem& I : Items)
		{
			const TCHAR* Tag = TEXT("[ok]   ");
			switch (I.Severity)
			{
				case EDiagSeverity::Warn:  Tag = TEXT("[warn] "); break;
				case EDiagSeverity::Error: Tag = TEXT("[err]  "); break;
				default: break;
			}
			Out += FString::Printf(TEXT("%s%s\n"), Tag, *I.Message);
			if (!I.Suggestion.IsEmpty())
			{
				Out += FString::Printf(TEXT("       fix: %s\n"), *I.Suggestion);
			}
		}
		return Out.IsEmpty() ? TEXT("(no checks ran)") : Out;
	}

	static void Add(FDiagReport& R, EDiagSeverity Sev, FString Msg, FString Fix = FString())
	{
		R.Items.Add({Sev, MoveTemp(Msg), MoveTemp(Fix)});
	}

	static void DiagnoseRule(URulePassAsset* P, FDiagReport& R, const FString& Tag)
	{
		if (!P) { Add(R, EDiagSeverity::Error, FString::Printf(TEXT("%s: pass is null"), *Tag)); return; }
		if (!P->bEnabled)
		{
			Add(R, EDiagSeverity::Info, FString::Printf(TEXT("%s '%s': disabled (skipped)"), *Tag, *P->GetName()));
			return;
		}

		switch (P->OutputMode)
		{
			case EAreaSpawnOutputMode::Actors:
				if (P->SpawnActorClass.IsNull())
				{
					Add(R, EDiagSeverity::Error,
						FString::Printf(TEXT("%s '%s': OutputMode=Actors but SpawnActorClass is empty"), *Tag, *P->GetName()),
						TEXT("Pick TargetPoint or your own BP for SpawnActorClass."));
				}
				break;
			case EAreaSpawnOutputMode::ISM:
			case EAreaSpawnOutputMode::HISM:
				if (P->SpawnStaticMesh.IsNull())
				{
					Add(R, EDiagSeverity::Error,
						FString::Printf(TEXT("%s '%s': OutputMode=%s but SpawnStaticMesh is empty"),
							*Tag, *P->GetName(),
							P->OutputMode == EAreaSpawnOutputMode::HISM ? TEXT("HISM") : TEXT("ISM")),
						TEXT("Pick /Engine/BasicShapes/Cube to test, then swap to your asset."));
				}
				break;
			case EAreaSpawnOutputMode::PointSet:
				Add(R, EDiagSeverity::Info,
					FString::Printf(TEXT("%s '%s': PointSet mode (no in-level actors; writes asset to /Game/AreaScatter/Generated/)"), *Tag, *P->GetName()));
				break;
		}

		if (P->TargetCount <= 0)
		{
			Add(R, EDiagSeverity::Warn,
				FString::Printf(TEXT("%s '%s': TargetCount<=0"), *Tag, *P->GetName()),
				TEXT("Try TargetCount=200 to start."));
		}
		if (P->MaxSlopeDegrees <= 0.f)
		{
			Add(R, EDiagSeverity::Warn,
				FString::Printf(TEXT("%s '%s': MaxSlopeDegrees<=0 (everything will be filtered)"), *Tag, *P->GetName()),
				TEXT("Try MaxSlopeDegrees=30 (gentle slope) or 60 (steep)."));
		}
		if (P->GridResolution < 8 || P->GridResolution > 1024)
		{
			Add(R, EDiagSeverity::Warn,
				FString::Printf(TEXT("%s '%s': GridResolution out of [8, 1024]"), *Tag, *P->GetName()),
				TEXT("Default 128 is fine for most areas; raise for fine detail, lower for huge regions."));
		}
		if (P->PickMode == EAreaScatterPickMode::Cluster)
		{
			if (P->NumClusters <= 0)
			{
				Add(R, EDiagSeverity::Error,
					FString::Printf(TEXT("%s '%s': Cluster mode but NumClusters<=0"), *Tag, *P->GetName()),
					TEXT("Try NumClusters=4 with ClusterRadius=600."));
			}
		}
		if (UTexture2D* Tex = P->DensityTexture.LoadSynchronous())
		{
			const ETextureSourceFormat F = Tex->Source.GetFormat();
			if (F != TSF_BGRA8 && F != TSF_G8)
			{
				Add(R, EDiagSeverity::Warn,
					FString::Printf(TEXT("%s '%s': DensityTexture source format is not BGRA8/G8 (got %d)"), *Tag, *P->GetName(), static_cast<int32>(F)),
					TEXT("Re-import the texture; for grayscale density use 8-bit gray PNG."));
			}
		}
		if (P->AvoidShapeMode == EAreaScatterAvoidShape::MeshSDF && P->AvoidDistance > 0.f)
		{
			Add(R, EDiagSeverity::Info,
				FString::Printf(TEXT("%s '%s': AvoidShapeMode = MeshSDF — avoid actors must have collision; actors without bodies fall back silently."), *Tag, *P->GetName()));
		}
	}

	static bool DetectCycle(AAreaSpawnerActor* Start)
	{
		TSet<AAreaSpawnerActor*> Seen;
		TArray<AAreaSpawnerActor*> Stack;
		Stack.Add(Start);
		while (Stack.Num() > 0)
		{
			AAreaSpawnerActor* A = Stack.Pop();
			if (!A) { continue; }
			if (Seen.Contains(A)) { return true; }
			Seen.Add(A);
			for (AAreaSpawnerActor* Child : A->AdditionalAreas) { if (Child && Child != A) { Stack.Add(Child); } }
			for (AAreaSpawnerActor* Child : A->HoleAreas)        { if (Child && Child != A) { Stack.Add(Child); } }
		}
		return false;
	}

	FDiagReport DiagnoseSpawner(AAreaSpawnerActor* S)
	{
		FDiagReport R;
		if (!S)
		{
			Add(R, EDiagSeverity::Error, TEXT("No Area Spawner selected."),
				TEXT("Select an Area Spawner in the level, or click Refresh."));
			return R;
		}

		Add(R, EDiagSeverity::Info, FString::Printf(TEXT("Spawner: %s"), *S->GetActorLabel()));

		if (S->TargetCountOverride > 0)
		{
			Add(R, EDiagSeverity::Info,
				FString::Printf(TEXT("TargetCountOverride: %d (overriding every enabled pass's TargetCount)"), S->TargetCountOverride));
		}
		else
		{
			Add(R, EDiagSeverity::Info, TEXT("TargetCountOverride: 0 (each pass uses its rule's authored TargetCount)"));
		}

		// Spline
		if (!S->AreaSpline)
		{
			Add(R, EDiagSeverity::Error, TEXT("Spawner has no AreaSpline component."),
				TEXT("Re-create the spawner; the SplineComponent is auto-created."));
		}
		else
		{
			const int32 N = S->AreaSpline->GetNumberOfSplinePoints();
			if (N < 3)
			{
				Add(R, EDiagSeverity::Error,
					FString::Printf(TEXT("AreaSpline has %d points (need >=3 for a polygon)"), N),
					TEXT("Select the spawner; in the viewport Alt+drag spline tangents to add points."));
			}
			else
			{
				Add(R, EDiagSeverity::Info, FString::Printf(TEXT("AreaSpline: %d points, length %.0f cm"), N, S->AreaSpline->GetSplineLength()));
			}
			if (!S->AreaSpline->IsClosedLoop())
			{
				Add(R, EDiagSeverity::Warn, TEXT("AreaSpline is not closed-loop."),
					TEXT("Sampler treats it as closed regardless, but designer-intent should mark it closed."));
			}
		}

		// Multi-area
		for (AAreaSpawnerActor* Other : S->AdditionalAreas)
		{
			if (!Other) { Add(R, EDiagSeverity::Warn, TEXT("AdditionalAreas has a null entry."), TEXT("Remove or fix the slot.")); continue; }
			if (Other == S) { Add(R, EDiagSeverity::Warn, TEXT("AdditionalAreas references self."), TEXT("Remove the slot.")); continue; }
			if (!Other->AreaSpline || Other->AreaSpline->GetNumberOfSplinePoints() < 3)
			{
				Add(R, EDiagSeverity::Warn,
					FString::Printf(TEXT("AdditionalAreas '%s' has no usable spline."), *Other->GetActorLabel()),
					TEXT("Either fix that spawner's spline or remove it from this list."));
			}
		}
		for (AAreaSpawnerActor* Other : S->HoleAreas)
		{
			if (!Other) { Add(R, EDiagSeverity::Warn, TEXT("HoleAreas has a null entry."), TEXT("Remove or fix the slot.")); continue; }
			if (Other == S) { Add(R, EDiagSeverity::Warn, TEXT("HoleAreas references self."), TEXT("Remove the slot.")); continue; }
			if (!Other->AreaSpline || Other->AreaSpline->GetNumberOfSplinePoints() < 3)
			{
				Add(R, EDiagSeverity::Warn,
					FString::Printf(TEXT("HoleAreas '%s' has no usable spline."), *Other->GetActorLabel()),
					TEXT("Either fix that spawner's spline or remove it from this list."));
			}
		}
		if (DetectCycle(S))
		{
			Add(R, EDiagSeverity::Error, TEXT("Cycle detected in AdditionalAreas/HoleAreas references."),
				TEXT("A spawner cannot reference itself transitively. Check the chain."));
		}

		// Rules
		if (S->RuleStack)
		{
			Add(R, EDiagSeverity::Info, FString::Printf(TEXT("RuleStack: %s (%d passes)"), *S->RuleStack->GetName(), S->RuleStack->Passes.Num()));
			if (S->RuleStack->Passes.Num() == 0)
			{
				Add(R, EDiagSeverity::Error, TEXT("RuleStack has zero passes."), TEXT("Add at least one URulePassAsset to its Passes array."));
			}
			for (int32 i = 0; i < S->RuleStack->Passes.Num(); ++i)
			{
				DiagnoseRule(S->RuleStack->Passes[i], R, FString::Printf(TEXT("Pass[%d]"), i));
			}
		}
		else if (S->Rule)
		{
			DiagnoseRule(S->Rule, R, TEXT("Rule"));
		}
		else
		{
			Add(R, EDiagSeverity::Error, TEXT("No Rule or RuleStack assigned."),
				TEXT("Click 'Generate Sample Rules' to bootstrap, then assign one."));
		}

		// Final OK summary
		if (!R.HasErrors())
		{
			Add(R, EDiagSeverity::Info, TEXT("All required checks passed. Click Preview, then Spawn."));
		}
		return R;
	}
}
