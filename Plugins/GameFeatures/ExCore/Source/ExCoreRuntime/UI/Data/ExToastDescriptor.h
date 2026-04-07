#pragma once

#include "CoreMinimal.h"
#include "ExToastDescriptor.generated.h"

/**
 * Toast 프로그레스바 설정
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExToastProgressConfig
{
    GENERATED_BODY()

    // 프로그레스바 표시 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast|Progress")
    bool bShowProgressBar = false;

    // true: Duration에 연동하여 자동 감소
    // false: 외부에서 SetProgress()로 수동 제어 (로딩용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast|Progress",
        meta=(EditCondition="bShowProgressBar", EditConditionHides))
    bool bAutoCountdown = true;
};

/**
 * Toast 전용 Descriptor
 */
USTRUCT(BlueprintType)
struct EXCORERUNTIME_API FExToastDescriptor
{
    GENERATED_BODY()

    // Toast 메시지 (RichText 마크업 사용 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
    FText Message;

    // 자동 닫힘 시간 (초). 0이면 수동 닫기 전까지 유지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast",
        meta=(ClampMin="0.0"))
    float Duration = 3.0f;

    // 프로그레스바 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast|Progress")
    FExToastProgressConfig ProgressConfig;
};
