// PlayerSaveGame.cpp

#include "PlayerSaveGame.h"
#include "Net/UnrealNetwork.h"

UPlayerSaveGame::UPlayerSaveGame()
{
}

void UPlayerSaveGame::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPlayerSaveGame, SavedPlayerInfo);
}