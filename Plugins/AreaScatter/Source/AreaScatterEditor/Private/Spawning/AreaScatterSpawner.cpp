// Copyright (c) ActorScatter authors. All rights reserved.

#include "Spawning/AreaScatterSpawner.h"

#include "AreaScatterTypes.h"
#include "AreaScatterInstanceHost.h"
#include "AreaScatterPointSet.h"
#include "AreaSpawnBatch.h"
#include "AreaSpawnerActor.h"
#include "Picking/AreaScatterPicker.h"
#include "RulePassAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "AreaScatterSpawner"

namespace AreaScatter
{
	static FQuat ComposeRotation(const FAreaScatterCandidate& C, const URulePassAsset& Rule, FRandomStream& Rand)
	{
		FQuat AlignQ = FQuat::Identity;
		if (Rule.bAlignToNormal)
		{
			const FVector NormalUp = C.Normal.GetSafeNormal();
			const FVector BlendedUp = FMath::Lerp(FVector::UpVector, NormalUp, Rule.NormalBlend).GetSafeNormal();
			AlignQ = FQuat::FindBetweenNormals(FVector::UpVector, BlendedUp);
		}

		const float Yaw = Rand.FRandRange(Rule.RandomYawRange.X, Rule.RandomYawRange.Y);
		const FQuat YawQ(FVector::UpVector, FMath::DegreesToRadians(Yaw));

		const float TiltDeg = Rand.FRandRange(Rule.RandomTiltRange.X, Rule.RandomTiltRange.Y);
		FQuat TiltQ = FQuat::Identity;
		if (TiltDeg > KINDA_SMALL_NUMBER)
		{
			const float TiltAxisYaw = Rand.FRandRange(0.f, 2.f * PI);
			const FVector TiltAxis(FMath::Cos(TiltAxisYaw), FMath::Sin(TiltAxisYaw), 0.f);
			TiltQ = FQuat(TiltAxis, FMath::DegreesToRadians(TiltDeg));
		}

		return AlignQ * YawQ * TiltQ;
	}

	static FString MakeFolderPath(const AAreaSpawnerActor& Spawner, const FGuid& BatchId)
	{
		return FString::Printf(TEXT("AreaScatter/%s_%s"),
			*Spawner.GetActorLabel(), *BatchId.ToString(EGuidFormats::Short));
	}

	static UAreaSpawnBatch* EnsureBatch(
		AAreaSpawnerActor& Spawner,
		const URulePassAsset& Rule,
		UAreaSpawnBatch* ExistingBatch)
	{
		if (ExistingBatch)
		{
			ExistingBatch->Modify();
			return ExistingBatch;
		}
		if (Spawner.bAutoClearPreviousBatch && Spawner.LastBatch)
		{
			ClearBatch(Spawner.LastBatch);
			Spawner.LastBatch = nullptr;
		}
		UAreaSpawnBatch* Batch = NewObject<UAreaSpawnBatch>(&Spawner);
		Batch->BatchId = FGuid::NewGuid();
		Batch->CreatedAt = FDateTime::Now();
		Batch->SourceSpawner = &Spawner;
		Batch->RuleAsset = &Rule;
		Batch->Params.Seed = Rule.Seed;
		Batch->Params.TargetCount = Rule.TargetCount;
		Batch->Params.PickedCount = 0;
		Batch->Params.MaxSlopeDegrees = Rule.MaxSlopeDegrees;
		Batch->Params.AvoidDistance = Rule.AvoidDistance;
		Batch->Params.MinPointDistance = Rule.MinPointDistance;
		return Batch;
	}

	// ---- Output: Actors ----------------------------------------------------

	static void SpawnAsActors(
		AAreaSpawnerActor& Spawner,
		const URulePassAsset& Rule,
		const FPickingResult& Picks,
		UAreaSpawnBatch& Batch,
		const FString& FolderPath)
	{
		UClass* Cls = Rule.SpawnActorClass.LoadSynchronous();
		if (!Cls)
		{
			UE_LOG(LogAreaScatter, Warning, TEXT("Spawner: SpawnActorClass not set for Actors-mode rule '%s'."), *Rule.GetName());
			return;
		}
		UWorld* World = Spawner.GetWorld();
		if (!World) { return; }

		FRandomStream Rand(Rule.Seed ^ 0x5F3759DF);
		Batch.SpawnedActors.Reserve(Batch.SpawnedActors.Num() + Picks.Picked.Num());
		for (const FAreaScatterCandidate& C : Picks.Picked)
		{
			const float ZOff = Rand.FRandRange(Rule.ZOffsetRange.X, Rule.ZOffsetRange.Y);
			const float Scale = Rand.FRandRange(Rule.UniformScaleRange.X, Rule.UniformScaleRange.Y);
			const FVector Loc = C.Location + FVector(0.f, 0.f, ZOff);
			const FQuat Rot = ComposeRotation(C, Rule, Rand);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* New = World->SpawnActor<AActor>(Cls, Loc, Rot.Rotator(), SpawnParams);
			if (!New) { continue; }

			if (FMath::Abs(Scale - 1.f) > KINDA_SMALL_NUMBER)
			{
				New->SetActorScale3D(FVector(Scale));
			}
			New->SetFolderPath(FName(*FolderPath));
			Batch.SpawnedActors.Add(New);
		}
	}

