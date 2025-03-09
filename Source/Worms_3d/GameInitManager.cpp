#include "GameInitManager.h"
#include "GameInitManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "AVoxelBuilding.h"
#include "WormGameMode.h"

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

void AGameInitManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AGameInitManager::StartInitializationSequence()
{
    UE_LOG(LogTemp, Warning, TEXT("Starting game initialization sequence"));
    
    // Create and show loading widget for all local players
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->IsLocalController())
        {
            // Create loading widget
            if (LoadingWidgetClass)
            {
                LoadingWidget = CreateWidget<UGameLoadingWidget>(PC, LoadingWidgetClass);
                if (LoadingWidget)
                {
                    LoadingWidget->AddToViewport(9999); // High Z-order to be on top
                    LoadingWidget->ShowLoadingScreen(LoadingScreenDuration);
                    
                    UE_LOG(LogTemp, Warning, TEXT("Loading widget created for player controller"));
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to create loading widget"));
                }
            }
            
            // Only handle the first local player controller for now
            break;
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
        
                
        
        case 3: // Fourth step - position players on voxel buildings
        {
            // Update loading progress
            UpdateLoadingProgress(0.8f, TEXT("Positioning players..."));

            // Position players on top of buildings
            if (PlayerSpawnManager)
            {
                // Start the positioning process - Use the new direct teleport function
                PlayerSpawnManager->TeleportPlayersToBuildings();
    
                // Schedule the next step with a delay to allow teleportation to complete
                GetWorld()->GetTimerManager().SetTimer(
                    InitStepTimerHandle,
                    this,
                    &AGameInitManager::ExecuteInitializationStep,
                    1.0f,
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
    // Update loading widget if available
    if (LoadingWidget)
    {
        LoadingWidget->SetLoadingProgress(Progress, StatusText);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Loading progress: %.0f%% - %s"), Progress * 100.0f, *StatusText);
}

void AGameInitManager::CompleteInitialization()
{
    UE_LOG(LogTemp, Warning, TEXT("Game initialization sequence complete"));
    
    // Dismiss loading widget after a short delay to ensure last message is seen
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

AGameInitManager* AWormGameMode::SetupGameInitialization()
{
    // Check if we already have a game init manager
    if (GameInitManager)
    {
        return GameInitManager;
    }
    
    // Determine which class to use
    TSubclassOf<AGameInitManager> ClassToUse = GameInitManagerClass;
    if (!ClassToUse)
    {
        ClassToUse = AGameInitManager::StaticClass();
    }
    
    // Create the game init manager
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AGameInitManager* InitManager = GetWorld()->SpawnActor<AGameInitManager>(
        ClassToUse,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams
    );
    
    if (InitManager)
    {
        UE_LOG(LogTemp, Log, TEXT("Game initialization manager created successfully"));
        return InitManager;
    }
    
    UE_LOG(LogTemp, Error, TEXT("Failed to create game initialization manager"));
    return nullptr;
}