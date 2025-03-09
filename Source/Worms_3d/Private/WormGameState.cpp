#include "WormGameState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameMode.h"
#include "Worms_3d/AVoxelBuilding.h"

AWormGameState::AWormGameState()
{
    CurrentPlayerIndex = 0;
    RemainingTurnTime = 0.0f;
    TurnDuration = 30.0f;
    
    // Create NetworkLoadingManager component
    LoadingManager = CreateDefaultSubobject<UNetworkLoadingManager>(TEXT("LoadingManager"));
}

// Add this function in GetLifetimeReplicatedProps
void AWormGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate these properties to all clients
    DOREPLIFETIME(AWormGameState, CurrentPlayerIndex);
    DOREPLIFETIME(AWormGameState, RemainingTurnTime);
    DOREPLIFETIME(AWormGameState, TurnDuration);
    DOREPLIFETIME(AWormGameState, PlayerNames);
    DOREPLIFETIME(AWormGameState, PlayerIsAlive);
    DOREPLIFETIME(AWormGameState, CurrentPlayerName);
}

void AWormGameState::UpdatePlayerList(const TArray<AController*>& Controllers)
{
    // Empty lists
    PlayerNames.Empty();
    PlayerIsAlive.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("Updating player list with %d controllers"), Controllers.Num());
    
    // Add name of each player and their status
    for (AController* Controller : Controllers)
    {
        if (Controller)
        {
            FString PlayerName = Controller->GetName();
            bool IsAlive = false;
            
            // Check if controller has a pawn and if it's alive
            AWormCharacter* Character = nullptr;
            if (Controller->GetPawn())
            {
                Character = Cast<AWormCharacter>(Controller->GetPawn());
                PlayerName = Controller->GetPawn()->GetName();
            }
            
            // Determine if player is alive
            IsAlive = (Character && Character->GetHealth() > 0);
            
            PlayerNames.Add(PlayerName);
            PlayerIsAlive.Add(IsAlive);
            
            UE_LOG(LogTemp, Log, TEXT("Added player: %s (Alive: %s)"), 
                *PlayerName, IsAlive ? TEXT("Yes") : TEXT("No"));
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Player list updated, now contains %d players"), PlayerNames.Num());
}

// Improve GetRemainingPlayersCount to only count alive players
int32 AWormGameState::GetRemainingPlayersCount() const
{
    int32 AliveCount = 0;
    
    // Count alive players
    for (bool IsAlive : PlayerIsAlive)
    {
        if (IsAlive)
        {
            AliveCount++;
        }
    }
    
    return AliveCount;
}

// Add this function to update active player
void AWormGameState::SetCurrentPlayer(int32 PlayerIndex)
{
    CurrentPlayerIndex = PlayerIndex;
    
    // Update active player name for easier replication
    if (PlayerNames.IsValidIndex(CurrentPlayerIndex))
    {
        CurrentPlayerName = PlayerNames[CurrentPlayerIndex];
        UE_LOG(LogTemp, Log, TEXT("Current player set to: %s (index: %d)"), 
            *CurrentPlayerName, CurrentPlayerIndex);
    }
    else
    {
        CurrentPlayerName = TEXT("No active player");
        UE_LOG(LogTemp, Warning, TEXT("Invalid player index: %d"), CurrentPlayerIndex);
    }
}

void AWormGameState::SetCurrentPlayerByIndex(int32 NewIndex)
{
    // Check if index is valid
    if (!PlayerNames.IsValidIndex(NewIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid player index: %d (only %d players)"), 
            NewIndex, PlayerNames.Num());
        return;
    }
    
    // Store old index for comparison
    int32 OldIndex = CurrentPlayerIndex;
    
    // Update index
    CurrentPlayerIndex = NewIndex;
    
    // Force replication by explicitly marking as dirty
    MARK_PROPERTY_DIRTY_FROM_NAME(AWormGameState, CurrentPlayerIndex, this);
    
    // Update active player name
    FString OldName = CurrentPlayerName;
    CurrentPlayerName = PlayerNames[CurrentPlayerIndex];
    
    // Force replication of name as well
    MARK_PROPERTY_DIRTY_FROM_NAME(AWormGameState, CurrentPlayerName, this);
    
    UE_LOG(LogTemp, Log, TEXT("GameState: Current player changed from %s to %s (index %d to %d)"), 
        *OldName, *CurrentPlayerName, OldIndex, CurrentPlayerIndex);
    
    // Trigger delegate in multicast mode for all clients
    OnCurrentPlayerChanged.Broadcast(CurrentPlayerIndex);
}

TArray<AImprovedVoxelBuilding*> AWormGameState::GetAllVoxelBuildings() const
{
    // Use the utility function directly from AImprovedVoxelBuilding
    return AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
}

// Loading screen convenience methods

void AWormGameState::ShowLoadingScreen(float Duration)
{
    if (LoadingManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("GameState: Showing loading screen to all clients with duration %.1f"), Duration);
        LoadingManager->ShowLoadingScreen(Duration);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("LoadingManager not found in GameState!"));
    }
}

void AWormGameState::UpdateLoadingProgress(float Progress, const FString& StatusText)
{
    if (LoadingManager)
    {
        LoadingManager->UpdateLoadingProgress(Progress, StatusText);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("LoadingManager not found in GameState!"));
    }
}

void AWormGameState::DismissLoadingScreen()
{
    if (LoadingManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("GameState: Dismissing loading screen on all clients"));
        LoadingManager->DismissLoadingScreen();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("LoadingManager not found in GameState!"));
    }
}