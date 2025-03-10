#include "GameInitManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "AVoxelBuilding.h"
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
    LoadingScreenDuration = 5.0f;
    bAutoHandleInitialization = true;
    CurrentInitStep = 0;
}

void AGameInitManager::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize the network loading system first
    InitializeNetworkLoading();
    
    // Only execute on server or standalone
    if (GetLocalRole() == ROLE_Authority)
    {
        // Start initialization sequence if auto-handle is enabled
        if (bAutoHandleInitialization)
        {
            // Add a small delay to ensure everything is ready
            FTimerHandle StartupTimerHandle;
            GetWorld()->GetTimerManager().SetTimer(
                StartupTimerHandle,
                this,
                &AGameInitManager::StartInitializationSequence,
                0.5f,
                false
            );
        }
    }
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
        case 0: // Génération du terrain
        {
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
        
        case 1: // Préparation des joueurs - First gather controllers
        {
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
        
        case 2: // Spawn and position players first
        {
            UpdateLoadingProgress(0.5f, TEXT("Placement des joueurs..."));

            if (PlayerSpawnManager) {
                UE_LOG(LogTemp, Warning, TEXT("Teleporting players to buildings..."));
                
                // Teleport players to buildings with improved positioning
                PlayerSpawnManager->TeleportPlayersToBuildings();

                // Delay to allow players to be properly positioned
                GetWorld()->GetTimerManager().SetTimer(
                    InitStepTimerHandle,
                    this,
                    &AGameInitManager::ExecuteInitializationStep,
                    5.0f,
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
         
        case 3: // Initialisation des armes
        {
            UpdateLoadingProgress(0.6f, TEXT("Chargement des armes..."));
        
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                UE_LOG(LogTemp, Warning, TEXT("✅ Initializing weapons for all players"));
                GameMode->InitializeWeaponsForAllPlayers();
            }
        
            // Increased delay to ensure weapons are properly initialized
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
    
        case 4: // Now initialize weapons AFTER players are positioned
        {
            UpdateLoadingProgress(0.9f, TEXT("Chargement des armes..."));
            
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                UE_LOG(LogTemp, Warning, TEXT("✅ Initializing weapons for all players after positioning"));
                
                // First gather fresh controller list
                GameMode->GatherAllPlayerControllers();
                
                // Now initialize weapons
                GameMode->InitializeWeaponsForAllPlayers();
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
        
        case 5: // Finalisation
        {
            UpdateLoadingProgress(1.0f, TEXT("Prêt !"));

            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                // Final check to ensure all characters are stable
                GameMode->GatherAllPlayerControllers();
    
                // For each character, ensure movement properties are set correctly
                for (AController* Controller : GameMode->AllPlayerControllers) {
                    AWormCharacter* Character = GameMode->GetWormCharacterFromController(Controller);
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
            
                        UE_LOG(LogTemp, Warning, TEXT("Final stability check for %s complete"), *Character->GetName());
                    }
                }
    
                // Start the first turn
                GameMode->StartNextTurn();
            }

            // Only complete initialization AFTER all steps are done
            UE_LOG(LogTemp, Warning, TEXT("Game initialization sequence complete"));
            CompleteInitialization();
            break;
        }
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
    UE_LOG(LogTemp, Warning, TEXT("Game initialization sequence complete"));
    
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
            1.0f,
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
