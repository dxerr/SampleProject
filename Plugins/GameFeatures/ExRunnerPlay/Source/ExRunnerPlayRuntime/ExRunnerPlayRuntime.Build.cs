// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ExRunnerPlayRuntime : ModuleRules
{
	public ExRunnerPlayRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Debug"),
				System.IO.Path.Combine(ModuleDirectory, "Tags"),
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
				"ExCoreRuntime", // Framework Dependency
				"GameplayTags",	// GameplayTag types (FGameplayTag, native tags)
				"Mover",
				"MotionWarping",
				"EnhancedInput",
				"ModelViewViewModel",
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
