#include "WormPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameMode.h"
#include "WormGameState.h"
#include "Worms_3d/Misc/PlayerSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Worms_3d/UI/WMainHUDWidget.h"

AWormPlayerController::AWormPlayerController()
{
    // Empty constructor
    bReplicates = true;
}

void AWormPlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    // Load player settings from save slot
    if (IsLocalController()) {
        LoadPlayerSettings();
        
        // If we're on a client, immediately send settings to server
        if (GetLocalRole() < ROLE_Authority)
        {
            UE_LOG(LogTemp, Log, TEXT("Client: Sending player settings to server immediately"));
            ServerSetPlayerSettings(PlayerSettings);
        }
    }
    
    // Start UI check timers
    if (IsLocalController()) {
        GetWorldTimerManager().SetTimer(
            UICheckTimerHandle,
            this,
            &AWormPlayerController::CheckAndCreateUI,
            0.5f,
            true
        );
        
        GetWorldTimerManager().SetTimer(
            PlayerUITimerHandle, 
            this, 
            &AWormPlayerController::CreatePlayerUI, 
            1.0f, 
            false
        );
    }
}

void AWormPlayerController::LoadPlayerSettings()
{
    // Load player settings from save slot
    if (UGameplayStatics::DoesSaveGameExist("PlayerSettingsSave", 0))
    {
        UPlayerSaveGame* LoadedGame = Cast<UPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot("PlayerSettingsSave", 0));
        if (LoadedGame)
        {
            // Load data into PlayerSettings
            PlayerSettings = LoadedGame->SavedPlayerInfo;
            UE_LOG(LogTemp, Log, TEXT("Player settings loaded for %s"), *GetName());
            
            // Log the loaded character class for debugging
            if (PlayerSettings.MyPlayerCharacter) {
                UE_LOG(LogTemp, Log, TEXT("Loaded character class: %s"), *PlayerSettings.MyPlayerCharacter->GetName());
            } else {
                UE_LOG(LogTemp, Warning, TEXT("No character class in loaded settings!"));
            }
        } else {
            UE_LOG(LogTemp, Error, TEXT("Could not load player settings for %s"), *GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No player settings save found for %s"), *GetName());
    }
}

void AWormPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Add PlayerSettings to the list of replicated properties
    DOREPLIFETIME(AWormPlayerController, PlayerSettings);
    DOREPLIFETIME(AWormPlayerController, bIsReady);
}

void AWormPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AWormPlayerController::CreatePlayerUI()
{
    // Only create UI for local controller
    if (!IsLocalController())
    {
        UE_LOG(LogTemp, Verbose, TEXT("CreatePlayerUI: Failed - Not a local controller"));
        return;
    }
    
    // Create main HUD if defined
    if (MainHUDWidgetClass && !MainHUDWidget)
    {
        UE_LOG(LogTemp, Log, TEXT("Creating main HUD widget..."));
        CreateMainHUD();
    }
    else if (!MainHUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreatePlayerUI: MainHUDWidgetClass not defined"));
    }
    else if (MainHUDWidget)
    {
        UE_LOG(LogTemp, Log, TEXT("MainHUDWidget already exists"));
    }
}

bool AWormPlayerController::ServerSetPlayerSettings_Validate(const FPlayerData& NewSettings)
{
    // Simple validation to ensure we have a character class
    return NewSettings.MyPlayerCharacter != nullptr;
}

void AWormPlayerController::ServerSetPlayerSettings_Implementation(const FPlayerData& NewSettings)
{
    PlayerSettings = NewSettings;
    
    // Log more detailed information about the received settings
    if (PlayerSettings.MyPlayerCharacter) {
        UE_LOG(LogTemp, Warning, TEXT("Server received player settings for %s with character class: %s"), 
               *GetName(), *PlayerSettings.MyPlayerCharacter->GetName());
    } else {
        UE_LOG(LogTemp, Error, TEXT("Server received player settings with NULL character class for %s"), *GetName());
    }
    
    // Mark property as dirty to ensure it's replicated to other clients
    MARK_PROPERTY_DIRTY_FROM_NAME(AWormPlayerController, PlayerSettings, this);
}

void AWormPlayerController::OnNetCleanup(UNetConnection* Connection)
{
    Super::OnNetCleanup(Connection);
    bIsFullyInitialized = false;
}

void AWormPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);
    
    if (GetNetMode() != NM_Standalone)
    {
        // Signal ready to server
        ServerSignalReady();
    }
    else
    {
        bIsReady = true;
    }
}

