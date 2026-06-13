#include "FPSEnemyAIController.h"
#include "FPSEnemy.h"
#include "FPSCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "mmmfps.h"

void AFPSEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 周期性调用 Think()。最后一个参数 true = 循环执行
	GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &AFPSEnemyAIController::Think, ThinkInterval, true);
}

void AFPSEnemyAIController::OnUnPossess()
{
	Super::OnUnPossess();
	GetWorld()->GetTimerManager().ClearTimer(ThinkTimer);
}

void AFPSEnemyAIController::Think()
{
	// 自己控制的敌人（身体）
	AFPSEnemy* Enemy = Cast<AFPSEnemy>(GetPawn());
	if (!Enemy || Enemy->IsDead())
	{
		StopMovement();
		return;
	}

	// 目标：选距离最近、还活着的玩家（联机：枚举所有玩家，不再写死玩家 0）。
	// FindNearestPlayer 内部已跳过死亡玩家，所以这里不必再单独判死。
	APawn* PlayerPawn = FindNearestPlayer();
	if (!PlayerPawn)
	{
		StopMovement();   // 场上没有活着的玩家：原地待命
		return;
	}

	const float Dist = FVector::Dist(Enemy->GetActorLocation(), PlayerPawn->GetActorLocation());

	if (Dist <= Enemy->AttackRange)
	{
		// 贴脸：停下近战，平滑转向玩家
		StopMovement();
		Enemy->SetAimTarget(PlayerPawn);
		Enemy->TryMeleeAttack(PlayerPawn);
	}
	else if (Dist <= Enemy->ShootRange)
	{
		// 中距离：停下开枪，平滑转向玩家，保持距离（不再往前冲）
		StopMovement();
		Enemy->SetAimTarget(PlayerPawn);
		Enemy->TryShoot(PlayerPawn);
	}
	else
	{
		// 追击：清空瞄准目标，让身体朝移动方向转（bOrientRotationToMovement 接管）
		Enemy->SetAimTarget(nullptr);

		// 够不着：沿 NavMesh 寻路追进射程。
		// MoveToActor 对“会动的目标 Actor”会自动持续跟踪、自动重寻路，
		// 只在“当前没在移动”时补发一次即可——之前每 0.25s 重发会反复中断寻路、
		// 清零加速度，导致跑步动画（ShouldMove 需要加速度≠0）失效。
		if (GetMoveStatus() != EPathFollowingStatus::Moving)
		{
			MoveToActor(PlayerPawn, Enemy->ShootRange * 0.8f);
		}
	}
}

APawn* AFPSEnemyAIController::FindNearestPlayer() const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return nullptr;
	}
	const FVector MyLoc = MyPawn->GetActorLocation();

	APawn* Nearest = nullptr;
	float NearestDistSq = -1.f;

	// 服务器上枚举所有玩家控制器（AI 在服务器跑，看得到全部玩家）
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		APawn* P = PC->GetPawn();
		if (!P)
		{
			continue;   // 没附身（重生间隙）就跳过
		}
		// 死了的玩家不追
		if (const AFPSCharacter* FP = Cast<AFPSCharacter>(P))
		{
			if (FP->IsDead())
			{
				continue;
			}
		}
		const float DistSq = FVector::DistSquared(MyLoc, P->GetActorLocation());
		if (Nearest == nullptr || DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = P;
		}
	}
	return Nearest;
}
