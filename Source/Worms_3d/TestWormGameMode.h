// TestWormGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ADestructibleTerrain.h"
#include "WormWeapon.h"
#include "ChunkBasedDestructibleTerrain.h"
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

    // Flag pour utiliser le terrain en chunks 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    bool bUseChunkBasedTerrain = false;

    // Classe pour le terrain en chunks 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    TSubclassOf<class AActor> ChunkBasedTerrainClass;

    // Instance du terrain en chunks
    UPROPERTY(BlueprintReadOnly, Category = "Terrain")
    class AActor* ChunkBasedTerrain;

    // Toggle entre les systèmes de terrain
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void ToggleTerrainSystem();

    // Obtenir le nom du système de terrain actif
    UFUNCTION(BlueprintPure, Category = "Terrain")
    FString GetActiveDestructionSystemName() const;

    // Créer et initialiser le terrain basé sur des chunks
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void SpawnChunkBasedTerrain();
    
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
    
    // Classe d'ancien terrain destructible (pour comparaison)
    UPROPERTY(EditDefaultsOnly, Category = "Terrain")
    TSubclassOf<ADestructibleTerrain> OldDestructibleTerrainClass;
    
    // Instance de l'ancien terrain destructible
    UPROPERTY(BlueprintReadOnly, Category = "Terrain")
    ADestructibleTerrain* OldDestructibleTerrain;
    
    // Créer le terrain destructible
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void SpawnDestructibleTerrain();
    
    // Initialiser les armes pour le joueur
    UFUNCTION(BlueprintCallable, Category = "Weapons")
    void InitializeWeaponsForPlayer();

protected:
    // Timer handles
    FTimerHandle WeaponSpawnTimerHandle;
};