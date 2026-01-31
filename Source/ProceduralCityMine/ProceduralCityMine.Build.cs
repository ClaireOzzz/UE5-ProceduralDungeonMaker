// Copyright Chateau Pageot, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProceduralCityMine : ModuleRules
{
	public ProceduralCityMine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SimpleGridRuntime",
				"ProceduralCityGenerator",
			}
		);
	}
}
