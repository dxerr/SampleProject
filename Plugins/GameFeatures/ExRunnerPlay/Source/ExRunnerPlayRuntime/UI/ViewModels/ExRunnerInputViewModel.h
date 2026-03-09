// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ExRunnerInputViewModel.generated.h"

/**
 * UI 터치 버튼 입력(ViewBinding)을 수신하여 
 * 로컬 플레이어의 UExRunnerInputComponent 로 라우팅해주는 브릿지 뷰모델
 */
UCLASS(BlueprintType)
class EXRUNNERPLAYRUNTIME_API UExRunnerInputViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// UI 점프 버튼 클릭 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnJumpButtonClicked();

	// UI 슬라이드 버튼 클릭 시 (ViewBinding 이벤트)
	UFUNCTION(BlueprintCallable, Category="ExUI|RunnerInput")
	void OnSlideButtonClicked();

private:
	// 안전하게 로컬 플레이어의 Runner Input Component를 가져오는 내부 헬퍼
	class UExRunnerInputComponent* GetRunnerInputComponent() const;
};
