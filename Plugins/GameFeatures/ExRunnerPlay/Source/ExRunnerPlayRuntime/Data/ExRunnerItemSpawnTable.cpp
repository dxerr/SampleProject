// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerItemSpawnTable.h"
#include "ExItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const UExItemDefinition* UExRunnerItemSpawnTable::SelectRandomCoin(float CurrentSpeed) const
{
	return SelectWeightedRandom(CoinEntries, CurrentSpeed);
}

const UExItemDefinition* UExRunnerItemSpawnTable::SelectRandomBuff(float CurrentSpeed) const
{
	return SelectWeightedRandom(BuffEntries, CurrentSpeed);
}

const UExItemDefinition* UExRunnerItemSpawnTable::SelectWeightedRandom(const TArray<FExItemSpawnEntry>& Entries, float CurrentSpeed)
{
	// 속도 조건을 만족하는 엔트리만 필터링
	float TotalWeight = 0.f;
	TArray<const FExItemSpawnEntry*> ValidEntries;
	
	for (const FExItemSpawnEntry& Entry : Entries)
	{
		if (Entry.ItemDefinition && CurrentSpeed >= Entry.MinSpeedRequired)
		{
			ValidEntries.Add(&Entry);
			TotalWeight += Entry.Weight;
		}
	}

	if (ValidEntries.Num() == 0 || TotalWeight <= 0.f)
	{
		return nullptr;
	}

	// 가중치 기반 랜덤 선택
	float RandValue = FMath::FRandRange(0.f, TotalWeight);
	float AccumulatedWeight = 0.f;

	for (const FExItemSpawnEntry* Entry : ValidEntries)
	{
		AccumulatedWeight += Entry->Weight;
		if (RandValue <= AccumulatedWeight)
		{
			return Entry->ItemDefinition;
		}
	}

	// 부동소수점 오차 방지: 마지막 항목 반환
	return ValidEntries.Last()->ItemDefinition;
}

#if WITH_EDITOR
EDataValidationResult UExRunnerItemSpawnTable::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto ValidateEntries = [&](const TArray<FExItemSpawnEntry>& Entries, const FString& ArrayName)
	{
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			const FExItemSpawnEntry& Entry = Entries[i];
			if (Entry.ItemDefinition == nullptr)
			{
				Context.AddError(FText::Format(
					FText::FromString(TEXT("{0}[{1}]의 ItemDefinition이 비어 있습니다. 유효한 에셋을 할당하세요.")),
					FText::FromString(ArrayName),
					FText::AsNumber(i)
				));
				Result = EDataValidationResult::Invalid;
			}
			if (Entry.Weight <= 0.0f)
			{
				Context.AddWarning(FText::Format(
					FText::FromString(TEXT("{0}[{1}]의 가중치(Weight)가 0 이하입니다. 0 초과여야 스폰될 수 있습니다.")),
					FText::FromString(ArrayName),
					FText::AsNumber(i)
				));
			}
		}
	};

	ValidateEntries(CoinEntries, TEXT("CoinEntries"));
	ValidateEntries(BuffEntries, TEXT("BuffEntries"));

	return Result;
}
#endif
