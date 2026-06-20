// 主菜单控件的 C++ 父类：把"开房/加入/退出"按钮的逻辑全放在这里。
// Designer 里的 Widget 只要【认这个类做父类】+ 控件按下面指定的名字命名，
// 引擎的 BindWidget 机制就会自动把按钮/输入框绑到这些指针上——你一根节点都不用连。

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FPSMenuWidget.generated.h"

class UButton;
class UEditableTextBox;

UCLASS()
class MMMFPS_API UFPSMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 控件构建后引擎调用一次：在这里把按钮的 OnClicked 绑到处理函数（相当于蓝图连 On Clicked）
	virtual void NativeConstruct() override;

	// —— 下面这些指针名必须和 Designer 里控件名【完全一致】，引擎据名字自动绑定 ——
	// BindWidgetOptional：即便名字没对上也不会让 Widget 编译失败，只是指针为空（更宽容、好排错）。

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BtnHost = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BtnJoin = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BtnQuit = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UEditableTextBox* TxtIP = nullptr;

	/** 玩家名输入框（Designer 里控件名必须叫 TxtPlayerName）。开房/加入都会把它写进 GameInstance。 */
	UPROPERTY(meta = (BindWidgetOptional))
	UEditableTextBox* TxtPlayerName = nullptr;

	/** 敌人数量输入框（Designer 里控件名必须叫 TxtEnemyCount）。填数字，仅开房时生效。 */
	UPROPERTY(meta = (BindWidgetOptional))
	UEditableTextBox* TxtEnemyCount = nullptr;

	// 三个按钮的点击处理（必须是 UFUNCTION 才能绑到 OnClicked 这种动态委托）
	UFUNCTION()
	void OnHostClicked();

	UFUNCTION()
	void OnJoinClicked();

	UFUNCTION()
	void OnQuitClicked();
};
