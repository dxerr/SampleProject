// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ExModalWidget.generated.h"

UENUM(BlueprintType)
enum class EExModalResult : uint8
{
	Confirm,			// 메뉴의 '확인' 혹은 긍정적 결제 동의 액션
	Cancel,				// '취소'나 ESC 창 닫기
	Declined,			// 통신 실패나 거절 상태 통지
	Custom_1,
	Custom_2
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExModalResult, EExModalResult, Result);

/**
 * 게임 도중 팝업되는 다가가는 경고창, 에러 메시지 팝업, 
 * 서버 응답 통보(HTTP 통신 시 로딩창 등)를 만들기 위한 전용 베이스 클래스입니다.
 * 자신을 띄운 주체에게 닫힐 때 결과를 브로드캐스트하는 기능이 핵심입니다.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExModalWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	// 팝업은 기본적으로 무조건 입력을 낚아챕니다(가장 최상위 위젯). 
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/** 외부에서 Modal 종료 이벤트를 바인딩할 수 있게 제공하는 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "ExUI|Modal")
	FOnExModalResult OnModalResultDelegate;

	/**
	 * 모달 기능을 닫으면서 결과를 송출합니다. BP상의 뒤로가기 버튼 등에 연결해 사용합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExUI|Modal")
	void CloseModalWithResult(EExModalResult Result);

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** 모달 위젯이 시작될 때 애니메이션 훅 */
	UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On Modal Activated"))
	void BP_OnModalActivated();

	/** 모달 위젯이 끝날 때 애니메이션 훅 */
	UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On Modal Deactivated"))
	void BP_OnModalDeactivated();
};
