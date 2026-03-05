// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ExHUDLayoutWidget.generated.h"

// 전방 선언
class UCommonActivatableWidgetStack;
class UExWindowWidget;

/**
 * 게임의 루트 스크린을 책임지는 HUD 레이아웃용 베이스 클래스입니다.
 * 내부에 GameStack과 MenuStack을 보유하며 활성화 시 UExUIManagerSubsystem에 이를 등록합니다.
 * ZOrder를 통해 GameStack(하위)과 MenuStack(상위)을 BP 단에서 분리 구성해야 합니다.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class EXCORERUNTIME_API UExHUDLayoutWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	/**
	 * HUD Layout이 활성화될 때의 기본 입력 설정 정책을 반환합니다.
	 * (CommonUI에서는 수동 SetInputMode 대신 이 함수를 오버라이드하여 결정합니다.)
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/**
	 * 블루프린트 디자이너가 초기화 타이밍에 시각적 애니메이션 등을 구성할 수 있도록 노출된 이벤트
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On HUD Activated"))
	void BP_OnHUDActivated();

	/**
	 * HUD가 화면에서 사라질 때 정리용 이벤트
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="ExUI", meta=(DisplayName="On HUD Deactivated"))
	void BP_OnHUDDeactivated();

	/**
	 * 게임 인 게임(퀘스트 트래커, 미니맵, 데미지 플로팅 등) 화면을 담는 컨테이너 스택
	 * BP에서 반드시 'GameStack'이라는 이름으로 위젯을 배치하여 바인딩해야 합니다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> GameStack;

	/**
	 * 인벤토리, 플레이어 탭, 일시정지, 게임 안내 팝업을 담는 컨테이너 스택
	 * BP에서 반드시 'MenuStack'이라는 이름으로 위젯을 배치하여 바인딩해야 합니다. ZOrder가 GameStack보다 높아야 합니다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

	/**
	 * ESC 버튼이나 패드의 뒤로가기/스타트 버튼을 눌렀을 때 기본적으로 불려질 
	 * 최상단 일시정지 메뉴(WindowWidget)의 클래스. BP에서 기본값을 지정합니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="ExUI")
	TSubclassOf<UExWindowWidget> EscapeMenuClass;

private:
	/**
	 * 로컬 플레이어 기반 UExUIManagerSubsystem을 찾아 현재의 스택 포인터들을 주입시키는 등록 함수
	 */
	void RegisterStacksToManager();
};
