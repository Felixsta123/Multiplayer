// WormGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TestVisibleTerrain.h"
#include "../AWormCharacter.h"
#include "ADestructibleTerrain.h"
#include "Worms_3d/AVoxelBuilding.h" // Added VoxelBuilding include
#include "WormGameMode.generated.h"

UCLASS()
class WORMS_3D_API AWormGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AWormGameMode();

    // Override standard GameMode functions
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    // Appliquer les paramètres de terrain à tous les bâtiments générés
    UFUNCTION(BlueprintCallable, Category = "Voxel Building")
    void ApplyTerrainSettings();
    // Turn management functions
    UFUNCTION(BlueprintCallable, Category = "Turns")
    void StartNextTurn();
    
    UFUNCTION(BlueprintCallable, Category = "Turns")
    void EndCurrentTurn();
    
    UFUNCTION(BlueprintNativeEvent, Category = "Turns")
    void OnTurnStarted(AController* ActiveController);
    
    UFUNCTION(BlueprintNativeEvent, Category = "Turns")
    void OnTurnEnded(AController* PreviousController);

    // Array of active controllers
    UPROPERTY(BlueprintReadWrite, Category = "Turns")
    TArray<AController*> AllPlayerControllers;
    
    // Spawn points
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    TArray<AActor*> SpawnPoints;

    // Utility function to get character controlled by a controller
    UFUNCTION(BlueprintCallable, Category = "Helpers")
    AWormCharacter* GetWormCharacterFromController(AController* Controller);

    // Index of active controller
    UPROPERTY(BlueprintReadWrite, Category = "Turns")
    int32 CurrentPlayerIndex;
    
    // Turn duration in seconds
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Turns")
    float TurnDuration;
    
    // Remaining time for current turn
    UPROPERTY(BlueprintReadWrite, Category = "Turns")
    float RemainingTurnTime;

    
    // Available weapon types for distribution to players
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TArray<TSubclassOf<AWormWeapon>> AvailableWeaponTypes;

    // Function to distribute weapons to characters
    UFUNCTION(BlueprintCallable, Category = "Game")
    void InitializeWeaponsForAllPlayers();
    
    // Voxel building properties
    UPROPERTY(EditDefaultsOnly, Category = "Voxel Building")
    TSubclassOf<AImprovedVoxelBuilding> VoxelBuildingClass;
    
    UPROPERTY(EditDefaultsOnly, Category = "Voxel Building")
    int32 NumberOfBuildings;
    
    UPROPERTY(EditDefaultsOnly, Category = "Voxel Building")
    float SpawnAreaSize;

    // Function to generate voxel buildings
    UFUNCTION(BlueprintCallable, Category = "Voxel Building")
    void GenerateVoxelBuildings();
    
    // Function to find all voxel buildings in the level
    UFUNCTION(BlueprintCallable, Category = "Voxel Building")
    TArray<AImprovedVoxelBuilding*> GetAllVoxelBuildings();
    
protected:
    // Miscellaneous variables 
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    int32 NewVar;
    
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    bool local;
    

    // Turn timer handle
    FTimerHandle TurnTimerHandle;
    
    // Delayed spawn timer handles
    FTimerHandle TerrainSpawnTimerHandle;
    FTimerHandle VoxelBuildingsSpawnTimerHandle;
    FTimerHandle WeaponSpawnTimerHandle;

    // Function to collect all controllers
    UFUNCTION(BlueprintCallable, Category = "Game")
    void GatherAllPlayerControllers();
    
    // Function called when time expires
    UFUNCTION()
    void OnTurnTimeExpired();
    
    // Function to check game end condition
    UFUNCTION(BlueprintCallable, Category = "Game")
    bool CheckGameEndCondition();
    
};