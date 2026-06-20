// 游戏实例：在【整个游戏生命周期内常驻】，切换关卡也不会销毁。
// 因此把"开房 / 加入 / 退出"这类需要跨关卡切换仍存活的联机逻辑放在这里最合适——
// 角色、GameMode 在切关卡时会被销毁重建，唯独 GameInstance 一直活着。
// 这三个函数都标了 BlueprintCallable，主菜单 Widget 的按钮点击事件直接调用它们。

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"   // ENetworkFailure / ETravelFailure 的枚举定义
#include "FPSGameInstance.generated.h"

class UNetDriver;

UCLASS()
class MMMFPS_API UFPSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** 开房：以"监听服务器(listen server)"身份打开游戏关卡，自己也能玩、同时开放端口等别人连进来 */
	UFUNCTION(BlueprintCallable, Category = "Network")
	void HostGame();

	/** 加入：作为客户端连接到房主的 IP（可带端口，如 "192.168.1.5:7777"，不带则默认 7777） */
	UFUNCTION(BlueprintCallable, Category = "Network")
	void JoinGame(const FString& IPAddress);

	/** 退出游戏 */
	UFUNCTION(BlueprintCallable, Category = "Network")
	void QuitGame();

	/** 菜单出现时调用：清掉"正在切换"标志，确保回到菜单后永远能再次开房/加入。 */
	UFUNCTION(BlueprintCallable, Category = "Network")
	void NotifyAtMenu() { bTravelInProgress = false; }

	/** 开房时要打开的游戏关卡。用【完整路径】而不是短名——短名在独立进程/真实关卡跳转时
	 *  常解析失败、落到空的 Untitled 关卡。完整路径解析稳定。可在蓝图默认值里改。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	FName GameLevelName = TEXT("/Game/FirstPerson/Lvl_FirstPerson");

	/** 玩家自己设的名字（主菜单输入框写入）。开房/加入时通过 URL 的 ?Name= 选项带给服务器，
	 *  服务器登录时设到 PlayerState.PlayerName → 记分板/击杀提示显示它，而不是电脑名。 */
	UPROPERTY(BlueprintReadWrite, Category = "Network")
	FString PlayerName;

	/** 本场敌人数量（主菜单输入框写入）。开房时刷怪笼 BeginPlay 读它当 MaxAlive。
	 *  0 = 不覆盖、沿用刷怪笼自带的值（直接进关卡、没走菜单时就是这种情况）。 */
	UPROPERTY(BlueprintReadWrite, Category = "Network")
	int32 EnemyCount = 0;

	/** 连接失败/跳转失败时退回的主菜单地图（默认就是启动菜单地图）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	FName MenuLevelName = TEXT("/Game/Lvl_MainMenu");

protected:
	/** GameInstance 启动时引擎调用一次：在这里绑定"网络失败/跳转失败"回调，
	 *  这样连不上服务器(如填了死 IP / 房主没开)时能优雅退回菜单，而不是走引擎默认路径导致崩溃。 */
	virtual void Init() override;

	/** 网络失败回调（连不上服务器、连接被拒、掉线等）：记日志 + 退回主菜单。 */
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/** 跳转失败回调（ClientTravel/加载关卡失败）：记日志 + 退回主菜单。 */
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** 退回主菜单地图（OpenLevel 到 MenuLevelName）。 */
	void ReturnToMenu();

	/** 是否正在切换关卡/建立连接中。开房/加入时置真；用来挡住"加入卡住时又点开房/加入"这类重入——
	 *  正是它导致引擎在半连接状态里加载新世界、拿不到 GameMode、spawn 玩家失败而崩溃。 */
	bool bTravelInProgress = false;
};
