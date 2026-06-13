// 计分与胜负 GameMode：击杀够 ScoreToWin 个敌人 = 胜利；玩家死亡 = 失败。
// 单机版：分数存这里、HUD 直接读。联机时要搬到 GameState（见 CLAUDE.md 网络部分）。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FPSGameMode.generated.h"

UCLASS()
class MMMFPS_API AFPSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFPSGameMode();

	/** 开局把下面的 ScoreToWin 同步进 GameState（让客户端也拿得到胜利目标） */
	virtual void BeginPlay() override;

	/** 胜利所需击杀数（PvP：谁先到这个数谁赢。在 BP_FirstPersonGameMode 里调） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 ScoreToWin = 5;

	/** PvP：玩家死亡后多久重生（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	float RespawnDelay = 3.f;

	/** 敌人被击杀时调用：加分，达标则判胜（PvE 遗留，PvP 不触发） */
	void OnEnemyKilled();

	/** 玩家死亡时调用：判负（PvE 遗留） */
	void OnPlayerDied();

	/** 玩家死亡时由角色调用：本模式（竞争刷怪）杀玩家不计分，仅给受害者记一次死亡 + 延时重生。仅服务器。 */
	void OnPlayerKilled(AController* Killer, AController* Victim);

	/** 一个 AI 敌人被击杀时由敌人调用——给“凶手玩家”记一杀；达 ScoreToWin 则该玩家胜利。仅服务器。 */
	void OnEnemyKilledBy(AController* KillerController);

	// ---- 给 HUD 读的状态 ----
	int32 GetScore() const { return Score; }
	bool IsGameOver() const { return bGameOver; }
	bool DidPlayerWin() const { return bPlayerWon; }

private:
	/** 当前击杀数 */
	int32 Score = 0;

	/** 是否已分出胜负（之后不再计分） */
	bool bGameOver = false;

	/** 胜负结果 */
	bool bPlayerWon = false;

	/** 结束游戏：记录胜负 + 暂停（PvE 遗留） */
	void EndGame(bool bWon);

	/** PvP：在某个 PlayerStart 重生一个控制器（生成新 pawn 并附身） */
	void RespawnController(AController* Controller);
};
