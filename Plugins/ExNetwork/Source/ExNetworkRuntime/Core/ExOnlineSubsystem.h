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

/** 게임 전환 시작 BP 델리게이트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExOnGameStartedDynDelegate, bool, bSuccess, const FString&, ErrorMessage);

/**
 * UExOnlineSubsystem
 *
 * ExNetwork 플러그인의 단일 진입점.
 * 외부 모듈은 이 클래스의 공개 API만 호출하면 된다.
 *
 * 사용 예:
 *   UExOnlineSubsystem* Net = GetGameInstance()->GetSubsystem<UExOnlineSubsystem>();
 *   Net->FindQuickMatch(Config);                       // 매칭 시작
 *   Net->OnMatchFound.AddDynamic(this, &MyFunc);       // 매칭 결과 수신
 *   Net->StartGame(Config);                            // 게임 전환 (매칭 완료 후 호출)
 *   Net->OnGameStarted.AddDynamic(this, &MyFunc);      // 게임 전환 결과 수신
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

	/** 현재 EOS 로그인 상태. 매칭 시작 전 확인 권장. */
	UFUNCTION(BlueprintPure, Category = "ExNetwork|Auth")
	bool IsLoggedIn() const;

	/** 로그인 완료 델리게이트. bSuccess=false 시 ErrorMessage 확인. */
	UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Auth")
	FExOnLoginCompleteDynDelegate OnLoginComplete;

	/** 현재 서버 전략 타입 문자열 ("ListenServer" / "DedicatedServer" / "None") */
	UFUNCTION(BlueprintPure, Category = "ExNetwork|Server")
	FString GetServerTypeString() const;

	// ------------------------------------------------------------------
	// 매칭 (Phase 3)
	// ------------------------------------------------------------------

	/**
	 * Quick Match를 시작한다.
	 * 빈 Lobby를 검색하여 참가하고, 없으면 새로 생성한다.
	 * 완료(성공/실패) 시 OnMatchFound 브로드캐스트.
	 *
	 * 실패 조건:
	 *   - 로그인 미완료
	 *   - 이미 매칭 진행 중
	 *   - EOS 서비스 오류
	 */
	UFUNCTION(BlueprintCallable, Category = "ExNetwork|Match")
	void FindQuickMatch(const FExMatchConfig& Config);

	/** 진행 중인 매칭을 취소하고 Lobby를 파괴한다. */
	UFUNCTION(BlueprintCallable, Category = "ExNetwork|Match")
	void CancelMatch();

	/** 현재 매칭 상태. */
	UFUNCTION(BlueprintPure, Category = "ExNetwork|Match")
	EExMatchState GetMatchState() const { return CurrentMatchState; }

	/**
	 * 매칭 완료 델리게이트.
	 * bSuccess=true: 매칭 성공, 게임 시작 가능 상태 (Ready)
	 * bSuccess=false: 매칭 실패, ErrorMessage에 원인
	 */
	UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Match")
	FExOnMatchFoundDynDelegate OnMatchFound;

	// ------------------------------------------------------------------
	// 게임 전환 (Phase 4)
	// ------------------------------------------------------------------

	/**
	 * 매칭 완료 후 게임 맵으로 전환한다.
	 * 내부적으로 Lobby를 파괴하고 ServerTravel을 수행한다.
	 * 서버 권한에서만 동작. 클라이언트가 호출하면 경고 후 무시.
	 *
	 * 완료(성공/실패) 시 OnGameStarted 브로드캐스트.
	 *
	 * 실패 조건:
	 *   - 로그인 미완료
	 *   - 매칭 Ready 상태가 아님
	 *   - Config.MapPath 비어있음
	 *   - 클라이언트에서 호출
	 *
	 * @param Config FindQuickMatch에 사용한 것과 동일한 Config.
	 *               Config.MapPath에 전환할 맵 경로 필수.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExNetwork|Match")
	void StartGame(const FExMatchConfig& Config);

	/**
	 * 게임 전환 델리게이트.
	 * bSuccess=true: ServerTravel 시작됨
	 * bSuccess=false: 전환 실패, ErrorMessage에 원인
	 */
	UPROPERTY(BlueprintAssignable, Category = "ExNetwork|Match")
	FExOnGameStartedDynDelegate OnGameStarted;

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
