#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ADestructibleTerrain.h"
#include "WormWeapon.h"
#include "TestWormGameMode.generated.h"

UCLASS()
class WORMS_3D_API ATestWormGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    ATestWormGameMode();

    // Fonctions de base du GameMode
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // Terrain destructible
    UPROPERTY(BlueprintReadOnly, Category = "Terrain")
    ADestructibleTerrain* DestructibleTerrain;

    // Créer le terrain destructible
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void SpawnDestructibleTerrain();
    
    // Réinitialise le tour (pour le mode solo)
    UFUNCTION(BlueprintCallable, Category = "Game")
    void ResetTurn();
    
    // Durée du tour en secondes
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Game")
    float TurnDuration;
    
    // Temps restant pour le tour actuel
    UPROPERTY(BlueprintReadOnly, Category = "Game")
    float RemainingTurnTime;
    
    // Classes d'armes disponibles
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TArray<TSubclassOf<AWormWeapon>> AvailableWeaponTypes;
    
    // Initialiser les armes pour le joueur
    UFUNCTION(BlueprintCallable, Category = "Weapons")
    void InitializeWeaponsForPlayer();

protected:
    // Timer handles
    FTimerHandle WeaponSpawnTimerHandle;
};