#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Worms_3d/Init/NetworkLoadingManager.h"
#include "../AWormCharacter.h"

#include "WormGameState.generated.h"
//struct that holds player DamagePlayerNames;DamageValues
USTRUCT(BlueprintType)
struct FPlayerDamageInfo
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlayerName;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DamageValue;
    
    FPlayerDamageInfo() : DamageValue(0.0f) {}
    
    FPlayerDamageInfo(const FString& InName, float InDamage) 
        : PlayerName(InName), DamageValue(InDamage) {}
};

USTRUCT(BlueprintType)
struct FTeamInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TeamId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TeamName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TeamColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<AWormCharacter*> TeamMembers;

    FTeamInfo() : TeamId(-1) {}
};

UCLASS()
class WORMS_3D_API AWormGameState : public AGameState
{
    GENERATED_BODY()
    
public:
    AWormGameState();
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentPlayerChanged, int32, NewPlayerIndex);
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UGameLoadingWidget> LoadingWidgetClass;
    // Replicated properties for all clients
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTurnTimerUpdated);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActivePlayerChanged);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamStatusUpdated);

    // Timer pour le tour actuel - déjà existant mais on ajoute un ReplicatedUsing
    UPROPERTY(ReplicatedUsing=OnRep_RemainingTurnTime, BlueprintReadOnly, Category = "UI")
    float RemainingTurnTime;

    // Joueur actif pour le tour
    UPROPERTY(ReplicatedUsing=OnRep_CurrentPlayerIndex, BlueprintReadOnly, Category = "UI")
    int32 CurrentPlayerIndex;

    // Les deux fonctions ci-dessous existent déjà, mais on va ajouter les callbacks OnRep
    UFUNCTION()
    void OnRep_CurrentPlayerIndex();

    UFUNCTION()
    void OnRep_RemainingTurnTime();

    UFUNCTION()
    void OnRep_Teams();
    UPROPERTY(BlueprintAssignable, Category = "UI")
    FOnTurnTimerUpdated OnTurnTimerUpdated;

    UPROPERTY(BlueprintAssignable, Category = "UI")
    FOnActivePlayerChanged OnActivePlayerChanged;

    UPROPERTY(BlueprintAssignable, Category = "UI")
    FOnTeamStatusUpdated OnTeamStatusUpdated;

    // Fonctions utilitaires pour les widgets
    UFUNCTION(BlueprintPure, Category = "UI")
    bool IsLocalPlayerTurn() const;

    UFUNCTION(BlueprintPure, Category = "UI")
    AWormCharacter* GetActiveCharacter() const;

    
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

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game")
    bool bGameOver;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game")
    FString WinnerName;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game")
    TArray<FPlayerDamageInfo> PlayerDamageDealt;

    // Add this delegate for game over events
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameOverEvent, const FString&, WinnerName);

    UPROPERTY(BlueprintAssignable, Category = "Game")
    FOnGameOverEvent OnGameOver;

    // Add these functions
    UFUNCTION(BlueprintCallable, Category = "Game")
    void AddDamageDealt(const FString& PlayerName, float Damage);

    UFUNCTION(BlueprintCallable, Category = "Game")
    void CheckGameOverCondition();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void TriggerGameOver(const FString& Winner);
    void ShowGameOverWidget();


    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ShowGameOverWidget();

    UPROPERTY(Replicated, BlueprintReadWrite, Category = "Teams")
    TArray<FTeamInfo> Teams;

    // Après les autres fonctions
    UFUNCTION(BlueprintCallable, Category = "Teams")
    TArray<AWormCharacter*> GetTeamMembers(int32 TeamIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Teams")
    void InitializeTeams(int32 NumTeams);

    UFUNCTION(BlueprintCallable, Category = "Teams")
    void AddCharacterToTeam(AWormCharacter* Character, int32 TeamId);
};