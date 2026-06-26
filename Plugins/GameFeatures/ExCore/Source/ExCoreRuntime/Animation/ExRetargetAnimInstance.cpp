// Copyright ExFrameWork. All Rights Reserved.

#include "ExRetargetAnimInstance.h"
#include "Retargeter/IKRetargetProfile.h"
#include "Retargeter/RetargetOps/BlendToSourceOp.h"
#include "Retargeter/IKRetargeter.h"
#include "Components/SkeletalMeshComponent.h"

// ─── 링크 전략 ────────────────────────────────────────────────────────────────
// UIKRetargetBlendToSourceController 는 UCLASS(MinimalAPI) 로 선언되어
// GetSettings() / SetSettings() 가 DLL 외부로 export 되지 않는다.
// URetargetProfileLibrary::GetOpControllerFromRetargetProfile 도 마찬가지.
//
// 해결: 컨트롤러를 통하지 않고 FRetargetProfile::RetargetOpProfiles 내부의
//       FInstancedStruct 를 직접 포인터(GetMutablePtr)로 변경한다.
//   - 값 복사(FIKRetargetBlendToSourceOpSettings Settings = ...) 를 하지 않으므로
//     copy ctor / dtor → vtable 참조 → LNK2001 이 발생하지 않는다.
//   - GetOpSettingsByTypeInProfile<T> 는 헤더 inline 템플릿 → 링크 불필요.
// ─────────────────────────────────────────────────────────────────────────────

void UExRetargetAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	// IKRetargeter 및 RetargetProfile 초기화는 매 프레임 UpdateRetargeterFromTag에서 처리
}

void UExRetargetAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// BP EventGraph: IKRetargeter_Map 태그 조회 + Copy Retarget Profile from Retarget Asset
	UpdateRetargeterFromTag();

	// BP UpdateRetargetProfile 함수: 커브 기반 BlendToSource Lerp
	UpdateBlendToSourceOp();
}

void UExRetargetAnimInstance::UpdateRetargeterFromTag()
{
	// BP EventGraph 이식:
	// Get Owning Component → Component Tags[0] → IKRetargeter_Map.FIND → SET IKRetargeter
	// → Copy Retarget Profile from Retarget Asset → SET RetargetProfile

	USkeletalMeshComponent* MeshComp = GetOwningComponent();
	if (!MeshComp || MeshComp->ComponentTags.IsEmpty())
	{
		IKRetargeter = nullptr;
		return;
	}

	const FName Tag = MeshComp->ComponentTags[0];
	TObjectPtr<UIKRetargeter>* Found = RetargeterAssetMap.Find(Tag);

	IKRetargeter = (Found && *Found) ? *Found : nullptr;

	if (IKRetargeter)
	{
		// 매 프레임 에셋 기본값으로 RetargetProfile 리셋
		// BP: Copy Retarget Profile from Retarget Asset(IKRetargeter) → SET RetargetProfile
		// FillProfileWithAssetSettings 는 IKRIG_API 로 export 됨.
		RetargetProfile.FillProfileWithAssetSettings(IKRetargeter);
	}
}

void UExRetargetAnimInstance::UpdateBlendToSourceOp()
{
	if (!IKRetargeter)
	{
		return;
	}

	// ── 포인터 직접 접근 (값 복사 금지) ──────────────────────────────────────
	// GetOpSettingsByTypeInProfile<T> : 헤더 inline 템플릿
	//   내부적으로 FInstancedStruct::GetMutablePtr<T>() 를 사용 → reinterpret_cast
	//   vtable 참조 없음.
	TArray<FIKRetargetBlendToSourceOpSettings*> MatchingOps;
	RetargetProfile.GetOpSettingsByTypeInProfile<FIKRetargetBlendToSourceOpSettings>(MatchingOps);

	for (FIKRetargetBlendToSourceOpSettings* Settings : MatchingOps)
	{
		if (!Settings)
		{
			continue;
		}

		// 체인별 커브 Lerp 적용 (BP UpdateRetargetProfile Then 1 Loop 이식)
		for (FIKRetargetBlendToSourceChainSettings& Chain : Settings->Chains)
		{
			const FName* CurveName = ChainCurveMap.Find(Chain.TargetChainName);
			if (!CurveName)
			{
				// False 분기: 맵에 없는 체인 → 원본 설정 유지
				continue;
			}

			const float Alpha = GetCurveValue(*CurveName);

			// Lerp BlendToSource → 1.0 (소스 포즈로 완전 블렌드)
			// BP: Lerp(A=현재값, B=1.0, Alpha=커브값)
			Chain.BlendToSource = FMath::Lerp(Chain.BlendToSource, 1.0, Alpha);

			// Lerp TranslationPerAxisAlpha → ZeroVector (per-axis 가중치 해제)
			// BP: Lerp(Vector)(A=현재값, B=(0,0,0), Alpha=커브값)  ← B 스크린샷 확인
			Chain.TranslationPerAxisAlpha = FMath::Lerp(
				Chain.TranslationPerAxisAlpha, FVector::ZeroVector, static_cast<double>(Alpha));

			// TranslationAlpha, RotationAlpha, ApplyPelvisOffset → passthrough (수정 안 함)
		}
	}
}
