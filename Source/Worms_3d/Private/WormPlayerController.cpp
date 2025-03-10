#include "WormPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UWormGameUI.h"
#include "WormGameState.h"
#include "Worms_3d/PlayerSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

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
}

void AWormPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Periodic check to ensure UI delegate bindings are valid
    if (IsLocalController() && GameUIWidget)
    {
        // This requires adding a method to check delegate bindings
        UWormGameUI* WormUI = Cast<UWormGameUI>(GameUIWidget);
        if (WormUI)
        {
            WormUI->EnsureDelegateBinding();
        }
    }
}

void AWormPlayerController::CheckAndCreateUI()
{
    // Only create UI for local controllers with valid UI class
    if (IsLocalController() && !GameUIWidget && GameUIClass)
    {
        UE_LOG(LogTemp, Log, TEXT("Checking if ready to create UI for local controller: %s"), *GetName());
        
        // Verify GameState is available
        AWormGameState* GameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
        if (GameState)
        {
            UE_LOG(LogTemp, Log, TEXT("GameState is available. Creating UI widget..."));
            
            // Create UI
            GameUIWidget = CreateWidget<UUserWidget>(this, GameUIClass);
            if (GameUIWidget)
            {
                GameUIWidget->AddToViewport();
                UE_LOG(LogTemp, Log, TEXT("UI widget added to viewport successfully"));
                
                // Stop the timer, we're done
                GetWorldTimerManager().ClearTimer(UICheckTimerHandle);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to create UI widget!"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("GameState not yet available, will try again..."));
        }
    }
    else if (!IsLocalController())
    {
        // Not a local controller, stop the timer
        UE_LOG(LogTemp, Log, TEXT("Not a local controller, stopping UI creation timer"));
        GetWorldTimerManager().ClearTimer(UICheckTimerHandle);
    }
}

void AWormPlayerController::CreatePlayerUI()
{
    // Verify UI class is defined
    if (!PlayerUIWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerUIWidgetClass not defined in WormPlayerController"));
        return;
    }
    
    // Create widget if not already done
    if (!PlayerUIWidget)
    {
        PlayerUIWidget = CreateWidget<UUserWidget>(this, PlayerUIWidgetClass);
        
        if (PlayerUIWidget)
        {
            PlayerUIWidget->AddToViewport(1); // Z-Order 1 to be above base game UI
            UE_LOG(LogTemp, Log, TEXT("Interface utilisateur du joueur créée avec succès"));
        }
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