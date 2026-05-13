// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ExRunnerPlayRuntime : ModuleRules
{
	public ExRunnerPlayRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				ModuleDirectory,
				System.IO.Path.Combine(ModuleDirectory, "Debug"),
				System.IO.Path.Combine(ModuleDirectory, "Tags"),
				System.IO.Path.Combine(ModuleDirectory, "Components"),
				System.IO.Path.Combine(ModuleDirectory, "Data"),
				System.IO.Path.Combine(ModuleDirectory, "Struct"),
				System.IO.Path.Combine(ModuleDirectory, "Actors"),
				System.IO.Path.Combine(ModuleDirectory, "InputStrategies"),
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ExFrameWork", // Required for GameModeDataSet and other base classes
				"ExCoreRuntime", // Framework Dependency
				"GameplayTags",	// GameplayTag types (FGameplayTag, native tags)
				"Mover",
				"MotionWarping",
				"EnhancedInput",
				"ModelViewViewModel",
				"UMG",
				"CommonUI",
				"Sentry",
				"BinkMediaPlayer",
				"ExNetworkRuntime",	// ExNetwork 매칭 시스템 (Phase 3 테스트용)
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"GameplayDebugger", // ExRunnerDebuggerCategory용
				"NetCore", // UEPushModelPrivate용
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
