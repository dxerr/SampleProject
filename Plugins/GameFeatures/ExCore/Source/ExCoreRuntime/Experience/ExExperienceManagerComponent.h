// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "ExExperienceManagerComponent.generated.h"

class UExExperienceDefinition;

/**
 * GameState에 부착되어 현재 레벨(경험)에 필요한 UI와 시스템을 
 * 데이터 주도(Data-Driven) 방식으로 로딩하고 관리하는 컴포넌트입니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EXCORERUNTIME_API UExExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UExExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 로딩이 완료되었을 때 호출할 델리게이트 (외부 서브시스템이나 GameMode에서 대기할 때 사용)
	DECLARE_MULTICAST_DELEGATE(FOnExperienceLoadComplete);
	FOnExperienceLoadComplete OnExperienceLoadCompleteEvent;

	// 서버 전용: 맵 로드 시 이 경험 데이터를 설정하고 리플리케이트 시킵니다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="ExExperience")
	void ServerSetCurrentExperience(const UExExperienceDefinition* InExperience);

	// 클라이언트/서버에서 현재 경험 데이터 가져오기
	UFUNCTION(BlueprintPure, Category="ExExperience")
	const UExExperienceDefinition* GetCurrentExperience() const { return CurrentExperience; }

	// 현재 로딩이 끝났는지 여부
	UFUNCTION(BlueprintPure, Category="ExExperience")
	bool IsExperienceLoaded() const { return bLoadComplete; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 어떤 경험 데이터 에셋을 로드할지 결정 (서버에서 설정 후 복제됨)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperience)
	TObjectPtr<const UExExperienceDefinition> CurrentExperience;

	UFUNCTION()
	void OnRep_CurrentExperience();

private:
	// 내부적으로 로딩 시작
	void StartExperienceLoad();

	// 로딩(동기/비동기 모두 포함)이 끝났을 때의 처리 (UI 스택 등록 등)
	void OnExperienceLoadComplete();

	bool bLoadComplete = false;
};
