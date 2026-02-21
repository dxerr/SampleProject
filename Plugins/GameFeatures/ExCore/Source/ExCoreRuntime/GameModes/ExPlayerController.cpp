// Copyright ExFrameWork. All Rights Reserved.

#include "ExPlayerController.h"
#include "ExCheatManager.h"

AExPlayerController::AExPlayerController()
{
	// 생성자에서 CheatClass 설정 → AddCheats() 시점에 UExCheatManager 자동 생성
	CheatClass = UExCheatManager::StaticClass();
}
