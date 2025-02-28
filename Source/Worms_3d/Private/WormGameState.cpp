// WormGameState.cpp
#include "WormGameState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

AWormGameState::AWormGameState()
{
	CurrentPlayerIndex = 0;
	RemainingTurnTime = 0.0f;
	TurnDuration = 30.0f;
}

// Ajoutez cette fonction dans GetLifetimeReplicatedProps
void AWormGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
	// Répliquer ces propriétés vers tous les clients
	DOREPLIFETIME(AWormGameState, CurrentPlayerIndex);
	DOREPLIFETIME(AWormGameState, RemainingTurnTime);
	DOREPLIFETIME(AWormGameState, TurnDuration);
	DOREPLIFETIME(AWormGameState, PlayerNames);
	DOREPLIFETIME(AWormGameState, PlayerIsAlive);
	DOREPLIFETIME(AWormGameState, CurrentPlayerName);
	DOREPLIFETIME(AWormGameState, DestructibleTerrain);
	DOREPLIFETIME(AWormGameState, TestTerrain);

}


void AWormGameState::UpdatePlayerList(const TArray<AController*>& Controllers)
{
	// Vider les listes
	PlayerNames.Empty();
	PlayerIsAlive.Empty();
    
	UE_LOG(LogTemp, Log, TEXT("Updating player list with %d controllers"), Controllers.Num());
    
	// Ajouter le nom de chaque joueur et son statut
	for (AController* Controller : Controllers)
	{
		if (Controller)
		{
			FString PlayerName = Controller->GetName();
			bool IsAlive = false;
            
			// Vérifier si le contrôleur possède un pawn et s'il est en vie
			AWormCharacter* Character = nullptr;
			if (Controller->GetPawn())
			{
				Character = Cast<AWormCharacter>(Controller->GetPawn());
				PlayerName = Controller->GetPawn()->GetName();
			}
            
			// Détermine si le joueur est en vie
			IsAlive = (Character && Character->GetHealth() > 0);
            
			PlayerNames.Add(PlayerName);
			PlayerIsAlive.Add(IsAlive);
            
			UE_LOG(LogTemp, Log, TEXT("Added player: %s (Alive: %s)"), 
				*PlayerName, IsAlive ? TEXT("Yes") : TEXT("No"));
		}
	}
    
	UE_LOG(LogTemp, Log, TEXT("Player list updated, now contains %d players"), PlayerNames.Num());
}

// Améliorez GetRemainingPlayersCount pour ne compter que les joueurs vivants
int32 AWormGameState::GetRemainingPlayersCount() const
{
	int32 AliveCount = 0;
    
	// Compter les joueurs vivants
	for (bool IsAlive : PlayerIsAlive)
	{
		if (IsAlive)
		{
			AliveCount++;
		}
	}
    
	return AliveCount;
}

// Ajoutez cette fonction pour mettre à jour le joueur actif
void AWormGameState::SetCurrentPlayer(int32 PlayerIndex)
{
	CurrentPlayerIndex = PlayerIndex;
    
	// Mettre à jour le nom du joueur actif pour faciliter la réplication
	if (PlayerNames.IsValidIndex(CurrentPlayerIndex))
	{
		CurrentPlayerName = PlayerNames[CurrentPlayerIndex];
		UE_LOG(LogTemp, Log, TEXT("Current player set to: %s (index: %d)"), 
			*CurrentPlayerName, CurrentPlayerIndex);
	}
	else
	{
		CurrentPlayerName = TEXT("Aucun joueur actif");
		UE_LOG(LogTemp, Warning, TEXT("Invalid player index: %d"), CurrentPlayerIndex);
	}
}

void AWormGameState::SetCurrentPlayerByIndex(int32 NewIndex)
{
	// Vérifier si l'index est valide
	if (!PlayerNames.IsValidIndex(NewIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid player index: %d (only %d players)"), 
			NewIndex, PlayerNames.Num());
		return;
	}
    
	// Stocker l'ancien index pour comparaison
	int32 OldIndex = CurrentPlayerIndex;
    
	// Mettre à jour l'index
	CurrentPlayerIndex = NewIndex;
    
	// Forcer la réplication en marquant explicitement comme dirty
	MARK_PROPERTY_DIRTY_FROM_NAME(AWormGameState, CurrentPlayerIndex, this);
    
	// Mettre à jour le nom du joueur actif
	FString OldName = CurrentPlayerName;
	CurrentPlayerName = PlayerNames[CurrentPlayerIndex];
    
	// Forcer la réplication du nom également
	MARK_PROPERTY_DIRTY_FROM_NAME(AWormGameState, CurrentPlayerName, this);
    
	UE_LOG(LogTemp, Log, TEXT("GameState: Current player changed from %s to %s (index %d to %d)"), 
		*OldName, *CurrentPlayerName, OldIndex, CurrentPlayerIndex);
    
	// Déclencher le delegate en mode multicast pour tous les clients
	OnCurrentPlayerChanged.Broadcast(CurrentPlayerIndex);
}


