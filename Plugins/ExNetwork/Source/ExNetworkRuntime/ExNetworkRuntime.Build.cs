// Copyright ExFrameWork. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ExNetworkRuntime : ModuleRules
{
	public ExNetworkRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// ExFrameWork 정책: Public/Private 폴더 분리 없이 평면 배치.
		PublicIncludePaths.AddRange(
			new string[]
			{
				ModuleDirectory,
				Path.Combine(ModuleDirectory, "Core"),
				Path.Combine(ModuleDirectory, "Events"),
				Path.Combine(ModuleDirectory, "Strategies"),
				Path.Combine(ModuleDirectory, "Providers"),
				Path.Combine(ModuleDirectory, "Providers", "EOS"),
				Path.Combine(ModuleDirectory, "Providers", "Null"),
				Path.Combine(ModuleDirectory, "Match"),
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"OnlineBase",
				"EOSShared",
				"OnlineSubsystemEOS",
				"EOSSDK",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
