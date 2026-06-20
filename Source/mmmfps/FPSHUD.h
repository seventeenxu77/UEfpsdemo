// 屏幕中心十字准星 HUD。把 GameMode 的 HUD Class 设成它即可生效。
// 用最朴素的 Canvas 画线（带中心缺口的十字），不依赖任何 UMG 资产。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FPSHUD.generated.h"

UCLASS()
class MMMFPS_API AFPSHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** 每条准星臂的长度（像素） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairSize = 10.f;

	/** 准星线宽（像素） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairThickness = 2.f;

	/** 准星中心留空（像素），免得正中目标被线挡住 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairGap = 5.f;

	/** 准星颜色 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor::White;

	// ---------- 受击红屏 ----------

	/** 红屏渐隐速度（每秒减少多少强度，越大消得越快） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageFlash")
	float DamageFlashFadeSpeed = 1.5f;

	/** 红屏最强时的不透明度（别太高，免得挡视线） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageFlash")
	float DamageFlashMaxOpacity = 0.4f;

	/** 玩家受击时由 FPSCharacter 调用，触发一次红屏 */
	void NotifyPlayerDamaged() { DamageFlashAlpha = 1.f; }

	/** 每帧绘制 HUD 时引擎自动调用 */
	virtual void DrawHUD() override;

private:
	/** 受击红屏当前强度（0~1）：被打设 1，每帧渐隐到 0 */
	float DamageFlashAlpha = 0.f;

	/** 画记分板（玩家 + 所有 AI 的击杀/死亡，按击杀排序）。按住 Tab 时调用。 */
	void DrawScoreboard(class AFPSGameState* GS);
};
