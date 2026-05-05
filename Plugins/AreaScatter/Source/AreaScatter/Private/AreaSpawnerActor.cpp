// Copyright (c) ActorScatter authors. All rights reserved.

#include "AreaSpawnerActor.h"
#include "AreaScatterTypes.h"
#include "Components/SplineComponent.h"

#if WITH_EDITOR
FAreaSpawnerOpDelegate AAreaSpawnerActor::OnPreviewRequested;
FAreaSpawnerOpDelegate AAreaSpawnerActor::OnSpawnRequested;
FAreaSpawnerOpDelegate AAreaSpawnerActor::OnClearRequested;
#endif

AAreaSpawnerActor::AAreaSpawnerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	AreaSpline = CreateDefaultSubobject<USplineComponent>(TEXT("AreaSpline"));
	RootComponent = AreaSpline;
	AreaSpline->SetClosedLoop(true);
}

#if WITH_EDITOR
void AAreaSpawnerActor::Preview()
{
	if (OnPreviewRequested.IsBound())
	{
		OnPreviewRequested.Execute(this);
	}
	else
	{
		UE_LOG(LogAreaScatter, Warning, TEXT("Preview requested but Editor module not loaded."));
	}
}

void AAreaSpawnerActor::Spawn()
{
	if (OnSpawnRequested.IsBound())
	{
		OnSpawnRequested.Execute(this);
	}
	else
	{
		UE_LOG(LogAreaScatter, Warning, TEXT("Spawn requested but Editor module not loaded."));
	}
}

void AAreaSpawnerActor::ClearLastBatch()
{
	if (OnClearRequested.IsBound())
	{
		OnClearRequested.Execute(this);
	}
	else
	{
		UE_LOG(LogAreaScatter, Warning, TEXT("Clear requested but Editor module not loaded."));
	}
}
#endif
