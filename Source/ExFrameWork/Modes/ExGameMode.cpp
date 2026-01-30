// Copyright ExFrameWork. All Rights Reserved.

#include "ExGameMode.h"
#include "../Data/Modes/ExGameModeDataSet.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

AExGameMode::AExGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AExGameMode::BeginPlay()
{
	Super::BeginPlay();
	
}

void AExGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AExGameMode::SpawnLocalPlayer(AController* NewPlayer)
{
	if (!NewPlayer || !GameModeDataSet)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 1. 컨테이너 폰 스폰 (이미 존재하는 경우 생략 가능하지만, 명시적 스폰 로직 구현)
	if (GameModeDataSet->ContainerPawnClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		
		FVector SpawnLocation = FVector::ZeroVector;
		FRotator SpawnRotation = FRotator::ZeroRotator;

		// 플레이어 스타트 등 적절한 위치 찾기 (생략 시 0,0,0)
		AActor* PlayerStart = FindPlayerStart(NewPlayer);
		if (PlayerStart)
		{
			SpawnLocation = PlayerStart->GetActorLocation();
			SpawnRotation = PlayerStart->GetActorRotation();
		}

		APawn* ContainerPawn = World->SpawnActor<APawn>(GameModeDataSet->ContainerPawnClass, SpawnLocation, SpawnRotation, SpawnParams);
		
		if (ContainerPawn)
		{
			// 2. 컨트롤러 빙의
			NewPlayer->Possess(ContainerPawn);

			// 3. 로컬 플레이어 액터(비주얼) 스폰 및 부착
			if (GameModeDataSet->LocalPlayerClass)
			{
				AActor* VisualActor = World->SpawnActor<AActor>(GameModeDataSet->LocalPlayerClass, SpawnLocation, SpawnRotation, SpawnParams);
				if (VisualActor)
				{
					// 컨테이너가 캐릭터라면 Mesh에 부착
					ACharacter* ContainerCharacter = Cast<ACharacter>(ContainerPawn);
					if (ContainerCharacter)
					{
						VisualActor->AttachToComponent(ContainerCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale);
						
						// 컨테이너 메시 숨기기 (껍데기 역할이므로 숨김)
						ContainerCharacter->GetMesh()->SetHiddenInGame(true);
						ContainerCharacter->GetMesh()->SetVisibility(false); 
					}
					else
					{
						// 일반 폰이라면 RootComponent에 부착
						VisualActor->AttachToComponent(ContainerPawn->GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
					}
				}
			}
		}
	}
}
