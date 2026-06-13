#include "FPSEnemySpawner.h"
#include "FPSEnemy.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "mmmfps.h"

AFPSEnemySpawner::AFPSEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFPSEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	// 联机：只有服务器刷怪。敌人是复制 actor，服务器刷出来会自动同步到所有客户端；
	// 客户端别自己刷，否则数量翻倍、各看各的一批。
	if (!HasAuthority())
	{
		return;
	}

	// 周期性补刷；开局先刷一个，别让玩家干等
	GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &AFPSEnemySpawner::TrySpawn, SpawnInterval, true);
	TrySpawn();
}

void AFPSEnemySpawner::TrySpawn()
{
	if (!EnemyClass)
	{
		return;
	}

	// 场上敌人已经够多就不刷（维持 MaxAlive 的压力，不爆量）
	TArray<AActor*> Alive;
	UGameplayStatics::GetAllActorsOfClass(this, AFPSEnemy::StaticClass(), Alive);
	if (Alive.Num() >= MaxAlive)
	{
		return;
	}

	// 在自身周围的导航网格上找一个随机可达点——这样敌人一定落在能寻路的地面上
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation Result;
	if (Nav && Nav->GetRandomReachablePointInRadius(GetActorLocation(), SpawnRadius, Result))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		// 抬高一点避免和地面穿插（敌人胶囊半高约 88）
		const FVector SpawnLoc = Result.Location + FVector(0.f, 0.f, 90.f);
		GetWorld()->SpawnActor<AFPSEnemy>(EnemyClass, SpawnLoc, FRotator::ZeroRotator, Params);
	}
}
