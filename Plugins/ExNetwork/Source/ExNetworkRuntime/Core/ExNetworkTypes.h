// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 서버 모델 타입.
 * IExNetServerStrategy와 ExMatchTypes 양쪽에서 참조하므로
 * 별도 헤더로 분리하여 순환 include를 방지한다.
 * UENUM이 아닌 순수 C++ enum class로 정의하여
 * .generated.h 없이도 어디서든 include 가능하다.
 */
enum class EExServerType : uint8
{
	ListenServer,
	DedicatedServer,
};
