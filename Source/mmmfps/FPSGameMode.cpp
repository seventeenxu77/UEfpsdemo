#include "FPSGameMode.h"
#include "FPSGameState.h"
#include "FPSPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "mmmfps.h"

AFPSGameMode::AFPSGameMode()
{
	// 指定本模式使用我们自己的、带复制属性的 GameState / PlayerState——
	// 这样客户端也能拿到比赛状态(胜负)和每个玩家的击杀数。
	GameStateClass = AFPSGameState::StaticClass();
	PlayerStateClass = AFPSPlayerState::StaticClass();
}

void AFPSGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 把 GameMode 上可编辑的 ScoreToWin 同步进 GameState（会复制给所有客户端）
	if (AFPSGameState* GS = Cast<AFPSGameState>(GameState))
	{
		GS->KillsToWin = ScoreToWin;
	}
}

void AFPSGameMode::OnEnemyKilled()
{
	// 已经结束就不再计分（防止结算后还在加）
	if (bGameOver)
	{
		return;
	}

	++Score;
	UE_LOG(Logmmmfps, Warning, TEXT("击杀 +1，当前 %d / %d"), Score, ScoreToWin);

	if (Score >= ScoreToWin)
	{
		EndGame(true);   // 达标 → 胜利
	}
}

void AFPSGameMode::OnPlayerDied()
{
	if (bGameOver)
	{
		return;
	}
	EndGame(false);      // 玩家死 → 失败
}

void AFPSGameMode::EndGame(bool bWon)
{
	bGameOver = true;
	bPlayerWon = bWon;
	UE_LOG(Logmmmfps, Error, TEXT("游戏结束：%s"), bWon ? TEXT("胜利！") : TEXT("失败"));

	// 暂停游戏，让玩家停在结算画面（HUD 的胜负大字仍会照常绘制）
	UGameplayStatics::SetGamePaused(this, true);
}

void AFPSGameMode::OnPlayerKilled(AController* Killer, AController* Victim)
{
	// 本模式（竞争刷怪）杀玩家不计分——只给受害者记一次死亡，并延时重生让他继续打。
	if (!Victim)
	{
		return;
	}
	if (const AFPSGameState* GS = Cast<AFPSGameState>(GameState))
	{
		if (GS->bMatchOver)
		{
			return;   // 比赛结束就不再重生
		}
	}

	if (AFPSPlayerState* VictimPS = Victim->GetPlayerState<AFPSPlayerState>())
	{
		VictimPS->AddDeath();
	}

	// RespawnDelay 秒后重生受害者（在 PlayerStart 生成新 pawn 并附身）
	FTimerHandle Handle;
	FTimerDelegate Del;
	Del.BindUObject(this, &AFPSGameMode::RespawnController, Victim);
	GetWorldTimerManager().SetTimer(Handle, Del, RespawnDelay, false);
}

void AFPSGameMode::OnEnemyKilledBy(AController* KillerController)
{
	AFPSGameState* GS = Cast<AFPSGameState>(GameState);
	if (!GS || GS->bMatchOver || !KillerController)
	{
		return;   // 没 GameState / 已结束 / 无凶手：不计分
	}

	// 只有“玩家”才有 AFPSPlayerState；AI 等拿不到，就不会误加分
	if (AFPSPlayerState* KillerPS = KillerController->GetPlayerState<AFPSPlayerState>())
	{
		KillerPS->AddKill();
		UE_LOG(Logmmmfps, Warning, TEXT("[计分] %s 杀敌 %d/%d"),
			*KillerPS->GetPlayerName(), KillerPS->Kills, GS->KillsToWin);

		// 达标 → 该玩家胜利、结束比赛（bMatchOver / WinningPlayer 复制给所有客户端）
		if (KillerPS->Kills >= GS->KillsToWin)
		{
			GS->bMatchOver = true;
			GS->WinningPlayer = KillerPS;
			UE_LOG(Logmmmfps, Error, TEXT("[比赛结束] 获胜者：%s"), *KillerPS->GetPlayerName());
		}
	}
}

void AFPSGameMode::RespawnController(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	if (const AFPSGameState* GS = Cast<AFPSGameState>(GameState))
	{
		if (GS->bMatchOver)
		{
			return;   // 比赛已结束就不重生
		}
	}
	// 在某个 PlayerStart 生成新 pawn 并附身（RestartPlayer 内部会 ChoosePlayerStart）
	RestartPlayer(Controller);
}
