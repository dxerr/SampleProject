// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SDockTab;
class FSpawnTabArgs;

/**
 * VaultAssetCheckTool 의 Window 메뉴 진입점 / 도킹 탭 등록 담당.
 *
 * 모듈(FVaultAssetCheckToolModule)은 Register()/Unregister() 만 호출하며,
 * 메뉴·탭 관련 로직은 모두 이 클래스에 격리되어 있다. (기존 리포트 로직과 독립)
 */
class FVaultAssetCheckToolMenu
{
public:
	/** 전역 탭매니저에 Nomad 탭을 등록한다. (Window 메뉴에 자동 노출) */
	static void Register();

	/** 등록한 탭 스포너를 해제한다. */
	static void Unregister();

	/** Window 메뉴/탭에 사용되는 탭 ID. */
	static const FName TabName;

private:
	/** 탭 콘텐츠(SVaultAssetCheckToolPanel)를 생성한다. */
	static TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
};
