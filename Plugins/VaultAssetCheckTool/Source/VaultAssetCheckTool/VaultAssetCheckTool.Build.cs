// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VaultAssetCheckTool : ModuleRules
{
	public VaultAssetCheckTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
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
				"EditorStyle",            // FEditorStyle (UI 패널 스타일)
				"WorkspaceMenuStructure", // Window 메뉴 Tools 카테고리 등록
				"UMG",
				"UMGEditor",
				"InputCore",
				"BuildSettings",
				"Json",
				"Paper2D",
				"Projects",
				"PhysicsCore",            // UBodySetupCore::GetCollisionTraceFlag (StaticMesh 콜리전 판정)
				"FreeType2",              // Font 글리프 커버리지 정적 분석 (FT_New_Memory_Face)
				"UnrealEd",
				"TargetPlatform",
				"AndroidRuntimeSettings",
				"IOSRuntimeSettings"
				// ... add private dependencies that you statically link with here ...
			}
			);

		// 엔진 버전에 따라 플랫폼/설정 모듈 이름이 다르므로 분기 처리한다.
		if (Target.Version.MajorVersion >= 5)
		{
			// UE5: ProjectPackagingSettings는 DeveloperToolSettings로,
			//      WindowsTargetSettings는 WindowsTargetPlatformSettings로 분리되었다.
			// Niagara 분석 리포트는 UE5에서만 지원한다. UE4에서는 모듈/플러그인에 의존하지 않도록
			// 의존성을 추가하지 않고, ReportNiagara.cpp도 스텁으로 컴파일된다.
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DeveloperToolSettings",
					"WindowsTargetPlatformSettings",
					"Niagara"
				}
				);
		}
		else
		{
			// UE4: ProjectPackagingSettings는 UnrealEd(위 공통 의존성)에 포함되고,
			//      WindowsTargetSettings는 WindowsTargetPlatform 모듈에 있다.
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"WindowsTargetPlatform"
				}
				);
		}

		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
