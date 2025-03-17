#include "WormTutorialGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Worms_3d/Init/PlayerSpawnManager.h"
#include "Worms_3d/Building/AVoxelBuilding.h"
#include "Worms_3d/Env/EnvironmentalEventsManager.h"
#include "WormGameState.h"
#include "WTutorialWidget.h"
#include "WormPlayerController.h"
#include "EngineUtils.h"
#include "TutorialTargetBuilding.h"
#include "TutorialWaterTrigger.h"
#include "WormTutorialCharacter.h"

AWormTutorialGameMode::AWormTutorialGameMode()
{
    // Default setup
    NumTeams = 1;
    CharactersPerTeam = 1;
    
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
    bHasPlayerDestroyedTarget = false;
    bUseGameInitManager = false;
    CurrentStageIndex = 0;
}
void AWormTutorialGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    // Disable team mechanics for tutorial - we just need one player
    NumTeams = 1;
    CharactersPerTeam = 1;
    
    // Initialize game state for single player
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (WormGS)
    {
        WormGS->InitializeTeams(1); // Just one team for tutorial
    }
    
    // Start tutorial with slight delay to ensure everything is loaded
    FTimerHandle StartTutorialTimer;
    GetWorld()->GetTimerManager().SetTimer(
        StartTutorialTimer,
        this,
        &AWormTutorialGameMode::InitializeTutorial,
        1.0f,
        false
    );
}

void AWormTutorialGameMode::InitializeTutorial()
{
    UE_LOG(LogTemp, Warning, TEXT("Initializing tutorial..."));
    
    // 1. Find player controller
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("No PlayerController found for tutorial"));
        return;
    }

    // 2. Spawn our tutorial character if needed
    AWormTutorialCharacter* TutorialChar = nullptr;
    
    // Check if controller already has a valid character
    AWormCharacter* ExistingCharacter = Cast<AWormCharacter>(PC->GetPawn());
    if (ExistingCharacter && ExistingCharacter->IsA<AWormTutorialCharacter>())
    {
        // Use existing character if it's the right type
        TutorialChar = Cast<AWormTutorialCharacter>(ExistingCharacter);
        PlayerCharacter = TutorialChar;
        UE_LOG(LogTemp, Warning, TEXT("Using existing tutorial character: %s"), *PlayerCharacter->GetName());
    }
    else
    {
        // Spawn new character if needed
        FVector SpawnLocation = FVector(0, 0, 300); // Default spawn location
        FRotator SpawnRotation = FRotator::ZeroRotator;
        
        // Use spawn point if available
        AActor* StartSpot = FindPlayerStart(PC);
        if (StartSpot)
        {
            SpawnLocation = StartSpot->GetActorLocation() + FVector(0, 0, 100);
            SpawnRotation = StartSpot->GetActorRotation();
        }
        
        // Spawn tutorial character
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        
        UClass* CharClass = TutorialCharacterClass ? 
            TutorialCharacterClass.Get() : AWormTutorialCharacter::StaticClass();
            
        TutorialChar = GetWorld()->SpawnActor<AWormTutorialCharacter>(
            CharClass, 
            SpawnLocation, 
            SpawnRotation,
            SpawnParams
        );
        
        PlayerCharacter = TutorialChar;
        
        if (TutorialChar)
        {
            // Setup character
            TutorialChar->Health = 100.0f;
            TutorialChar->TeamId = 0;
            TutorialChar->CharacterIndexInTeam = 0;
            TutorialChar->InGameName = "Tutorial Player";
            
            // Unpossess old pawn if needed
            if (PC->GetPawn() && PC->GetPawn() != TutorialChar)
            {
                PC->UnPossess();
            }
            
            // Possess tutorial character
            PC->Possess(TutorialChar);
            UE_LOG(LogTemp, Warning, TEXT("Spawned new tutorial character: %s"), *TutorialChar->GetName());
            
            // Add to team in GameState
            AWormGameState* WormGS = GetGameState<AWormGameState>();
            if (WormGS)
            {
                WormGS->AddCharacterToTeam(TutorialChar, 0);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn tutorial character!"));
            return;
        }
    }
    
    // 3. Initialize weapons
    if (TutorialChar && AvailableWeaponTypes.Num() > 0)
    {
        TutorialChar->SetIsMyTurn(true);
        TutorialChar->SetAvailableWeapons(AvailableWeaponTypes);
        UE_LOG(LogTemp, Warning, TEXT("Weapons initialized for tutorial character"));
    }
    
    // 4. Connect event handlers - THIS IS CRITICAL FOR PROGRESSION
    if (TutorialChar)
    {
        // Make absolutely sure we're connecting to the events
        TutorialChar->OnCharacterMoved.Clear();
        TutorialChar->OnCharacterJumped.Clear();
        TutorialChar->OnCharacterFired.Clear();
        
        TutorialChar->OnCharacterMoved.AddDynamic(this, &AWormTutorialGameMode::OnPlayerMoved);
        TutorialChar->OnCharacterJumped.AddDynamic(this, &AWormTutorialGameMode::OnPlayerJumped);
        TutorialChar->OnCharacterFired.AddDynamic(this, &AWormTutorialGameMode::OnPlayerFired);
        
        UE_LOG(LogTemp, Warning, TEXT("Connected tutorial character events for progression tracking"));
    }
    SetupWaterSystem();

    // 5. Start tutorial UI
    StartTutorial();
}

