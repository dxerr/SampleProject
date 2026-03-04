// Copyright ExFrameWork. All Rights Reserved.

#include "ExPlayerCameraManager.h"

#include "Kismet/GameplayStatics.h"

AExPlayerCameraManager::AExPlayerCameraManager()
	: Super()
{
	// 카메라 시스템은 틱이 매우 중요함
	PrimaryActorTick.bCanEverTick = true;
	
	TargetSkyDome = nullptr;
}

void AExPlayerCameraManager::BeginPlay()
{
	Super::BeginPlay();

	// TargetSkyDomeTag가 지정되어 있다면 맵에서 해당 태그를 가진 액터를 찾아 자동 등록
	if (TargetSkyDomeTag != NAME_None)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), TargetSkyDomeTag, FoundActors);
		
		if (FoundActors.Num() > 0 && IsValid(FoundActors[0]))
		{
			SetTargetSkyDome(FoundActors[0]);
			
			// 추가 편의성: 여러 개가 발견된 경우 경고 로그 출력
			if (FoundActors.Num() > 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ExPlayerCameraManager] 태그 '%s'를 가진 스카이돔 액터가 여러 개 발견되었습니다. 첫 번째 액터(%s)에 바인딩합니다."), *TargetSkyDomeTag.ToString(), *FoundActors[0]->GetName());
			}
		}
	}
}

void AExPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	// 기본 카메라 갱신 처리 (위치/회전 계산 완료)
	Super::UpdateCamera(DeltaTime);

	// 스카이돔 동기화 처리
	if (IsValid(TargetSkyDome))
	{
		FVector NewLocation = GetCameraLocation();

		if (bFollowXYOnly)
		{
			// X, Y는 카메라를 따라가고 Z는 설정된 유지 높이 적용
			NewLocation.Z = InitialSkyDomeZ;
		}

		// 카메라 프레임 내 즉시 이동 처리
		TargetSkyDome->SetActorLocation(NewLocation);
	}
}

void AExPlayerCameraManager::SetTargetSkyDome(AActor* InSkyDome)
{
	if (IsValid(InSkyDome))
	{
		TargetSkyDome = InSkyDome;
		InitialSkyDomeZ = InSkyDome->GetActorLocation().Z;
	}
	else
	{
		TargetSkyDome = nullptr;
	}
}
