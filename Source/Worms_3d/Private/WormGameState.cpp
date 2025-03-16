#include "WormGameState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameMode.h"
#include "WormPlayerController.h"
#include "Worms_3d/Building/AVoxelBuilding.h"

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
    DOREPLIFETIME(AWormGameState, TurnDuration);
    DOREPLIFETIME(AWormGameState, PlayerNames);
    DOREPLIFETIME(AWormGameState, PlayerIsAlive);
    DOREPLIFETIME(AWormGameState, CurrentPlayerName);
    DOREPLIFETIME(AWormGameState, bGameOver);
    DOREPLIFETIME(AWormGameState, WinnerName);
    DOREPLIFETIME(AWormGameState, PlayerDamageDealt);

    DOREPLIFETIME_CONDITION_NOTIFY(AWormGameState, RemainingTurnTime, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(AWormGameState, CurrentPlayerIndex, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(AWormGameState, Teams, COND_None, REPNOTIFY_Always);
    
}

void AWormGameState::UpdatePlayerList(const TArray<AController*>& Controllers)
{
    PlayerNames.Empty();
    PlayerIsAlive.Empty();
    
    for (AController* Controller : Controllers)
    {
        if (Controller)
        {
            FString PlayerName;
            AWormPlayerController* WPC = Cast<AWormPlayerController>(Controller);
            
            // Si c'est un WormPlayerController avec un nom personnalisé, utiliser ce nom
            if (WPC && !WPC->PlayerSettings.MyPlayerName.IsEmpty())
            {
                PlayerName = WPC->PlayerSettings.MyPlayerName.ToString();
            }
            else
            {
                // Sinon, fallback au nom du Pawn
                if (Controller->GetPawn())
                {
                    PlayerName = Controller->GetPawn()->GetName();
                }
                else
                {
                    PlayerName = Controller->GetName();
                }
            }
            
            bool IsAlive = false;
            AWormCharacter* Character = nullptr;
            
            if (Controller->GetPawn())
            {
                Character = Cast<AWormCharacter>(Controller->GetPawn());
            }
            
            IsAlive = (Character && Character->GetHealth() > 0);
            
            PlayerNames.Add(PlayerName);
            PlayerIsAlive.Add(IsAlive);
        }
    }
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


void AWormGameState::AddDamageDealt(const FString& PlayerName, float Damage)
{
    if (HasAuthority())
    {
        // Find existing entry for this player
        bool bFoundPlayer = false;
        for (int32 i = 0; i < PlayerDamageDealt.Num(); i++)
        {
            if (PlayerDamageDealt[i].PlayerName == PlayerName)
            {
                PlayerDamageDealt[i].DamageValue += Damage;
                bFoundPlayer = true;
                UE_LOG(LogTemp, Log, TEXT("Player %s has dealt %.1f total damage"), 
                    *PlayerName, PlayerDamageDealt[i].DamageValue);
                break;
            }
        }
        
        // If player not found, add new entry
        if (!bFoundPlayer)
        {
            PlayerDamageDealt.Add(FPlayerDamageInfo(PlayerName, Damage));
            UE_LOG(LogTemp, Log, TEXT("Player %s has dealt %.1f damage (first record)"), *PlayerName, Damage);
        }
        
        // Force replication
        MARK_PROPERTY_DIRTY_FROM_NAME(AWormGameState, PlayerDamageDealt, this);
    }
}

void AWormGameState::CheckGameOverCondition()
{
    if (HasAuthority() && !bGameOver)
    {
        UE_LOG(LogTemp, Warning, TEXT("Checking game over condition"));
        
        // Méthode directe: Compter le nombre de personnages vivants dans le monde
        TArray<AActor*> AllWormChars;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllWormChars);
        
        int32 AliveCount = 0;
        AWormCharacter* LastAliveChar = nullptr;
        
        for (AActor* Actor : AllWormChars)
        {
            AWormCharacter* Character = Cast<AWormCharacter>(Actor);
            if (Character && Character->GetHealth() > 0)
            {
                AliveCount++;
                LastAliveChar = Character;
                UE_LOG(LogTemp, Warning, TEXT("Character %s is alive with health %.1f"), 
                   *Character->GetName(), Character->GetHealth());
            }
            else if (Character)
            {
                UE_LOG(LogTemp, Warning, TEXT("Character %s is dead with health %.1f"), 
                   *Character->GetName(), Character->GetHealth());
            }
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Found %d alive characters"), AliveCount);
        
        // Game over si un seul joueur est encore vivant
        if (AliveCount <= 1 && LastAliveChar)
        {
            UE_LOG(LogTemp, Warning, TEXT("Game over! Winner is %s"), *LastAliveChar->GetName());
            
            // Trouver le nom du joueur gagnant
             WinnerName = LastAliveChar->GetName();
            AController* WinnerController = LastAliveChar->GetController();
            
            if (WinnerController)
            {
                // Trouver l'index du contrôleur pour obtenir le nom correct
                for (int32 i = 0; i < PlayerNames.Num(); i++)
                {
                    if (WinnerController == UGameplayStatics::GetPlayerController(GetWorld(), i))
                    {
                        WinnerName = PlayerNames[i];
                        break;
                    }
                }
            }
            
            TriggerGameOver(WinnerName);
        }
    }
}

void AWormGameState::TriggerGameOver(const FString& Winner)
{
    if (HasAuthority() && !bGameOver)
    {
        bGameOver = true;
        WinnerName = Winner;
        
        // Force replication
        MARK_PROPERTY_DIRTY_FROM_NAME(AWormGameState, bGameOver, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(AWormGameState, WinnerName, this);
        
        // Broadcast event to all clients
        OnGameOver.Broadcast(Winner);
        
        UE_LOG(LogTemp, Warning, TEXT("Game over triggered! Winner: %s"), *Winner);
        
        // Show results widget after a short delay
        FTimerHandle ShowWidgetTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            ShowWidgetTimerHandle,
            this,
            &AWormGameState::ShowGameOverWidget,
            2.0f, // 2 second delay before showing widget
            false
        );
    }
}

void AWormGameState::ShowGameOverWidget()
{
    // Use RPC to show the widget on all clients
    Multicast_ShowGameOverWidget();
}

void AWormGameState::Multicast_ShowGameOverWidget_Implementation()
{
    // Find the Game Mode
    AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (GameMode)
    {
        GameMode->ShowGameOverWidget();
    }
    
    // Also show it on listen server or in standalone
    if (GetWorld()->GetNetMode() == NM_ListenServer || GetWorld()->GetNetMode() == NM_Standalone)
    {
        // Get the local player controller
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            // Try to show the widget directly on this machine
            AWormGameMode* LocalGameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
            if (LocalGameMode)
            {
                LocalGameMode->ShowGameOverWidget();
            }
        }
    }
}


TArray<AWormCharacter*> AWormGameState::GetTeamMembers(int32 TeamIndex) const
{
    if (Teams.IsValidIndex(TeamIndex))
    {
        return Teams[TeamIndex].TeamMembers;
    }
    return TArray<AWormCharacter*>();
}

void AWormGameState::InitializeTeams(int32 NumTeams)
{
    UE_LOG(LogTemp, Warning, TEXT("========================"));
    UE_LOG(LogTemp, Warning, TEXT("Initializing %d teams"), NumTeams);
    UE_LOG(LogTemp, Warning, TEXT("========================"));

    Teams.Empty();

    /*	switch (TeamId)
    {
        case 0: return FLinearColor(0.0f, 0.5f, 1.0f); // Blue
        case 1: return FLinearColor(1.0f, 0.2f, 0.2f); // Red
        case 2: return FLinearColor(0.2f, 0.8f, 0.2f); // Green
        case 3: return FLinearColor(1.0f, 0.8f, 0.0f); // Yellow
        default: return FLinearColor(0.7f, 0.7f, 0.7f); // Gray
        */
    for (int32 i = 0; i < NumTeams; i++)
    {
        FTeamInfo NewTeam;
        NewTeam.TeamId = i;
        NewTeam.TeamName = FString::Printf(TEXT("Team %d"), i + 1);
        switch (i)
        {
        case 0: NewTeam.TeamColor = FLinearColor(0.0f, 0.5f, 1.0f); break; // Blue
        case 1: NewTeam.TeamColor = FLinearColor(1.0f, 0.2f, 0.2f); break; // Red
        case 2: NewTeam.TeamColor = FLinearColor(0.2f, 0.8f, 0.2f); break; // Green
        case 3: NewTeam.TeamColor = FLinearColor(1.0f, 0.8f, 0.0f); break; // Yellow
        default: NewTeam.TeamColor = FLinearColor(0.7f, 0.7f, 0.7f); break; // Gray
        }
        Teams.Add(NewTeam);
        UE_LOG(LogTemp, Warning, TEXT("Created Team %d - Name: %s"), i, *NewTeam.TeamName);
    }
    
    ForceNetUpdate();
}


void AWormGameState::AddCharacterToTeam(AWormCharacter* Character, int32 TeamId)
{
    if (!Character) 
    {
        UE_LOG(LogTemp, Error, TEXT("Attempted to add NULL character to team %d"), TeamId);
        return;
    }
    
    if (!Teams.IsValidIndex(TeamId))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid team index %d when adding character %s"), 
            TeamId, *Character->GetName());
        return;
    }

    Teams[TeamId].TeamMembers.Add(Character);
    
    // Log détaillé du personnage ajouté
    UE_LOG(LogTemp, Warning, TEXT("Added character to Team %d:"), TeamId);
    UE_LOG(LogTemp, Warning, TEXT("  - Name: %s"), *Character->GetName());
    UE_LOG(LogTemp, Warning, TEXT("  - Health: %.1f"), Character->GetHealth());
    UE_LOG(LogTemp, Warning, TEXT("  - TeamId: %d"), Character->TeamId);
    UE_LOG(LogTemp, Warning, TEXT("  - CharacterIndexInTeam: %d"), Character->CharacterIndexInTeam);
    
    // Afficher l'état actuel de l'équipe
    UE_LOG(LogTemp, Warning, TEXT("Team %d now has %d members:"), 
        TeamId, Teams[TeamId].TeamMembers.Num());
    for (int32 i = 0; i < Teams[TeamId].TeamMembers.Num(); i++)
    {
        AWormCharacter* TeamMember = Teams[TeamId].TeamMembers[i];
        if (TeamMember)
        {
            UE_LOG(LogTemp, Warning, TEXT("  %d. %s"), i + 1, *TeamMember->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("  %d. NULL MEMBER"), i + 1);
        }
    }

    ForceNetUpdate();
}
void AWormGameState::OnRep_CurrentPlayerIndex()
{
    // Notify widgets that the active player has changed
    UE_LOG(LogTemp, Warning, TEXT("OnRep_CurrentPlayerIndex called: Current player now %d"), CurrentPlayerIndex);
    
    // Update CurrentPlayerName to match CurrentPlayerIndex
    if (PlayerNames.IsValidIndex(CurrentPlayerIndex))
    {
        CurrentPlayerName = PlayerNames[CurrentPlayerIndex];
    }
    
    // Broadcast to all registered listeners
    OnCurrentPlayerChanged.Broadcast(CurrentPlayerIndex);
    OnActivePlayerChanged.Broadcast();
}

