// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerGameState.h"
#include "../Components/ExPathManager.h"
#include "Net/UnrealNetwork.h"

AExRunnerGameState::AExRunnerGameState()
{
	// PathManager 생성
	PathManager = CreateDefaultSubobject<UExPathManager>(TEXT("PathManager"));
	
	// GameState에서 컴포넌트 리플리케이션을 위해 Replicates 활성화
	bReplicates = true;
	if (PathManager)
	{
		PathManager->SetIsReplicated(true);
	}
}

void AExRunnerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExRunnerGameState, CurrentPathDistance);
	DOREPLIFETIME(AExRunnerGameState, RealPlayerPathDistance);
}
