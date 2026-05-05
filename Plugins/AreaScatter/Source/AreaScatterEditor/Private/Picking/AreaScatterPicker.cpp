// Copyright (c) ActorScatter authors. All rights reserved.

#include "Picking/AreaScatterPicker.h"

#include "AreaScatterTypes.h"
#include "RulePassAsset.h"
#include "Sampling/AreaScatterSampler.h"
#include "Components/PrimitiveComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TextureResource.h"

namespace AreaScatter
{
	static float AABBDistanceXY(const FVector& P, const FAreaScatterAvoidShape& A)
	{
		const float Dx = FMath::Max(0.f, FMath::Abs(P.X - A.Center.X) - A.Extent.X);
		const float Dy = FMath::Max(0.f, FMath::Abs(P.Y - A.Center.Y) - A.Extent.Y);
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	}

	/**
	 * 2D capsule from an actor AABB: longer-axis becomes capsule axis, radius =
	 * shorter half-extent. Distance is "point to capsule axis segment minus radius",
	 * clamped at 0 when inside.
	 */
	static float CapsuleDistanceXY(const FVector& P, const FAreaScatterAvoidShape& A)
	{
		const FVector2D C(A.Center.X, A.Center.Y);
		const FVector2D Pt(P.X, P.Y);
		const float Ex = A.Extent.X;
		const float Ey = A.Extent.Y;
		const bool bAxisX = Ex >= Ey;
		const float HalfLen = FMath::Max(0.f, (bAxisX ? Ex : Ey) - (bAxisX ? Ey : Ex));
		const float Radius = bAxisX ? Ey : Ex;
		const FVector2D Axis = bAxisX ? FVector2D(1.f, 0.f) : FVector2D(0.f, 1.f);
		const FVector2D Local = Pt - C;
		const float TRaw = FVector2D::DotProduct(Local, Axis);
		const float T = FMath::Clamp(TRaw, -HalfLen, HalfLen);
		const FVector2D Closest = C + Axis * T;
		const float Dist = FVector2D::Distance(Pt, Closest);
		return FMath::Max(0.f, Dist - Radius);
	}

	static float DistanceToShape(const FVector& P, const FAreaScatterAvoidShape& A, EAreaScatterAvoidShape Mode)
	{
		return Mode == EAreaScatterAvoidShape::Capsule ? CapsuleDistanceXY(P, A) : AABBDistanceXY(P, A);
	}

	/**
	 * 3D shortest distance from candidate to actor's collision shapes via Chaos
	 * `FBodyInstance::GetSquaredDistanceToBody`. Iterates primitive components,
	 * takes the min. Returns +inf if no body has collision.
	 *
	 * Caller is expected to have done an AABB pre-reject already (see picker
	 * filter loop) — this routine is the expensive step.
	 */
	static float MeshSDFDistance(const FVector& P, const FAreaScatterAvoidShape& A)
	{
		AActor* Actor = A.SourceActor.Get();
		if (!Actor) { return TNumericLimits<float>::Max(); }

		float Best = TNumericLimits<float>::Max();
		TArray<UPrimitiveComponent*> Prims;
		Actor->GetComponents<UPrimitiveComponent>(Prims);
		for (UPrimitiveComponent* Prim : Prims)
		{
			if (!Prim) { continue; }
			FBodyInstance* Body = Prim->GetBodyInstance();
			if (!Body || !Body->IsValidBodyInstance()) { continue; }
			float SqDist = 0.f;
			FVector Closest;
			if (Body->GetSquaredDistanceToBody(P, SqDist, Closest))
			{
				const float D = FMath::Sqrt(SqDist);
				if (D < Best) { Best = D; }
			}
		}
		return Best;
	}

	static float DistanceXY(const FVector& A, const FVector& B)
	{
		const float Dx = A.X - B.X;
		const float Dy = A.Y - B.Y;
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	}

	static void SeededShuffle(TArray<FAreaScatterCandidate>& InOut, FRandomStream& Rand)
	{
		for (int32 i = InOut.Num() - 1; i > 0; --i)
		{
			const int32 j = Rand.RandRange(0, i);
			if (j != i) { InOut.Swap(i, j); }
		}
	}

	// ---- Density texture sampling ------------------------------------------

	/**
	 * Reads a single channel from a UTexture2D's source mip. Editor-time only:
	 * relies on Source being available, so the texture must not be cooked-only.
	 */
	struct FDensityTextureView
	{
		TArray64<uint8> Pixels;
		int32 Width = 0;
		int32 Height = 0;
		ETextureSourceFormat Format = TSF_Invalid;