void AWormGameState::OnRep_RemainingTurnTime()
{
    // Notify widgets that the timer has been updated
    UE_LOG(LogTemp, Warning, TEXT("OnRep_RemainingTurnTime called: %.2f seconds"), RemainingTurnTime);
    
    // Broadcast to all registered listeners
    OnTurnTimerUpdated.Broadcast();
}

void AWormGameState::OnRep_Teams()
{
    // Notify widgets that the teams have been updated
    UE_LOG(LogTemp, Warning, TEXT("OnRep_Teams called: %d Teams updated"), Teams.Num());
    
    // Broadcast to all registered listeners
    OnTeamStatusUpdated.Broadcast();
}

bool AWormGameState::IsLocalPlayerTurn() const
{
    // Get the local player controller
    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!LocalPC)
    {
        return false;
    }
    
    // Get active character
    AWormCharacter* ActiveChar = nullptr;
    for (int32 i = 0; i < Teams.Num(); i++)
    {
        for (AWormCharacter* Character : Teams[i].TeamMembers)
        {
            if (Character && Character->IsMyTurn())
            {
                ActiveChar = Character;
                break;
            }
        }
        if (ActiveChar) break;
    }
    
    if (!ActiveChar) return false;
    
    // Check if the local player is controlling the active character
    return (LocalPC == ActiveChar->GetController());
}

// Improved GetActiveCharacter function for team-based play
AWormCharacter* AWormGameState::GetActiveCharacter() const
{
    // Look through all teams to find the active character
    for (int32 i = 0; i < Teams.Num(); i++)
    {
        for (AWormCharacter* Character : Teams[i].TeamMembers)
        {
            if (Character && Character->IsMyTurn())
            {
                return Character;
            }
        }
    }
    
    // Fallback to previous method if no active character found in teams
    if (CurrentPlayerIndex >= 0 && CurrentPlayerIndex < GetWorld()->GetNumPlayerControllers())
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), CurrentPlayerIndex);
        if (PC)
        {
            return Cast<AWormCharacter>(PC->GetPawn());
        }
    }
    
    return nullptr;
}