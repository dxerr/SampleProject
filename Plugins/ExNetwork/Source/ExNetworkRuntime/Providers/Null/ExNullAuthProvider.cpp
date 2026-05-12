// Copyright ExFrameWork. All Rights Reserved.

#include "ExNullAuthProvider.h"
#include "Core/ExNetworkLog.h"

FExNullAuthProvider::FExNullAuthProvider()
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExNullAuthProvider] 생성됨 — 오프라인 테스트 모드. EOS 인증이 동작하지 않습니다."));
}

void FExNullAuthProvider::Login(int32 LocalUserNum)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExNullAuthProvider] Login 시뮬레이션 — LocalUserNum=%d, 즉시 성공 처리."), LocalUserNum);
	bLoggedIn = true;
	OnLoginComplete.Broadcast(true, TEXT(""));
}

void FExNullAuthProvider::Logout(int32 LocalUserNum)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExNullAuthProvider] Logout 시뮬레이션 — LocalUserNum=%d"), LocalUserNum);
	bLoggedIn = false;
	OnLogoutComplete.Broadcast(true);
}

bool FExNullAuthProvider::IsLoggedIn(int32 LocalUserNum) const
{
	return bLoggedIn;
}
