#include "FPSMenuWidget.h"
#include "FPSGameInstance.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "mmmfps.h"

void UFPSMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 菜单一出现就清掉 GameInstance 的"切换中"标志——保证从游戏/连接失败回到菜单后，仍能再次开房/加入。
	if (UFPSGameInstance* GI = Cast<UFPSGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		GI->NotifyAtMenu();
	}

	// 把按钮的点击事件绑到处理函数——等价于在蓝图里给每个按钮连一根 "On Clicked"
	if (BtnHost) { BtnHost->OnClicked.AddDynamic(this, &UFPSMenuWidget::OnHostClicked); }
	if (BtnJoin) { BtnJoin->OnClicked.AddDynamic(this, &UFPSMenuWidget::OnJoinClicked); }
	if (BtnQuit) { BtnQuit->OnClicked.AddDynamic(this, &UFPSMenuWidget::OnQuitClicked); }

	if (!BtnHost || !BtnJoin || !BtnQuit)
	{
		UE_LOG(Logmmmfps, Warning,
			TEXT("[菜单] 有按钮没绑上——检查 Designer 里控件名是否为 BtnHost / BtnJoin / BtnQuit"));
	}

	// 显示鼠标光标 + 切到"仅 UI"输入模式。
	// 放在这里（而不是只靠关卡蓝图）：菜单一显示就强制开启，独立进程 / 打包后都可靠生效。
	// 说明：PIE 里即使不设也能看到编辑器光标，会掩盖问题；独立进程必须由代码真正开启。
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(this, 0);
	}
	if (PC)
	{
		PC->bShowMouseCursor = true;

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());                              // 焦点给菜单，按钮才接收点击
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);   // 不锁鼠标，能自由移动
		PC->SetInputMode(InputMode);
	}
}

void UFPSMenuWidget::OnHostClicked()
{
	if (UFPSGameInstance* GI = Cast<UFPSGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		// 玩家名：写进 GameInstance，开房时会拼进 ?Name= 带给服务器
		if (TxtPlayerName)
		{
			GI->PlayerName = TxtPlayerName->GetText().ToString();
		}
		// 敌人数量：只在填了【纯数字】时覆盖；夹在 1..20 之间防止填 0 或离谱大值
		if (TxtEnemyCount)
		{
			const FString S = TxtEnemyCount->GetText().ToString();
			if (S.IsNumeric())
			{
				GI->EnemyCount = FMath::Clamp(FCString::Atoi(*S), 1, 20);
			}
		}
		GI->HostGame();
	}
}

void UFPSMenuWidget::OnJoinClicked()
{
	UFPSGameInstance* GI = Cast<UFPSGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI)
	{
		return;
	}

	// 玩家名：写进 GameInstance，加入时会拼进 ?Name= 带给服务器（这样你在房主那边显示的是你设的名）
	if (TxtPlayerName)
	{
		GI->PlayerName = TxtPlayerName->GetText().ToString();
	}

	// 读 IP 输入框（不再默认 127.0.0.1）；空 / 格式错由 JoinGame 内部校验后拒绝。
	// 同机测试请在输入框里【显式填 127.0.0.1】。
	FString IP;
	if (TxtIP)
	{
		IP = TxtIP->GetText().ToString();
	}
	GI->JoinGame(IP);
}

void UFPSMenuWidget::OnQuitClicked()
{
	if (UFPSGameInstance* GI = Cast<UFPSGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		GI->QuitGame();
	}
}
