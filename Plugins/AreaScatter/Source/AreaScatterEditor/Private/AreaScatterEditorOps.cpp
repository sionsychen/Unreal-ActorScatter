// Copyright (c) ActorScatter authors. All rights reserved.

#include "AreaScatterEditorOps.h"

#include "AreaScatterTypes.h"
#include "AreaSpawnBatch.h"
#include "AreaSpawnerActor.h"
#include "Picking/AreaScatterPicker.h"
#include "RulePassAsset.h"
#include "RuleStackAsset.h"
#include "Sampling/AreaScatterSampler.h"
#include "Spawning/AreaScatterSpawner.h"

#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#define LOCTEXT_NAMESPACE "AreaScatterOps"

namespace AreaScatter
{
	static void ToastError(const FText& Msg)
	{
		FMessageLog Log("AreaScatter");
		Log.Error(Msg);
		Log.Open(EMessageSeverity::Error);
	}

	static TArray<URulePassAsset*> ResolvePasses(const AAreaSpawnerActor& Spawner)
	{
		TArray<URulePassAsset*> Out;
		if (Spawner.RuleStack)
		{
			for (URulePassAsset* P : Spawner.RuleStack->Passes)
			{
				if (P && P->bEnabled) { Out.Add(P); }
			}
		}
		else if (Spawner.Rule && Spawner.Rule->bEnabled)
		{
			Out.Add(Spawner.Rule);
		}
		return Out;
	}

	static bool ValidateBasic(AAreaSpawnerActor* Spawner, FString& OutError)
	{
		if (!Spawner)
		{
			OutError = TEXT("No spawner.");
			return false;
		}
		if (!Spawner->AreaSpline || Spawner->AreaSpline->GetNumberOfSplinePoints() < 3)
		{
			OutError = TEXT("Spline needs at least 3 points (closed loop).");
			return false;
		}
		if (!Spawner->GetWorld())
		{
			OutError = TEXT("No world available (level not loaded?).");
			return false;
		}
		return true;
	}

	static void LogPickerTips(const URulePassAsset& Pass, const FSamplingResult& Sampling, const FPickingResult& Picks)
	{
		if (Picks.Picked.Num() > 0) { return; }
		const FString PassTag = Pass.DisplayName.IsEmpty() ? Pass.GetName() : Pass.DisplayName;
		if (Sampling.CellsInside == 0)
		{
			UE_LOG(LogAreaScatter, Warning,
				TEXT("TIP [%s]: 0 cells inside the spline polygon. Move the spline so it overlaps your geometry, "
					 "or check the spline points form a closed loop with the right XY footprint."),
				*PassTag);
			return;
		}
		if (Sampling.CellsHit == 0)
		{
			UE_LOG(LogAreaScatter, Warning,
				TEXT("TIP [%s]: %d cells inside but 0 ray hits. Increase TraceHalfHeight (currently %.0f) or "
					 "make sure the spawner Z is near the surface — traces span [SpawnerZ ± TraceHalfHeight]."),
				*PassTag, Sampling.CellsInside, Pass.TraceHalfHeight);
			return;
		}
		if (Sampling.Candidates.Num() == 0)
		{
			UE_LOG(LogAreaScatter, Warning,
				TEXT("TIP [%s]: %d ray hits but 0 candidates after slope/component filter. "
					 "Raise MaxSlopeDegrees (currently %.1f) or set RequireComponentClassContains='' to accept any surface (currently '%s')."),
				*PassTag, Sampling.CellsHit, Pass.MaxSlopeDegrees, *Pass.RequireComponentClassContains);
			return;
		}
		if (Picks.CandidateCount == 0)
		{
			UE_LOG(LogAreaScatter, Warning,
				TEXT("TIP [%s]: %d candidates were filtered out by avoid/attract/density. "
					 "Lower AvoidDistance (currently %.0f cm), widen attract band, or disable density filters."),
				*PassTag, Sampling.Candidates.Num(), Pass.AvoidDistance);
			return;
		}
		// Candidates exist but spacing was too tight.
		UE_LOG(LogAreaScatter, Warning,
			TEXT("TIP [%s]: 0 picks despite %d filtered candidates. MinPointDistance (%.0f cm) is likely too large for the area; halve it."),
			*PassTag, Picks.CandidateCount, Pass.MinPointDistance);
	}

	static bool RunSampleAndPick(
		AAreaSpawnerActor& Spawner,
		URulePassAsset& Pass,
		FSamplingResult& OutSampling,
		FPickingResult& OutPicks,
		FString& OutError)
	{
		FSamplingInput In;
		In.World = Spawner.GetWorld();
		In.Spline = Spawner.AreaSpline;
		In.Rule = &Pass;
		In.IgnoreActor = &Spawner;
		for (AAreaSpawnerActor* Other : Spawner.AdditionalAreas)
		{
			if (Other && Other != &Spawner && Other->AreaSpline)
			{
				In.AdditionalSplines.Add(Other->AreaSpline);
			}
		}
		for (AAreaSpawnerActor* Other : Spawner.HoleAreas)
		{
			if (Other && Other != &Spawner && Other->AreaSpline)
			{
				In.HoleSplines.Add(Other->AreaSpline);
			}
		}
		if (!RunSampler(In, OutSampling, OutError)) { return false; }
		RunPicker(OutSampling, Pass, OutPicks, Spawner.TargetCountOverride);
		LogPickerTips(Pass, OutSampling, OutPicks);
		return true;
	}

