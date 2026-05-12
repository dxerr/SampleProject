// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/IExAuthProvider.h"
#include "Core/IExNetServerStrategy.h"
#include "ExOnlineSubsystem.generated.h"

class IOnlineSubsystem;

/**
 * 로그인 완료 시 BP에서 구독 가능한 Dynamic Multicast 델리게이트.
 * C++ 구독용은 Events/ExNetEvents.h 의 FExOnLoginCompleteDelegate 사용.
 *
 * 주의: DECLARE_DYNAMIC_MULTICAST_DELEGATE 는 .generated.h 바로 앞에 있어야
 *       UHT가 정상 처리한다. 별도 헤더로 분리하면 UHT 에러 발생.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExOnLoginCompleteDynDelegate, bool, bSuccess, const FString&, ErrorMessage);

/**
 * UExOnlineSubsystem
 *
 * ExNetwork 플러그인의 단일 진입점(Single Entry Gate).
 * GameInstanceSubsystem으로 구현되어 게임 전체 수명 동안 단일 인스턴스를 유지한다.
 *
 * Phase 2 기능:
 *   - EOS OSS 준비 완료 시점을 OnOnlineSubsystemCreated 콜백으로 정확히 감지
 *   - EOS Device ID 자동 로그인
 *   - 서버 환경(Listen/Dedicated) 자동 감지 및 Strategy 선택
 */
UCLASS()
class EXNETWORKRUNTIME_API UExOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 현재 로그인 상태를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "ExNetwork|Auth")
	bool IsLoggedIn() const;

	/** 로그인 완료 시 브로드캐스트. BP와 C++ 모두에서 구독 가능. */
	UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Auth")
	FExOnLoginCompleteDynDelegate OnLoginComplete;

	/** 현재 서버 전략 타입 문자열을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "ExNetwork|Server")
	FString GetServerTypeString() const;

private:

	/** EOS OSS 획득 시도. NULL 서브시스템이면 nullptr 반환. */
	IOnlineSubsystem* TryGetEOSSubsystem() const;

	/** OnOnlineSubsystemCreated 콜백 — EOS 생성 시점에 정확히 호출됨 */
	void HandleOnlineSubsystemCreated(IOnlineSubsystem* NewSubsystem);

	/** OSS 확정 후 AuthProvider 생성 및 로그인 시작 */
	void InitAuthProviderAndLogin(IOnlineSubsystem* OSS);

	/** 인증 완료 콜백 */
	void HandleAuthLoginComplete(bool bSuccess, const FString& ErrorMessage);

	/** OnOnlineSubsystemCreated 구독 핸들 (Deinitialize 시 해제) */
	FDelegateHandle SubsystemCreatedHandle;

	/** 인증 Provider (EOS 구현체) */
	TUniquePtr<IExAuthProvider> AuthProvider;

	/** 서버 모델 Strategy */
	TUniquePtr<IExNetServerStrategy> ServerStrategy;
};
