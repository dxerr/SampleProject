// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystems/ExBackendCommunicationSubsystem.h"

void UExBackendCommunicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	CurrentRequestState = EExRequestState::Idle;
	
	// Http = &FHttpModule::Get(); // 필요 시 초기화
}

void UExBackendCommunicationSubsystem::Deinitialize()
{
	OnLoginSuccess.Clear();
	OnLoginFailed.Clear();

	Super::Deinitialize();
}

void UExBackendCommunicationSubsystem::RequestLogin(const FString& Username, const FString& Password)
{
	if (CurrentRequestState == EExRequestState::Requesting)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExBackendCommunicationSubsystem] 이미 요청 중입니다."));
		return;
	}

	CurrentRequestState = EExRequestState::Requesting;
	UE_LOG(LogTemp, Log, TEXT("[ExBackendCommunicationSubsystem] %s 유저로 로그인 요청을 시작합니다."), *Username);

	// TODO: 실제 HTTP 요청(FHttpRequestRef) 코드를 작성, 실패 시 지수 백오프 기반의 재시도 로직 포함

	// 현재 구현은 뼈대이므로 즉시 성공 시뮬레이션을 수행합니다.
	// 실제 환경에서는 OnResponseReceived 콜백 내부에서 상태를 변경해야 합니다.
	CurrentRequestState = EExRequestState::Success;
	OnLoginSuccess.Broadcast(Username + TEXT("_DisplayName"));
}
