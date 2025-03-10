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
            
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                2.0f,
                false
            );
            
            CurrentInitStep++;
            break;
        }
        
        case 1: // Préparation des joueurs
        {
            UpdateLoadingProgress(0.4f, TEXT("Préparation des joueurs..."));
            
            // S'assurer que tous les controllers sont prêts
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                GameMode->GatherAllPlayerControllers();
            }
            
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                1.0f,
                false
            );
            
            CurrentInitStep++;
            break;
        }
        
        case 2: // Initialisation des armes
        {
            UpdateLoadingProgress(0.6f, TEXT("Chargement des armes..."));
            
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                GameMode->InitializeWeaponsForAllPlayers();
            }
            
            GetWorld()->GetTimerManager().SetTimer(
                InitStepTimerHandle,
                this,
                &AGameInitManager::ExecuteInitializationStep,
                1.0f,
                false
            );
            
            CurrentInitStep++;
            break;
        }
        
        case 3: // Spawn and position players
        {
            UpdateLoadingProgress(0.8f, TEXT("Placing players..."));

            if (PlayerSpawnManager) {
                // REMOVE static flag - it causes issues with multiple game instances
                //static bool bAlreadyTeleportedPlayers = false;
                //if (!bAlreadyTeleportedPlayers) {
                //    bAlreadyTeleportedPlayers = true;
                PlayerSpawnManager->TeleportPlayersToBuildings();
                //}

                // INCREASE this delay to give more time for network replication
                GetWorld()->GetTimerManager().SetTimer(
                    InitStepTimerHandle,
                    this,
                    &AGameInitManager::ExecuteInitializationStep,
                    4.0f,  // Increased from 2.5f to ensure proper replication
                    false
                );
            } else {
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
         
        
        case 4: // Finalisation
        {
            UpdateLoadingProgress(1.0f, TEXT("Prêt !"));
            
            AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(this));
            if (GameMode) {
                GameMode->StartNextTurn();
            }
            
            CompleteInitialization();
            break;
        }
        
        default:
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
