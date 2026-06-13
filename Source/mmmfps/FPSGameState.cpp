#include "FPSGameState.h"
#include "FPSPlayerState.h"
#include "Net/UnrealNetwork.h"

void AFPSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFPSGameState, KillsToWin);
	DOREPLIFETIME(AFPSGameState, bMatchOver);
	DOREPLIFETIME(AFPSGameState, WinningPlayer);
}
