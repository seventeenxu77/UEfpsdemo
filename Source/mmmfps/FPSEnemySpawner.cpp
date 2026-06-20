#include "FPSEnemySpawner.h"
#include "FPSEnemy.h"
#include "FPSGameState.h"
#include "FPSGameInstance.h"
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

	// 敌人数量：主菜单设过(EnemyCount>0)就用它覆盖刷怪笼自带的 MaxAlive；
	// 没走菜单直接进关卡时 EnemyCount=0，沿用本刷怪笼放置时的原值。
	if (const UFPSGameInstance* GI = GetWorld()->GetGameInstance<UFPSGameInstance>())
	{
		if (GI->EnemyCount > 0)
		{
			MaxAlive = GI->EnemyCount;
		}
	}

	// 记分板：开局就把 BotKills/BotDeaths 撑到 MaxAlive 大小——
	// 这样固定的几个 bot 槽位(#0..#MaxAlive-1)即使还没击杀，也会以 0/0 出现在记分板上。
	if (AFPSGameState* GS = GetWorld()->GetGameState<AFPSGameState>())
	{
		if (GS->BotKills.Num() < MaxAlive) { GS->BotKills.SetNumZeroed(MaxAlive); }
		if (GS->BotDeaths.Num() < MaxAlive) { GS->BotDeaths.SetNumZeroed(MaxAlive); }
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

	// 比赛已分出胜负就停止刷怪
	if (const AFPSGameState* GS = GetWorld()->GetGameState<AFPSGameState>())
	{
		if (GS->bMatchOver)
		{
			return;
		}
	}

	// 统计当前【活着】的敌人 + 它们占用的槽位号（尸体不算占位，让槽位能立刻重生续命）
	TArray<AActor*> All;
	UGameplayStatics::GetAllActorsOfClass(this, AFPSEnemy::StaticClass(), All);

	TSet<int32> UsedIds;
	int32 LivingCount = 0;
	for (AActor* A : All)
	{
		AFPSEnemy* E = Cast<AFPSEnemy>(A);
		if (!E || E->IsDead())
		{
			continue;
		}
		++LivingCount;
		if (E->BotId >= 0)
		{
			UsedIds.Add(E->BotId);
		}
	}

	if (LivingCount >= MaxAlive)
	{
		return;   // 活着的够多了，不再刷
	}

	// 找最小的空闲槽位号 [0, MaxAlive)——某 AI 死后这个号空出来，新刷的 AI 顶上、续这个号的击杀数
	int32 FreeId = 0;
	while (UsedIds.Contains(FreeId))
	{
		++FreeId;
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
		if (AFPSEnemy* NewEnemy = GetWorld()->SpawnActor<AFPSEnemy>(EnemyClass, SpawnLoc, FRotator::ZeroRotator, Params))
		{
			NewEnemy->BotId = FreeId;   // 打上槽位号：它的击杀数按这个号在 GameState.BotKills 里累加
		}
	}
}