void AWormPlayerController::OnRep_IsReady()
{
    if (bIsReady)
    {
        // Client knows it's ready - can prepare UI/etc
        if (IsLocalController())
        {
            // Create UI
            CreatePlayerUI();
        }
    }
}

bool AWormPlayerController::ServerSignalReady_Validate()
{
    return true;
}

void AWormPlayerController::ServerSignalReady_Implementation()
{
    if (!bIsReady)
    {
        bIsReady = true;
        
        // Notify GameMode
        AWormGameMode* GameMode = Cast<AWormGameMode>(GetWorld()->GetAuthGameMode());
        if (GameMode)
        {
            GameMode->NotifyPlayerReady(this);
        }
    }
}

void AWormPlayerController::CreateMainHUD()
{
    // Only create HUD for local controller
    if (!IsLocalController() || !MainHUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateMainHUD: Failed to create HUD - Not a local controller or widget class not defined"));
        return;
    }
    
    // If the widget already exists, do nothing
    if (MainHUDWidget)
    {
        UE_LOG(LogTemp, Verbose, TEXT("CreateMainHUD: HUD already exists"));
        return;
    }
    
    // Create widget instance
    MainHUDWidget = CreateWidget<UWMainHUDWidget>(this, MainHUDWidgetClass);
    if (MainHUDWidget)
    {
        // Add widget to viewport with high priority (10) to ensure it's above other widgets
        MainHUDWidget->AddToViewport(10);
        UE_LOG(LogTemp, Log, TEXT("CreateMainHUD: Main HUD created and added to viewport successfully"));
        
        // Schedule a refresh of the widget to ensure it's properly connected to GameState
        FTimerHandle RefreshTimerHandle;
        GetWorldTimerManager().SetTimer(
            RefreshTimerHandle,
            [this]()
            {
                RefreshMainHUD();
            },
            1.0f, // Small delay to ensure GameState is ready
            false
        );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CreateMainHUD: Failed to create UWMainHUDWidget"));
    }
}

// New function to explicitly refresh the HUD components
void AWormPlayerController::RefreshMainHUD()
{
    if (!MainHUDWidget)
    {
        return;
    }
    
    // Find GameState
    AWormGameState* GameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    if (!GameState)
    {
        UE_LOG(LogTemp, Warning, TEXT("RefreshMainHUD: GameState not found"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Manually refreshing main HUD components"));
    
    // Access child widgets and trigger manual updates
    if (MainHUDWidget->TeamStatusWidget)
    {
        MainHUDWidget->TeamStatusWidget->UpdateTeamStatus();
    }
    
    if (MainHUDWidget->TurnTimerWidget)
    {
        MainHUDWidget->TurnTimerWidget->UpdateTimer();
    }
    
    if (MainHUDWidget->PlayerStatusWidget)
    {
        MainHUDWidget->PlayerStatusWidget->UpdatePlayerStatus();
    }
    
    if (MainHUDWidget->ActiveCharacterInfoWidget)
    {
        MainHUDWidget->ActiveCharacterInfoWidget->OnActivePlayerChanged();
    }
    
    // Force broadcasting delegates to ensure widgets receive updates
    GameState->OnActivePlayerChanged.Broadcast();
    GameState->OnTurnTimerUpdated.Broadcast();
    GameState->OnTeamStatusUpdated.Broadcast();
}

void AWormPlayerController::CheckAndCreateUI()
{
    // Only check for local controllers
    if (IsLocalController())
    {
        // Create main HUD if class is defined
        if (!MainHUDWidget && MainHUDWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("Creating main HUD..."));
            CreateMainHUD();
        }
        
        // If we've created all necessary widgets, stop the timer
        if (MainHUDWidget || !MainHUDWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("All UI widgets have been created, stopping timer"));
            GetWorldTimerManager().ClearTimer(UICheckTimerHandle);
            
            // Add a periodic refresh to ensure UI stays updated
            GetWorldTimerManager().SetTimer(
                UIRefreshTimerHandle,
                this,
                &AWormPlayerController::RefreshMainHUD,
                2.0f, // Refresh every 2 seconds
                true
            );
        }
    }
    else
    {
        // Not a local controller, stop the timer
        UE_LOG(LogTemp, Log, TEXT("Not a local controller, stopping UI creation timer"));
        GetWorldTimerManager().ClearTimer(UICheckTimerHandle);
    }
}