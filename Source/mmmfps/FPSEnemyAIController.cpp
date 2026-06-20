#include "FPSEnemyAIController.h"
#include "FPSEnemy.h"
#include "FPSCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

	// 目标：选距离最近、还活着的【任何人】——玩家或别的 AI（全员混战）。
	// FindNearestTarget 内部已跳过死亡目标和自己。
	AActor* Target = FindNearestTarget();
	if (!Target)
	{
		StopMovement();   // 场上没有可打的目标：原地待命
		return;
	}

	const float Dist = FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());

	if (Dist <= Enemy->AttackRange)
	{
		// 贴脸：停下近战，平滑转向目标
		StopMovement();
		Enemy->SetAimTarget(Target);
		Enemy->TryMeleeAttack(Target);
	}
	else if (Dist <= Enemy->ShootRange)
	{
		// 中距离：停下开枪，平滑转向目标，保持距离（不再往前冲）
		StopMovement();
		Enemy->SetAimTarget(Target);
		Enemy->TryShoot(Target);
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
			MoveToActor(Target, Enemy->ShootRange * 0.8f);
		}
	}
}

AActor* AFPSEnemyAIController::FindNearestTarget() const
{
	const AFPSEnemy* MyEnemy = Cast<AFPSEnemy>(GetPawn());
	if (!MyEnemy)
	{
		return nullptr;
	}
	const FVector MyLoc = MyEnemy->GetActorLocation();

	AActor* Nearest = nullptr;
	float NearestDistSq = -1.f;

	// 1) 所有玩家（枚举玩家控制器，跳过死亡/没附身的）
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
		if (const AFPSCharacter* FP = Cast<AFPSCharacter>(P))
		{
			if (FP->IsDead())
			{
				continue;   // 死了的玩家不追
			}
		}
		const float DistSq = FVector::DistSquared(MyLoc, P->GetActorLocation());
		if (Nearest == nullptr || DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = P;
		}
	}

	// 2) 所有其他 AI（全员混战：把别的敌人也当目标，跳过自己和死的）
	for (TActorIterator<AFPSEnemy> It(GetWorld()); It; ++It)
	{
		AFPSEnemy* E = *It;
		if (!E || E == MyEnemy || E->IsDead())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(MyLoc, E->GetActorLocation());
		if (Nearest == nullptr || DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = E;
		}
	}

	return Nearest;
}