		bool Open(UTexture2D* Tex)
		{
			if (!Tex) { return false; }
			FTextureSource& Src = Tex->Source;
			if (!Src.IsValid()) { return false; }
			Width = Src.GetSizeX();
			Height = Src.GetSizeY();
			Format = Src.GetFormat();
			if (Format != TSF_BGRA8 && Format != TSF_G8) { return false; }
			return Src.GetMipData(Pixels, 0);
		}

		float Sample(int32 Channel, float U, float V) const
		{
			if (Width == 0 || Height == 0 || Pixels.Num() == 0) { return 1.f; }
			const int32 X = FMath::Clamp(static_cast<int32>(U * Width), 0, Width - 1);
			const int32 Y = FMath::Clamp(static_cast<int32>(V * Height), 0, Height - 1);
			if (Format == TSF_G8)
			{
				return static_cast<float>(Pixels[Y * Width + X]) / 255.f;
			}
			const int64 Idx = (static_cast<int64>(Y) * Width + X) * 4;
			if (!Pixels.IsValidIndex(Idx + 3)) { return 1.f; }
			// BGRA channel order: user channel 0..3 maps R/G/B/A.
			int32 ByteIdx = 0;
			switch (Channel) { case 0: ByteIdx = 2; break; case 1: ByteIdx = 1; break; case 2: ByteIdx = 0; break; default: ByteIdx = 3; break; }
			return static_cast<float>(Pixels[Idx + ByteIdx]) / 255.f;
		}
	};

	static TArray<FAreaScatterCandidate> ApplyFilters(
		const FSamplingResult& Sampling,
		const URulePassAsset& Rule,
		FRandomStream& Rand)
	{
		TArray<FAreaScatterCandidate> Pool;
		Pool.Reserve(Sampling.Candidates.Num());
		const float AvoidDist = Rule.AvoidDistance;
		const bool bUseAttract = Sampling.Attracts.Num() > 0 &&
			(Rule.AttractDistanceMin > 0.f || Rule.AttractDistanceMax > 0.f);
		const float AttractMin = Rule.AttractDistanceMin;
		const float AttractMax = Rule.AttractDistanceMax;

		// Density texture
		FDensityTextureView TexView;
		const bool bUseTex = TexView.Open(Rule.DensityTexture.LoadSynchronous());
		const FVector2D BMin = Sampling.Bounds2D.Min;
		const FVector2D BSize = Sampling.Bounds2D.GetSize();
		const float TexScale = FMath::Max(0.f, Rule.DensityTextureScale);

		const FRichCurve* AltCurve = (Rule.bUseDensityByAltitude) ? Rule.DensityByAltitude.GetRichCurveConst() : nullptr;
		const bool bUseAlt = AltCurve != nullptr && AltCurve->GetNumKeys() > 0;

		for (const FAreaScatterCandidate& C : Sampling.Candidates)
		{
			// Avoid (AABB / Capsule / MeshSDF). MeshSDF gets an AABB pre-reject.
			if (AvoidDist > 0.f)
			{
				bool bAvoided = false;
				for (const FAreaScatterAvoidShape& Shape : Sampling.Avoids)
				{
					if (Rule.AvoidShapeMode == EAreaScatterAvoidShape::MeshSDF)
					{
						// Cheap reject: AABB+padding test first
						const float AABB = AABBDistanceXY(C.Location, Shape);
						if (AABB > AvoidDist) { continue; }
						const float SDF = MeshSDFDistance(C.Location, Shape);
						if (SDF < AvoidDist) { bAvoided = true; break; }
					}
					else
					{
						if (DistanceToShape(C.Location, Shape, Rule.AvoidShapeMode) < AvoidDist) { bAvoided = true; break; }
					}
				}
				if (bAvoided) { continue; }
			}

			// Attract band (always uses AABB / Capsule — MeshSDF would need both directions, overkill for Phase 4).
			if (bUseAttract)
			{
				const EAreaScatterAvoidShape AttractShape =
					(Rule.AvoidShapeMode == EAreaScatterAvoidShape::Capsule)
						? EAreaScatterAvoidShape::Capsule
						: EAreaScatterAvoidShape::AABB;
				float NearestAttract = TNumericLimits<float>::Max();
				for (const FAreaScatterAvoidShape& Shape : Sampling.Attracts)
				{
					const float D = DistanceToShape(C.Location, Shape, AttractShape);
					if (D < NearestAttract) { NearestAttract = D; }
				}
				if (NearestAttract < AttractMin) { continue; }
				if (AttractMax > 0.f && NearestAttract > AttractMax) { continue; }
			}

			// Density (curve * texture). Roll once per candidate for repeatability.
			float Accept = 1.f;
			if (bUseAlt)
			{
				Accept *= FMath::Clamp(AltCurve->Eval(static_cast<float>(C.Location.Z)), 0.f, 1.f);
			}
			if (bUseTex && BSize.X > 0.f && BSize.Y > 0.f)
			{
				const float U = (static_cast<float>(C.Location.X) - BMin.X) / BSize.X;
				const float V = (static_cast<float>(C.Location.Y) - BMin.Y) / BSize.Y;
				const float TexVal = TexView.Sample(Rule.DensityTextureChannel, U, V);
				Accept *= FMath::Clamp(TexVal * TexScale, 0.f, 1.f);
			}
			if (Accept < 1.f && Rand.FRand() > Accept) { continue; }

			Pool.Add(C);
		}
		return Pool;
	}