	// ---- Output: ISM / HISM ------------------------------------------------

	static AAreaScatterInstanceHost* GetOrCreateHost(
		AAreaSpawnerActor& Spawner,
		UAreaSpawnBatch& Batch,
		const FString& FolderPath)
	{
		// Re-use a host if a previous pass already created one for this batch.
		for (TWeakObjectPtr<AActor>& Weak : Batch.SpawnedActors)
		{
			if (AAreaScatterInstanceHost* H = Cast<AAreaScatterInstanceHost>(Weak.Get())) { return H; }
		}
		UWorld* World = Spawner.GetWorld();
		if (!World) { return nullptr; }
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAreaScatterInstanceHost* Host = World->SpawnActor<AAreaScatterInstanceHost>(
			AAreaScatterInstanceHost::StaticClass(),
			Spawner.GetActorLocation(),
			FRotator::ZeroRotator,
			Params);
		if (Host)
		{
			Host->SetActorLabel(FString::Printf(TEXT("ISMHost_%s"), *Batch.BatchId.ToString(EGuidFormats::Short)));
			Host->SetFolderPath(FName(*FolderPath));
			Batch.SpawnedActors.Add(Host);
		}
		return Host;
	}

	static void SpawnAsInstances(
		AAreaSpawnerActor& Spawner,
		const URulePassAsset& Rule,
		const FPickingResult& Picks,
		UAreaSpawnBatch& Batch,
		const FString& FolderPath,
		bool bHierarchical)
	{
		UStaticMesh* Mesh = Rule.SpawnStaticMesh.LoadSynchronous();
		if (!Mesh)
		{
			UE_LOG(LogAreaScatter, Warning, TEXT("Spawner: SpawnStaticMesh not set for ISM-mode rule '%s'."), *Rule.GetName());
			return;
		}
		AAreaScatterInstanceHost* Host = GetOrCreateHost(Spawner, Batch, FolderPath);
		if (!Host) { return; }

		Host->Modify();
		UInstancedStaticMeshComponent* ISMC = nullptr;
		if (bHierarchical)
		{
			UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(
				Host, MakeUniqueObjectName(Host, UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("HISMC")));
			ISMC = H;
		}
		else
		{
			ISMC = NewObject<UInstancedStaticMeshComponent>(
				Host, MakeUniqueObjectName(Host, UInstancedStaticMeshComponent::StaticClass(), TEXT("ISMC")));
		}
		ISMC->SetStaticMesh(Mesh);
		for (int32 i = 0; i < Rule.InstanceMaterialOverrides.Num(); ++i)
		{
			if (UMaterialInterface* M = Rule.InstanceMaterialOverrides[i].LoadSynchronous())
			{
				ISMC->SetMaterial(i, M);
			}
		}
		ISMC->SetMobility(EComponentMobility::Static);
		ISMC->SetupAttachment(Host->GetRootComponent());
		ISMC->RegisterComponent();
		Host->AddInstanceComponent(ISMC);
		Host->ScatterInstanceComponents.Add(ISMC);

		FRandomStream Rand(Rule.Seed ^ 0x5F3759DF);
		ISMC->PreAllocateInstancesMemory(Picks.Picked.Num());
		for (const FAreaScatterCandidate& C : Picks.Picked)
		{
			const float ZOff = Rand.FRandRange(Rule.ZOffsetRange.X, Rule.ZOffsetRange.Y);
			const float Scale = Rand.FRandRange(Rule.UniformScaleRange.X, Rule.UniformScaleRange.Y);
			const FVector Loc = C.Location + FVector(0.f, 0.f, ZOff);
			const FQuat Rot = ComposeRotation(C, Rule, Rand);
			FTransform T(Rot, Loc, FVector(Scale));
			ISMC->AddInstance(T, /*bWorldSpace=*/true);
		}
	}

	// ---- Output: Point Set asset -------------------------------------------

