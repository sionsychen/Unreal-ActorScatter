// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AreaScatterTypes.h"

class UWorld;
class USplineComponent;
class URulePassAsset;
class AActor;

namespace AreaScatter
{
	/** 2D shoelace polygon area. */
	AREASCATTEREDITOR_API float PolygonArea2D(TArrayView<const FVector2D> Polygon);

	/** Ray-casting point-in-polygon (ignores Z). */
	AREASCATTEREDITOR_API bool PointInPolygon2D(const FVector2D& Point, TArrayView<const FVector2D> Polygon);

	/** Equally spaced samples along a closed-loop spline; XY only. */
	AREASCATTEREDITOR_API void SampleSplineOutline2D(const USplineComponent& Spline, int32 NumSamples, TArray<FVector2D>& OutPolygon);

	/** Result of one sampling pass. */
	struct AREASCATTEREDITOR_API FSamplingResult
	{
		TArray<FAreaScatterCandidate> Candidates;
		TArray<FAreaScatterAvoidShape> Avoids;
		TArray<FAreaScatterAvoidShape> Attracts;
		FBox2D Bounds2D = FBox2D(ForceInit);
		int32 GridResolution = 0;
		int32 CellsInside = 0;
		int32 CellsHit = 0;
	};

	struct AREASCATTEREDITOR_API FSamplingInput
	{
		UWorld* World = nullptr;
		const USplineComponent* Spline = nullptr;          // primary positive region
		TArray<const USplineComponent*> AdditionalSplines; // boolean OR
		TArray<const USplineComponent*> HoleSplines;       // boolean SUBTRACT
		const URulePassAsset* Rule = nullptr;
		AActor* IgnoreActor = nullptr;     // typically the spawner itself
	};

	/**
	 * Phase-1 sampler: walks a regular XY grid over the spline AABB, traces
	 * straight down through the visibility channel, fills FSamplingResult with
	 * candidates that pass the rule's slope/component filter. Also collects
	 * avoid AABBs for actors inside the spline that match the avoid patterns.
	 */
	AREASCATTEREDITOR_API bool RunSampler(const FSamplingInput& In, FSamplingResult& Out, FString& OutError);
}
