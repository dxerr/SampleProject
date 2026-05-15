// Copyright ExFrameWork. All Rights Reserved.

#include "ExLobbyMatchViewModel.h"
#include "ExOnlineSubsystem.h"        
#include "ExMatchTypes.h"              
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "Subsystems/ExDataCenterSubsystem.h"
#include "Data/ExRunnerConfig.h"
#include "UI/Widgets/ExPopupWidget.h"
#include "UI/Data/ExPopupDescriptor.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogExLobbyMatchVM, Log, All);

const FString UExLobbyMatchViewModel::DefaultMatchMode = TEXT("Runner");
const FString UExLobbyMatchViewModel::DefaultMapPath   = TEXT("/ExRunnerPlay/Map/L_ExRunnerTest");

// ─────────────────────────────────────────────────────────────────────────────
// 초기화
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::AutoInitialize(UObject* WorldContextObject)
{
	if (CachedOnlineSubsystem)
	{
		// 이미 초기화된 인스턴스입니다. (예: 팝업이 닫히면서 뷰가 재활성화된 경우)
		return;
	}

	if (!WorldContextObject)
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] AutoInitialize: WorldContextObject가 없습니다."));
		return;
	}

	// Widget->GetOwningLocalPlayer()로 LocalPlayer 직접 캐싱 — GetUIManager()에서 사용
	// ULocalPlayerSubsystem은 GetFirstPlayerController()가 아닌 소유 LocalPlayer 기준으로 접근해야 함
	if (UUserWidget* Widget = Cast<UUserWidget>(WorldContextObject))
	{
		CachedLocalPlayer = Widget->GetOwningLocalPlayer();
		if (!CachedLocalPlayer.IsValid())
		{
			UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] AutoInitialize: Widget에서 LocalPlayer를 얻지 못했습니다."));
		}
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return;

	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;

	CachedOnlineSubsystem = GI->GetSubsystem<UExOnlineSubsystem>();
	if (!CachedOnlineSubsystem)
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] AutoInitialize: UExOnlineSubsystem을 찾을 수 없습니다."));
		return;
	}

	// 인게임에서 로비로 돌아온 경우 (MatchState가 Idle이 아닐 때) 상태 초기화
	if (CachedOnlineSubsystem->GetMatchState() != EExMatchState::Idle)
	{
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] AutoInitialize: 이전 매칭 상태가 남아있어 초기화 (ResetMatchState) 진행."));
		CachedOnlineSubsystem->ResetMatchState();
	}

	// 로그인 미완료 시 OnLoginComplete 구독 — 버튼 클릭 타이밍 문제 대비
	if (!CachedOnlineSubsystem->IsLoggedIn())
	{
		CachedOnlineSubsystem->OnLoginComplete.AddDynamic(this, &UExLobbyMatchViewModel::OnLoginCompleteCallback);
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] AutoInitialize: 로그인 대기 중 — OnLoginComplete 구독 등록."));
	}

	UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] AutoInitialize 완료. 로그인 상태: %s"),
		CachedOnlineSubsystem->IsLoggedIn() ? TEXT("로그인됨") : TEXT("대기 중"));
}

