#include "FPSHUD.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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

	// 比赛结束：屏幕正中弹胜/负大字（看获胜者是不是本机玩家）
	if (GS && GS->bMatchOver)
	{
		const AFPSPlayerState* MyPS = OwningPC ? OwningPC->GetPlayerState<AFPSPlayerState>() : nullptr;
		const bool bIWon = (MyPS && GS->WinningPlayer == MyPS);
		const FString Result = bIWon ? TEXT("胜  利") : TEXT("失  败");
		const FLinearColor Color = bIWon ? FLinearColor(0.2f, 1.f, 0.3f) : FLinearColor(1.f, 0.25f, 0.25f);
		float TW = 0.f, TH = 0.f;
		GetTextSize(Result, TW, TH, nullptr, 5.f);
		DrawText(Result, Color, (Canvas->SizeX - TW) * 0.5f, (Canvas->SizeY - TH) * 0.5f, nullptr, 5.f);
	}
}
