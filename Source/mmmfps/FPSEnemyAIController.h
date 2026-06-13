// 敌人的“大脑”：每隔 ThinkInterval 秒做一次决策——
// 目标太远 → 寻路追过去；进入攻击范围 → 停下挥拳。

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "FPSEnemyAIController.generated.h"

UCLASS()
class MMMFPS_API AFPSEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	/** 决策频率（秒）。不必每帧想，0.25s 一次足够且省性能 */
	UPROPERTY(EditAnywhere, Category = "AI")
	float ThinkInterval = 0.25f;

protected:
	/** 附身到敌人身上时启动“思考”定时器 */
	virtual void OnPossess(APawn* InPawn) override;

	/** 脱离时停掉定时器（敌人死亡时会触发） */
	virtual void OnUnPossess() override;

	/** 决策主循环：追击 or 攻击 */
	void Think();

	/** 在所有玩家里挑距离本敌人最近、且还活着的那个作目标（联机：服务器枚举全部玩家，不再写死玩家0） */
	APawn* FindNearestPlayer() const;

	FTimerHandle ThinkTimer;
};
