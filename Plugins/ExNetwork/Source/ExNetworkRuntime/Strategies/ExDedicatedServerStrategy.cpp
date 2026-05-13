// Copyright ExFrameWork. All Rights Reserved.

#include "ExDedicatedServerStrategy.h"
#include "Core/ExNetworkLog.h"

FExDedicatedServerStrategy::FExDedicatedServerStrategy()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExDedicatedServerStrategy] 생성됨 — Dedicated Server 모드. Phase 4 이후 구현 예정."));
}

EExServerType FExDedicatedServerStrategy::GetServerType() const
{
	return EExServerType::DedicatedServer;
}

void FExDedicatedServerStrategy::CreateMatch(const FExMatchConfig& Config)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] CreateMatch — 미구현 (Phase 4+)."));
}

void FExDedicatedServerStrategy::JoinMatch(const FString& SessionId)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] JoinMatch — 미구현 (Phase 4+). SessionId=%s"), *SessionId);
}

void FExDedicatedServerStrategy::StartGameSession(const FString& MapPath)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] StartGameSession — 미구현 (Phase 4+). MapPath=%s"), *MapPath);
}

void FExDedicatedServerStrategy::DestroyMatch()
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] DestroyMatch — 미구현 (Phase 4+)."));
}
