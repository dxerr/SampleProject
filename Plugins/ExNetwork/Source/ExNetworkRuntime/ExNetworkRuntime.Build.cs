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
				// Phase 3 추가
				Path.Combine(ModuleDirectory, "Match"),
				// Phase 3 이후 추가 예정:
				// Path.Combine(ModuleDirectory, "Player"),
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
				"OnlineBase",       // SEARCH_LOBBIES 등 OnlineSessionNames 접근
				// EOS SDK 직접 호출을 위한 의존성
				"EOSShared",
				"OnlineSubsystemEOS",
				"EOSSDK",  // EOS SDK 헤더 직접 접근 (eos_platform.h, eos_connect.h 등)
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
