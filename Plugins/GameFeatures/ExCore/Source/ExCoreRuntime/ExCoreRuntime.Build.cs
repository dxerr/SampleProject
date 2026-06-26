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
				Path.Combine(ModuleDirectory, "Data", "Base"),	        // DataCenter 3-Base 클래스
				Path.Combine(ModuleDirectory, "Components"),
				Path.Combine(ModuleDirectory, "Actors"),
				Path.Combine(ModuleDirectory, "Player"),
				Path.Combine(ModuleDirectory, "Tags"),
				Path.Combine(ModuleDirectory, "Events"),
				Path.Combine(ModuleDirectory, "Util"),
				Path.Combine(ModuleDirectory, "Util", "Events"),
				Path.Combine(ModuleDirectory, "Debug"),
				Path.Combine(ModuleDirectory, "Items"),
				Path.Combine(ModuleDirectory, "Items", "Effects"),
				Path.Combine(ModuleDirectory, "Struct", "Items"),
				Path.Combine(ModuleDirectory, "Subsystems"),             // ExDataCenterSubsystem, ExAssetPreloadSubsystem
				Path.Combine(ModuleDirectory, "Struct", "Subsystems"),   // FExPreloadOptions
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
				"Niagara",		// 아이템 VFX (TSoftObjectPtr<UNiagaraSystem>)
				"EnhancedInput",
				"InputCore", // EKeys::LeftMouseButton 링크 에러 해결
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