	static void SpawnAsPointSet(
		AAreaSpawnerActor& Spawner,
		const URulePassAsset& Rule,
		const FPickingResult& Picks,
		UAreaSpawnBatch& Batch)
	{
		const FString DirPath = TEXT("/Game/AreaScatter/Generated");
		const FString AssetName = FString::Printf(TEXT("PS_%s_%s"),
			*Spawner.GetActorLabel(),
			*Batch.BatchId.ToString(EGuidFormats::Short));
		const FString PackagePath = DirPath / AssetName;

		UPackage* Pkg = CreatePackage(*PackagePath);
		Pkg->FullyLoad();

		UAreaScatterPointSet* PS = NewObject<UAreaScatterPointSet>(Pkg, *AssetName, RF_Public | RF_Standalone);
		PS->SourceSpawnerLabel = Spawner.GetActorLabel();
		PS->SourceRuleName = Rule.GetName();
		PS->BatchId = Batch.BatchId;
		PS->CreatedAt = FDateTime::Now();
		PS->Points.Reserve(Picks.Picked.Num());

		FRandomStream Rand(Rule.Seed ^ 0x5F3759DF);
		for (const FAreaScatterCandidate& C : Picks.Picked)
		{
			const float ZOff = Rand.FRandRange(Rule.ZOffsetRange.X, Rule.ZOffsetRange.Y);
			const float Scale = Rand.FRandRange(Rule.UniformScaleRange.X, Rule.UniformScaleRange.Y);
			const FQuat Rot = ComposeRotation(C, Rule, Rand);
			FAreaScatterPoint P;
			P.Transform = FTransform(Rot, C.Location + FVector(0.f, 0.f, ZOff), FVector(Scale));
			P.Normal = C.Normal;
			P.SlopeDegrees = C.SlopeDegrees;
			PS->Points.Add(MoveTemp(P));
		}

		FAssetRegistryModule::AssetCreated(PS);
		Pkg->MarkPackageDirty();

		const FString FilePath = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Pkg, PS, *FilePath, SaveArgs);
		UE_LOG(LogAreaScatter, Display, TEXT("Spawner: PointSet asset %s -> %s (%d points)."),
			bSaved ? TEXT("saved") : TEXT("FAILED"), *PackagePath, PS->Points.Num());
	}

	// ---- Public entry ------------------------------------------------------

	UAreaSpawnBatch* SpawnBatch(
		AAreaSpawnerActor& Spawner,
		const URulePassAsset& Rule,
		const FPickingResult& Picks,
		UAreaSpawnBatch* ExistingBatch)
	{
		if (Picks.Picked.Num() == 0) { return ExistingBatch; }
		if (!Spawner.GetWorld()) { return ExistingBatch; }

		FScopedTransaction Tx(LOCTEXT("AreaScatterSpawn", "Area Scatter Spawn"));
		Spawner.Modify();

		UAreaSpawnBatch* Batch = EnsureBatch(Spawner, Rule, ExistingBatch);
		if (!Batch) { return nullptr; }
		Batch->Params.PickedCount += Picks.Picked.Num();

		const FString FolderPath = MakeFolderPath(Spawner, Batch->BatchId);

		switch (Rule.OutputMode)
		{
			case EAreaSpawnOutputMode::Actors:    SpawnAsActors(Spawner, Rule, Picks, *Batch, FolderPath); break;
			case EAreaSpawnOutputMode::ISM:       SpawnAsInstances(Spawner, Rule, Picks, *Batch, FolderPath, /*bHierarchical=*/false); break;
			case EAreaSpawnOutputMode::HISM:      SpawnAsInstances(Spawner, Rule, Picks, *Batch, FolderPath, /*bHierarchical=*/true); break;
			case EAreaSpawnOutputMode::PointSet:  SpawnAsPointSet(Spawner, Rule, Picks, *Batch); break;
			default: break;
		}

		Spawner.LastBatch = Batch;
		UE_LOG(LogAreaScatter, Display, TEXT("Spawner: pass '%s' [%d] mode=%d -> batch %s (actors total %d)."),
			Rule.DisplayName.IsEmpty() ? *Rule.GetName() : *Rule.DisplayName,
			Picks.Picked.Num(), static_cast<int32>(Rule.OutputMode),
			*Batch->BatchId.ToString(EGuidFormats::Short), Batch->SpawnedActors.Num());
		return Batch;
	}

	int32 ClearBatch(UAreaSpawnBatch* Batch)
	{
		if (!Batch) { return 0; }
		FScopedTransaction Tx(LOCTEXT("AreaScatterClear", "Area Scatter Clear Batch"));
		Batch->Modify();

		int32 Removed = 0;
		for (TWeakObjectPtr<AActor>& Weak : Batch->SpawnedActors)
		{
			if (AActor* A = Weak.Get())
			{
				A->Modify();
				if (UWorld* W = A->GetWorld())
				{
					W->DestroyActor(A);
					++Removed;
				}
			}
		}
		Batch->SpawnedActors.Reset();
		UE_LOG(LogAreaScatter, Display, TEXT("Spawner: cleared %d actors from batch %s."),
			Removed, *Batch->BatchId.ToString(EGuidFormats::Short));
		return Removed;
	}
}

#undef LOCTEXT_NAMESPACE
