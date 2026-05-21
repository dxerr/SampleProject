// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystems/ExGameFlowSubsystem.h"
#include "Tags/ExFlowTags.h"
#include "Experience/ExExperienceDefinition.h"
#include "Misc/PackageName.h"
#include "ExOnlineSubsystem.h"
#include "UI/Subsystems/ExUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"


DEFINE_LOG_CATEGORY_STATIC(LogExGameFlow, Log, All);

void UExGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 초기 상태 지정
	CurrentFlowState = ExFlowTags::Flow_Boot;

	// 허용되는 상태 전이 맵 (Transition Map) 구성
	// Boot -> Auth.IDP (정식 인증 경로) 또는 Lobby (개발/인증 우회 경로)
	AllowedTransitions.Add(ExFlowTags::Flow_Boot, { ExFlowTags::Flow_Auth_IDP, ExFlowTags::Flow_Lobby });
	
	// Auth.IDP -> Lobby 또는 Boot(재시도)
	AllowedTransitions.Add(ExFlowTags::Flow_Auth_IDP, { ExFlowTags::Flow_Lobby, ExFlowTags::Flow_Boot });
	
	// Lobby -> InGame 또는 Auth.IDP(로그아웃)
	AllowedTransitions.Add(ExFlowTags::Flow_Lobby, { ExFlowTags::Flow_InGame, ExFlowTags::Flow_Auth_IDP });
	
	// InGame -> Lobby(복귀)
	AllowedTransitions.Add(ExFlowTags::Flow_InGame, { ExFlowTags::Flow_Lobby });

	// 네트워크 실패 이벤트 구독을 다음 틱으로 지연 (의존성 타이밍 문제 회피)
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float) -> bool
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UExOnlineSubsystem* OnlineSub = GI->GetSubsystem<UExOnlineSubsystem>())
			{
				OnlineSub->OnMatchConnectionFailed.AddDynamic(this, &UExGameFlowSubsystem::OnMatchConnectionFailed);
				UE_LOG(LogExGameFlow, Log, TEXT("[ExGameFlowSubsystem] OnMatchConnectionFailed 바인딩 완료."));
			}
		}
		return false;
	}), 0.1f);
}

void UExGameFlowSubsystem::Deinitialize()
{
	// 델리게이트 바인딩 해제
	OnFlowStateChanged.Clear();
	OnRequestTravel.Clear();

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Deinitialize();
}

void UExGameFlowSubsystem::SetFlowState(FGameplayTag NewState)
{
	if (CurrentFlowState == NewState)
	{
		return;
	}

	// 상태 전이 유효성 검사
	bool bIsAllowedTransition = false;
	if (const TArray<FGameplayTag>* ValidNextStates = AllowedTransitions.Find(CurrentFlowState))
	{
		bIsAllowedTransition = ValidNextStates->Contains(NewState);
	}

	if (!bIsAllowedTransition)
	{
		UE_LOG(LogExGameFlow, Warning, TEXT("[ExGameFlowSubsystem] Invalid transition attempted from %s to %s"), 
			*CurrentFlowState.ToString(), *NewState.ToString());
		return;
	}

	FGameplayTag OldState = CurrentFlowState;
	CurrentFlowState = NewState;

	// 변경 델리게이트 브로드캐스트
	OnFlowStateChanged.Broadcast(OldState, CurrentFlowState);

	UE_LOG(LogExGameFlow, Log, TEXT("[ExGameFlowSubsystem] Flow State Changed: %s -> %s"), 
		*OldState.ToString(), *CurrentFlowState.ToString());
}

void UExGameFlowSubsystem::RequestTravel(const FString& MapURL)
{
	// 짧은 맵 이름(예: L_ExRunnerTest)은 패키징 빌드에서 인식되지 않으므로
	// FPackageName을 통해 전체 패키지 경로로 변환을 시도합니다.
	FString ResolvedURL = MapURL;
	if (FPackageName::IsShortPackageName(MapURL))
	{
		FString FullPath;
		if (FPackageName::SearchForPackageOnDisk(MapURL, &FullPath))
		{
			// 파일 경로 → 패키지 경로로 변환 (예: .../L_ExRunnerTest.umap → /ExRunnerPlay/Map/L_ExRunnerTest)
			FPackageName::TryConvertFilenameToLongPackageName(FullPath, ResolvedURL);
			UE_LOG(LogExGameFlow, Log, TEXT("[ExGameFlowSubsystem] Map name resolved: %s -> %s"), *MapURL, *ResolvedURL);
		}
		else
		{
			UE_LOG(LogExGameFlow, Warning, TEXT("[ExGameFlowSubsystem] Could not resolve short map name '%s'. Make sure the map is cooked and included in the build."), *MapURL);
		}
	}

	// GameMode 등 델리게이트를 수신할 수 있는 체계로 알림
	UE_LOG(LogExGameFlow, Log, TEXT("[ExGameFlowSubsystem] Requesting Travel to %s"), *ResolvedURL);
	OnRequestTravel.Broadcast(ResolvedURL);
}

void UExGameFlowSubsystem::TransitionToExperience(const UExExperienceDefinition* ExperienceConfig)
{
	if (!ExperienceConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("[ExGameFlow] TransitionToExperience 실패: ExperienceConfig가 유효하지 않습니다."));
		return;
	}

	if (ExperienceConfig->MapToLoad.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[ExGameFlow] TransitionToExperience 실패: %s 데이터 에셋 안에 대상 MapToLoad이 지정되지 않았습니다."), *ExperienceConfig->GetName());
		return;
	}

	// TSoftObjectPtr<UWorld> 경로에서 로드용 URL 스트링 추출 (.umap을 제외하고 패키지 이름만 추출)
	FString MapURL = ExperienceConfig->MapToLoad.ToSoftObjectPath().GetLongPackageName();

	UE_LOG(LogTemp, Log, TEXT("[ExGameFlow] Experience [%s] 전환 요청. 대상 맵: %s"), *ExperienceConfig->GetName(), *MapURL);

	SetFlowState(ExFlowTags::Flow_InGame);
	OnRequestTravel.Broadcast(MapURL);
}

void UExGameFlowSubsystem::OnMatchConnectionFailed(const FString& ErrorMessage)
{
	UE_LOG(LogExGameFlow, Error, TEXT("[ExGameFlowSubsystem] 네트워크 접속 실패 수신: %s"), *ErrorMessage);

	// 맵 트래블 진행 중(로비로 복귀)일 수 있으므로, 에러 메시지를 캐싱하고 맵 로드 완료 후에 띄웁니다.
	PendingErrorMessage = ErrorMessage;

	if (!PostLoadMapHandle.IsValid())
	{
		PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([this](UWorld* LoadedWorld)
		{
			if (!PendingErrorMessage.IsEmpty() && LoadedWorld)
			{
				// UI 생성을 위해 한 프레임(또는 소폭) 지연
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float) -> bool
				{
					if (UGameInstance* GI = GetGameInstance())
					{
						if (APlayerController* PC = GI->GetFirstLocalPlayerController())
						{
							if (ULocalPlayer* LP = PC->GetLocalPlayer())
							{
								if (UExUIManagerSubsystem* UIManager = LP->GetSubsystem<UExUIManagerSubsystem>())
								{
									UIManager->ShowAcknowledgeBP(
										FText::FromString(TEXT("서버 연결 실패")),
										FText::FromString(PendingErrorMessage)
									);
									PendingErrorMessage.Empty();
								}
							}
						}
					}
					return false;
				}), 0.5f);
			}

			// 1회용 호출 후 바인딩 해제
			FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
			PostLoadMapHandle.Reset();
		});
	}
}
