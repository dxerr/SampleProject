// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/VaultAssetCheckToolMenu.h"
#include "Editor/SVaultAssetCheckToolPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "VaultAssetCheckToolMenu"

const FName FVaultAssetCheckToolMenu::TabName(TEXT("VaultAssetCheckTool"));

void FVaultAssetCheckToolMenu::Register()
{
	// Nomad 탭을 전역 탭매니저에 등록하고 Tools 워크스페이스 그룹을 지정하면
	// 레벨 에디터의 "Window" 메뉴에 항목이 자동으로 노출된다.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabName,
		FOnSpawnTab::CreateStatic(&FVaultAssetCheckToolMenu::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Vault Asset Check Tool"))
		.SetTooltipText(LOCTEXT("TabTooltip", "에셋 리포트(텍스처/메시/UMG/사운드 등)를 추출하는 도구"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
}

void FVaultAssetCheckToolMenu::Unregister()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
	}
}

TSharedRef<SDockTab> FVaultAssetCheckToolMenu::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVaultAssetCheckToolPanel)
		];
}

#undef LOCTEXT_NAMESPACE
