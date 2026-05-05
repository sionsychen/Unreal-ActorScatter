// Copyright (c) ActorScatter authors. All rights reserved.

#include "Sampling/AreaScatterSampler.h"

#include "AreaScatterTypes.h"
#include "RulePassAsset.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "AreaScatterSampler"

namespace AreaScatter
{
	float PolygonArea2D(TArrayView<const FVector2D> Polygon)
	{
		const int32 N = Polygon.Num();
		if (N < 3) { return 0.f; }
		double S = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[(i + 1) % N];
			S += static_cast<double>(A.X) * static_cast<double>(B.Y) - static_cast<double>(B.X) * static_cast<double>(A.Y);
		}
		return static_cast<float>(FMath::Abs(S) * 0.5);
	}

	bool PointInPolygon2D(const FVector2D& P, TArrayView<const FVector2D> Polygon)
	{
		const int32 N = Polygon.Num();
		if (N < 3) { return false; }
		bool bInside = false;
		int32 j = N - 1;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[j];
			if (((A.Y > P.Y) != (B.Y > P.Y)) &&
				(P.X < (B.X - A.X) * (P.Y - A.Y) / ((B.Y - A.Y) + 1e-12) + A.X))
			{
				bInside = !bInside;
			}
			j = i;
		}
		return bInside;
	}

	void SampleSplineOutline2D(const USplineComponent& Spline, int32 NumSamples, TArray<FVector2D>& Out)
	{
		Out.Reset();
		const float Length = Spline.GetSplineLength();
		if (Length <= 0.f || NumSamples < 3)
		{
			return;
		}
		Out.Reserve(NumSamples);
		for (int32 i = 0; i < NumSamples; ++i)
		{
			const float D = Length * static_cast<float>(i) / static_cast<float>(NumSamples);
			const FVector P = Spline.GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
			Out.Emplace(P.X, P.Y);
		}
	}

	static bool MatchesAny(const FString& Name, const TArray<FString>& Patterns)
	{
		for (const FString& Pat : Patterns)
		{
			if (Pat.IsEmpty()) { continue; }
			if (Name.Contains(Pat, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	static void CollectShapes(
		UWorld& World,
		const TArray<FVector2D>& Polygon,
		const TArray<FString>& NamePatterns,
		const TArray<FName>& ClassNames,
		const TArray<FString>& ExcludePatterns,
		AActor* IgnoreActor,
		TArray<FAreaScatterAvoidShape>& Out)
	{
		const TSet<FName> ClassSet(ClassNames);
		const bool bHasName = NamePatterns.Num() > 0;
		const bool bHasClass = ClassSet.Num() > 0;
		if (!bHasName && !bHasClass) { return; }

		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			AActor* A = *It;
			if (!A || A == IgnoreActor) { continue; }

			const FVector Loc = A->GetActorLocation();
			if (!PointInPolygon2D(FVector2D(Loc.X, Loc.Y), Polygon)) { continue; }

			const FString Label = A->GetActorLabel();
			if (MatchesAny(Label, ExcludePatterns)) { continue; }

			const FName ClassName = A->GetClass() ? A->GetClass()->GetFName() : NAME_None;
			const bool bMatchName = bHasName && MatchesAny(Label, NamePatterns);
			const bool bMatchClass = bHasClass && ClassSet.Contains(ClassName);
			if (!bMatchName && !bMatchClass) { continue; }

			FVector Origin, Extent;
			A->GetActorBounds(/*OnlyCollidingComponents=*/false, Origin, Extent, /*IncludeFromChildActors=*/true);

			FAreaScatterAvoidShape Shape;
			Shape.Center = Origin;
			Shape.Extent = Extent;
			Shape.ActorName = FName(*Label);
			Shape.ClassName = ClassName;
			Shape.SourceActor = A;
			Out.Add(MoveTemp(Shape));
		}
	}

	static void CollectAvoids(
		UWorld& World,
		const TArray<FVector2D>& Polygon,
		const URulePassAsset& Rule,
		AActor* IgnoreActor,
		TArray<FAreaScatterAvoidShape>& Out)
	{
		CollectShapes(World, Polygon,
			Rule.AvoidNamePatterns, Rule.AvoidClassNames, Rule.ExcludeNamePatterns,
			IgnoreActor, Out);
	}

	static void CollectAttracts(
		UWorld& World,
		const TArray<FVector2D>& Polygon,
		const URulePassAsset& Rule,
		AActor* IgnoreActor,
		TArray<FAreaScatterAvoidShape>& Out)
	{
		// Attract uses Avoid's exclude list as a courtesy filter (skip ground covers).
		CollectShapes(World, Polygon,
			Rule.AttractNamePatterns, Rule.AttractClassNames, Rule.ExcludeNamePatterns,
			IgnoreActor, Out);
	}

	bool RunSampler(const FSamplingInput& In, FSamplingResult& Out, FString& OutError)
	{
		Out = FSamplingResult{};
		if (!In.World || !In.Spline || !In.Rule)
		{
			OutError = TEXT("Sampler input incomplete (world/spline/rule).");
			return false;
		}

		// Polygons: primary + additional positives, plus holes.
		TArray<TArray<FVector2D>> PositivePolys;
		TArray<TArray<FVector2D>> HolePolys;

		auto AddPolyFromSpline = [&In](const USplineComponent* S, TArray<TArray<FVector2D>>& Bucket)
		{
			if (!S) { return; }
			TArray<FVector2D> P;
			SampleSplineOutline2D(*S, In.Rule->OutlineSampleCount, P);
			if (P.Num() >= 3) { Bucket.Add(MoveTemp(P)); }
		};

		AddPolyFromSpline(In.Spline, PositivePolys);
		for (const USplineComponent* S : In.AdditionalSplines) { AddPolyFromSpline(S, PositivePolys); }
		for (const USplineComponent* S : In.HoleSplines) { AddPolyFromSpline(S, HolePolys); }

		if (PositivePolys.Num() == 0)
		{
			OutError = TEXT("No positive spline polygon (primary + additional). Draw a closed-loop spline.");
			return false;
		}

		FBox2D Bounds(ForceInit);
		for (const TArray<FVector2D>& Poly : PositivePolys)
		{
			for (const FVector2D& P : Poly) { Bounds += P; }
		}
		Out.Bounds2D = Bounds;

		auto IsInside = [&PositivePolys, &HolePolys](const FVector2D& P) -> bool
		{
			bool bInPositive = false;
			for (const TArray<FVector2D>& Poly : PositivePolys)
			{
				if (PointInPolygon2D(P, Poly)) { bInPositive = true; break; }
			}
			if (!bInPositive) { return false; }
			for (const TArray<FVector2D>& Poly : HolePolys)
			{
				if (PointInPolygon2D(P, Poly)) { return false; }
			}
			return true;
		};

		const int32 GridRes = FMath::Clamp(In.Rule->GridResolution, 8, 1024);
		Out.GridResolution = GridRes;
		const FVector2D Size = Bounds.GetSize();
		const float Dx = Size.X / static_cast<float>(GridRes);
		const float Dy = Size.Y / static_cast<float>(GridRes);

		// Avoid + attract lists. Use union polygon for actor-inside check (broad sweep).
		// Pass each positive poly individually so any inside-actor counts.
		auto CollectShapesUnion = [&](const TArray<FString>& NamePats, const TArray<FName>& ClsNames, TArray<FAreaScatterAvoidShape>& OutShapes)
		{
			TArray<FAreaScatterAvoidShape> All;
			for (const TArray<FVector2D>& Poly : PositivePolys)
			{
				CollectShapes(*In.World, Poly, NamePats, ClsNames, In.Rule->ExcludeNamePatterns, In.IgnoreActor, All);
			}
			// Dedupe by ActorName (cheap; same actor can be inside two unioned polys).
			TSet<FName> Seen;
			for (FAreaScatterAvoidShape& S : All)
			{
				if (Seen.Contains(S.ActorName)) { continue; }
				Seen.Add(S.ActorName);
				OutShapes.Add(MoveTemp(S));
			}
		};
		CollectShapesUnion(In.Rule->AvoidNamePatterns, In.Rule->AvoidClassNames, Out.Avoids);
		CollectShapesUnion(In.Rule->AttractNamePatterns, In.Rule->AttractClassNames, Out.Attracts);

		// Trace setup
		const FVector ActorLoc = In.IgnoreActor ? In.IgnoreActor->GetActorLocation() : FVector::ZeroVector;
		const float TopZ = ActorLoc.Z + In.Rule->TraceHalfHeight;
		const float BotZ = ActorLoc.Z - In.Rule->TraceHalfHeight;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(AreaScatterSampler), /*bTraceComplex=*/true);
		if (In.IgnoreActor) { Params.AddIgnoredActor(In.IgnoreActor); }

		const FString& RequireSubstr = In.Rule->RequireComponentClassContains;

		Out.Candidates.Reserve(GridRes * GridRes / 2);
		for (int32 iy = 0; iy < GridRes; ++iy)
		{
			for (int32 ix = 0; ix < GridRes; ++ix)
			{
				const float X = Bounds.Min.X + (ix + 0.5f) * Dx;
				const float Y = Bounds.Min.Y + (iy + 0.5f) * Dy;
				if (!IsInside(FVector2D(X, Y))) { continue; }
				++Out.CellsInside;

				FHitResult Hit;
				const bool bHit = In.World->LineTraceSingleByChannel(
					Hit,
					FVector(X, Y, TopZ),
					FVector(X, Y, BotZ),
					ECC_Visibility,
					Params);
				if (!bHit) { continue; }
				++Out.CellsHit;

				const FVector& Normal = Hit.ImpactNormal;
				const float NormalZ = FMath::Clamp(Normal.Z, -1.f, 1.f);
				const float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(NormalZ));
				if (SlopeDeg > In.Rule->MaxSlopeDegrees) { continue; }

				if (!RequireSubstr.IsEmpty())
				{
					const UPrimitiveComponent* Comp = Hit.GetComponent();
					const FString CompClass = Comp && Comp->GetClass() ? Comp->GetClass()->GetName() : FString();
					if (!CompClass.Contains(RequireSubstr, ESearchCase::IgnoreCase))
					{
						continue;
					}
				}

				FAreaScatterCandidate Cand;
				Cand.Location = Hit.ImpactPoint;
				Cand.Normal = Normal;
				Cand.SlopeDegrees = SlopeDeg;
				Out.Candidates.Add(MoveTemp(Cand));
			}
		}

		UE_LOG(LogAreaScatter, Display,
			TEXT("Sampler: positives=%d holes=%d bounds=(%.0f,%.0f)..(%.0f,%.0f) grid=%d inside=%d hit=%d candidates=%d avoids=%d attracts=%d"),
			PositivePolys.Num(), HolePolys.Num(),
			Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y,
			GridRes, Out.CellsInside, Out.CellsHit, Out.Candidates.Num(), Out.Avoids.Num(), Out.Attracts.Num());
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
