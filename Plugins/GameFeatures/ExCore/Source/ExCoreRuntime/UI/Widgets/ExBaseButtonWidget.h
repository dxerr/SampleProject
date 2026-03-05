// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ExBaseButtonWidget.generated.h"

class UCommonTextBlock;

/**
 * 프로젝트 내 모든 버튼의 기본이 되는 CommonUI 호환 버튼입니다.
 * 
 * 마우스 클릭점과 패드/콘솔 포커스를 동시에 지원하며,
 * InputActionWidget (또는 연관된 아이콘 위젯) 이름을 강제하여
 * 호버링이나 클릭 시 자동으로 패드 액션 아이콘이 렌더링되게 구성합니다.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExBaseButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	// 에디터에서 텍스트를 바로 변경할 수 있도록 노출된 텍스트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExUI|Button", meta = (ExposeOnSpawn = "true"))
	FText ButtonText;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	// 버튼 내부의 텍스트 블록. BP에서 "ButtonTextBlock" 이름으로 필수로 바인딩되어야 합니다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ButtonTextBlock;

	/** 버튼 상태 변화(선택됨/포커스됨 등) 시 비주얼 업데이트 이벤트 */
	UFUNCTION(BlueprintImplementableEvent, Category="ExUI|Button", meta=(DisplayName="Update Button Visuals"))
	void BP_UpdateButtonVisuals();
};
