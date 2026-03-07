// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ViewModels/ExPlayerStatsViewModel.h"
#include "ExRunnerStatsViewModel.generated.h"

class UExRunnerStatComponent;
class APlayerController;

/**
 * ExCore의 ExPlayerStatsViewModel 기능을 확장하여 RunnerPlay 모듈 전용 스탯(Speed, Distance 등)을 바인딩합니다.
 * Zero-Tick 이벤트 옵저버 패턴을 준수합니다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Ex Runner Stats ViewModel"))
class EXRUNNERPLAYRUNTIME_API UExRunnerStatsViewModel : public UExPlayerStatsViewModel
{
	GENERATED_BODY()

public:
	// -- UI가 관찰할 MVVM 바인딩 변수 (FieldNotify 속성 포함) --

	/** 현재 러너 스피드. View Bindings의 Source/Target으로 직접 사용 가능 */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess))
	float CurrentSpeed = 0.0f;

public:
	// -- Getter / Setter --

	float GetCurrentSpeed() const { return CurrentSpeed; }
	void SetCurrentSpeed(float NewSpeed);

	/** 현재 스피드를 텍스트 포맷으로 반환 (파생 계산값 - Source 전용) */
	UFUNCTION(BlueprintPure, FieldNotify, Category="ExUI|RunnerViewModel")
	FText GetCurrentSpeedText() const;

	// -- 바인딩 초기화 세팅 --

	/**
	 * 실제 관찰 대상 데이터 모델(StatComponent)을 전달받아 바인딩합니다.
	 * 위젯 생성 직후 OnInitialized 등에서 주입합니다.
	 */
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerViewModel")
	void InitializeRunnerBindings(UExRunnerStatComponent* InStatComponent);

	/**
	 * [Blueprint 단일 노드 편의 함수]
	 * PlayerController를 전달하면 내부에서 다음을 자동 처리합니다:
	 *   1. Controller → Pawn 탐색
	 *   2. Pawn 및 Pawn에 Attach된 모든 Actor에서 ExRunnerStatComponent 탐색
	 *   3. 찾은 StatComponent로 InitializeRunnerBindings 호출
	 *
	 * Core BP(ExSandboxCharacter_Mover)가 Runner 클래스를 직접 참조하지 않아도
	 * WBP_ExRunnerSpeedBar에서 한 노드로 초기화를 완료할 수 있습니다.
	 *
	 * @param InController  로컬 플레이어의 PlayerController (GetOwningPlayer() 전달)
	 */
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerViewModel")
	void AutoInitialize(APlayerController* InController);

private:
	// 상태 감지용 컴포넌트 포인터 (안전 참조)
	TWeakObjectPtr<UExRunnerStatComponent> BoundStatComponent;

	/**
	 * Stat Component 내부에서 스피드 변경 Broadcast가 울리면 호출되는 콜백
	 */
	UFUNCTION()
	void OnSpeedUpdated(float NewSpeed);
};