void AWormTutorialGameMode::StartTutorial()
{
    UE_LOG(LogTemp, Warning, TEXT("Starting tutorial"));
    
    // Reset progress state
    CurrentStageIndex = 0;
    bHasPlayerMoved = false;
    bHasPlayerJumped = false;
    bHasPlayerFired = false;
    bHasPlayerDestroyedTarget = false;
    
    // Create tutorial UI
    if (TutorialWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            // Remove existing widget if any
            if (TutorialWidget)
            {
                TutorialWidget->RemoveFromParent();
                TutorialWidget = nullptr;
            }
            
            // Create and cast to the proper widget type
            TutorialWidget = CreateWidget<UWTutorialWidget>(PC, TutorialWidgetClass);
            if (TutorialWidget)
            {
                TutorialWidget->AddToViewport(100); // High Z-order to be on top
                
                // Call blueprint event to initialize UI
                OnStageCompleted(CurrentStageIndex);
                UE_LOG(LogTemp, Warning, TEXT("Tutorial UI created and displayed"));
            }
        }
    }
    TArray<AActor*> FoundTargets;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATutorialTargetBuilding::StaticClass(), FoundTargets);
    
    if (FoundTargets.Num() > 0)
    {
        // Use the first found target building
        TargetBuilding = Cast<ATutorialTargetBuilding>(FoundTargets[0]);
        
        if (TargetBuilding)
        {
            UE_LOG(LogTemp, Warning, TEXT("Found pre-placed tutorial target building: %s"), *TargetBuilding->GetName());
            
            // Connect to the destruction event
            TargetBuilding->OnTargetDestroyed.AddDynamic(this, &AWormTutorialGameMode::OnTargetDestroyed);
            UE_LOG(LogTemp, Warning, TEXT("Connected to target destruction event"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("No pre-placed tutorial target building found in scene!"));
    }
    // Make sure player character is in control
    if (PlayerCharacter)
    {
        PlayerCharacter->SetIsMyTurn(true);
    }
}

void AWormTutorialGameMode::OnPlayerMoved()
{
    UE_LOG(LogTemp, Warning, TEXT("Player moved detected!"));
    bHasPlayerMoved = true;
    
    if (CurrentStageIndex == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Completing movement tutorial stage"));
        AdvanceToNextStage();
    }
}
void AWormTutorialGameMode::OnTargetDestroyed()
{
    UE_LOG(LogTemp, Warning, TEXT("Tutorial target building destroyed!"));
    bHasPlayerDestroyedTarget = true;
    
    if (CurrentStageIndex == 3) // Target destruction stage
    {
        UE_LOG(LogTemp, Warning, TEXT("Completing target destruction tutorial stage"));
        AdvanceToNextStage();
    }
}
void AWormTutorialGameMode::OnPlayerJumped()
{
    UE_LOG(LogTemp, Warning, TEXT("Player jumped detected!"));
    bHasPlayerJumped = true;
    
    if (CurrentStageIndex == 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Completing jump tutorial stage"));
        AdvanceToNextStage();
    }
}

void AWormTutorialGameMode::OnPlayerFired()
{
    UE_LOG(LogTemp, Warning, TEXT("Player fired weapon detected!"));
    bHasPlayerFired = true;
    
    if (CurrentStageIndex == 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("Completing weapon firing tutorial stage"));
        AdvanceToNextStage();
    }
}

