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

	/** 玩家登录时引擎调用（服务器）：在这里读连接 URL 的 ?Name= 选项，强制把 PlayerState 名字设成玩家自选名，
	 *  压过引擎默认的电脑名/Player0。客户端的名字走这里（URL 带过来）。 */
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = TEXT("")) override;

	/** 玩家登录完成后调用（服务器）：监听服务器【房主自己】的名字常不从 URL ?Name= 生效，
	 *  这里对【本地玩家】(房主)用 GameInstance 里存的名字兜底改名。 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 胜利所需击杀数（大乱斗：玩家或 AI 谁先到这个数谁赢。可在 BP_FPSGameMode 默认值里调） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 ScoreToWin = 20;

	/** PvP：玩家死亡后多久重生（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	float RespawnDelay = 3.f;

	/** 敌人被击杀时调用：加分，达标则判胜（PvE 遗留，PvP 不触发） */
	void OnEnemyKilled();

	/** 玩家死亡时调用：判负（PvE 遗留） */
	void OnPlayerDied();

	/** 玩家死亡时由角色调用：给凶手记一杀（大乱斗：杀玩家也算分）+ 给受害者记死亡 + 延时重生。仅服务器。 */
	void OnPlayerKilled(AController* Killer, AController* Victim);

	/** 统一计分：任何角色被击杀都调它，把这一杀记到凶手头上。
	 *  凶手是【玩家】→记到它的 PlayerState.Kills；凶手是【AI】→记到 GameState.BotKills[BotId]。
	 *  谁先到 ScoreToWin 就判胜（玩家或 AI 都行）。仅服务器。 */
	void RegisterKill(AController* Killer);

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
