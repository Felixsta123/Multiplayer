#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameLoadingWidget.h"
#include "PlayerSpawnManager.h"
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

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
    
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Component to manage player spawn points
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPlayerSpawnManager* PlayerSpawnManager;

    // Widget class for the loading screen
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UGameLoadingWidget> LoadingWidgetClass;
    
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
    // Current instance of the loading widget
    UPROPERTY()
    UGameLoadingWidget* LoadingWidget;
    
    // Timer handle for initialization steps
    FTimerHandle InitStepTimerHandle;
    
    // Current initialization step
    int32 CurrentInitStep;
    
    // Execute the current initialization step
    void ExecuteInitializationStep();
};