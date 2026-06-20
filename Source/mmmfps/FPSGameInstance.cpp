#include "FPSGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"   // GEngine->OnNetworkFailure / OnTravelFailure
#include "mmmfps.h"

// 把玩家名清洗成可安全放进 URL 选项的形式：去掉会破坏 "?key=value" 解析的字符，并限长。
//  - '?' / '=' 是 URL 选项分隔符，名字里有它们会让服务器解析错位；
//  - 空格替换成下划线（URL 里空格易出问题）；首尾空白去掉；最多 16 字符。
static FString SanitizePlayerName(const FString& In)
{
	FString S = In;
	S.ReplaceInline(TEXT("?"), TEXT(""));
	S.ReplaceInline(TEXT("="), TEXT(""));
	S.ReplaceInline(TEXT(" "), TEXT("_"));
	S.TrimStartAndEndInline();
	return S.Left(16);
}

// 校验服务器地址是否为合法 IPv4（可带可选端口，如 127.0.0.1 或 192.168.1.5:7777）。
// 只在格式正确时才发起连接——空地址 / 乱填会被挡下，避免连不上把切换流程卡住甚至崩溃。
static bool IsValidServerAddress(const FString& In)
{
	FString Addr = In;
	Addr.TrimStartAndEndInline();
	if (Addr.IsEmpty())
	{
		return false;
	}

	// 拆掉可选的 ":端口"，端口必须是纯数字
	FString HostPart, PortPart;
	if (Addr.Split(TEXT(":"), &HostPart, &PortPart))
	{
		if (PortPart.IsEmpty() || !PortPart.IsNumeric())
		{
			return false;
		}
		Addr = HostPart;
	}

	// 主机部分必须是 IPv4：四段、每段 0~255 的数字
	TArray<FString> Parts;
	Addr.ParseIntoArray(Parts, TEXT("."), /*InCullEmpty=*/false);
	if (Parts.Num() != 4)
	{
		return false;
	}
	for (const FString& P : Parts)
	{
		if (P.IsEmpty() || !P.IsNumeric())
		{
			return false;
		}
		const int32 V = FCString::Atoi(*P);
		if (V < 0 || V > 255)
		{
			return false;
		}
	}
	return true;
}

void UFPSGameInstance::HostGame()
{
	// 重入保护：已经在切换/连接中就忽略，避免"加入卡住时又点开房"导致引擎在半连接状态崩溃。
	if (bTravelInProgress)
	{
		UE_LOG(Logmmmfps, Warning, TEXT("[网络] 已在切换/连接中，忽略本次开房"));
		return;
	}
	bTravelInProgress = true;

	// "listen" 选项 = 把这台机器变成【监听服务器】：它既是服务器(裁判)，本地也开一个玩家在玩，
	// 同时开放网络端口等客户端连进来。OpenLevel 会切到游戏关卡并以服务器身份重新加载世界。
	// 选项 "listen" 让本机变监听服务器；再把玩家名拼进 ?Name= → 服务器登录时设到自己的 PlayerState。
	FString Options = TEXT("listen");
	const FString CleanName = SanitizePlayerName(PlayerName);
	if (!CleanName.IsEmpty())
	{
		Options += FString::Printf(TEXT("?Name=%s"), *CleanName);
	}

	UE_LOG(Logmmmfps, Warning, TEXT("[网络] 开房：以监听服务器打开关卡 %s（选项 %s）"), *GameLevelName.ToString(), *Options);
	UGameplayStatics::OpenLevel(this, GameLevelName, /*bAbsolute=*/true, Options);
}

void UFPSGameInstance::JoinGame(const FString& IPAddress)
{
	// 重入保护：已经在切换/连接中就忽略（防"加入卡住时又点加入/开房"）。
	if (bTravelInProgress)
	{
		UE_LOG(Logmmmfps, Warning, TEXT("[网络] 已在切换/连接中，忽略本次加入"));
		return;
	}

	// 先校验地址：空 / 格式不对 → 直接拒绝（在置 bTravelInProgress 之前返回，所以不会卡住后续开房/加入）。
	if (!IsValidServerAddress(IPAddress))
	{
		UE_LOG(Logmmmfps, Warning, TEXT("[网络] 加入取消：地址为空或格式不对「%s」，请输入形如 127.0.0.1 的 IPv4 地址（可带 :端口）。"), *IPAddress);
		return;
	}

	// 作为客户端连到房主 IP。ClientTravel 是"连接到远程服务器"的标准做法：
	// 它让本机离开当前(菜单)关卡，连进房主的世界，之后由服务器把世界复制过来。
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		UE_LOG(Logmmmfps, Error, TEXT("[网络] 加入失败：拿不到 PlayerController"));
		return;
	}

	// 把玩家名拼进连接 URL 的 ?Name= → 服务器据此设这个客户端的 PlayerState 名字。
	FString URL = IPAddress;
	const FString CleanName = SanitizePlayerName(PlayerName);
	if (!CleanName.IsEmpty())
	{
		URL += FString::Printf(TEXT("?Name=%s"), *CleanName);
	}

	bTravelInProgress = true;   // 标记进入连接流程；连不上时由失败回调复位
	UE_LOG(Logmmmfps, Warning, TEXT("[网络] 加入：连接到 %s"), *URL);
	PC->ClientTravel(URL, ETravelType::TRAVEL_Absolute);
}

void UFPSGameInstance::QuitGame()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

void UFPSGameInstance::Init()
{
	Super::Init();

	// 绑定引擎的失败回调。GameInstance 整局常驻，绑一次即可、无需解绑。
	// 不绑的话，连不上服务器时走引擎默认处理(尝试落到 ServerDefaultMap 等)，容易留下空世界/崩溃。
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UFPSGameInstance::HandleNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UFPSGameInstance::HandleTravelFailure);
	}
}

void UFPSGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 连不上服务器 / 连接被拒 / 掉线都会走这里。常见：填了死 IP 或房主没开。
	UE_LOG(Logmmmfps, Error, TEXT("[网络] 连接失败(%d)：%s —— 退回主菜单"), (int32)FailureType, *ErrorString);
	bTravelInProgress = false;   // 失败了，解除占用，允许重新开房/加入
	ReturnToMenu();
}

void UFPSGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(Logmmmfps, Error, TEXT("[网络] 跳转失败(%d)：%s —— 退回主菜单"), (int32)FailureType, *ErrorString);
	bTravelInProgress = false;
	ReturnToMenu();
}

void UFPSGameInstance::ReturnToMenu()
{
	// 回到菜单地图。失败回调可能在连接流程内触发，这里直接 OpenLevel 重新加载干净的菜单世界。
	UGameplayStatics::OpenLevel(this, MenuLevelName);
}
