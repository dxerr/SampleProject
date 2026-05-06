// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerPositionSyncComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UExRunnerPositionSyncComponent::UExRunnerPositionSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	
	// 컴포넌트 리플리케이션 활성화 (클라이언트로 전송)
	SetIsReplicatedByDefault(true);
}

void UExRunnerPositionSyncComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UExRunnerPositionSyncComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 서버의 위치를 클라이언트에게 브로드캐스트
	DOREPLIFETIME(UExRunnerPositionSyncComponent, ServerAuthLocation);
}

void UExRunnerPositionSyncComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 1. 서버 권한일 경우 자신의 위치를 업데이트
	if (OwnerActor->HasAuthority())
	{
		ServerAuthLocation = OwnerActor->GetActorLocation();
	}

	// 2. 디버그 드로우 로직
	const FVector LocalLocation = OwnerActor->GetActorLocation();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 서버 위치에 붉은색 캡슐 그리기 (HalfHeight=96, Radius=42)
	DrawDebugCapsule(World, ServerAuthLocation, 96.f, 42.f, FQuat::Identity, FColor::Red, false, -1.f, 0, 2.f);

	// 현재 로컬 위치와 서버 위치 선으로 잇기
	DrawDebugLine(World, LocalLocation, ServerAuthLocation, FColor::Orange, false, -1.f, 0, 1.f);

	// 서버 위치와 로컬 위치의 오차(Distance) 텍스트로 표시
	const float Distance = FVector::Dist(LocalLocation, ServerAuthLocation);
	FString DebugMsg = FString::Printf(TEXT("Server Loc Error: %.1f cm"), Distance);

	// 텍스트는 캐릭터 머리 위 쪽에 표시
	const FVector TextLocation = LocalLocation + FVector(0.f, 0.f, 120.f);
	DrawDebugString(World, TextLocation, DebugMsg, nullptr, FColor::Red, 0.f, false, 1.f);
}
