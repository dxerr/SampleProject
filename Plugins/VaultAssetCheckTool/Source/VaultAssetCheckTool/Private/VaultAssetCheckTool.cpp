// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckTool.h"
#include "Editor/VaultAssetCheckToolMenu.h"

#define LOCTEXT_NAMESPACE "FVaultAssetCheckToolModule"

void FVaultAssetCheckToolModule::StartupModule()
{
	// UI(Window 메뉴 + 도킹 탭)는 독립된 UI 레이어에서 등록한다.
	// 기존 리포트 로직(Export*)과 분리되어 있어 레이아웃/기능 수정 시 UI 파일만 건드리면 된다.
	FVaultAssetCheckToolMenu::Register();
}

void FVaultAssetCheckToolModule::ShutdownModule()
{
	FVaultAssetCheckToolMenu::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVaultAssetCheckToolModule, VaultAssetCheckTool)
