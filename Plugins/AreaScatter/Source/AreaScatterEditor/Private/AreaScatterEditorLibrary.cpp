// Copyright (c) ActorScatter authors. All rights reserved.

#include "AreaScatterEditorLibrary.h"

#include "AreaScatterEditorOps.h"
#include "AreaSpawnerActor.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Docking/TabManager.h"

void UAreaScatterEditorLibrary::Preview(AAreaSpawnerActor* Spawner)
{
	AreaScatter::HandlePreview(Spawner);
}

void UAreaScatterEditorLibrary::Spawn(AAreaSpawnerActor* Spawner)
{
	AreaScatter::HandleSpawn(Spawner);
}

void UAreaScatterEditorLibrary::ClearLastBatch(AAreaSpawnerActor* Spawner)
{
	AreaScatter::HandleClear(Spawner);
}

AAreaSpawnerActor* UAreaScatterEditorLibrary::GetSelectedSpawner()
{
	if (!GEditor) { return nullptr; }
	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel) { return nullptr; }
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		if (AAreaSpawnerActor* A = Cast<AAreaSpawnerActor>(*It))
		{
			return A;
		}
	}
	return nullptr;
}

void UAreaScatterEditorLibrary::OpenPanel()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("AreaScatterPanel")));
}