// ─────────────────────────────────────────────────────────────────────────────
// 퍼블릭 API — View에서 호출할 단일 진입점
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::StartMultiPlay()
{
	if (bIsMatching)
	{
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] StartMultiPlay: 이미 매칭 진행 중 — 무시됩니다."));
		return;
	}

	if (!CachedOnlineSubsystem)
	{
		UE_LOG(LogExLobbyMatchVM, Error, TEXT("[ExLobbyMatchVM] StartMultiPlay: OnlineSubsystem이 없습니다. View(BP)의 OnActivated에서 AutoInitialize가 호출되었는지 확인하세요."));
		return;
	}

	if (!CachedOnlineSubsystem->IsLoggedIn())
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] StartMultiPlay: EOS 로그인 완료 대기 중입니다."));
		bPendingStartMultiPlay = true;  
		ShowResultPopup(
			FText::FromString(TEXT("로그인 준비 중")),
			FText::FromString(TEXT("EOS 서버에 연결 중입니다.\n연결 완료 후 자동으로 매칭을 시작합니다."))
		);
		return;
	}

	bIsMatching = true;
	RetryCount = 0;
	UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] StartMultiPlay 시작"));

	ShowMatchingPopup(false);

	PendingConfig = FExMatchConfig{};
	PendingConfig.MatchMode   = DefaultMatchMode;
	PendingConfig.MaxPlayers  = DefaultMaxPlayers;
	PendingConfig.MapPath     = DefaultMapPath;   

	UWorld* World = CachedOnlineSubsystem->GetWorld();
	if (World)
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UExDataCenterSubsystem* DataCenter = GI->GetSubsystem<UExDataCenterSubsystem>())
			{
				if (const UExRunnerConfig* Config = DataCenter->GetConfig<UExRunnerConfig>())
				{
					PendingConfig.ExpectedPlayerCount = Config->MatchFlow.ExpectedPlayerCount;
					PendingConfig.MaxWaitForPlayersSeconds = Config->MatchFlow.MaxWaitForPlayersSeconds;
				}
			}
		}
	}
	PendingConfig.bIsSinglePlay = false;

	CachedOnlineSubsystem->OnMatchFound.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
	CachedOnlineSubsystem->OnMatchFound.AddDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
	CachedOnlineSubsystem->OnGameStarted.RemoveDynamic(this, &UExLobbyMatchViewModel::OnGameStartedCallback);
	CachedOnlineSubsystem->OnGameStarted.AddDynamic(this, &UExLobbyMatchViewModel::OnGameStartedCallback);

	CachedOnlineSubsystem->FindQuickMatch(PendingConfig);
}

void UExLobbyMatchViewModel::StartSinglePlay()
{
	if (bIsMatching) return;
	if (!CachedOnlineSubsystem)
	{
		UE_LOG(LogExLobbyMatchVM, Error, TEXT("[ExLobbyMatchVM] StartSinglePlay: OnlineSubsystem이 없습니다. View(BP)의 OnActivated에서 AutoInitialize가 호출되었는지 확인하세요."));
		return;
	}

	if (!CachedOnlineSubsystem->IsLoggedIn())
	{
		bPendingStartSinglePlay = true;  
		ShowResultPopup(
			FText::FromString(TEXT("로그인 준비 중")),
			FText::FromString(TEXT("EOS 서버에 연결 중입니다.\n연결 완료 후 자동으로 시작합니다."))
		);
		return;
	}

	bIsMatching = true;
	RetryCount = 0;
	ShowMatchingPopup(true);

	PendingConfig = FExMatchConfig{};
	PendingConfig.MatchMode   = DefaultMatchMode;
	PendingConfig.MaxPlayers  = DefaultMaxPlayers;
	PendingConfig.MapPath     = DefaultMapPath;   
	PendingConfig.ExpectedPlayerCount = 1;
	PendingConfig.bIsSinglePlay = true;

	CachedOnlineSubsystem->OnMatchFound.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
	CachedOnlineSubsystem->OnMatchFound.AddDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
	CachedOnlineSubsystem->OnGameStarted.RemoveDynamic(this, &UExLobbyMatchViewModel::OnGameStartedCallback);
	CachedOnlineSubsystem->OnGameStarted.AddDynamic(this, &UExLobbyMatchViewModel::OnGameStartedCallback);

	CachedOnlineSubsystem->FindQuickMatch(PendingConfig);
}