	static void PickSpread(
		TArray<FAreaScatterCandidate>& Pool,
		int32 Target,
		float MinDist,
		FRandomStream& Rand,
		TArray<FAreaScatterCandidate>& Out)
	{
		if (Pool.Num() == 0) { return; }
		SeededShuffle(Pool, Rand);
		Out.Add(Pool[0]);
		Pool.RemoveAt(0, EAllowShrinking::No);

		while (Out.Num() < Target && Pool.Num() > 0)
		{
			int32 BestIdx = INDEX_NONE;
			float BestScore = -1.f;
			for (int32 i = 0; i < Pool.Num(); ++i)
			{
				const FVector& Loc = Pool[i].Location;
				float MinD = TNumericLimits<float>::Max();
				for (const FAreaScatterCandidate& P : Out)
				{
					const float D = DistanceXY(Loc, P.Location);
					if (D < MinD) { MinD = D; }
				}
				if (MinD < MinDist) { continue; }
				if (MinD > BestScore) { BestScore = MinD; BestIdx = i; }
			}
			if (BestIdx == INDEX_NONE) { break; }
			Out.Add(Pool[BestIdx]);
			Pool.RemoveAt(BestIdx, EAllowShrinking::No);
		}
	}

	static void PickCluster(
		TArray<FAreaScatterCandidate>& Pool,
		const URulePassAsset& Rule,
		int32 EffectiveTarget,
		FRandomStream& Rand,
		TArray<FAreaScatterCandidate>& Out)
	{
		if (Pool.Num() == 0) { return; }
		SeededShuffle(Pool, Rand);

		const int32 Target = FMath::Max(1, EffectiveTarget);
		const int32 NumClusters = FMath::Max(1, Rule.NumClusters);
		const float CenterMinDist = Rule.CenterMinDistance;
		const float ClusterRadius = Rule.ClusterRadius;
		const float MinDist = Rule.MinPointDistance;

		// Step 1: farthest-first centers
		TArray<FAreaScatterCandidate> Centers;
		Centers.Reserve(NumClusters);
		Centers.Add(Pool[0]);
		Pool.RemoveAt(0, EAllowShrinking::No);
		while (Centers.Num() < NumClusters && Pool.Num() > 0)
		{
			int32 BestIdx = INDEX_NONE;
			float BestScore = -1.f;
			for (int32 i = 0; i < Pool.Num(); ++i)
			{
				float MinD = TNumericLimits<float>::Max();
				for (const FAreaScatterCandidate& P : Centers)
				{
					const float D = DistanceXY(Pool[i].Location, P.Location);
					if (D < MinD) { MinD = D; }
				}
				if (MinD < CenterMinDist) { continue; }
				if (MinD > BestScore) { BestScore = MinD; BestIdx = i; }
			}
			if (BestIdx == INDEX_NONE) { break; }
			Centers.Add(Pool[BestIdx]);
			Pool.RemoveAt(BestIdx, EAllowShrinking::No);
		}

		// Step 2: per-cluster targets
		const bool bRandomSize = Rule.ClusterSizeMin > 0 && Rule.ClusterSizeMax > 0;
		TArray<int32> ClusterTargets;
		ClusterTargets.Reserve(Centers.Num());
		if (bRandomSize)
		{
			const int32 Lo = FMath::Max(1, Rule.ClusterSizeMin);
			const int32 Hi = FMath::Max(Lo, Rule.ClusterSizeMax);
			for (int32 i = 0; i < Centers.Num(); ++i)
			{
				ClusterTargets.Add(Rand.RandRange(Lo, Hi));
			}
		}
		else
		{
			const int32 Per = FMath::Max(1, Target / FMath::Max(1, Centers.Num()));
			for (int32 i = 0; i < Centers.Num(); ++i) { ClusterTargets.Add(Per); }
		}

		// Centers count toward picked
		Out.Append(Centers);

		// Step 3: fill clusters within radius respecting MinDist
		for (int32 ci = 0; ci < Centers.Num(); ++ci)
		{
			if (Out.Num() >= Target) { break; }
			const FVector& CenterLoc = Centers[ci].Location;
			const int32 SizeTarget = ClusterTargets[ci];
			int32 Added = 1; // center counts

			TArray<int32> InRadius;
			InRadius.Reserve(Pool.Num());
			for (int32 i = 0; i < Pool.Num(); ++i)
			{
				if (DistanceXY(Pool[i].Location, CenterLoc) <= ClusterRadius)
				{
					InRadius.Add(i);
				}
			}
			// Shuffle indices via Fisher-Yates
			for (int32 k = InRadius.Num() - 1; k > 0; --k)
			{
				const int32 j = Rand.RandRange(0, k);
				if (j != k) { Swap(InRadius[k], InRadius[j]); }
			}

			TArray<int32> Consumed;
			for (int32 PoolIdx : InRadius)
			{
				if (Added >= SizeTarget || Out.Num() >= Target) { break; }
				const FVector& Loc = Pool[PoolIdx].Location;
				bool bOk = true;
				for (const FAreaScatterCandidate& P : Out)
				{
					if (DistanceXY(Loc, P.Location) < MinDist) { bOk = false; break; }
				}
				if (!bOk) { continue; }
				Out.Add(Pool[PoolIdx]);
				Consumed.Add(PoolIdx);
				++Added;
			}
			// Remove consumed from pool (descending index order to keep validity)
			Consumed.Sort([](int32 A, int32 B) { return A > B; });
			for (int32 Idx : Consumed) { Pool.RemoveAt(Idx, EAllowShrinking::No); }
		}

		// Step 4: backfill globally with farthest-first
		while (Out.Num() < Target && Pool.Num() > 0)
		{
			int32 BestIdx = INDEX_NONE;
			float BestScore = -1.f;
			for (int32 i = 0; i < Pool.Num(); ++i)
			{
				float MinD = TNumericLimits<float>::Max();
				for (const FAreaScatterCandidate& P : Out)
				{
					const float D = DistanceXY(Pool[i].Location, P.Location);
					if (D < MinD) { MinD = D; }
				}
				if (MinD < MinDist) { continue; }
				if (MinD > BestScore) { BestScore = MinD; BestIdx = i; }
			}
			if (BestIdx == INDEX_NONE) { break; }
			Out.Add(Pool[BestIdx]);
			Pool.RemoveAt(BestIdx, EAllowShrinking::No);
		}
	}

