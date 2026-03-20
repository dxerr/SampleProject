// Fill out your copyright notice in the Description page of Project Settings.

#include "ExRunnerHUDLayout.h"

TOptional<FUIInputConfig> UExRunnerHUDLayout::GetDesiredInputConfig() const
{
	// 러너 게임 특성상 게임 패드/가상 조이스틱/터치 입력을 온전히 받아야 하므로 Game 입력 모드로 고정합니다.
	// UI 네비게이션은 비활성화되고 폰(Pawn)이 입력을 독점적으로 처리하게 됩니다.
#if WITH_EDITOR
	// [수정] 에디터 테스트 시 F8(Eject) 뷰포트 마우스 갇힘 버그 방지를 위해 캡처 완화
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CaptureDuringMouseDown, /*bHideCursor=*/ false);
#else
	// 모바일 실제 기기나 패키징 빌드에서는 입력 유실 방지를 위해 영구 캡처 및 커서 숨김 유지
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, /*bHideCursor=*/ true);
#endif
}

void UExRunnerHUDLayout::NativeOnActivated()
{
	Super::NativeOnActivated();
	
	// 러너 게임 HUD가 화면에 띄워졌을 때 C++에서 추가로 해야 할 초기화 작업이 있다면 여기에 작성합니다.
	// 뷰모델(ExRunnerInputViewModel, ExRunnerStatsViewModel)들은 
	// WBP 내부 각 하위 위젯들의 OnActivated나 AutoInitialize 시점에서 스스로 초기화하도록 위임하는 것이 좋습니다.
}

void UExRunnerHUDLayout::NativeOnDeactivated()
{
	// 러너 게임 HUD가 내려갈 때 정리 작업
	Super::NativeOnDeactivated();
}
