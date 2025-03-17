#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialManager.generated.h"

/**
 * Manager actor that coordinates tutorial events and progress
 */
UCLASS()
class WORMS_3D_API ATutorialManager : public AActor
{
    GENERATED_BODY()
    
public:
    ATutorialManager();
    
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    
    // Tutorial stages
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tutorial")
    TArray<FString> TutorialStages;
    
    UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
    int32 CurrentStageIndex;
    
    // References to tutorial components
    UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
    class AWormTutorialCharacter* PlayerCharacter;
    
    UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
    class ATutorialTargetBuilding* TargetBuilding;
    
    UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
    class AEnvironmentalEventsManager* WaterManager;
    
    // Tutorial UI Widget
    UPROPERTY(EditDefaultsOnly, Category = "Tutorial")
    TSubclassOf<class UWTutorialWidget> TutorialWidgetClass;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    class UWTutorialWidget* TutorialWidget;
    
    // Timer for water rising demo
    FTimerHandle WaterRiseTimerHandle;
    
    // Tutorial methods
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void StartTutorial();
    
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void AdvanceToNextStage();
    
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void CompleteTutorial();
    
    // Tutorial event handlers
    UFUNCTION()
    void OnPlayerMoved();
    
    UFUNCTION()
    void OnPlayerJumped();
    
    UFUNCTION()
    void OnPlayerFired();
    
    UFUNCTION()
    void OnTargetDestroyed();
    
    UFUNCTION()
    void TriggerWaterRise();
    
protected:
    // Set up event bindings
    void SetupEventBindings();
    
    // Update tutorial UI
    void UpdateTutorialUI();
    
    // Track player progress
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasPlayerMoved;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasPlayerJumped;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasPlayerFired;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasDestroyedTarget;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasSurvivedWaterHazard;
};