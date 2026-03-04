// Copyright ExFrameWork. All Rights Reserved.

#include "ExPlayerController.h"
#include "ExCheatManager.h"
#include "ExPlayerCameraManager.h"

AExPlayerController::AExPlayerController()
{
	// 생성자에서 CheatClass 설정 → AddCheats() 시점에 UExCheatManager 자동 생성
	CheatClass = UExCheatManager::StaticClass();

	// 커스텀 카메라 매니저 설정 (스카이돔 추적 등 연출 최적화용)
	PlayerCameraManagerClass = AExPlayerCameraManager::StaticClass();
}
