// Copyright (c) ActorScatter authors. All rights reserved.

#include "AreaScatterSampleAssets.h"

#include "AreaScatterTypes.h"
#include "RulePassAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AreaScatter
{
	static UClass* TryFindClass(const TCHAR* Path)
	{
		return LoadObject<UClass>(nullptr, Path);
	}

	static UStaticMesh* TryFindMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	static URulePassAsset* WriteAsset(
		const FString& AssetName,
		TFunctionRef<void(URulePassAsset&)> Configure,
		bool bOverwrite,
		bool& bOutWritten)
	{
		bOutWritten = false;
		const FString DirPath = TEXT("/Game/AreaScatter/Samples");
		const FString PackagePath = DirPath / AssetName;

		// Skip if exists and not overwriting.
		if (!bOverwrite && FPackageName::DoesPackageExist(PackagePath))
		{
			UE_LOG(LogAreaScatter, Display, TEXT("Sample asset %s already exists, skipping."), *PackagePath);
			return nullptr;
		}

		UPackage* Pkg = CreatePackage(*PackagePath);
		Pkg->FullyLoad();

		URulePassAsset* Asset = NewObject<URulePassAsset>(Pkg, *AssetName, RF_Public | RF_Standalone);
		Configure(*Asset);

		FAssetRegistryModule::AssetCreated(Asset);
		Pkg->MarkPackageDirty();

		const FString FilePath = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Pkg, Asset, *FilePath, SaveArgs);
		if (bSaved)
		{
			bOutWritten = true;
			UE_LOG(LogAreaScatter, Display, TEXT("Sample asset written: %s"), *PackagePath);
		}
		else
		{
			UE_LOG(LogAreaScatter, Warning, TEXT("Sample asset save FAILED: %s"), *PackagePath);
		}
		return Asset;
	}

	int32 GenerateSampleRules(bool bOverwrite)
	{
		int32 Count = 0;

		// 1) Grass: Spread + Actors
		UClass* TargetPointCls = TryFindClass(TEXT("/Script/Engine.TargetPoint"));
		{
			bool bWritten = false;
			WriteAsset(TEXT("DA_Sample_Grass_Spread"), [TargetPointCls](URulePassAsset& A)
			{
				A.DisplayName = TEXT("Grass — Spread (sample)");
				A.bEnabled = true;
				A.ConfigMode = EAreaScatterConfigMode::Simple;
				A.GridResolution = 128;
				A.MaxSlopeDegrees = 45.f;
				A.RequireComponentClassContains = TEXT("Landscape");
				A.MinPointDistance = 100.f;       // 1 m
				A.AvoidDistance = 0.f;            // off
				A.PickMode = EAreaScatterPickMode::Spread;
				A.Seed = 42;
				A.OutputMode = EAreaSpawnOutputMode::Actors;
				if (TargetPointCls) { A.SpawnActorClass = TargetPointCls; }
				A.TargetCount = 200;
				A.bAlignToNormal = true;
				A.NormalBlend = 0.5f;
				A.RandomYawRange = FVector2D(0.f, 360.f);
				A.RandomTiltRange = FVector2D(0.f, 5.f);
				A.UniformScaleRange = FVector2D(1.f, 1.f);
			}, bOverwrite, bWritten);
			if (bWritten) { ++Count; }
		}

		// 2) Trees: Cluster + ISM (Cube placeholder)
		UStaticMesh* CubeMesh = TryFindMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
		{
			bool bWritten = false;
			WriteAsset(TEXT("DA_Sample_Trees_Cluster"), [CubeMesh](URulePassAsset& A)
			{
				A.DisplayName = TEXT("Trees — Cluster (sample, ISM Cube placeholder)");
				A.bEnabled = true;
				A.ConfigMode = EAreaScatterConfigMode::Detailed;
				A.GridResolution = 128;
				A.MaxSlopeDegrees = 35.f;
				A.RequireComponentClassContains = TEXT("Landscape");
				A.MinPointDistance = 250.f;
				A.AvoidDistance = 0.f;
				A.PickMode = EAreaScatterPickMode::Cluster;
				A.NumClusters = 5;
				A.ClusterRadius = 800.f;
				A.CenterMinDistance = 2500.f;
				A.ClusterSizeMin = 8;
				A.ClusterSizeMax = 18;
				A.Seed = 1337;
				A.OutputMode = EAreaSpawnOutputMode::ISM;
				if (CubeMesh) { A.SpawnStaticMesh = CubeMesh; }
				A.TargetCount = 80;
				A.bAlignToNormal = true;
				A.NormalBlend = 0.4f;
				A.RandomYawRange = FVector2D(0.f, 360.f);
				A.RandomTiltRange = FVector2D(0.f, 8.f);
				A.UniformScaleRange = FVector2D(0.6f, 1.4f);
			}, bOverwrite, bWritten);
			if (bWritten) { ++Count; }
		}

		// 3) Rocks: Spread + AABB avoid by name pattern
		{
			bool bWritten = false;
			WriteAsset(TEXT("DA_Sample_Rocks_AvoidBuildings"), [TargetPointCls](URulePassAsset& A)
			{
				A.DisplayName = TEXT("Rocks — Avoid Buildings (sample)");
				A.bEnabled = true;
				A.ConfigMode = EAreaScatterConfigMode::Simple;
				A.GridResolution = 128;
				A.MaxSlopeDegrees = 50.f;
				A.RequireComponentClassContains = TEXT("Landscape");
				A.MinPointDistance = 350.f;
				A.AvoidDistance = 500.f;
				A.AvoidShapeMode = EAreaScatterAvoidShape::AABB;
				A.AvoidNamePatterns = { TEXT("Bldg"), TEXT("House"), TEXT("Wall"), TEXT("Roof"), TEXT("Floor") };
				A.PickMode = EAreaScatterPickMode::Spread;
				A.Seed = 7;
				A.OutputMode = EAreaSpawnOutputMode::Actors;
				if (TargetPointCls) { A.SpawnActorClass = TargetPointCls; }
				A.TargetCount = 60;
				A.bAlignToNormal = true;
				A.NormalBlend = 0.7f;
				A.RandomYawRange = FVector2D(0.f, 360.f);
				A.RandomTiltRange = FVector2D(0.f, 12.f);
				A.UniformScaleRange = FVector2D(0.8f, 1.5f);
			}, bOverwrite, bWritten);
			if (bWritten) { ++Count; }
		}

		return Count;
	}
}
