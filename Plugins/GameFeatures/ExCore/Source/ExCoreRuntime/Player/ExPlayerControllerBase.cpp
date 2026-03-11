// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ExPlayerControllerBase.h"
#include "Experience/ExExperienceManagerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Debug/ExCheatManager.h"
#include "GameModes/ExPlayerCameraManager.h"
#include "GameModes/ExGameModeBase.h"
#include "Experience/ExExperienceDefinition.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "UI/Widgets/ExHUDLayoutWidget.h"
#include "Engine/LocalPlayer.h"

DEFINE_LOG_CATEGORY_STATIC(LogExCorePC, Log, All);

AExPlayerControllerBase::AExPlayerControllerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 생성자에서 CheatClass 설정 → AddCheats() 시점에 UExCheatManager 자동 생성
	CheatClass = UExCheatManager::StaticClass();

	// 커스텀 카메라 매니저 설정 (스카이돔 추적 등 연출 최적화용)
	PlayerCameraManagerClass = AExPlayerCameraManager::StaticClass();
}

void AExPlayerControllerBase::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	// 에픽의 CommonUI 확장이나, 나중에 입력 제어 확장이 필요한 경우 여기서 부모 함수 호출 전/후로 처리합니다.
	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void AExPlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();
	// UI 로딩 대기 로직은 LocalPlayer가 완전히 할당된 이후인 ReceivedPlayer()로 이동했습니다.
}

void AExPlayerControllerBase::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] ReceivedPlayer called."));

	// 로컬 플레이어가 확실히 할당되었으므로, GameState의 ExperienceManager 로딩을 기다립니다.
	if (IsLocalController())
	{
		UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] IsLocalController = True."));
		if (AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] GameState is valid."));
			if (UExExperienceManagerComponent* ExpManager = GameState->GetComponentByClass<UExExperienceManagerComponent>())
			{
				UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] ExpManager found."));
				if (ExpManager->IsExperienceLoaded())
				{
					UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] Experience already loaded. Firing immediately."));
					OnExperienceLoadComplete();
				}
				else
				{
					UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] Experience NOT loaded yet. Binding callback."));
					ExpManager->OnExperienceLoadCompleteEvent.RemoveAll(this);
					ExpManager->OnExperienceLoadCompleteEvent.AddUObject(this, &ThisClass::OnExperienceLoadComplete);
				}
			}
			else
			{
				UE_LOG(LogExCorePC, Error, TEXT("[ExPlayerControllerBase] ExpManager is NULL on GameState!"));
			}
		}
		else
		{
			UE_LOG(LogExCorePC, Error, TEXT("[ExPlayerControllerBase] GameState is NULL during ReceivedPlayer!"));
		}
	}
}

void AExPlayerControllerBase::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] PostSeamlessTravel called."));

	// Seamless Travel 도착 시 로컬 플레이어는 이미 있으나, 맵(GameState)이 새로 바뀌었으므로 UI 재세팅 필요
	if (IsLocalController())
	{
		UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] Re-evaluating ExperienceManager after Seamless Travel."));
		if (AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			if (UExExperienceManagerComponent* ExpManager = GameState->GetComponentByClass<UExExperienceManagerComponent>())
			{
				if (ExpManager->IsExperienceLoaded())
				{
					UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] Experience already loaded on Travel. Firing immediately."));
					OnExperienceLoadComplete();
				}
				else
				{
					UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] Experience NOT loaded yet on Travel. Binding callback."));
					// 중복 바인드 방지를 위해 제거 후 등록
					ExpManager->OnExperienceLoadCompleteEvent.RemoveAll(this);
					ExpManager->OnExperienceLoadCompleteEvent.AddUObject(this, &ThisClass::OnExperienceLoadComplete);
				}
			}
		}
	}
}

void AExPlayerControllerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		if (UExExperienceManagerComponent* ExpManager = GameState->GetComponentByClass<UExExperienceManagerComponent>())
		{
			ExpManager->OnExperienceLoadCompleteEvent.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// ... existing code ...

void AExPlayerControllerBase::OnExperienceLoadComplete()
{
	UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] OnExperienceLoadComplete triggered."));

	// 내 환경이 세팅 끝났으니 UI 로드
	if (IsLocalController() && GetNetMode() != NM_DedicatedServer)
	{
		if (AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			if (UExExperienceManagerComponent* ExpManager = GameState->GetComponentByClass<UExExperienceManagerComponent>())
			{
				if (const UExExperienceDefinition* CurrentExperience = ExpManager->GetCurrentExperience())
				{
					if (CurrentExperience->DefaultHUDLayout)
					{
						// [Fix] RemoveAllViewportWidgets() 대신 UExUIManagerSubsystem을 사용하거나, 생성된 기존 뷰포트를 수동으로 정리하는 것이 더 안전합니다.
						// 여기서는 타 시스템 위젯을 지우지 않고 새 HUDLayout을 띄웁니다.
						// 필요 시 AExRunnerHUD처럼 멤버 변수로 UExHUDLayoutWidget* SpawnedHUD를 가지고 RemoveFromParent 처리합니다.

						UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] Creating HUD: %s"), *CurrentExperience->DefaultHUDLayout.ToString());
						UExHUDLayoutWidget* NewHUD = CreateWidget<UExHUDLayoutWidget>(this, CurrentExperience->DefaultHUDLayout.Get());
						if (NewHUD)
						{
							NewHUD->AddToViewport(); 
							UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] HUD Widget Created & Added to Viewport."));
						}
						else
						{
							UE_LOG(LogExCorePC, Error, TEXT("[ExPlayerControllerBase] CreateWidget FAILED for HUDLayout."));
						}
					}
					else
					{
						UE_LOG(LogExCorePC, Error, TEXT("[ExPlayerControllerBase] CurrentExperience->DefaultHUDLayout is NULL."));
					}
				}
				else
				{
					UE_LOG(LogExCorePC, Error, TEXT("[ExPlayerControllerBase] CurrentExperience is NULL."));
				}
			}
		}
	}
	else
	{
		UE_LOG(LogExCorePC, Warning, TEXT("[ExPlayerControllerBase] Skipping HUD creation - Not local controller or is Dedicated Server."));
	}

	// 서버에 준비되었음을 알림
	Server_NotifyReadyForMatch();
}

void AExPlayerControllerBase::Server_NotifyReadyForMatch_Implementation()
{
	// 서버 컨텍스트에서 실행됨
	// UI 로딩이 끝났으므로 매치 시작 조건을 재평가합니다.
	if (AExGameModeBase* GameMode = Cast<AExGameModeBase>(GetWorld()->GetAuthGameMode()))
	{
		GameMode->CheckAndStartMatch();
	}
}

bool AExPlayerControllerBase::Server_NotifyReadyForMatch_Validate()
{
	return true;
}
