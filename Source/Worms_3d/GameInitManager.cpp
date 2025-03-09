#include "GameInitManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "AVoxelBuilding.h"
#include "WormGameMode.h"
#include "WormGameState.h"

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
    // Only execute on server or standalone
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }
    
    // Execute steps based on current step index
    switch (CurrentInitStep)
    {
        case 0: // Initial step - wait for game mode to generate voxel buildings
        {
            // Update loading progress
            UpdateLoadingProgress(0.2f, TEXT("Generating terrain..."));
            
            // Check if the game mode is ready and has generated buildings
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode)
            {
                // Make sure voxel buildings get generated
                GameMode->GenerateVoxelBuildings();
                
                UE_LOG(LogTemp, Warning, TEXT("Requested voxel building generation from game mode"));
            }
            
            // Schedule next step
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                2.0f, // Wait for voxel buildings to be generated
                false
            );
            
            // Move to next step
            CurrentInitStep++;
            break;
        }
        
        case 1: // Second step - position player starts
        {
            // Update loading progress
            UpdateLoadingProgress(0.4f, TEXT("Positioning player spawns..."));
            
            // Check if we have buildings before positioning player starts
            TArray<AImprovedVoxelBuilding*> Buildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
            
            if (Buildings.Num() > 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("Found %d voxel buildings, positioning player starts"), Buildings.Num());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("No voxel buildings found, will try to position spawns anyway"));
            }
            
            // Schedule next step
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                1.0f,
                false
            );
            
            // Move to next step
            CurrentInitStep++;
            break;
        }
        
        case 2: // Third step - initialize weapons
        {
            // Update loading progress
            UpdateLoadingProgress(0.6f, TEXT("Loading weapons..."));
            
            // Get the game mode and initialize weapons
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode)
            {
                GameMode->InitializeWeaponsForAllPlayers();
                UE_LOG(LogTemp, Warning, TEXT("Requested weapon initialization from game mode"));
            }
            
            // Schedule next step
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                1.0f,
                false
            );
            
            // Move to next step
            CurrentInitStep++;
            break;
        }
        
        // This is just the case for step 3 in the ExecuteInitializationStep function
        case 3: // Fourth step - position players on voxel buildings
        {
            // Update loading progress
            UpdateLoadingProgress(0.8f, TEXT("Positioning players..."));

            // Position players on top of buildings only if we have a PlayerSpawnManager
            if (PlayerSpawnManager)
            {
                // Set a static guard flag to prevent multiple calls
                static bool bAlreadyTeleportedPlayers = false;
    
                if (!bAlreadyTeleportedPlayers)
                {
                    // Set flag first to prevent recursive calls
                    bAlreadyTeleportedPlayers = true;
        
                    // Start the positioning process - Use the teleport function
                    PlayerSpawnManager->TeleportPlayersToBuildings();
        
                    UE_LOG(LogTemp, Warning, TEXT("Called TeleportPlayersToBuildings once"));
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Skipped duplicate call to TeleportPlayersToBuildings"));
                }

                // Schedule the next step with a delay to allow teleportation to complete
                GetWorld()->GetTimerManager().SetTimer(
                    InitStepTimerHandle,
                    this,
                    &AGameInitManager::ExecuteInitializationStep,
                    2.0f,  // Increased from 1.0f to give more time for teleportation
                    false
                );
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("No PlayerSpawnManager available"));

                // Skip to next step
                GetWorld()->GetTimerManager().SetTimer(
                    InitStepTimerHandle,
                    this,
                    &AGameInitManager::ExecuteInitializationStep,
                    1.0f,
                    false
                );
            }

            // Move to next step
            CurrentInitStep++;
            break;
        }
        
        case 4: // Final step - complete initialization
        {
            // Update loading progress to 100%
            UpdateLoadingProgress(1.0f, TEXT("Ready!"));
            
            // Start the first turn in the game mode
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode)
            {
                GameMode->StartNextTurn();
                UE_LOG(LogTemp, Warning, TEXT("Started first game turn"));
            }
            
            // Complete initialization (will dismiss loading screen after a short delay)
            CompleteInitialization();
            break;
        }
        
        default:
            // Should not get here
            UE_LOG(LogTemp, Error, TEXT("Invalid initialization step: %d"), CurrentInitStep);
            CompleteInitialization();
            break;
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
