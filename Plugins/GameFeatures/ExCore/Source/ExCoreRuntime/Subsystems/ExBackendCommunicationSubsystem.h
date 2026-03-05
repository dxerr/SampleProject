// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Http.h"
#include "ExBackendCommunicationSubsystem.generated.h"

/** API 요청 상태 */
UENUM(BlueprintType)
enum class EExRequestState : uint8
{
	Idle,
	Requesting,
	Success,
	Failed
};

// 로그인 시도 결과를 알리기 위한 델리게이트 쌍
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoginSuccess, const FString&, PlayerDisplayName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginFailed, int32, ErrorCode, const FString&, ErrorMessage);

/**
 * ExCore의 웹 통신(HTTP, IDP 인증 등)을 담당하는 전역 서브시스템.
 */
UCLASS()
class EXCORERUNTIME_API UExBackendCommunicationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 외부 IDP 시스템 로그인 시도 (가상) */
	UFUNCTION(BlueprintCallable, Category = "ExBackend")
	void RequestLogin(const FString& Username, const FString& Password);

	UFUNCTION(BlueprintPure, Category = "ExBackend")
	EExRequestState GetCurrentRequestState() const { return CurrentRequestState; }

public:
	UPROPERTY(BlueprintAssignable, Category = "ExBackend")
	FOnLoginSuccess OnLoginSuccess;

	UPROPERTY(BlueprintAssignable, Category = "ExBackend")
	FOnLoginFailed OnLoginFailed;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "ExBackend|Config")
	int32 MaxRetryCount = 3;

	UPROPERTY(EditDefaultsOnly, Category = "ExBackend|Config")
	float RequestTimeout = 10.0f;

private:
	EExRequestState CurrentRequestState;

	// (향후 구현 시) Http 모듈 객체 참조 보관
	// FHttpModule* Http;
};
