// WormGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "../AWormCharacter.h"
#include "Worms_3d/AVoxelBuilding.h" // Added VoxelBuilding include
#include "../W_GameLoadingScreen.h"
#include "WormGameMode.generated.h"

UCLASS()
class WORMS_3D_API AWormGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AWormGameMode();
    // Dans la section des includes de WormGameMode.h

    // Dans la section public: de la classe AWormGameMode
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UW_GameLoadingScreen> LoadingWidgetClass;
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
    void VerifyWeaponsForAllPlayers();

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
    UPROPERTY(BlueprintReadOnly, Category = "Game Initialization")
    class AGameInitManager* GameInitManager;
    
    // Class to use for the game init manager (optional, will use default if not set)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Initialization")
    TSubclassOf<AGameInitManager> GameInitManagerClass;
    
    // Create and configure the game initialization manager
    UFUNCTION(BlueprintCallable, Category = "Game Initialization")
    AGameInitManager* SetupGameInitialization();
    
    // Whether to use loading screen and sequenced initialization
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Game Initialization")
    bool bUseGameInitManager = true;
    // Function to collect all controllers
    UFUNCTION(BlueprintCallable, Category = "Game")
    void GatherAllPlayerControllers();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> GameOverWidgetClass;

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ShowGameOverWidget();
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


    // Function called when time expires
    UFUNCTION()
    void OnTurnTimeExpired();
    
    // Function to check game end condition
    UFUNCTION(BlueprintCallable, Category = "Game")
    bool CheckGameEndCondition();
    
};