// ─────────────────────────────────────────────────────────────────────────────
// ExOnlineSubsystem 콜백
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::OnMatchFoundCallback(bool bSuccess, const FString& ErrorMessage)
{
	if (!bSuccess && ErrorMessage == TEXT("Timeout"))
	{
		if (RetryCount < 2)
		{
			RetryCount++;
			UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] 매칭 타임아웃. 재시도 %d/2"), RetryCount);
			
			if (ActiveMatchingPopup)
			{
				FText NewDesc = FText::FromString(FString::Printf(TEXT("상대 플레이어를 찾는 중입니다...\n\n[ 참가 인원 : 1 / %d 명 ]\n[ 검색 시도 : %d / 3 회 ]\n\n취소하려면 아래 버튼을 누르세요."), PendingConfig.ExpectedPlayerCount, RetryCount + 1));
				ActiveMatchingPopup->UpdateBodyText(NewDesc);
			}

			CachedOnlineSubsystem->FindQuickMatch(PendingConfig);
			return;
		}
	}

	bIsMatching = false;

	if (CachedOnlineSubsystem)
	{
		CachedOnlineSubsystem->OnMatchFound.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
	}

	CloseMatchingPopup();

	if (bSuccess)
	{
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] 매칭 완료 — 성공. StartGame 호출."));
		ShowResultPopup(
			FText::FromString(TEXT("매칭 완료!")),
			FText::FromString(TEXT("상대 플레이어를 찾았습니다!\n게임을 시작합니다..."))
		);

		CachedOnlineSubsystem->StartGame(PendingConfig);
	}
	else
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] 매칭 실패 — %s"), *ErrorMessage);
		ShowErrorPopup(
			FText::FromString(TEXT("매칭 실패")),
			FText::FromString(FString::Printf(TEXT("다시 시도해 주세요.\n(%s)"), *ErrorMessage))
		);
	}
}

void UExLobbyMatchViewModel::OnMatchingPopupResult(EExModalResult Result, const FText& InputText)
{
	// 팝업이 이미 닫힌 상태라면 무시 (OnMatchFoundCallback이 먼저 닫은 경우)
	if (!bIsMatching)
	{
		return;
	}

	// 취소 버튼 클릭 = 매칭 중단
	UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] 사용자가 매칭을 취소했습니다."));

	bIsMatching = false;
	
	if (ActiveMatchingPopup)
	{
		ActiveMatchingPopup->OnPopupResult.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchingPopupResult);
	}
	ActiveMatchingPopup = nullptr;

	// OnMatchFound / OnGameStarted 바인딩 해제
	if (CachedOnlineSubsystem)
	{
		CachedOnlineSubsystem->OnMatchFound.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
		CachedOnlineSubsystem->OnGameStarted.RemoveDynamic(this, &UExLobbyMatchViewModel::OnGameStartedCallback);
		CachedOnlineSubsystem->CancelMatch();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 내부 헬퍼
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::ShowMatchingPopup(bool bIsSinglePlay)
{
	UExUIManagerSubsystem* UIMgr = GetUIManager();
	if (!UIMgr) return;

	FText Title = bIsSinglePlay ? FText::FromString(TEXT("시작 준비중입니다.")) : FText::FromString(TEXT("매칭 중..."));
	FText Desc = bIsSinglePlay ? FText::FromString(TEXT("게임 시작을 준비 중입니다.\n\n취소하려면 아래 버튼을 누르세요.")) 
                               : FText::FromString(FString::Printf(TEXT("상대 플레이어를 찾는 중입니다...\n\n[ 참가 인원 : 1 / %d 명 ]\n[ 검색 시도 : %d / 3 회 ]\n\n취소하려면 아래 버튼을 누르세요."), PendingConfig.ExpectedPlayerCount, RetryCount + 1));

	ActiveMatchingPopup = UIMgr->ShowAcknowledgeBP(
		Title,
		Desc
	);

	if (ActiveMatchingPopup)
	{
		// 팝업 버튼 클릭(취소) 결과 수신
		ActiveMatchingPopup->OnPopupResult.AddUniqueDynamic(this, &UExLobbyMatchViewModel::OnMatchingPopupResult);
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] 매칭 대기 팝업 표시 완료."));
	}
	else
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] ShowMatchingPopup: UIManager가 팝업을 생성하지 못했습니다."));
	}
}

