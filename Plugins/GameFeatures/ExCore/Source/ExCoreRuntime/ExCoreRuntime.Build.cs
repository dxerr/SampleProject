// Copyright ExFrameWork. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ExCoreRuntime : ModuleRules
{
	public ExCoreRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// Path.Combine(ModuleDirectory, "ExCoreRuntime"), // Self
				Path.Combine(ModuleDirectory, "GameModes"),
				Path.Combine(ModuleDirectory, "Data"),
				Path.Combine(ModuleDirectory, "Components"),
				Path.Combine(ModuleDirectory, "Actors"),
				Path.Combine(ModuleDirectory, "Tags"),
				Path.Combine(ModuleDirectory, "Events"),
				Path.Combine(ModuleDirectory, "Util"),
				Path.Combine(ModuleDirectory, "Util", "Events"),
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ModularGameplay",
				"GameFeatures",
				"GameplayAbilities",
				"GameplayTags",	// NEW: Native GameplayTag 지원
				"EnhancedInput",
				"Mover",
				"MotionWarping", // Centralized Warp Logic
				// ... add other public dependencies that you statically link with here ...
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
