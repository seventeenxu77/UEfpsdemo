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

	/** 获胜者的 PlayerState（复制给所有人；HUD 据此判断本机玩家是赢是输）。
	 *  注意：只有【玩家】获胜时才非空；AI 获胜时这里是 null、看 WinnerName。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	AFPSPlayerState* WinningPlayer = nullptr;

	/** 获胜者名字（用于屏幕显示）：玩家名，或 AI 获胜时写"敌人 #i"。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	FString WinnerName;

	/** 每个 AI 槽位(BotId)的累计击杀数。服务器改、复制给所有人——
	 *  AI 死了重生还是同一个槽位，分数接着累加，所以 AI 也能稳定攒到胜利目标。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	TArray<int32> BotKills;

	/** 每个 AI 槽位(BotId)的累计死亡数（记分板显示用，和 BotKills 一一对应） */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	TArray<int32> BotDeaths;

	/** 声明要复制的属性 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
