// Copyright ExFrameWork. All Rights Reserved.

#include "ExLobbyMatchViewModel.h"
#include "ExOnlineSubsystem.h"        // ExNetworkRuntime: PublicIncludePaths에 Core/ 등록
#include "ExMatchTypes.h"              // ExNetworkRuntime: PublicIncludePaths에 Match/ 등록
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "UI/Widgets/ExPopupWidget.h"
#include "UI/Data/ExPopupDescriptor.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogExLobbyMatchVM, Log, All);

const FString UExLobbyMatchViewModel::DefaultMatchMode = TEXT("Runner");

// ─────────────────────────────────────────────────────────────────────────────
// 초기화
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::AutoInitialize(UObject* WorldContextObject)
{
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

void UExLobbyMatchViewModel::StartQuickMatch()
{
	// 중복 호출 방지
	if (bIsMatching)
	{
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] StartQuickMatch: 이미 매칭 진행 중 — 무시됩니다."));
		return;
	}

	if (!CachedOnlineSubsystem)
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] StartQuickMatch: OnlineSubsystem이 없습니다. AutoInitialize 호출 여부를 확인하세요."));
		return;
	}

	// 로그인 미완료 시 차단 — EOS Connect Login 완료 전 Lobby 작업은 반드시 실패
	if (!CachedOnlineSubsystem->IsLoggedIn())
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] StartQuickMatch: EOS 로그인 완료 대기 중입니다. 잠시 후 다시 시도하세요."));
		ShowResultPopup(
			FText::FromString(TEXT("로그인 준비 중")),
			FText::FromString(TEXT("EOS 서버에 연결 중입니다.\n잠시 후 다시 시도하세요."))
		);
		return;
	}


	bIsMatching = true;
	UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] StartQuickMatch 시작 — MatchMode=%s, MaxPlayers=%d"),
		*DefaultMatchMode, DefaultMaxPlayers);

	// 1. 대기 팝업 표시
	ShowMatchingPopup();

	// 2. OnMatchFound 델리게이트 바인딩 (중복 방지를 위해 먼저 제거)
	CachedOnlineSubsystem->OnMatchFound.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
	CachedOnlineSubsystem->OnMatchFound.AddDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);

	// 3. FindQuickMatch 호출
	FExMatchConfig Config;
	Config.MatchMode = DefaultMatchMode;
	Config.MaxPlayers = DefaultMaxPlayers;
	Config.MapPath = TEXT("");

	CachedOnlineSubsystem->FindQuickMatch(Config);
}

// ─────────────────────────────────────────────────────────────────────────────
// ExOnlineSubsystem 콜백
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::OnMatchFoundCallback(bool bSuccess, const FString& ErrorMessage)
{
	bIsMatching = false;

	// 델리게이트 바인딩 해제 (1회성 처리)
	if (CachedOnlineSubsystem)
	{
		CachedOnlineSubsystem->OnMatchFound.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
	}

	// 대기 팝업 닫기
	CloseMatchingPopup();

	if (bSuccess)
	{
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] 매칭 완료 — 성공."));
		ShowResultPopup(
			FText::FromString(TEXT("매칭 완료!")),
			FText::FromString(TEXT("상대 플레이어를 찾았습니다!\n게임을 준비합니다."))
		);
		// ── Phase 4 연결 자리 ──────────────────────────────────────────
		// CachedOnlineSubsystem->StartGameSession(Config.MapPath);
	}
	else
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] 매칭 실패 — %s"), *ErrorMessage);
		ShowResultPopup(
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
	ActiveMatchingPopup = nullptr;

	// OnMatchFound 바인딩 해제
	if (CachedOnlineSubsystem)
	{
		CachedOnlineSubsystem->OnMatchFound.RemoveDynamic(this, &UExLobbyMatchViewModel::OnMatchFoundCallback);
		CachedOnlineSubsystem->CancelMatch();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 내부 헬퍼
// ─────────────────────────────────────────────────────────────────────────────

void UExLobbyMatchViewModel::ShowMatchingPopup()
{
	UExUIManagerSubsystem* UIMgr = GetUIManager();
	if (!UIMgr) return;

	ActiveMatchingPopup = UIMgr->ShowAcknowledgeBP(
		FText::FromString(TEXT("매칭 중...")),
		FText::FromString(TEXT("상대 플레이어를 찾는 중입니다.\n\n취소하려면 아래 버튼을 누르세요."))
	);

	if (ActiveMatchingPopup)
	{
		// 팝업 버튼 클릭(취소) 결과 수신
		ActiveMatchingPopup->OnPopupResult.AddDynamic(this, &UExLobbyMatchViewModel::OnMatchingPopupResult);
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
		UE_LOG(LogExLobbyMatchVM, Log, TEXT("[ExLobbyMatchVM] EOS 로그인 완료 — 이제 StartQuickMatch 호출 가능."));
	}
	else
	{
		UE_LOG(LogExLobbyMatchVM, Warning, TEXT("[ExLobbyMatchVM] EOS 로그인 실패 — %s"), *ErrorMessage);
	}
}
