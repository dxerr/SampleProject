// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ExBaseButtonWidget.generated.h"

class UCommonTextBlock;
class UBorder;

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

	// 배경 영역 전용 알파값 (텍스트에는 영향을 주지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExUI|Button", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackgroundAlpha = 1.0f;

	/** 외부에서 배경 알파를 동적으로 변경할 때 사용 */
	UFUNCTION(BlueprintCallable, Category = "ExUI|Button")
	void SetBackgroundAlpha(float InAlpha);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	/** 현재 BackgroundAlpha 값을 배경 위젯에 실제 적용 */
	void ApplyBackgroundAlpha();

	// 버튼 내부의 텍스트 블록. BP에서 "ButtonTextBlock" 이름으로 필수로 바인딩되어야 합니다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ButtonTextBlock;

	// 버튼의 실제 렌더링 영역(배경). BP에서 "ButtonBackground" 이름으로 스타일을 적용한 Border 등을 바인딩합니다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ButtonBackground;

	/** 버튼 상태 변화(선택됨/포커스됨 등) 시 비주얼 업데이트 이벤트 */
	UFUNCTION(BlueprintImplementableEvent, Category="ExUI|Button", meta=(DisplayName="Update Button Visuals"))
	void BP_UpdateButtonVisuals();
};
