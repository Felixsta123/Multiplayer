// WormGameState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "../AWormCharacter.h"
#include "TestVisibleTerrain.h"
#include "ADestructibleTerrain.h"
#include "WormGameState.generated.h"

UCLASS()
class WORMS_3D_API AWormGameState : public AGameState
{
	GENERATED_BODY()
    
public:
	AWormGameState();
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentPlayerChanged, int32, NewPlayerIndex);



	
	// Propriétés répliquées pour tous les clients
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
	int32 CurrentPlayerIndex;
    
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
	float RemainingTurnTime;
    
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
	float TurnDuration;
    
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
	TArray<FString> PlayerNames;
    
	// Ajoutez ces propriétés dans AWormGameState
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
	TArray<bool> PlayerIsAlive;  // Pour suivre quels joueurs sont encore en vie

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
	FString CurrentPlayerName;   // Nom du joueur actif, plus facile à répliquer qu'un indice

	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetCurrentPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Game")
	int32 GetRemainingPlayersCount() const;
	
	UFUNCTION(BlueprintCallable, Category = "Game")
	void UpdatePlayerList(const TArray<AController*>& Controllers);
	
	// Le delegate public pour les événements de changement de joueur
	UPROPERTY(BlueprintAssignable, Category = "Game")
	FOnCurrentPlayerChanged OnCurrentPlayerChanged;

	// Une fonction pour mettre à jour le joueur actif qui sera appelée depuis le GameMode
	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetCurrentPlayerByIndex(int32 NewIndex);
	// Override pour la réplication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Test")
	ATestVisibleTerrain* TestTerrain;
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Terrain")
	ADestructibleTerrain* DestructibleTerrain;
};