// Copyright ExFrameWork. All Rights Reserved.

#include "ExListenServerStrategy.h"
#include "Core/ExNetworkLog.h"

FExListenServerStrategy::FExListenServerStrategy()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 생성됨 — Listen Server 모드."));
}

EExServerType FExListenServerStrategy::GetServerType() const
{
	return EExServerType::ListenServer;
}

void FExListenServerStrategy::CreateMatch()
{
	// Phase 3에서 EOS Lobby 생성 로직으로 채워진다.
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] CreateMatch — Phase 3에서 구현 예정."));
}

void FExListenServerStrategy::JoinMatch(const FString& SessionId)
{
	// Phase 3에서 EOS Lobby 참가 로직으로 채워진다.
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] JoinMatch — SessionId=%s, Phase 3에서 구현 예정."), *SessionId);
}

void FExListenServerStrategy::StartGameSession(const FString& MapPath)
{
	// Phase 4에서 ServerTravel 호출로 채워진다.
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] StartGameSession — MapPath=%s, Phase 4에서 구현 예정."), *MapPath);
}

void FExListenServerStrategy::DestroyMatch()
{
	// Phase 3에서 EOS Lobby 삭제 로직으로 채워진다.
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] DestroyMatch — Phase 3에서 구현 예정."));
}
