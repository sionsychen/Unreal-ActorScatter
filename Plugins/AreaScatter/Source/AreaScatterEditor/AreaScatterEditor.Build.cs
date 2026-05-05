// Copyright (c) ActorScatter authors. All rights reserved.

using UnrealBuildTool;

public class AreaScatterEditor : ModuleRules
{
	public AreaScatterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AreaScatter",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorSubsystem",
			"EditorScriptingUtilities",
			"Blutility",
			"UMG",
			"UMGEditor",
			"MessageLog",
			"LevelEditor",
			"WorkspaceMenuStructure",
			"ToolMenus",
			"Projects",
		});

		PublicIncludePaths.AddRange(new string[]
		{
			System.IO.Path.Combine(ModuleDirectory, "Public", "Sampling"),
			System.IO.Path.Combine(ModuleDirectory, "Public", "Picking"),
			System.IO.Path.Combine(ModuleDirectory, "Public", "Spawning"),
			System.IO.Path.Combine(ModuleDirectory, "Public", "Slate"),
		});
	}
}
