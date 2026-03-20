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
				ModuleDirectory,
				Path.Combine(ModuleDirectory, "GameModes"),
				Path.Combine(ModuleDirectory, "Data"),
				Path.Combine(ModuleDirectory, "Components"),
				Path.Combine(ModuleDirectory, "Actors"),
				Path.Combine(ModuleDirectory, "Tags"),
				Path.Combine(ModuleDirectory, "Events"),
				Path.Combine(ModuleDirectory, "Util"),
				Path.Combine(ModuleDirectory, "Util", "Events"),
				Path.Combine(ModuleDirectory, "Debug"),
				Path.Combine(ModuleDirectory, "UI"),
				Path.Combine(ModuleDirectory, "UI", "Subsystems"),
				Path.Combine(ModuleDirectory, "UI", "Widgets"),
				Path.Combine(ModuleDirectory, "UI", "ViewModels"),
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
				"AudioMixer",		// Quartz Clock (UQuartzSubsystem, UQuartzClockHandle)
				"ModularGameplay",
				"GameFeatures",
				"GameplayAbilities",
				"GameplayTags",	// NEW: Native GameplayTag 지원
				"EnhancedInput",
				"Mover",
				"MotionWarping", // Centralized Warp Logic
				
				// --- UI Architecture Modules ---
				"UMG",
				"CommonUI",
				"CommonInput",
				"ModelViewViewModel",
				
				// ... add other public dependencies that you statically link with here ...
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore"
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
