#include "WormGameInstance.h"

UWormGameInstance::UWormGameInstance()
{
    // Default constructor
}

void UWormGameInstance::StoreGameResults(const FString& Winner, const TArray<FString>& Players, const TArray<FPlayerDamageInfo>& DamageStats)
{
    WinnerName = Winner;
    PlayerNames = Players;
    PlayerDamageDealt = DamageStats;
    
    UE_LOG(LogTemp, Log, TEXT("Game results stored in Game Instance: Winner=%s, Players=%d, Damage Stats=%d"),
        *WinnerName, PlayerNames.Num(), PlayerDamageDealt.Num());
}

void UWormGameInstance::ClearGameResults()
{
    WinnerName = TEXT("");
    PlayerNames.Empty();
    PlayerDamageDealt.Empty();
}