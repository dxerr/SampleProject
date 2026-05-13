// Copyright ExFrameWork. All Rights Reserved.

#include "ExListenServerStrategy.h"
#include "Core/ExNetworkLog.h"
#include "Providers/IExLobbyProvider.h"
#include "Engine/World.h"

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
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] JoinMatch — FindAndJoinOrCreate 사용 권장."));
}

void FExListenServerStrategy::StartGameSession(const FString& MapPath, UWorld* World)
{
	if (!ensureMsgf(World, TEXT("[ExListenServerStrategy] StartGameSession: World 없음.")))
	{
		return;
	}

	// 서버 권한 체크 — 클라이언트는 ServerTravel 호출 금지
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogExNetwork, Warning, TEXT("[ExListenServerStrategy] StartGameSession: 클라이언트는 호출 불가. 서버만 ServerTravel 수행."));
		return;
	}

	if (!ensureMsgf(!MapPath.IsEmpty(), TEXT("[ExListenServerStrategy] StartGameSession: MapPath가 비어있습니다.")))
	{
		return;
	}

	// Lobby 명시적 파괴 — 새 플레이어 참가 차단 후 게임 전환
	if (LobbyProvider && LobbyProvider->IsInLobby())
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] StartGameSession: Lobby 파괴 후 ServerTravel 진행."));
		LobbyProvider->DestroyLobby();
	}

	// ServerTravel — ?listen 옵션으로 클라이언트가 새 맵에서도 접속 유지
	// UE 엔진이 연결된 모든 클라이언트를 자동으로 새 맵으로 이동시킨다.
	const FString TravelURL = MapPath + TEXT("?listen");
	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] ServerTravel 실행 — URL=%s"), *TravelURL);

	World->ServerTravel(TravelURL);
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

	UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Quick Match 시작 — Lobby 검색 중..."));

	LobbyProvider->OnFindComplete.AddLambda(
		[this, Config, OnComplete](bool bSuccess, int32 ResultCount)
		{
			LobbyProvider->OnFindComplete.Clear();
			OnFindComplete(bSuccess, ResultCount, Config, OnComplete);
		}
	);

	LobbyProvider->FindLobbies(Config);
}

void FExListenServerStrategy::OnFindComplete(bool bSuccess, int32 ResultCount, FExMatchConfig Config, TFunction<void(bool, const FString&)> OnComplete)
{
	if (bSuccess && ResultCount > 0)
	{
		UE_LOG(LogExNetwork, Log, TEXT("[ExListenServerStrategy] Lobby %d개 발견 — 첫 번째 Lobby 참가 시도."), ResultCount);

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
