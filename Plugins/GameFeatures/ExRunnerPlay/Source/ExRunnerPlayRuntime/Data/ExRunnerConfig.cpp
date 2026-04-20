// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UExRunnerConfig::IsDataValid(FDataValidationContext& Context) const
{
	bool bIsErrorEmpty = true;

	if (Curve.FixedCurveRadius <= 0.f)
	{
		Context.AddError(FText::FromString(TEXT("Curve.FixedCurveRadius는 0보다 커야 합니다.")));
		bIsErrorEmpty = false;
	}

	if (Curve.MinStraightChunks >= Curve.MaxStraightChunks)
	{
		Context.AddError(FText::FromString(TEXT("Curve.MinStraightChunks는 MaxStraightChunks보다 작아야 합니다.")));
		bIsErrorEmpty = false;
	}

	if (ObstacleSpawn.SpawnProbability < 0.f || ObstacleSpawn.SpawnProbability > 1.f)
	{
		Context.AddError(FText::FromString(TEXT("ObstacleSpawn.SpawnProbability는 0.0 ~ 1.0 사이어야 합니다.")));
		bIsErrorEmpty = false;
	}

	return bIsErrorEmpty ? Super::IsDataValid(Context) : EDataValidationResult::Invalid;
}
#endif
