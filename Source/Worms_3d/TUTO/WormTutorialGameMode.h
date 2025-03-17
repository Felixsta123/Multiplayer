#pragma once

#include "CoreMinimal.h"
#include "WormGameMode.h"
#include "TutorialTargetBuilding.h"
#include "Worms_3d/AWormCharacter.h"
#include "WormTutorialGameMode.generated.h"

/**
 * Tutorial game mode for single player training
 */
UCLASS()
class WORMS_3D_API AWormTutorialGameMode : public AWormGameMode
{
    GENERATED_BODY()

public:
    AWormTutorialGameMode();

    virtual void BeginPlay() override;
    
    // Tutorial stages
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tutorial")
    TArray<FString> TutorialStages;
    
    UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
    int32 CurrentStageIndex;
    
    // Reference to player character
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    AWormCharacter* PlayerCharacter;
    
    // Reference to dummy target character
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    AWormCharacter* DummyTarget;
    
    UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Buildings")
    TSubclassOf<ATutorialTargetBuilding> TargetBuildingClass;
    
    // Tutorial UI Widget
    UPROPERTY(EditDefaultsOnly, Category = "Tutorial")
    TSubclassOf<UUserWidget> TutorialWidgetClass;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    UUserWidget* TutorialWidget;
    // Character class specifically for tutorial
    UPROPERTY(EditDefaultsOnly, Category = "Tutorial")
    TSubclassOf<AWormCharacter> TutorialCharacterClass;

    
    // Tutorial methods
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void StartTutorial();
    
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void AdvanceToNextStage();
    
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void CompleteStage(int32 StageIndex);
    
    UFUNCTION(BlueprintNativeEvent, Category = "Tutorial")
    void OnStageCompleted(int32 StageIndex);
    
    // Override turn management to not switch characters in tutorial
    virtual void StartNextTurn() override;
    
    // Generate tutorial environment
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void GenerateTutorialEnvironment();
    
    // Check if player has completed current stage objective
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    bool CheckStageObjective();
    
protected:
    // Track player progress
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasPlayerMoved;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasPlayerJumped;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasPlayerFired;
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    bool bHasPlayerDestroyedTarget;
    // In protected section:
    UFUNCTION()
    void OnTargetDestroyed();
    
    UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
    ATutorialTargetBuilding* TargetBuilding;
    
    // Set up player and dummy for tutorial
    void SetupCharacters();
    void InitializeTutorial();

    // Initialize water system for tutorial
    void SetupWaterSystem();
    
    // Initialize tutorial buildings
    void SetupBuildings();
    
    // Handle player inputs for tutorial progress tracking
    UFUNCTION()
    void OnPlayerMoved();
    
    UFUNCTION()
    void OnPlayerJumped();
    
    UFUNCTION()
    void OnPlayerFired();
};