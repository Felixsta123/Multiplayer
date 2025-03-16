// WormGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "../AWormCharacter.h"
#include "Worms_3d/Building/AVoxelBuilding.h" // Added VoxelBuilding include
#include "Worms_3d/UI/W_GameLoadingScreen.h"
#include "Worms_3d/Env//WaterSystem.h"
#include "WormGameMode.generated.h"


USTRUCT(BlueprintType)
struct FCharacterNameList
{
    GENERATED_BODY()
    
    UPROPERTY(EditDefaultsOnly, Category = "Names")
    TArray<FString> Names;
};
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
    UPROPERTY(EditDefaultsOnly, Category = "Identification")
    TMap<FString, FCharacterNameList> CharacterNamesByType;
    // Turn duration in seconds
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Turns")
    float TurnDuration;
    
    // Remaining time for current turn
    UPROPERTY(BlueprintReadWrite, Category = "Turns")
    float RemainingTurnTime;

    // Ajouter dans la déclaration de classe:
     UPROPERTY(EditDefaultsOnly, Category = "Water System")
     TSubclassOf<AActor> WaterSystemManagerClass;
    
     UPROPERTY(BlueprintReadOnly, Category = "Water System")
     AActor* WaterSystemManager;
    
     UFUNCTION(BlueprintCallable, Category = "Water System")
     void InitializeWaterSystem();
    
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
    void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
                  FString& ErrorMessage);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> GameOverWidgetClass;

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ShowGameOverWidget();
    void StartRestartSequence();
    virtual void NotifyPlayerReady(APlayerController* PC);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teams")
    int32 NumTeams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teams")
    int32 CharactersPerTeam = 3;

    UPROPERTY(BlueprintReadWrite, Category = "Teams")
    int32 CurrentTeamIndex;

    UPROPERTY(BlueprintReadWrite, Category = "Teams")
    int32 CurrentCharacterIndex;

    UFUNCTION()
    void StartTurnTimer();
    FString GetCharacterInGameName(UClass* CharacterClass, int32 TeamId, int32 CharIndexInTeam);
    // Offset pour les positions de spawn d'une même équipe
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Setup")
    float TeamSpawnOffset = 100.0f;
    FTimerHandle TurnTimerHandle;
    UPROPERTY(BlueprintReadWrite, Category = "Turns")
    bool bDeferTurnEnding = false;
protected:
    // Miscellaneous variables 
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    int32 NewVar;
    
    UPROPERTY(BlueprintReadWrite, Category = "Game")
    bool local;
    bool bInitializationStarted;

    TArray<APlayerController*> ReadyPlayers;
    virtual void CheckAllPlayersReady();
    // Turn timer handle

    // Function called when time expires
    UFUNCTION()
    void OnTurnTimeExpired();
    bool CheckGameOverCondition();
    
    
};