	void RunPicker(
		const FSamplingResult& Sampling,
		const URulePassAsset& Rule,
		FPickingResult& Out,
		int32 TargetCountOverride)
	{
		Out = FPickingResult{};
		const int32 EffectiveTarget = TargetCountOverride > 0 ? TargetCountOverride : Rule.TargetCount;
		Out.RequestedCount = EffectiveTarget;

		FRandomStream Rand(Rule.Seed);
		TArray<FAreaScatterCandidate> Pool = ApplyFilters(Sampling, Rule, Rand);
		Out.CandidateCount = Pool.Num();
		if (Pool.Num() == 0) { return; }

		TArray<FAreaScatterCandidate> Picked;
		Picked.Reserve(EffectiveTarget);

		if (Rule.PickMode == EAreaScatterPickMode::Cluster)
		{
			PickCluster(Pool, Rule, EffectiveTarget, Rand, Picked);
		}
		else
		{
			PickSpread(Pool, FMath::Max(1, EffectiveTarget), Rule.MinPointDistance, Rand, Picked);
		}

		// Stats
		if (Picked.Num() >= 2)
		{
			float Sum = 0.f, Lo = TNumericLimits<float>::Max(), Hi = -TNumericLimits<float>::Max();
			int32 N = 0;
			for (int32 i = 0; i < Picked.Num(); ++i)
			{
				for (int32 j = i + 1; j < Picked.Num(); ++j)
				{
					const float D = DistanceXY(Picked[i].Location, Picked[j].Location);
					Sum += D; Lo = FMath::Min(Lo, D); Hi = FMath::Max(Hi, D); ++N;
				}
			}
			if (N > 0)
			{
				Out.MinInterDist = Lo;
				Out.MaxInterDist = Hi;
				Out.AvgInterDist = Sum / static_cast<float>(N);
			}
		}

		Out.Picked = MoveTemp(Picked);
		UE_LOG(LogAreaScatter, Display,
			TEXT("Picker[%s]: target=%d (override=%d) picked=%d candidates(after filter)=%d min=%.1f avg=%.1f max=%.1f cm"),
			Rule.PickMode == EAreaScatterPickMode::Cluster ? TEXT("Cluster") : TEXT("Spread"),
			Out.RequestedCount, TargetCountOverride, Out.Picked.Num(), Out.CandidateCount,
			Out.MinInterDist, Out.AvgInterDist, Out.MaxInterDist);
	}
}