void AWormTutorialGameMode::AdvanceToNextStage()
{
    if (CurrentStageIndex < TutorialStages.Num() - 1)
    {
        CurrentStageIndex++;
        OnStageCompleted(CurrentStageIndex);
        
        // Special stage handling
        if (CurrentStageIndex == 4) // Water hazard stage
        {
            // Start water rising for demonstration after a short delay
            FTimerHandle WaterRiseTimer;
            GetWorld()->GetTimerManager().SetTimer(
                WaterRiseTimer,
                this,
                &AWormTutorialGameMode::TriggerWaterRise,
                5.0f,
                false
            );
        }
    }
    else
    {
        // Tutorial complete
        UE_LOG(LogTemp, Warning, TEXT("Tutorial completed!"));
        CompleteTutorial();
    }
}


void AWormTutorialGameMode::CompleteTutorial()
{
    // Show completion message in UI
    if (TutorialWidget)
    {
        // This will be handled in Blueprint via OnStageCompleted
        OnStageCompleted(TutorialStages.Num() - 1);
    }
    
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

// Override this to keep player in control during tutorial
void AWormTutorialGameMode::StartNextTurn()
{
    // Do nothing - don't switch turns in tutorial
    if (PlayerCharacter)
    {
        PlayerCharacter->SetIsMyTurn(true);
    }
}

// Override this to prevent turn ending in tutorial
void AWormTutorialGameMode::EndCurrentTurn()
{
    // Do nothing - ignore turn ending in tutorial
    if (PlayerCharacter)
    {
        PlayerCharacter->SetIsMyTurn(true);
    }
}

void AWormTutorialGameMode::OnStageCompleted_Implementation(int32 CurrentStage)
{
    // Update tutorial UI - cast to the proper widget type
    UWTutorialWidget* TutWidget = Cast<UWTutorialWidget>(TutorialWidget);
    if (TutWidget)
    {
        // Set current instruction text
        TutWidget->SetInstructionText(TutorialStages[CurrentStage]);
        
        // Update progress indicator
        TutWidget->UpdateProgressIndicator(CurrentStage, TutorialStages.Num() - 1);
    }
}

void AWormTutorialGameMode::SetupWaterTriggerZone()
{
    // Check if a water trigger already exists
    TArray<AActor*> FoundTriggers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATutorialWaterTrigger::StaticClass(), FoundTriggers);
    
    if (FoundTriggers.Num() > 0)
    {
        // Use existing trigger
        WaterTrigger = Cast<ATutorialWaterTrigger>(FoundTriggers[0]);
        if (WaterTrigger)
        {
            UE_LOG(LogTemp, Warning, TEXT("Found existing water trigger"));
            WaterTrigger->OnWaterObserved.AddDynamic(this, &AWormTutorialGameMode::OnWaterObserved);
        }
    }
    else
    {
        // Create a new trigger
        FVector TriggerLocation = FVector(0, 0, 0);
        
        // Try to position it near water area if we have water manager
        if (WaterSystemManager)
        {
            // Position it near the water's edge
            float WaterLevel = -800.0f; // Default value
            TriggerLocation = FVector(1000, 1000, WaterLevel + 200.0f);
        }
        
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        
        WaterTrigger = GetWorld()->SpawnActor<ATutorialWaterTrigger>(
            ATutorialWaterTrigger::StaticClass(),
            TriggerLocation,
            FRotator::ZeroRotator,
            SpawnParams
        );
        
        if (WaterTrigger)
        {
            UE_LOG(LogTemp, Warning, TEXT("Created water trigger at %s"), *TriggerLocation.ToString());
            
            // Scale the trigger box appropriately
            FVector BoxExtent(200.0f, 200.0f, 100.0f);
            WaterTrigger->SetActorScale3D(BoxExtent);
            
            // Subscribe to its event
            WaterTrigger->OnWaterObserved.AddDynamic(this, &AWormTutorialGameMode::OnWaterObserved);
        }
    }
}
void AWormTutorialGameMode::SetupWaterSystem()
{
    // Check if a water system manager already exists
    TArray<AActor*> FoundManagers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnvironmentalEventsManager::StaticClass(), FoundManagers);

    if (FoundManagers.Num() > 0)
    {
        // Use existing manager
        WaterSystemManager = Cast<AEnvironmentalEventsManager>(FoundManagers[0]);
        if (WaterSystemManager)
        {
            UE_LOG(LogTemp, Warning, TEXT("Found existing water system manager"));
        }
    }
    else
    {
        // Create a new manager
        FVector ManagerLocation = FVector(0, 0, 0);

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        WaterSystemManager = GetWorld()->SpawnActor<AEnvironmentalEventsManager>(
            AEnvironmentalEventsManager::StaticClass(),
            ManagerLocation,
            FRotator::ZeroRotator,
            SpawnParams
        );

        if (WaterSystemManager)
        {
            UE_LOG(LogTemp, Warning, TEXT("Created new water system manager at %s"), *ManagerLocation.ToString());
        }
    }

    // Setup water trigger zone
    SetupWaterTriggerZone();
}


