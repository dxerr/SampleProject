// Copyright ExFrameWork. All Rights Reserved.

#include "ExRunnerItemSpawnTable.h"
#include "ExItemDefinition.h"

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
