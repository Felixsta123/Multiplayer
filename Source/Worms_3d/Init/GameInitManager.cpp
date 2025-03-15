#include "GameInitManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Worms_3d/Building/AVoxelBuilding.h"
#include "WormGameMode.h"
#include "WormGameState.h"
#include "WormPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

AGameInitManager::AGameInitManager()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create the player spawn manager component
    PlayerSpawnManager = CreateDefaultSubobject<UPlayerSpawnManager>(TEXT("PlayerSpawnManager"));
    
    // Default values
    LoadingScreenDuration = 50.0f;
    bAutoHandleInitialization = true;
    CurrentInitStep = 0;
}

void AGameInitManager::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize the network loading system first
    InitializeNetworkLoading();
}

void AGameInitManager::InitializeNetworkLoading()
{
    // Get a reference to the game state
    WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(this));
}

void AGameInitManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AGameInitManager::StartInitializationSequence()
{
    UE_LOG(LogTemp, Warning, TEXT("Starting game initialization sequence"));
    
    // ALWAYS use NetworkLoadingManager if available
    if (WormGameState && WormGameState->LoadingManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("Using NetworkLoadingManager to show loading screen on all clients"));
        WormGameState->ShowLoadingScreen(LoadingScreenDuration);
    }
    else
    {
        // Only as a fallback - log warning that network loading won't work
        UE_LOG(LogTemp, Warning, TEXT("WARNING: No NetworkLoadingManager available - only local loading screen will show"));
        
        // Create and show loading widget for local player only
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (PC && PC->IsLocalController())
            {
                // Create loading widget
                if (LoadingWidgetClass)
                {
                    LoadingWidget = CreateWidget<UW_GameLoadingScreen>(PC, LoadingWidgetClass);
                    if (LoadingWidget)
                    {
                        LoadingWidget->AddToViewport(9999);
                        LoadingWidget->ShowLoadingScreen(LoadingScreenDuration);
                        UE_LOG(LogTemp, Warning, TEXT("Loading widget created for local player controller only"));
                    }
                }
                break;
            }
        }
    }
    
    // Reset and start initialization steps
    CurrentInitStep = 0;
    ExecuteInitializationStep();
}

