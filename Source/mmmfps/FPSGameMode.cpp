#include "FPSGameMode.h"
#include "FPSGameState.h"
#include "FPSPlayerState.h"
#include "FPSEnemy.h"
#include "FPSGameInstance.h"
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

FString AFPSGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	// 先让引擎做默认登录处理（注册玩家、设默认名等）
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	// 再读连接 URL 里的 ?Name=，有就强制改名——在 Super 之后跑，所以压过引擎默认的电脑名/Player0。
	const FString DesiredName = UGameplayStatics::ParseOption(Options, TEXT("Name"));
	// 诊断：把"连接 URL 带来的选项"和"解析出的名字"打出来——客户端名字若仍不对，看这行就知道是 URL 没带过来、
	// 还是带过来了却被后续覆盖（被覆盖的话，pawn BeginPlay 里的 Server_SetPlayerName 兜底会再纠正）。
	UE_LOG(Logmmmfps, Warning, TEXT("[名字] InitNewPlayer：连接URL选项「%s」→ 解析出 Name=「%s」"), *Options, *DesiredName);
	if (!DesiredName.IsEmpty())
	{
		ChangeName(NewPlayerController, DesiredName, /*bNameChange=*/false);
	}

	return Result;
}

void AFPSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 监听服务器房主的登录 URL 往往不带 ?Name=（那是给远程客户端的），所以 InitNewPlayer 改不到它。
	// 这里专门给【本地玩家】(就是房主自己)用 GameInstance 里存的名字兜底——客户端 IsLocalController()=false，不受影响。
	if (NewPlayer && NewPlayer->IsLocalController())
	{
		if (const UFPSGameInstance* GI = GetWorld()->GetGameInstance<UFPSGameInstance>())
		{
			if (!GI->PlayerName.IsEmpty())
			{
				ChangeName(NewPlayer, GI->PlayerName, /*bNameChange=*/false);
			}
		}
	}
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
	// 大乱斗：杀玩家也算分。给凶手记一杀 + 给受害者记死亡 + 延时重生让他继续打。
	if (!Victim)
	{
		return;
	}
	if (const AFPSGameState* GS = Cast<AFPSGameState>(GameState))
	{
		if (GS->bMatchOver)
		{
			return;   // 比赛已结束：不再计分/重生
		}
	}

	// 给凶手记这一杀（排除自杀：没凶手、或凶手就是受害者本人）
	if (Killer && Killer != Victim)
	{
		RegisterKill(Killer);
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

void AFPSGameMode::RegisterKill(AController* Killer)
{
	AFPSGameState* GS = Cast<AFPSGameState>(GameState);
	if (!GS || GS->bMatchOver || !Killer)
	{
		return;   // 没 GameState / 已结束 / 无凶手：不计分
	}

	int32 NewKills = 0;
	FString KillerName;

	// 凶手是【玩家】才有 AFPSPlayerState；AI 控制器拿不到，会落到下面的分支
	AFPSPlayerState* KillerPS = Killer->GetPlayerState<AFPSPlayerState>();
	if (KillerPS)
	{
		// 玩家：记到自己的 PlayerState（跨重生持久、引擎自动复制）
		KillerPS->AddKill();
		NewKills = KillerPS->Kills;
		KillerName = KillerPS->GetPlayerName();
	}
	else if (const AFPSEnemy* KillerBot = Cast<AFPSEnemy>(Killer->GetPawn()))
	{
		// AI：记到 GameState 的槽位 BotKills[BotId]（跨重生持久、会复制）
		const int32 Id = KillerBot->BotId;
		if (Id < 0)
		{
			return;   // 没分配槽位的 AI 不计分
		}
		if (GS->BotKills.Num() <= Id)
		{
			GS->BotKills.SetNumZeroed(Id + 1);   // 按需扩容到能放下这个槽位
		}
		NewKills = ++GS->BotKills[Id];
		KillerName = FString::Printf(TEXT("Enemy%03d"), Id + 1);   // 和记分板一致：槽位 0 → Enemy001
	}
	else
	{
		return;   // 既不是玩家也不是 AI：不计分
	}

	UE_LOG(Logmmmfps, Warning, TEXT("[计分] %s 击杀 %d/%d"), *KillerName, NewKills, GS->KillsToWin);

	// 达标 → 该凶手胜利、结束比赛（复制给所有客户端）。
	// WinningPlayer 只在“玩家获胜”时非空（HUD 据此判断本机玩家赢没赢）；AI 获胜时为 null，看 WinnerName。
	if (NewKills >= GS->KillsToWin)
	{
		GS->bMatchOver = true;
		GS->WinningPlayer = KillerPS;   // 玩家赢→非空；AI 赢→nullptr
		GS->WinnerName = KillerName;
		UE_LOG(Logmmmfps, Error, TEXT("[比赛结束] 获胜者：%s"), *KillerName);
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
