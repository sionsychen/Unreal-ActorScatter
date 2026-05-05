// Copyright (c) ActorScatter authors. All rights reserved.

#include "AreaScatterEditorModule.h"
#include "AreaScatterEditorOps.h"
#include "AreaSpawnerActor.h"
#include "Slate/SAreaScatterPanel.h"

#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FAreaScatterEditorModule"

namespace
{
	static const FName AreaScatterTabName(TEXT("AreaScatterPanel"));

	TSharedRef<SDockTab> SpawnAreaScatterTab(const FSpawnTabArgs& /*Args*/)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SAreaScatterPanel)
			];
	}
}

void FAreaScatterEditorModule::StartupModule()
{
#if WITH_EDITOR
	AAreaSpawnerActor::OnPreviewRequested.BindStatic(&AreaScatter::HandlePreview);
	AAreaSpawnerActor::OnSpawnRequested.BindStatic(&AreaScatter::HandleSpawn);
	AAreaSpawnerActor::OnClearRequested.BindStatic(&AreaScatter::HandleClear);

	const TSharedPtr<FWorkspaceItem> MenuRoot = WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory();
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			AreaScatterTabName,
			FOnSpawnTab::CreateStatic(&SpawnAreaScatterTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Area Scatter"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Open the Area Scatter control panel."))
		.SetGroup(MenuRoot.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
#endif
}

void FAreaScatterEditorModule::ShutdownModule()
{
#if WITH_EDITOR
	AAreaSpawnerActor::OnPreviewRequested.Unbind();
	AAreaSpawnerActor::OnSpawnRequested.Unbind();
	AAreaSpawnerActor::OnClearRequested.Unbind();

	if (FGlobalTabmanager::Get()->HasTabSpawner(AreaScatterTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AreaScatterTabName);
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAreaScatterEditorModule, AreaScatterEditor)
