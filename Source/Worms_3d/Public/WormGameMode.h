// Fichier AWormGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TestVisibleTerrain.h"
#include "../AWormCharacter.h"
#include "ADestructibleTerrain.h"
#include "WormGameMode.generated.h"

UCLASS()
class WORMS_3D_API AWormGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AWormGameMode();

    // Override des fonctions standard de GameMode
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    
    // Fonctions pour la gestion des tours
    UFUNCTION(BlueprintCallable, Category = "Turns")
    void StartNextTurn();
    
    UFUNCTION(BlueprintCallable, Category = "Turns")
    void EndCurrentTurn();
    
    UFUNCTION(BlueprintNativeEvent, Category = "Turns")
    void OnTurnStarted(AController* ActiveController);
    
    UFUNCTION(BlueprintNativeEvent, Category = "Turns")
    void OnTurnEnded(AController* PreviousController);

    // Tableau des controllers actifs (comme dans le BP)
    UPROPERTY(BlueprintReadWrite, Category = "Turns")
    TArray<AController*> AllPlayerControllers;
    
    // Points de spawn
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    TArray<AActor*> SpawnPoints;

    // Fonction utilitaire pour obtenir le personnage contrôlé par un controller
    UFUNCTION(BlueprintCallable, Category = "Helpers")
    AWormCharacter* GetWormCharacterFromController(AController* Controller);

    // Index du controller actif
    UPROPERTY(BlueprintReadWrite, Category = "Turns")
    int32 CurrentPlayerIndex;
    
    // Durée du tour en secondes
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Turns")
    float TurnDuration;
    
    // Temps restant pour le tour actuel
    UPROPERTY(BlueprintReadWrite, Category = "Turns")
    float RemainingTurnTime;
    // Ajoutez ces propriétés dans la classe AWormGameMode:
    UPROPERTY(EditDefaultsOnly, Category = "Test")
    TSubclassOf<ATestVisibleTerrain> TestTerrainClass;

    UPROPERTY(BlueprintReadOnly, Category = "Test")
    ATestVisibleTerrain* TestTerrain;

    // Ajoutez cette fonction:
    UFUNCTION(BlueprintCallable, Category = "Test")
    void SpawnTestTerrain();

    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TArray<TSubclassOf<AWormWeapon>> AvailableWeaponTypes;

    // Fonction pour distribuer les armes aux personnages
    UFUNCTION(BlueprintCallable, Category = "Game")
    void InitializeWeaponsForAllPlayers();
    
protected:

    // Variables diverses comme dans votre BP
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    int32 NewVar;
    
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    bool local;
    
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    TArray<AActor*> Terrain;
    

    // Timer handle pour le tour
    FTimerHandle TurnTimerHandle;
    // Timer handle pour le spawn retardé
    FTimerHandle TerrainSpawnTimerHandle;

    FTimerHandle WeaponSpawnTimerHandle;
    // Fonction pour collecter tous les controllers
    UFUNCTION(BlueprintCallable, Category = "Game")
    void GatherAllPlayerControllers();
    
    // Fonction appelée quand le temps est écoulé
    UFUNCTION()
    void OnTurnTimeExpired();
    
    // Fonction pour vérifier la condition de fin de partie
    UFUNCTION(BlueprintCallable, Category = "Game")
    bool CheckGameEndCondition();
    
    UPROPERTY(EditDefaultsOnly, Category = "Terrain")
    TSubclassOf<ADestructibleTerrain> DestructibleTerrainClass;
    
    // Instance du terrain destructible
    UPROPERTY(BlueprintReadOnly, Category = "Terrain")
    ADestructibleTerrain* DestructibleTerrain;
    
    // Fonction pour spawner le terrain destructible
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void SpawnDestructibleTerrain();

};