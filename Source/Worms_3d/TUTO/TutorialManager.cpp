#include "TutorialManager.h"
#include "WormTutorialCharacter.h"
#include "TutorialTargetBuilding.h"
#include "Worms_3d/Env/EnvironmentalEventsManager.h"
#include "WTutorialWidget.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"
#include "WormTutorialGameMode.h"

ATutorialManager::ATutorialManager()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Default tutorial stages
    TutorialStages.Add("Welcome to Worms 3D! Use WASD keys to move around.");
    TutorialStages.Add("Press SPACE to jump.");
    TutorialStages.Add("Press LEFT MOUSE BUTTON to fire your weapon.");
    TutorialStages.Add("Buildings and terrain can be destroyed. Try shooting at the target wall.");
    TutorialStages.Add("Watch out for water! It's deadly if you fall in.");
    TutorialStages.Add("Congratulations! You've completed the tutorial.");
    
    // Initialize tracking variables
    bHasPlayerMoved = false;
    bHasPlayerJumped = false;
    bHasPlayerFired = false;
    bHasDestroyedTarget = false;
    bHasSurvivedWaterHazard = false;
    CurrentStageIndex = 0;
}

void ATutorialManager::BeginPlay()
{
    Super::BeginPlay();
    
    // Set up event bindings
    SetupEventBindings();
    
    // Create tutorial UI
    if (TutorialWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            TutorialWidget = CreateWidget<UWTutorialWidget>(PC, TutorialWidgetClass);
            if (TutorialWidget)
            {
                TutorialWidget->AddToViewport(100); // High Z-order to be on top
                UpdateTutorialUI();
            }
        }
    }
    
    // Find Tutorial GameMode to synchronize
    AWormTutorialGameMode* TutorialGameMode = Cast<AWormTutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (TutorialGameMode)
    {
        if (TutorialGameMode->PlayerCharacter)
        {
            PlayerCharacter = Cast<AWormTutorialCharacter>(TutorialGameMode->PlayerCharacter);
        }
        
        // Start tutorial after a delay
        FTimerHandle StartTutorialTimer;
        GetWorld()->GetTimerManager().SetTimer(
            StartTutorialTimer,
            this,
            &ATutorialManager::StartTutorial,
            2.0f,
            false
        );
    }
}

void ATutorialManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ATutorialManager::SetupEventBindings()
{
    // Find player character
    if (!PlayerCharacter)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC && PC->GetPawn())
        {
            PlayerCharacter = Cast<AWormTutorialCharacter>(PC->GetPawn());
        }
    }
    
    // Bind to player character events
    if (PlayerCharacter)
    {
        PlayerCharacter->OnCharacterMoved.AddDynamic(this, &ATutorialManager::OnPlayerMoved);
        PlayerCharacter->OnCharacterJumped.AddDynamic(this, &ATutorialManager::OnPlayerJumped);
        PlayerCharacter->OnCharacterFired.AddDynamic(this, &ATutorialManager::OnPlayerFired);
    }
    
    // Find target building
    TArray<AActor*> FoundTargets;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATutorialTargetBuilding::StaticClass(), FoundTargets);
    if (FoundTargets.Num() > 0)
    {
        TargetBuilding = Cast<ATutorialTargetBuilding>(FoundTargets[0]);
        if (TargetBuilding)
        {
            TargetBuilding->OnTargetDestroyed.AddDynamic(this, &ATutorialManager::OnTargetDestroyed);
        }
    }
    
    // Find water manager
    TArray<AActor*> FoundManagers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnvironmentalEventsManager::StaticClass(), FoundManagers);
    if (FoundManagers.Num() > 0)
    {
        WaterManager = Cast<AEnvironmentalEventsManager>(FoundManagers[0]);
    }
}

void ATutorialManager::StartTutorial()
{
    CurrentStageIndex = 0;
    UpdateTutorialUI();
    
    UE_LOG(LogTemp, Warning, TEXT("Tutorial started: %s"), *TutorialStages[CurrentStageIndex]);
}

void ATutorialManager::AdvanceToNextStage()
{
    if (CurrentStageIndex < TutorialStages.Num() - 1)
    {
        // Show completion animation for current stage
        if (TutorialWidget)
        {
            TutorialWidget->ShowObjectiveComplete();
        }
        
        // Schedule next stage with slight delay for animation
        FTimerHandle StageAdvanceTimer;
        GetWorld()->GetTimerManager().SetTimer(
            StageAdvanceTimer,
            [this]()
            {
                CurrentStageIndex++;
                UpdateTutorialUI();
                
                UE_LOG(LogTemp, Warning, TEXT("Advanced to tutorial stage %d: %s"), 
                    CurrentStageIndex, *TutorialStages[CurrentStageIndex]);
                
                // Special handling for water stage
                if (CurrentStageIndex == 4) // Water hazard stage
                {
                    TriggerWaterRise();
                }
            },
            1.5f, // Delay to show completion animation
            false
        );
    }
    else
    {
        // Tutorial complete
        CompleteTutorial();
    }
}

void ATutorialManager::UpdateTutorialUI()
{
    if (TutorialWidget)
    {
        // Set current instruction text
        TutorialWidget->SetInstructionText(TutorialStages[CurrentStageIndex]);
        
        // Update progress indicator
        TutorialWidget->UpdateProgressIndicator(CurrentStageIndex, TutorialStages.Num() - 1);
    }
}

void ATutorialManager::OnPlayerMoved()
{
    bHasPlayerMoved = true;
    
    if (CurrentStageIndex == 0) // Movement stage
    {
        AdvanceToNextStage();
    }
}

void ATutorialManager::OnPlayerJumped()
{
    bHasPlayerJumped = true;
    
    if (CurrentStageIndex == 1) // Jump stage
    {
        AdvanceToNextStage();
    }
}

void ATutorialManager::OnPlayerFired()
{
    bHasPlayerFired = true;
    
    if (CurrentStageIndex == 2) // Weapon firing stage
    {
        AdvanceToNextStage();
    }
}

void ATutorialManager::OnTargetDestroyed()
{
    bHasDestroyedTarget = true;
    
    if (CurrentStageIndex == 3) // Target destruction stage
    {
        AdvanceToNextStage();
    }
}

void ATutorialManager::TriggerWaterRise()
{
    if (WaterManager && WaterManager->WaterSystem)
    {
        // Start water rising slowly
        WaterManager->StartWaterRising();
        
        // Set a timer to advance to completion after a few seconds
        // (assuming player survived the water demo)
        GetWorld()->GetTimerManager().SetTimer(
            WaterRiseTimerHandle,
            [this]()
            {
                bHasSurvivedWaterHazard = true;
                AdvanceToNextStage();
            },
            10.0f, // Allow 10 seconds to observe water rising
            false
        );
    }
    else
    {
        // If no water system, just advance
        bHasSurvivedWaterHazard = true;
        AdvanceToNextStage();
    }
}

void ATutorialManager::CompleteTutorial()
{
    // Show final completion message
    if (TutorialWidget)
    {
        TutorialWidget->ShowObjectiveComplete();
        TutorialWidget->SetInstructionText("Tutorial Complete! You're now ready to play Worms 3D!");
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Tutorial completed!"));
    
    // Give player option to return to main menu after a delay
    FTimerHandle ExitTutorialTimer;
    GetWorld()->GetTimerManager().SetTimer(
        ExitTutorialTimer,
        []()
        {
            // Return to main menu
            UGameplayStatics::OpenLevel(GWorld, FName("MainMenuMap"));
        },
        10.0f, // Allow 10 seconds to read completion message
        false
    );
}