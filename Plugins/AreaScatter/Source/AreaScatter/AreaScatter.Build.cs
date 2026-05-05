// Copyright (c) ActorScatter authors. All rights reserved.

using UnrealBuildTool;

public class AreaScatter : ModuleRules
{
	public AreaScatter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