void AGameInitManager::ExecuteInitializationStep()
{
    if (GetLocalRole() != ROLE_Authority) return;
    
    switch (CurrentInitStep)
    {
        case 0: // Terrain generation
        {
            UE_LOG(LogTemp, Warning, TEXT("Starting initialization step %d: Terrain Generation"), CurrentInitStep);

            UpdateLoadingProgress(0.2f, TEXT("Génération du terrain..."));
            
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                GameMode->GenerateVoxelBuildings();
            }
            
            // Increased delay to ensure terrain is fully generated
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                3.0f, // Increased from 2.0f
                false
            );
            
            CurrentInitStep++;
            break;
        }
        
        case 1: // Player controller gathering
        {
            UE_LOG(LogTemp, Warning, TEXT("Starting initialization step %d: Controller Gathering"), CurrentInitStep);
              
            UpdateLoadingProgress(0.4f, TEXT("Préparation des joueurs..."));
            
            // Make sure all controllers are ready and their settings have been replicated
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                GameMode->GatherAllPlayerControllers();
                
                // Log all found controllers and their settings
                UE_LOG(LogTemp, Warning, TEXT("Found %d player controllers"), GameMode->AllPlayerControllers.Num());
                for (int32 i = 0; i < GameMode->AllPlayerControllers.Num(); i++) {
                    AController* PC = GameMode->AllPlayerControllers[i];
                    AWormPlayerController* WPC = Cast<AWormPlayerController>(PC);
                    if (WPC) {
                        UE_LOG(LogTemp, Warning, TEXT("Controller %d: %s, Has character class: %s"), 
                            i, *PC->GetName(), 
                            WPC->PlayerSettings.MyPlayerCharacter ? TEXT("Yes") : TEXT("No"));
                    }
                }
            }
            
            // Increased delay to allow settings to replicate
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                2.0f, // Increased from 1.0f
                false
            );
            
            CurrentInitStep++;
            break;
        }
        
        case 2: // Spawn and position players
        {
            UE_LOG(LogTemp, Warning, TEXT("Starting initialization step %d: Player Spawning"), CurrentInitStep);

            UpdateLoadingProgress(0.5f, TEXT("Placement des joueurs..."));

            if (PlayerSpawnManager) {
                UE_LOG(LogTemp, Warning, TEXT("Teleporting players to buildings..."));
                
                // Teleport players to buildings with improved positioning
                PlayerSpawnManager->TeleportPlayersToBuildings();

                // IMPORTANT: Increased delay to 7.0 seconds to ensure all characters are fully spawned and stable
                // before proceeding to weapon initialization
                GetWorld()->GetTimerManager().SetTimer(
                    InitStepTimerHandle,
                    this,
                    &AGameInitManager::ExecuteInitializationStep,
                    7.0f, // Increased from 5.0f
                    false
                );
            } else {
                UE_LOG(LogTemp, Error, TEXT("PlayerSpawnManager is NULL!"));
                GetWorld()->GetTimerManager().SetTimer(
                    InitStepTimerHandle,
                    this,
                    &AGameInitManager::ExecuteInitializationStep,
                    1.0f,
                    false
                );
            }

            CurrentInitStep++;
            break;
        }
         
        case 3: // Initial weapon preparation (with safety)
        {
            UE_LOG(LogTemp, Warning, TEXT("Starting initialization step %d: Initial Weapon Setup"), CurrentInitStep);

            UpdateLoadingProgress(0.7f, TEXT("Préparation des armes..."));
        
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                UE_LOG(LogTemp, Warning, TEXT("✅ First-phase weapon initialization"));
                
                // First gather controllers again to ensure the list is current
                GameMode->GatherAllPlayerControllers();
                
                // Pre-check all characters to verify they are ready
                bool allPlayersReady = true;
                for (AController* Controller : GameMode->AllPlayerControllers) {
                    AWormCharacter* Character = GameMode->GetWormCharacterFromController(Controller);
                    if (!Character || !IsValid(Character)) {
                        allPlayersReady = false;
                        UE_LOG(LogTemp, Warning, TEXT("Not all characters are ready yet; will retry"));
                        break;
                    }
                }
                
                if (allPlayersReady) {
                    // Initialize weapons for all players
                    GameMode->InitializeWeaponsForAllPlayers();
                } else {
                    // Not all players are ready, delay and retry the same step
                    GetWorld()->GetTimerManager().SetTimer(
                        InitStepTimerHandle,
                        this,
                        &AGameInitManager::ExecuteInitializationStep,
                        2.0f,
                        false
                    );
                    return; // Don't increment the step counter
                }
            }
        
            // Proceed to next step after a delay
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                3.0f, // Increased from 2.0f
                false
            );
        
            CurrentInitStep++;
            break;
        }
    
        case 4: // Final weapon verification
        {
            UE_LOG(LogTemp, Warning, TEXT("Starting initialization step %d: Final Weapon Verification"), CurrentInitStep);

            UpdateLoadingProgress(0.9f, TEXT("Vérification des armes..."));
            
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                UE_LOG(LogTemp, Warning, TEXT("✅ Final weapon verification phase"));
                
                // Gather fresh controller list
                GameMode->GatherAllPlayerControllers();
                
                // Verify all weapons
                GameMode->VerifyWeaponsForAllPlayers();
            }
            
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                3.0f,
                false
            );
            
            CurrentInitStep++;
            break;
        }
        
         case 5: // Final stabilization and game start
        {
            UpdateLoadingProgress(0.95f, TEXT("Stabilisation des joueurs..."));

            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                // Final check to ensure all characters are stable
                GameMode->GatherAllPlayerControllers();
    
                TArray<AActor*> AllCharacters;
                UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllCharacters);
        
                UE_LOG(LogTemp, Warning, TEXT("Stabilizing physics for %d characters"), AllCharacters.Num());
        
                // Pour chaque personnage, assurer que la physique est correctement configurée
                for (AActor* Actor : AllCharacters)
                {
                    AWormCharacter* Character = Cast<AWormCharacter>(Actor);
                    if (Character && Character->GetCharacterMovement()) {
                        // Reset velocity
                        Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            
                        // Force walking mode
                        Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            
                        // Apply slight downward force
                        Character->GetCharacterMovement()->AddForce(FVector(0, 0, -980.0f));
            
                        // Increase air control for stability
                        Character->GetCharacterMovement()->AirControl = 1.0f;
            
                        // Force update of physics state
                        Character->GetCharacterMovement()->UpdateComponentVelocity();
                        Character->GetCharacterMovement()->GetOwner()->GetRootComponent()->UpdateComponentToWorld();
                        Character->GetCharacterMovement()->GravityScale = 1.5f;
                        Character->ForceNetUpdate();

                        // Final weapon visibility check
                        if (Character->CurrentWeapon) {
                            Character->CurrentWeapon->EnsureWeaponVisibility();
                            Character->AttachWeaponToSocket(Character->CurrentWeapon);
                        }
            
                        UE_LOG(LogTemp, Warning, TEXT("Final stability check for %s complete"), *Character->GetName());
                    }
                }
    
                // Do NOT start the first turn here, that will happen in the next step
                // GameMode->StartNextTurn();
            }

            // Go to the final initialization step after a delay
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                3.0f,
                false
            );

            CurrentInitStep++;
            break;
        }

        case 6: // Multiple turn cycling to ensure proper physics
        {
            UpdateLoadingProgress(1.0f, TEXT("Prêt !"));

            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                UE_LOG(LogTemp, Warning, TEXT("Final character physics stabilization - cycling through turns"));
                
                // Get the game state to access team information
                AWormGameState* WormGS = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
                if (!WormGS) {
                    UE_LOG(LogTemp, Error, TEXT("GameState not found!"));
                    CompleteInitialization();
                    break;
                }
                
                // Calculate total character count from all teams
                int32 TotalCharacterCount = 0;
                for (const FTeamInfo& Team : WormGS->Teams) {
                    TotalCharacterCount += Team.TeamMembers.Num();
                }
                
                // Early safety check - if no characters found, move on
                if (TotalCharacterCount == 0) {
                    UE_LOG(LogTemp, Error, TEXT("No characters found for turn cycling!"));
                    GameMode->StartNextTurn(); // Call once to start the first actual turn
                    CompleteInitialization();
                    break;
                }
                
                UE_LOG(LogTemp, Warning, TEXT("Starting turn cycle initialization for %d characters"), TotalCharacterCount);
                
                // Set up recurring timer to cycle through each character's turn
                CycleCount = 0;
                MaxCycles = TotalCharacterCount * 2; // Cycle through each character twice
                
                GetWorld()->GetTimerManager().SetTimer(
                    TurnCycleTimerHandle,
                    this,
                    &AGameInitManager::CycleThroughTurns,
                    0.5f, // Initial delay before first cycle
                    false
                );
                
                // No need to increment CurrentInitStep as this is the final step
            }
            break;
        }
    }
}

