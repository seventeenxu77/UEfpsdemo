// 刷怪场：在自身周围的导航网格上随机生成敌人，并把场上存活数维持在 MaxAlive。
// 配合 FPSGameMode 的计分胜利：玩家持续清怪、击杀达标即胜。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSEnemySpawner.generated.h"

class AFPSEnemy;

UCLASS()
class MMMFPS_API AFPSEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AFPSEnemySpawner();

	/** 生成哪种敌人（BP 里设 BP_FPSEnemy） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<AFPSEnemy> EnemyClass;

	/** 在自身周围多大半径内的导航网格上随机刷（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float SpawnRadius = 1500.f;

	/** 每隔几秒检查一次是否补刷 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float SpawnInterval = 2.f;

	/** 场上同时最多存活几个敌人（控制压力，别一下子刷爆） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 MaxAlive = 3;

protected:
	virtual void BeginPlay() override;

	/** 定时调用：场上敌人不足 MaxAlive 就在导航网格随机点补一个 */
	void TrySpawn();

	FTimerHandle SpawnTimer;
};
