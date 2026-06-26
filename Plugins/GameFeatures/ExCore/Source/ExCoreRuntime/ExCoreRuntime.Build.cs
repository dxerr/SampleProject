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
				Path.Combine(ModuleDirectory, "Animation"),
				Path.Combine(ModuleDirectory, "GameModes"),
				Path.Combine(ModuleDirectory, "Data"),
				Path.Combine(ModuleDirectory, "Components"),
				Path.Combine(ModuleDirectory, "Actors"),
				Path.Combine(ModuleDirectory, "Tags"),
				Path.Combine(ModuleDirectory, "Events"),
				Path.Combine(ModuleDirectory, "Util"),
				Path.Combine(ModuleDirectory, "Util", "Events"),
				Path.Combine(ModuleDirectory, "Debug"),
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
				"IKRig",          // UExRetargetAnimInstance — BlendToSource op 런타임 제어
				"StructUtils",    // FInstancedStruct (GetMutablePtr in IKRetargetProfile)

				// --- UI Architecture Modules ---
				"UMG",
				"CommonUI",
				"CommonInput",
				"ModelViewViewModel",

				// --- Online / Session ---
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"ExNetworkRuntime",

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
