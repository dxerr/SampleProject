// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/IExAuthProvider.h"
#include "Core/IExNetServerStrategy.h"
#include "Core/ExNetworkTypes.h"
#include "Match/ExMatchTypes.h"
#include "ExOnlineSubsystem.generated.h"

class IOnlineSubsystem;

/** 로그인 완료 BP 델리게이트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExOnLoginCompleteDynDelegate, bool, bSuccess, const FString&, ErrorMessage);

/** 매칭 완료 BP 델리게이트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExOnMatchFoundDynDelegate, bool, bSuccess, const FString&, ErrorMessage);

/**
 * UExOnlineSubsystem
 *
 * ExNetwork 플러그인의 단일 진입점(Single Entry Gate).
 * GameInstanceSubsystem으로 구현되어 게임 전체 수명 동안 단일 인스턴스를 유지한다.
 */
UCLASS()
class EXNETWORKRUNTIME_API UExOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	// 인증 (Phase 2)
	// ------------------------------------------------------------------

	/** 현재 로그인 상태 */
	UFUNCTION(BlueprintPure, Category = "ExNetwork|Auth")
	bool IsLoggedIn() const;

	/** 로그인 완료 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Auth")
	FExOnLoginCompleteDynDelegate OnLoginComplete;

	/** 현재 서버 전략 타입 문자열 */
	UFUNCTION(BlueprintPure, Category = "ExNetwork|Server")
	FString GetServerTypeString() const;

	// ------------------------------------------------------------------
	// 매칭 (Phase 3)
	// ------------------------------------------------------------------

	/**
	 * Quick Match 시작.
	 * Lobby를 검색하여 빈 자리가 있으면 참가하고, 없으면 새로 생성한다.
	 * 완료 시 OnMatchFound 델리게이트 브로드캐스트.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExNetwork|Match")
	void FindQuickMatch(const FExMatchConfig& Config);

	/** 진행 중인 매칭 취소 및 Lobby 파괴 */
	UFUNCTION(BlueprintCallable, Category = "ExNetwork|Match")
	void CancelMatch();

	/** 현재 매칭 상태 */
	UFUNCTION(BlueprintPure, Category = "ExNetwork|Match")
	EExMatchState GetMatchState() const { return CurrentMatchState; }

	/** 매칭 완료 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Match")
	FExOnMatchFoundDynDelegate OnMatchFound;

private:

	IOnlineSubsystem* TryGetEOSSubsystem() const;
	void HandleOnlineSubsystemCreated(IOnlineSubsystem* NewSubsystem);
	void InitAuthProviderAndLogin(IOnlineSubsystem* OSS);
	void HandleAuthLoginComplete(bool bSuccess, const FString& ErrorMessage);

	FDelegateHandle SubsystemCreatedHandle;

	TUniquePtr<IExAuthProvider> AuthProvider;
	TUniquePtr<IExNetServerStrategy> ServerStrategy;

	EExMatchState CurrentMatchState = EExMatchState::Idle;
};
