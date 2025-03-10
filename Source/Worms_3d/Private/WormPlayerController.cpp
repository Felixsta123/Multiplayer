
// WormPlayerController.cpp
#include "WormPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UWormGameUI.h"
#include "WormGameState.h"
#include "Worms_3d/PlayerSaveGame.h"

AWormPlayerController::AWormPlayerController()
{
    // Constructeur vide
}
void AWormPlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    // Charger les paramètres du joueur depuis le slot de sauvegarde
    if (UGameplayStatics::DoesSaveGameExist("PlayerSettingsSave", 0))
    {
        UPlayerSaveGame* LoadedGame = Cast<UPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot("PlayerSettingsSave", 0));
        if (LoadedGame)
        {
            // Load data into PlayerSettings
            PlayerSettings = LoadedGame->SavedPlayerInfo;
            UE_LOG(LogTemp, Log, TEXT("Player settings loaded for %s"), *GetName());
            
            // ADD THIS: Force immediate replication to server
            if (GetLocalRole() < ROLE_Authority)
            {
                ServerSetPlayerSettings(PlayerSettings);
            }
        } else {
            UE_LOG(LogTemp, Error, TEXT("Could not load player settings for %s"), *GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No player settings save found for %s"), *GetName());
    }
    // Démarrer les timers d'UI
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



void AWormPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Add PlayerSettings to the list of replicated properties
    DOREPLIFETIME(AWormPlayerController, PlayerSettings);
}

void AWormPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Vérifier périodiquement que l'UI est toujours correctement liée
    if (IsLocalController() && GameUIWidget)
    {
        // Ceci nécessiterait d'ajouter une méthode dans votre UI pour vérifier la liaison
        UWormGameUI* WormUI = Cast<UWormGameUI>(GameUIWidget);
        if (WormUI)
        {
            WormUI->EnsureDelegateBinding();
        }
    }
}

void AWormPlayerController::CheckAndCreateUI()
{
    // Vérifier si ce contrôleur est local et prêt à créer l'UI
    if (IsLocalController() && !GameUIWidget && GameUIClass)
    {
        UE_LOG(LogTemp, Log, TEXT("Checking if ready to create UI for local controller: %s"), *GetName());
        
        // Vérifier si le GameState est disponible
        AWormGameState* GameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
        if (GameState)
        {
            UE_LOG(LogTemp, Log, TEXT("GameState is available. Creating UI widget..."));
            
            // Créer l'UI
            GameUIWidget = CreateWidget<UUserWidget>(this, GameUIClass);
            if (GameUIWidget)
            {
                GameUIWidget->AddToViewport();
                UE_LOG(LogTemp, Log, TEXT("UI widget added to viewport successfully"));
                
                // Arrêter le timer, nous avons réussi
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
        // Si ce n'est pas un contrôleur local, arrêter le timer
        UE_LOG(LogTemp, Log, TEXT("Not a local controller, stopping UI creation timer"));
        GetWorldTimerManager().ClearTimer(UICheckTimerHandle);
    }
}

void AWormPlayerController::CreatePlayerUI()
{
    // Vérifier que la classe UI est définie
    if (!PlayerUIWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerUIWidgetClass non défini dans WormPlayerController"));
        return;
    }
    
    // Créer le widget si ce n'est pas déjà fait
    if (!PlayerUIWidget)
    {
        PlayerUIWidget = CreateWidget<UUserWidget>(this, PlayerUIWidgetClass);
        
        if (PlayerUIWidget)
        {
            PlayerUIWidget->AddToViewport(1); // Z-Order 1 pour être au-dessus de l'UI du jeu de base
            UE_LOG(LogTemp, Log, TEXT("Interface utilisateur du joueur créée avec succès"));
        }
    }
}

void AWormPlayerController::ServerSetPlayerSettings_Implementation(const FPlayerData& NewSettings)
{
    PlayerSettings = NewSettings;
    UE_LOG(LogTemp, Warning, TEXT("Server received player settings for %s"), *GetName());
}