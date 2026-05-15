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

void FExDedicatedServerStrategy::StartGameSession(const FString& MapPath, UWorld* World)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] StartGameSession — 미구현 (Phase 4+). MapPath=%s"), *MapPath);
}

void FExDedicatedServerStrategy::DestroyMatch()
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] DestroyMatch — 미구현 (Phase 4+)."));
}

void FExDedicatedServerStrategy::CancelMatch()
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] CancelMatch — 미구현."));
}

void FExDedicatedServerStrategy::BeginSearchPhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnSearchComplete)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] BeginSearchPhase — 미구현."));
}

void FExDedicatedServerStrategy::EndSearchPhase()
{
}

void FExDedicatedServerStrategy::BeginCreatePhase(const FExMatchConfig& Config, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnCreateComplete)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] BeginCreatePhase — 미구현."));
}

void FExDedicatedServerStrategy::EndCreatePhase()
{
}

void FExDedicatedServerStrategy::BeginJoinPhase(const FExMatchConfig& Config, const FString& SessionId, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnJoinComplete)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] BeginJoinPhase — 미구현."));
}

void FExDedicatedServerStrategy::EndJoinPhase()
{
}

void FExDedicatedServerStrategy::BeginWaitPhase(const FExMatchConfig& Config, bool bIsHostFlag, EExMatchState ExpectedState, TFunction<void(bool, const FString&)> OnReadyCallback)
{
	UE_LOG(LogExNetwork, Warning, TEXT("[ExDedicatedServerStrategy] BeginWaitPhase — 미구현."));
}

void FExDedicatedServerStrategy::EndWaitPhase()
{
}

void FExDedicatedServerStrategy::ResetTransientState()
{
}

bool FExDedicatedServerStrategy::IsHost() const
{
	return true; // Dedicated server is always host
}

FString FExDedicatedServerStrategy::GetConnectString() const
{
	return TEXT("");
}
