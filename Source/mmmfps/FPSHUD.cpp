#include "FPSHUD.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "FPSCharacter.h"
#include "FPSGameMode.h"
#include "FPSGameState.h"
#include "FPSPlayerState.h"

void AFPSHUD::DrawHUD()
{
	Super::DrawHUD();

	// Canvas 只有在真正绘制时才有效
	if (!Canvas)
	{
		return;
	}

	// 受击红屏：被打时 NotifyPlayerDamaged 把 alpha 设为 1，这里每帧铺一层红、再渐隐
	if (DamageFlashAlpha > 0.f)
	{
		DrawRect(FLinearColor(1.f, 0.f, 0.f, DamageFlashAlpha * DamageFlashMaxOpacity),
			0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
		DamageFlashAlpha = FMath::Max(0.f, DamageFlashAlpha - GetWorld()->GetDeltaSeconds() * DamageFlashFadeSpeed);
	}

	// 屏幕正中（= 相机正前方 = 射线方向），所以准星就标在子弹会去的地方
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	// 四段短线，中间留 Gap，组成带缺口的十字准星
	// 左臂
	DrawLine(CX - CrosshairGap - CrosshairSize, CY, CX - CrosshairGap, CY, CrosshairColor, CrosshairThickness);
	// 右臂
	DrawLine(CX + CrosshairGap, CY, CX + CrosshairGap + CrosshairSize, CY, CrosshairColor, CrosshairThickness);
	// 上臂
	DrawLine(CX, CY - CrosshairGap - CrosshairSize, CX, CY - CrosshairGap, CrosshairColor, CrosshairThickness);
	// 下臂
	DrawLine(CX, CY + CrosshairGap, CX, CY + CrosshairGap + CrosshairSize, CrosshairColor, CrosshairThickness);

	// 本地玩家血量（左下角）——直接读本地控制的 pawn；CurrentHP 已复制，数值是准的
	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (const AFPSCharacter* P = Cast<AFPSCharacter>(PC->GetPawn()))
		{
			const FString HPText = FString::Printf(TEXT("HP  %.0f / %.0f"), P->GetCurrentHP(), P->GetMaxHP());
			const FLinearColor HPColor = P->IsDead() ? FLinearColor(1.f, 0.3f, 0.3f) : FLinearColor::White;
			DrawText(HPText, HPColor, 40.f, Canvas->SizeY - 70.f, nullptr, 1.6f);
		}
	}

	// 击杀数 + 胜负结算（联机：从【复制的】PlayerState/GameState 读，客户端也拿得到；
	// 不再读 GameMode——它只在服务器存在，客户端读为 null）
	APlayerController* OwningPC = GetOwningPlayerController();
	AFPSGameState* GS = GetWorld()->GetGameState<AFPSGameState>();

	// 左上角：本机玩家自己的击杀进度
	if (OwningPC && GS)
	{
		int32 MyKills = 0;
		if (const AFPSPlayerState* PS = OwningPC->GetPlayerState<AFPSPlayerState>())
		{
			MyKills = PS->Kills;
		}
		const FString ScoreText = FString::Printf(TEXT("击杀  %d / %d"), MyKills, GS->KillsToWin);
		DrawText(ScoreText, FLinearColor::White, 40.f, 40.f, nullptr, 1.6f);
	}

	// 比赛结束：屏幕正中弹胜/负大字（看获胜者是不是本机玩家；AI 获胜时 WinningPlayer 为 null → 所有玩家都显示失败）
	if (GS && GS->bMatchOver)
	{
		const AFPSPlayerState* MyPS = OwningPC ? OwningPC->GetPlayerState<AFPSPlayerState>() : nullptr;
		const bool bIWon = (MyPS && GS->WinningPlayer == MyPS);
		const FString Result = bIWon ? TEXT("胜  利") : TEXT("失  败");
		const FLinearColor Color = bIWon ? FLinearColor(0.2f, 1.f, 0.3f) : FLinearColor(1.f, 0.25f, 0.25f);
		float TW = 0.f, TH = 0.f;
		GetTextSize(Result, TW, TH, nullptr, 5.f);
		const float CenterX = (Canvas->SizeX - TW) * 0.5f;
		const float CenterY = (Canvas->SizeY - TH) * 0.5f;
		DrawText(Result, Color, CenterX, CenterY, nullptr, 5.f);

		// 下面一行小字：谁赢了（玩家名，或 AI 获胜时的"敌人 #i"）
		if (!GS->WinnerName.IsEmpty())
		{
			const FString WinLine = FString::Printf(TEXT("获胜者：%s"), *GS->WinnerName);
			float WW = 0.f, WH = 0.f;
			GetTextSize(WinLine, WW, WH, nullptr, 1.8f);
			DrawText(WinLine, FLinearColor::White, (Canvas->SizeX - WW) * 0.5f, CenterY + TH + 20.f, nullptr, 1.8f);
		}
	}

	// 记分板：按住 Tab 随时调出（玩家 + 所有 AI 的击杀/死亡）
	if (GS && OwningPC && OwningPC->IsInputKeyDown(EKeys::Tab))
	{
		DrawScoreboard(GS);
	}
}

