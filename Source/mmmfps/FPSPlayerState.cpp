#include "FPSPlayerState.h"
#include "Net/UnrealNetwork.h"

void AFPSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 登记需要复制的属性：服务器一改，引擎自动把新值发给所有客户端。
	DOREPLIFETIME(AFPSPlayerState, Kills);
	DOREPLIFETIME(AFPSPlayerState, Deaths);
}
