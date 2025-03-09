#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "../AWormCharacter.h"
#include "TestVisibleTerrain.h"
#include "ADestructibleTerrain.h"
#include "Worms_3d/AVoxelBuilding.h"
#include "../NetworkLoadingManager.h"
#include "WormGameState.generated.h"

UCLASS()
class WORMS_3D_API AWormGameState : public AGameState
{
    GENERATED_BODY()
    
public:
    AWormGameState();
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentPlayerChanged, int32, NewPlayerIndex);
   
    // Replicated properties for all clients
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
    int32 CurrentPlayerIndex;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
    float RemainingTurnTime;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
    float TurnDuration;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
    TArray<FString> PlayerNames;
    
    // Properties added to AWormGameState
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
    TArray<bool> PlayerIsAlive;  // To track which players are still alive

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turns")
    FString CurrentPlayerName;   // Name of active player, easier to replicate than an index

    // Network loading manager component for synchronized loading screens
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loading")
    UNetworkLoadingManager* LoadingManager;

    UFUNCTION(BlueprintCallable, Category = "Game")
    void SetCurrentPlayer(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "Game")
    int32 GetRemainingPlayersCount() const;
    
    UFUNCTION(BlueprintCallable, Category = "Game")
    void UpdatePlayerList(const TArray<AController*>& Controllers);
    
    // Public delegate for player change events
    UPROPERTY(BlueprintAssignable, Category = "Game")
    FOnCurrentPlayerChanged OnCurrentPlayerChanged;

    // Function to update active player which will be called from GameMode
    UFUNCTION(BlueprintCallable, Category = "Game")
    void SetCurrentPlayerByIndex(int32 NewIndex);
    
    // Override for replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // New function to get all voxel buildings
    UFUNCTION(BlueprintCallable, Category = "Voxel Buildings")
    TArray<AImprovedVoxelBuilding*> GetAllVoxelBuildings() const;

    // Loading screen convenience methods (calls through to LoadingManager)
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void ShowLoadingScreen(float Duration = 5.0f);
    
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void UpdateLoadingProgress(float Progress, const FString& StatusText);
    
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void DismissLoadingScreen();
};