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
    // Ne créer le HUD que pour le contrôleur local
    if (!IsLocalController())
    {
        UE_LOG(LogTemp, Verbose, TEXT("CreatePlayerUI: Échec - Contrôleur non local"));
        return;
    }
    
    // Créer le HUD principal si défini
    if (MainHUDWidgetClass && !MainHUDWidget)
    {
        CreateMainHUD();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CreatePlayerUI: MainHUDWidgetClass non défini"));
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
    // Ne créer le HUD que pour le contrôleur local
    if (!IsLocalController() || !MainHUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateMainHUD: Échec de création du HUD - Contrôleur non local ou classe de widget non définie"));
        return;
    }
    
    // Si le widget existe déjà, ne rien faire
    if (MainHUDWidget)
    {
        UE_LOG(LogTemp, Verbose, TEXT("CreateMainHUD: Le HUD existe déjà"));
        return;
    }
    
    // Créer l'instance du widget
    MainHUDWidget = CreateWidget<UWMainHUDWidget>(this, MainHUDWidgetClass);
    if (MainHUDWidget)
    {
        // Ajouter le widget au viewport avec une priorité élevée (10) pour s'assurer qu'il est au-dessus des autres widgets
        MainHUDWidget->AddToViewport(10);
        UE_LOG(LogTemp, Log, TEXT("CreateMainHUD: HUD principal créé et ajouté au viewport avec succès"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CreateMainHUD: Échec de création du widget UWMainHUDWidget"));
    }
}

void AWormPlayerController::CheckAndCreateUI()
{
    // Only create UI for local controllers
    if (IsLocalController())
    {
        AWormGameState* GameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
        
        // Créer le HUD principal si la classe est définie
        if (!MainHUDWidget && MainHUDWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("Création du HUD principal..."));
            CreateMainHUD();
        }
        
        // Si nous avons créé tous les widgets nécessaires, arrêter le timer
        if (MainHUDWidget || !MainHUDWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("Tous les widgets UI ont été créés, arrêt du timer"));
            GetWorldTimerManager().ClearTimer(UICheckTimerHandle);
        }
    }
    else
    {
        // Not a local controller, stop the timer
        UE_LOG(LogTemp, Log, TEXT("Pas un contrôleur local, arrêt du timer de création UI"));
        GetWorldTimerManager().ClearTimer(UICheckTimerHandle);
    }
}