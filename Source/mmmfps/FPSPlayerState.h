// 玩家状态：每个玩家天生自带一个 APlayerState，联机时引擎【自动复制给所有客户端】。
// 我们在它上面加 PvP 击杀/死亡数——这样所有人都能看到每个玩家的战绩（HUD、记分板都读它）。
// 为什么不放 GameMode：GameMode 只存在于服务器，客户端读不到；PlayerState 是引擎专门用来同步玩家数据的容器。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "FPSPlayerState.generated.h"

UCLASS()
class MMMFPS_API AFPSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	/** 该玩家的击杀数（服务器改、自动复制给所有人） */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	int32 Kills = 0;

	/** 该玩家的死亡数 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	int32 Deaths = 0;

	/** 声明要复制的属性（登记 Kills / Deaths） */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 仅服务器调用：加一个击杀 */
	void AddKill() { ++Kills; }

	/** 仅服务器调用：加一次死亡 */
	void AddDeath() { ++Deaths; }
};