// New function to cycle through turns to stabilize physics
void AGameInitManager::CycleThroughTurns()
{
    AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GameMode) {
        CompleteInitialization();
        return;
    }

    // Call StartNextTurn to cycle to the next character
    GameMode->StartNextTurn();
    
    CycleCount++;
    UE_LOG(LogTemp, Warning, TEXT("Turn cycle %d/%d complete"), CycleCount, MaxCycles);
    
    // Continue cycling if we haven't reached the max cycles
    if (CycleCount < MaxCycles) {
        // Schedule the next cycle with a delay
        GetWorld()->GetTimerManager().SetTimer(
            TurnCycleTimerHandle,
            this,
            &AGameInitManager::CycleThroughTurns,
            0.5f, // Delay between cycles
            false
        );
    } else {
        // We've completed all cycles, finish initialization
        UE_LOG(LogTemp, Warning, TEXT("All turn cycles complete. Game initialization sequence complete"));
        CompleteInitialization();
    }
}

void AGameInitManager::UpdateLoadingProgress(float Progress, const FString& StatusText)
{
    // First, try to update using the NetworkLoadingManager
    if (WormGameState && WormGameState->LoadingManager)
    {
        WormGameState->UpdateLoadingProgress(Progress, StatusText);
    }
    // Fallback to local loading widget if available
    else if (LoadingWidget)
    {
        LoadingWidget->SetLoadingProgress(Progress, StatusText);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Loading progress: %.0f%% - %s"), Progress * 100.0f, *StatusText);
}
void AGameInitManager::CompleteInitialization()
{
    // Add this check to ensure we've completed all steps
    if (CurrentInitStep < 5) {
        UE_LOG(LogTemp, Error, TEXT("Attempting to complete initialization at step %d before all steps finished!"), CurrentInitStep);
        return; // Don't complete if we haven't finished all steps
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Game initialization sequence complete"));
    
    // Increase the delay before dismissal
    float DismissDelay = 3.0f; // Increase from 1.0f to 3.0f
    
    // ALWAYS dismiss through NetworkLoadingManager if available
    if (WormGameState && WormGameState->LoadingManager)
    {
        // Delay dismissal a bit so players can see the "Ready!" message
        FTimerHandle DismissTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            DismissTimerHandle,
            [this]()
            {
                if (WormGameState)
                {
                    WormGameState->DismissLoadingScreen();
                }
            },
            DismissDelay,
            false
        );
    }
    else if (LoadingWidget)
    {
        // Only as fallback for local player
        FTimerHandle DismissTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            DismissTimerHandle,
            [this]()
            {
                if (LoadingWidget)
                {
                    LoadingWidget->DismissLoadingScreen();
                    LoadingWidget = nullptr;
                }
            },
            1.0f,
            false
        );
    }
}