	void HandlePreview(AAreaSpawnerActor* Spawner)
	{
		FString Err;
		if (!ValidateBasic(Spawner, Err)) { ToastError(FText::FromString(Err)); return; }

		const TArray<URulePassAsset*> Passes = ResolvePasses(*Spawner);
		if (Passes.Num() == 0)
		{
			ToastError(LOCTEXT("NoRules", "Spawner has no enabled rule (assign Rule or RuleStack)."));
			return;
		}

		UWorld* World = Spawner->GetWorld();
		FlushPersistentDebugLines(World);

		// Distinct hue per pass for visual differentiation.
		const FColor Palette[] = {
			FColor::Green, FColor::Yellow, FColor::Cyan, FColor::Magenta,
			FColor::Orange, FColor::Turquoise, FColor::Emerald, FColor::Purple
		};

		int32 TotalPicked = 0, TotalCandidates = 0;
		for (int32 i = 0; i < Passes.Num(); ++i)
		{
			URulePassAsset* P = Passes[i];
			FSamplingResult Sampling;
			FPickingResult Picks;
			if (!RunSampleAndPick(*Spawner, *P, Sampling, Picks, Err))
			{
				ToastError(FText::FromString(Err));
				return;
			}
			TotalPicked += Picks.Picked.Num();
			TotalCandidates += Picks.CandidateCount;

			const FColor Color = Palette[i % UE_ARRAY_COUNT(Palette)];
			// Draw avoids in red (only once would suffice, but each pass has its own avoid set)
			for (const FAreaScatterAvoidShape& A : Sampling.Avoids)
			{
				DrawDebugBox(World, A.Center, A.Extent, FColor(255, 80, 80), true, -1.f, 0, 1.5f);
			}
			// Draw attracts in blue
			for (const FAreaScatterAvoidShape& A : Sampling.Attracts)
			{
				DrawDebugBox(World, A.Center, A.Extent, FColor(80, 80, 255), true, -1.f, 0, 1.5f);
			}
			// Picked points
			for (const FAreaScatterCandidate& C : Picks.Picked)
			{
				DrawDebugSphere(World, C.Location, 25.f, 8, Color, true, -1.f, 0, 1.f);
				DrawDebugDirectionalArrow(World, C.Location, C.Location + C.Normal * 80.f, 25.f, Color, true, -1.f, 0, 1.f);
			}
		}
		UE_LOG(LogAreaScatter, Display, TEXT("Preview: %d passes -> %d points (candidates after filter: %d)"),
			Passes.Num(), TotalPicked, TotalCandidates);
	}

	void HandleSpawn(AAreaSpawnerActor* Spawner)
	{
		FString Err;
		if (!ValidateBasic(Spawner, Err)) { ToastError(FText::FromString(Err)); return; }

		const TArray<URulePassAsset*> Passes = ResolvePasses(*Spawner);
		if (Passes.Num() == 0)
		{
			ToastError(LOCTEXT("NoRules", "Spawner has no enabled rule (assign Rule or RuleStack)."));
			return;
		}

		if (UWorld* W = Spawner->GetWorld()) { FlushPersistentDebugLines(W); }

		UAreaSpawnBatch* Batch = nullptr;
		int32 TotalPicked = 0;
		for (URulePassAsset* P : Passes)
		{
			FSamplingResult Sampling;
			FPickingResult Picks;
			if (!RunSampleAndPick(*Spawner, *P, Sampling, Picks, Err))
			{
				ToastError(FText::FromString(Err));
				return;
			}
			if (Picks.Picked.Num() == 0)
			{
				UE_LOG(LogAreaScatter, Warning, TEXT("Spawn: pass '%s' produced 0 points (skipped)."),
					P->DisplayName.IsEmpty() ? *P->GetName() : *P->DisplayName);
				continue;
			}
			Batch = SpawnBatch(*Spawner, *P, Picks, Batch);
			TotalPicked += Picks.Picked.Num();
		}

		if (!Batch || TotalPicked == 0)
		{
			ToastError(LOCTEXT("NoPicks", "No candidate points survived sampling/picking."));
		}
	}

	void HandleClear(AAreaSpawnerActor* Spawner)
	{
		if (!Spawner || !Spawner->LastBatch)
		{
			UE_LOG(LogAreaScatter, Display, TEXT("Clear: nothing to clear."));
			return;
		}
		ClearBatch(Spawner->LastBatch);
		Spawner->Modify();
		Spawner->LastBatch = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