void AWormTutorialGameMode::InitializeWaterSystem()
{
    // Check if water manager already exists
    if (WaterSystemManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("Water system already initialized"));
        bWaterSystemInitialized = true;
        return;
    }
    
    // Find existing manager
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnvironmentalEventsManager::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        WaterSystemManager = FoundActors[0];
        UE_LOG(LogTemp, Warning, TEXT("Found existing water manager: %s"), *WaterSystemManager->GetName());
        bWaterSystemInitialized = true;
    }
    // Create one if needed 
    else if (WaterSystemManagerClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        WaterSystemManager = GetWorld()->SpawnActor<AActor>(
            WaterSystemManagerClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParams
        );
        
        if (WaterSystemManager)
        {
            UE_LOG(LogTemp, Warning, TEXT("Created new water manager for tutorial"));
            bWaterSystemInitialized = true;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create water manager for tutorial"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("No WaterSystemManagerClass set in tutorial game mode"));
    }
}

void AWormTutorialGameMode::TriggerWaterRise()
{
    // Make sure water system is initialized
    if (!bWaterSystemInitialized)
    {
        InitializeWaterSystem();
    }
    
    // Get the environmental manager
    AEnvironmentalEventsManager* EnvManager = Cast<AEnvironmentalEventsManager>(WaterSystemManager);
    if (EnvManager && EnvManager->WaterSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("Triggering water rise for tutorial..."));
        
        // Make the water visible first by setting it to a moderate height
        float CurrentLevel = EnvManager->WaterSystem->GetCurrentWaterLevel();
        float PlayerZ = PlayerCharacter ? PlayerCharacter->GetActorLocation().Z : 0.0f;
        
        // Set water to be visible but safe (below player)
        float SafeVisibleLevel = PlayerZ - 300.0f;
        
        // First make water visible at safe level (immediate)
        EnvManager->WaterSystem->SetWaterLevel(SafeVisibleLevel, true);
        
        // Then start it rising
        EnvManager->StartWaterRising();
        EnvManager->bRiseAfterEachTurn = false; // Make it rise continuously
        EnvManager->WaterRisePerTurn = 50.0f; // Set a slower rise rate for tutorial
        
        // Set a timer to advance to completion after a few seconds
        FTimerHandle WaterDemoTimer;
        GetWorld()->GetTimerManager().SetTimer(
            WaterDemoTimer,
            this,
            &AWormTutorialGameMode::AdvanceToNextStage,
            7.0f, // Allow time to observe water
            false
        );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get valid water system for tutorial"));
        // Still advance the tutorial even if water system fails
        AdvanceToNextStage();
    }
}

void AWormTutorialGameMode::OnWaterObserved()
{
    bHasObservedWater = true;
    UE_LOG(LogTemp, Warning, TEXT("Player has observed water!"));
    
    // If we're on the water stage, we can consider this step complete
    if (CurrentStageIndex == 4) // Water hazard stage
    {
        UE_LOG(LogTemp, Warning, TEXT("Water observation objective complete"));
        AdvanceToNextStage();
    }
}
