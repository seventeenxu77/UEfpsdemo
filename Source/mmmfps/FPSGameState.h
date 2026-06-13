// 比赛状态：AGameStateBase 也是引擎【自动复制给所有客户端】的容器，用来放"全场共享"的比赛信息。
// 我们在它上面放 PvP 死亡竞赛的状态：击杀目标、是否结束、谁赢了——所有客户端据此同步显示胜负。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "FPSGameState.generated.h"

class AFPSPlayerState;

UCLASS()
class MMMFPS_API AFPSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	/** 击杀目标：谁先到这个数就赢（服务器设、复制给所有人） */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	int32 KillsToWin = 5;

	/** 比赛是否结束 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	bool bMatchOver = false;

	/** 获胜者的 PlayerState（复制给所有人；HUD 据此判断本机玩家是赢是输） */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	AFPSPlayerState* WinningPlayer = nullptr;

	/** 声明要复制的属性 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
