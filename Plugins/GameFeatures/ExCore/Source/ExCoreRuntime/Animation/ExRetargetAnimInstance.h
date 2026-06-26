// Copyright ExFrameWork. All Rights Reserved.
//
// UExRetargetAnimInstance
// IK Retarget BlendToSource op을 커브 기반으로 런타임 제어하는 AnimInstance 기반 클래스.
// ABP_Ex_GenericRetarget / ABP_GenericRetarget의 EventGraph + UpdateRetargetProfile BP 함수를 C++로 이식.
//
// 사용법:
//   1. ABP의 Parent Class를 이 클래스로 변경
//   2. RetargeterAssetMap에 (ComponentTag명 → IKRetargeter 에셋) 매핑 설정  ← BP IKRetargeter_Map에 대응
//   3. ChainCurveMap에 (체인명 → 커브명) 매핑 설정
//   4. ABP EventGraph의 모든 노드 삭제 (C++에서 자동 처리)
//   5. ABP AnimGraph의 RetargetPoseFromMesh 노드 "Custom Retarget Profile" 핀을 RetargetProfile 프로퍼티에 바인딩
//
// 관련 문서: Md/Migrat/ExCore/IKRetarget_AnimInstance_CPP_Port.md

#pragma once

#include "Animation/AnimInstance.h"
#include "Retargeter/IKRetargetProfile.h"
#include "ExRetargetAnimInstance.generated.h"

class UIKRetargeter;

/**
 * IK Retarget 커브 기반 BlendToSource 제어 AnimInstance.
 *
 * 매 프레임:
 *   1. SkeletalMeshComponent의 ComponentTags[0]으로 RetargeterAssetMap을 조회 → IKRetargeter 결정
 *   2. IKRetargeter 에셋의 기본 Op 설정으로 RetargetProfile 리셋
 *   3. ChainCurveMap에 등록된 체인의 BlendToSource / TranslationPerAxisAlpha를 커브 값으로 Lerp
 */
UCLASS()
class EXCORERUNTIME_API UExRetargetAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/**
	 * ABP AnimGraph의 RetargetPoseFromMesh 노드 "Custom Retarget Profile" 핀에 바인딩.
	 * 매 프레임 에셋 기본값으로 리셋 후 커브 Lerp가 적용된다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	FRetargetProfile RetargetProfile;

	/**
	 * ComponentTag명 → IKRetargeter 에셋 매핑.
	 * BP EventGraph의 IKRetargeter_Map 변수에 대응.
	 * 태그가 없거나 맵에 없으면 해당 캐릭터는 리타겟 되지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retarget")
	TMap<FName, TObjectPtr<UIKRetargeter>> RetargeterAssetMap;

	/**
	 * 체인명 → 애니메이션 커브명 매핑.
	 * 맵에 등록된 체인만 해당 커브 값으로 BlendToSource Lerp가 적용된다.
	 * BP UpdateRetargetProfile 함수의 Chain_Curve_Map에 대응.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Retarget")
	TMap<FName, FName> ChainCurveMap;

	/**
	 * 현재 활성화된 IKRetargeter 에셋. 매 프레임 태그 조회로 갱신된다.
	 * BP의 IKRetargeter 변수에 대응.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Retarget")
	TObjectPtr<UIKRetargeter> IKRetargeter;

private:
	/**
	 * BP EventGraph 로직 이식:
	 * ComponentTags[0] → RetargeterAssetMap 조회 → IKRetargeter 갱신
	 * → FillProfileWithAssetSettings (프로파일 리셋)
	 */
	void UpdateRetargeterFromTag();

	/**
	 * BP UpdateRetargetProfile 함수 이식:
	 * "Blend to Source" op의 체인별 BlendToSource / TranslationPerAxisAlpha를
	 * ChainCurveMap 커브 값 기반으로 Lerp 갱신.
	 */
	void UpdateBlendToSourceOp();
};
