// Copyright (c) ActorScatter authors. All rights reserved.

#include "AreaScatterInstanceHost.h"

#include "Components/SceneComponent.h"

AAreaScatterInstanceHost::AAreaScatterInstanceHost()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