void UExLobbyMatchViewModel::CloseMatchingPopup()
{
	if (ActiveMatchingPopup)
	{
		// 팝업 결과 바인딩 해제 후 닫기
		ActiveMatchingPopup->OnPopupResult.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchingPopupResult);
		ActiveMatchingPopup->DeactivateWidget();
		ActiveMatchingPopup = nullptr;
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] 매칭 대기 팝업 닫힘."));
	}
}

void UExLobbyMatchViewModel::ShowResultPopup(const FText& Title, const FText& Body)
{
	UExUIManagerSubsystem* UIMgr = GetUIManager();
	if (!UIMgr) return;

	// Info 타입: 버튼 없이 3초 자동 닫힘
	UIMgr->ShowInfoBP(Title, Body, 3.0f);
}

void UExLobbyMatchViewModel::ShowErrorPopup(const FText& Title, const FText& Body)
{
	UExUIManagerSubsystem* UIMgr = GetUIManager();
	if (!UIMgr) return;

	// 에러 타입: 사용자가 반드시 '확인' 버튼을 눌러야 닫히는 Acknowledge 모달
	UIMgr->ShowAcknowledgeBP(Title, Body);
}

UExUIManagerSubsystem* UExLobbyMatchViewModel::GetUIManager() const
{
	// CachedLocalPlayer 우선 사용 — AutoInitialize에서 Widget->GetOwningLocalPlayer()로 캐싱됨
	// GetFirstPlayerController()는 서버 PC를 반환할 수 있어 LocalPlayer가 nullptr인 경우가 있음
	ULocalPlayer* LP = CachedLocalPlayer.Get();
	if (!LP)
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] GetUIManager: CachedLocalPlayer가 유효하지 않습니다."));
		return nullptr;
	}

	return LP->GetSubsystem<UExUIManagerSubsystem>();
}

// ─────────────────────────────────────────────────────────────────────────────
// 로그인 콜백
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::OnLoginCompleteCallback(bool bSuccess, const FString& ErrorMessage)
{
	// 콜백 구독 해제 (1회성)
	if (CachedOnlineSubsystem)
	{
		CachedOnlineSubsystem->OnLoginComplete.RemoveDynamic(this, &UExLobbyMatchViewModel::OnLoginCompleteCallback);
	}

	if (bSuccess)
	{
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] EOS 로그인 완료."));

		if (bPendingStartMultiPlay)
		{
			bPendingStartMultiPlay = false;
			StartMultiPlay();
		}
		else if (bPendingStartSinglePlay)
		{
			bPendingStartSinglePlay = false;
			StartSinglePlay();
		}
	}
	else
	{
		bPendingStartMultiPlay = false;
		bPendingStartSinglePlay = false;
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] EOS 로그인 실패 — %s"), *ErrorMessage);
		ShowErrorPopup(
			FText::FromString(TEXT("연결 실패")),
			FText::FromString(FString::Printf(TEXT("EOS 서버 연결에 실패했습니다.\n(%s)"), *ErrorMessage))
		);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 4: 게임 시작 콜백
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::OnGameStartedCallback(bool bSuccess, const FString& ErrorMessage)
{
	// 콜백 구독 해제 (1회성)
	if (CachedOnlineSubsystem)
	{
		CachedOnlineSubsystem->OnGameStarted.RemoveDynamic(this, &UExLobbyMatchViewModel::OnGameStartedCallback);
	}

	if (bSuccess)
	{
		// 성공: 서버가 ServerTravel을 수행하거나 클라이언트가 연결 대기 중
		// 맵 전환이 자동으로 이루어지므로 별도 UI 처리 불필요
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] StartGame 성공 — ServerTravel 대기 중."));
	}
	else
	{
		// 실패: 상태 리셋 후 오류 팝업 표시
		bIsMatching = false;
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] StartGame 실패 — %s"), *ErrorMessage);
		ShowErrorPopup(
			FText::FromString(TEXT("게임 시작 실패")),
			FText::FromString(FString::Printf(TEXT("게임을 시작하지 못했습니다.\n(%s)"), *ErrorMessage))
		);
	}
}
