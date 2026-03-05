// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ExWindowWidget.generated.h"

/**
 * 인벤토리, 상점, 메인 메뉴 등 전체 또는 부분 화면을 덮는 핵심 창 액티비티 위젯입니다.
 * MenuStack 에 Push 될 기본 대상이며, 기본적으로 'Menu(UI)' 전용 입력 모드를 가집니다.
 * 이 창이 떠있는 동안에는 캐릭터 조작이 불가능한 것이 원칙입니다.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExWindowWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	// 위젯 기본 입력 세팅 (오직 UI 포커스 및 마우스 커서 렌더링)
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** BP 쪽 비주얼 세팅용 (등장 애니메이션 등) */
	UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On Window Activated"))
	void BP_OnWindowActivated();

	/** BP 쪽 비주얼 세팅용 (퇴장 애니메이션 등) */
	UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On Window Deactivated"))
	void BP_OnWindowDeactivated();

	/** 
	 * 뒤로가기 액션 처리
	 * 보통 키보드 ESC 나 패드의 B(동그라미) 버튼에 의해 자동 반응합니다. 
	 * 반환 결과에 따라 Event Consume 여부가 결정됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI")
	virtual FEventReply OnHandleBackAction();

	// 뒤로가기 전용 바인드 핸들 (CommonUI Action)
	FUIActionBindingHandle BackHandle;
};
