// Copyright ExFrameWork. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FExChunkSpawnSettings.generated.h"

USTRUCT(BlueprintType)
struct EXRUNNERPLAYRUNTIME_API FExChunkSpawnSettings
{
	GENERATED_BODY()

public:
	/** 청크 풀링 사용 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|ChunkSpawn")
	bool bUsePooling = false;

	/** 시작 시점에 미리 생성해둘 청크 수 (풀링을 사용하지 않아도 이 수만큼 미리 배치됨) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|ChunkSpawn")
	int32 InitialPoolSize = 5;

	/** 맨 처음 생성되는 청크의 X좌표 시작 지점 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|ChunkSpawn")
	float SpawnStartX = 0.f;

	/** 바닥(Floor) 청크가 하나 생성될 때마다 전진하는 Y축(혹은 이전 청크 끝선에서의) 간격 값. 보통 Chunk BP의 길이에 맞춤 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|ChunkSpawn")
	float ChunkSpacing = 1000.f;

	/** 동시에 필드에 존재할 수 있는 최대 청크 개수. 뒤처진 청크는 삭제/풀 회수 됨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExRunner|ChunkSpawn")
	int32 MaxActiveChunks = 10;
};
