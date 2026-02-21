// Copyright ExFrameWork. All Rights Reserved.
// ExCore 기본 PlayerController — CheatManager 바인딩 포인트

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ExPlayerController.generated.h"

/**
 * AExPlayerController
 * ExFrameWork 기본 PlayerController
 * 
 * 역할:
 * - CheatClass에 UExCheatManager를 설정하여 치트 시스템 활성화
 * - 향후 프로젝트 공통 PlayerController 기능 확장 가능
 */
UCLASS()
class EXCORERUNTIME_API AExPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AExPlayerController();
};
