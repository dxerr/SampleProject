// Copyright ExFrameWork. All Rights Reserved.

#include "ExListenServerStrategy.h"
#include "Core/ExNetworkLog.h"
#include "Providers/IExLobbyProvider.h"

FExListenServerStrategy::FExListenServerStrategy()
{
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 생성됨 — Listen Server 모드."));
}

EExServerType FExListenServerStrategy::GetServerType() const
{
	return EExServerType::ListenServer;
}

void FExListenServerStrategy::SetLobbyProvider(TUniquePtr<IExLobbyProvider> InLobbyProvider)
{
	LobbyProvider = MoveTemp(InLobbyProvider);
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] LobbyProvider 주입 완료."));
}

void FExListenServerStrategy::CreateMatch(const FExMatchConfig& Config)
{
	if (!ensureMsgf(LobbyProvider, TEXT("[ExListenServerStrategy] CreateMatch: LobbyProvider 없음.")))
	{
		return;
	}
	LobbyProvider->CreateLobby(Config);
}

void FExListenServerStrategy::JoinMatch(const FString& SessionId)
{
	// Phase 3: ResultIndex 기반으로 변경됨. 직접 호출은 FindAndJoinOrCreate 사용 권장.
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] JoinMatch 호출 — FindAndJoinOrCreate 사용 권장."));
}

void FExListenServerStrategy::StartGameSession(const FString& MapPath)
{
	// Phase 4에서 ServerTravel 호출로 채워진다.
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] StartGameSession — MapPath=%s, Phase 4에서 구현 예정."), *MapPath);
}

void FExListenServerStrategy::DestroyMatch()
{
	if (!LobbyProvider)
	{
		return;
	}
	LobbyProvider->DestroyLobby();
}

void FExListenServerStrategy::FindAndJoinOrCreate(const FExMatchConfig& Config, TFunction<void(bool, const FString&)> OnComplete)
{
	if (!ensureMsgf(LobbyProvider, TEXT("[ExListenServerStrategy] FindAndJoinOrCreate: LobbyProvider 없음.")))
	{
		OnComplete(false, TEXT("LobbyProvider not set"));
		return;
	}

	// Config를 멤버에 값 복사 — 호출자 스택이 해제된 후에도 비동기 체인 전체에서 유효하게 유지
	PendingConfig = Config;

	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Quick Match 시작 — Lobby 검색 중..."));

	// FindLobbies 완료 콜백 바인딩 (PendingConfig 참조 — 멤버이므로 항상 유효)
	LobbyProvider->OnFindComplete.AddLambda(
		[this, OnComplete](bool bSuccess, int32 ResultCount)
		{
			LobbyProvider->OnFindComplete.Clear();
			OnFindComplete(bSuccess, ResultCount, PendingConfig, OnComplete);
		}
	);

	LobbyProvider->FindLobbies(PendingConfig);
}


void FExListenServerStrategy::OnFindComplete(bool bSuccess, int32 ResultCount, FExMatchConfig Config, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess && ResultCount > 0)
	{
		// 빈 Lobby 발견 → 참가
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby %d개 발견 — 첫 번째 Lobby에 참가 시도."), ResultCount);

		LobbyProvider->OnJoinComplete.AddLambda(
			[this, OnComplete](bool bJoinSuccess, const FString& ErrorMessage)
			{
				LobbyProvider->OnJoinComplete.Clear();
				OnJoinComplete(bJoinSuccess, ErrorMessage, OnComplete);
			}
		);

		LobbyProvider->JoinLobby(0);
	}
	else
	{
		// 빈 Lobby 없음 → 새로 생성
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 빈 Lobby 없음 — 새 Lobby 생성 중..."));

		LobbyProvider->OnCreateComplete.AddLambda(
			[this, OnComplete](bool bCreateSuccess, const FString& ErrorMessage)
			{
				LobbyProvider->OnCreateComplete.Clear();
				OnCreateComplete(bCreateSuccess, ErrorMessage, OnComplete);
			}
		);

		LobbyProvider->CreateLobby(Config);
	}
}

void FExListenServerStrategy::OnCreateComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 생성 성공 — 상대 플레이어 대기 중."));
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 생성 실패 — %s"), *ErrorMessage);
	}
	OnComplete(bSuccess, ErrorMessage);
}

void FExListenServerStrategy::OnJoinComplete(bool bSuccess, const FString& ErrorMessage, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby 참가 성공 — 매칭 완료."));
	}
	else
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] Lobby 참가 실패 — %s"), *ErrorMessage);
	}
	OnComplete(bSuccess, ErrorMessage);
}

void FExListenServerStrategy::CancelMatch()
{
	if (LobbyProvider && LobbyProvider->IsInLobby())
	{
		LobbyProvider->DestroyLobby();
	}
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] 매칭 취소."));
}
