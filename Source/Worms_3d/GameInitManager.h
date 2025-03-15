#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameLoadingWidget.h"
#include "PlayerSpawnManager.h"
#include "WormGameState.h"
#include "W_GameLoadingScreen.h"
#include "GameInitManager.generated.h"

/**
 * Actor to manage game initialization sequence including loading screen and player spawns
 */
UCLASS()
class WORMS_3D_API AGameInitManager : public AActor
{
    GENERATED_BODY()
    
public:    
    AGameInitManager();
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UW_GameLoadingScreen> LoadingWidgetClass;

    //LoadingWidget
    UPROPERTY()
    UW_GameLoadingScreen* LoadingWidget;
    int CycleCount;
    int MaxCycles;
    FTimerHandle TurnCycleTimerHandle;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
    
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Component to manage player spawn points
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPlayerSpawnManager* PlayerSpawnManager;


    // Duration to show the loading screen
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    float LoadingScreenDuration = 5.0f;
    
    // Whether to automatically handle initialization sequence
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization")
    bool bAutoHandleInitialization = true;
    
    // Start the initialization sequence
    UFUNCTION(BlueprintCallable, Category = "Initialization")
    void StartInitializationSequence();
    
    // Update loading progress
    UFUNCTION(BlueprintCallable, Category = "Initialization")
    void UpdateLoadingProgress(float Progress, const FString& StatusText);
    
    // Complete the initialization sequence
    UFUNCTION(BlueprintCallable, Category = "Initialization")
    void CompleteInitialization();
    
protected:
    // Reference to the game state for network synchronized loading
    UPROPERTY()
    AWormGameState* WormGameState;
    // Timer handle for initialization steps
    FTimerHandle InitStepTimerHandle;
    
    // Current initialization step
    int32 CurrentInitStep;
    
    // Execute the current initialization step
    void ExecuteInitializationStep();
    void CycleThroughTurns();

    // Initialize the network loading system
    void InitializeNetworkLoading();
};