void AFPSHUD::DrawScoreboard(AFPSGameState* GS)
{
	if (!Canvas || !GS)
	{
		return;
	}

	// 一行参赛者记录（玩家或 AI 槽位）
	struct FScoreRow
	{
		FString Name;
		int32 Kills;
		int32 Deaths;
		bool bMe;    // 是不是本机玩家（高亮用）
		bool bBot;   // 是不是 AI（配色用）
	};
	TArray<FScoreRow> Rows;

	APlayerController* MyPC = GetOwningPlayerController();
	const AFPSPlayerState* MyPS = MyPC ? MyPC->GetPlayerState<AFPSPlayerState>() : nullptr;

	// 玩家：从【复制的】PlayerArray 读，客户端也拿得到全部玩家战绩
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (const AFPSPlayerState* FPS = Cast<AFPSPlayerState>(PS))
		{
			Rows.Add(FScoreRow{ FPS->GetPlayerName(), FPS->Kills, FPS->Deaths, (FPS == MyPS), false });
		}
	}
	// AI：从 BotKills/BotDeaths 读（固定的几个槽位 #0..#MaxAlive-1）
	for (int32 i = 0; i < GS->BotKills.Num(); ++i)
	{
		const int32 D = (i < GS->BotDeaths.Num()) ? GS->BotDeaths[i] : 0;
		// 槽位号 i 从 0 起，显示成 Enemy001/Enemy002…（+1 让首个敌人是 001 而非 000）
		Rows.Add(FScoreRow{ FString::Printf(TEXT("Enemy%03d"), i + 1), GS->BotKills[i], D, false, true });
	}

	// 按击杀数从高到低排
	Rows.Sort([](const FScoreRow& A, const FScoreRow& B) { return A.Kills > B.Kills; });

	// —— 画半透明面板 + 表格 ——
	const float PanelW = 480.f;
	const float RowH = 30.f;
	const float HeadH = 78.f;
	const float PanelH = HeadH + RowH * Rows.Num() + 16.f;
	const float X = (Canvas->SizeX - PanelW) * 0.5f;
	const float Y = 90.f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.78f), X, Y, PanelW, PanelH);

	DrawText(FString::Printf(TEXT("记 分 板    （目标 %d 杀）"), GS->KillsToWin),
		FLinearColor(1.f, 0.85f, 0.2f), X + 20.f, Y + 12.f, nullptr, 1.5f);

	const FLinearColor HeadColor(0.65f, 0.65f, 0.65f);
	DrawText(TEXT("名字"), HeadColor, X + 24.f, Y + 48.f, nullptr, 1.1f);
	DrawText(TEXT("击杀"), HeadColor, X + 320.f, Y + 48.f, nullptr, 1.1f);
	DrawText(TEXT("死亡"), HeadColor, X + 410.f, Y + 48.f, nullptr, 1.1f);

	float RowY = Y + HeadH;
	for (const FScoreRow& R : Rows)
	{
		// 本机玩家=绿，AI=橙，其他玩家=白
		const FLinearColor C = R.bMe ? FLinearColor(0.3f, 1.f, 0.45f)
			: (R.bBot ? FLinearColor(1.f, 0.62f, 0.5f) : FLinearColor::White);
		DrawText(R.Name, C, X + 24.f, RowY, nullptr, 1.2f);
		DrawText(FString::FromInt(R.Kills), C, X + 326.f, RowY, nullptr, 1.2f);
		DrawText(FString::FromInt(R.Deaths), C, X + 416.f, RowY, nullptr, 1.2f);
		RowY += RowH;
	}
}
