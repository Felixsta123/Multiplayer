// UWormGameUI.cpp
#include "UWormGameUI.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"
#include "WormGameMode.h"
void UWormGameUI::NativeConstruct()
{
    Super::NativeConstruct();
    
    UE_LOG(LogTemp, Log, TEXT("UWormGameUI::NativeConstruct called"));
    
    // Initialiser les valeurs par défaut
    if (CurrentPlayerName)
        CurrentPlayerName->SetText(FText::FromString(TEXT("En attente...")));
    
    if (RemainingTime)
        RemainingTime->SetText(FText::FromString(TEXT("--:--")));
    
    if (TimeProgressBar)
        TimeProgressBar->SetPercent(1.0f);
    
    if (PlayersRemaining)
        PlayersRemaining->SetText(FText::FromString(TEXT("Joueurs: ?")));
    
    // Tenter d'obtenir une référence vers le GameState
    AGameStateBase* GameState = UGameplayStatics::GetGameState(this);
    WormGameState = Cast<AWormGameState>(GameState);
    
    if (WormGameState)
    {
        UE_LOG(LogTemp, Log, TEXT("GameState found in NativeConstruct"));
        
        // Important: Retirer toute liaison existante avant d'en ajouter une nouvelle
        WormGameState->OnCurrentPlayerChanged.RemoveDynamic(this, &UWormGameUI::OnCurrentPlayerChangedCallback);
        
        // S'abonner au delegate pour être notifié des changements de joueur
        WormGameState->OnCurrentPlayerChanged.AddDynamic(this, &UWormGameUI::OnCurrentPlayerChangedCallback);
        
        // Mettre à jour l'UI immédiatement
        UpdateUI();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GameState not available in NativeConstruct, will retry in NativeTick"));
    }
}

void UWormGameUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // N'actualiser l'UI que toutes les 0.5 secondes pour éviter les problèmes de timing
    static float TimeSinceLastUpdate = 0.0f;
    TimeSinceLastUpdate += InDeltaTime;
    
    if (TimeSinceLastUpdate >= 0.5f)
    {
        // Mettre à jour l'UI
        UpdateUI();
        TimeSinceLastUpdate = 0.0f;
    }
}
void UWormGameUI::OnCurrentPlayerChangedCallback(int32 NewPlayerIndex)
{
    UE_LOG(LogTemp, Log, TEXT("UI notified of player change to index %d"), NewPlayerIndex);
    
    // Vérifier que le GameState existe toujours
    if (!WormGameState)
    {
        UE_LOG(LogTemp, Error, TEXT("GameState is null in OnCurrentPlayerChangedCallback"));
        return;
    }
    
    // Vérifier que le nouvel index est valide
    if (!WormGameState->PlayerNames.IsValidIndex(NewPlayerIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid player index %d in callback"), NewPlayerIndex);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("New player is: %s"), 
        *WormGameState->PlayerNames[NewPlayerIndex]);
    
    // Forcer une mise à jour de l'UI immédiatement
    UpdateUI();
}

void UWormGameUI::UpdateUI()
{
    // Essayer d'obtenir le GameState s'il n'est pas déjà défini
    if (!WormGameState)
    {
        AGameStateBase* GameState = UGameplayStatics::GetGameState(this);
        WormGameState = Cast<AWormGameState>(GameState);
        
        if (!WormGameState)
        {
            // Toujours pas de GameState, on quitte sans mettre à jour
            return;
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("GameState found in UpdateUI"));
            
            // Important: Retirer toute liaison existante avant d'en ajouter une nouvelle
            WormGameState->OnCurrentPlayerChanged.RemoveDynamic(this, &UWormGameUI::OnCurrentPlayerChangedCallback);
            
            // S'abonner au delegate maintenant qu'on a trouvé le GameState
            WormGameState->OnCurrentPlayerChanged.AddDynamic(this, &UWormGameUI::OnCurrentPlayerChangedCallback);
        }
    }
    
    // Mise à jour du nom du joueur actif
    if (CurrentPlayerName)
    {
        FString PlayerName;
        
        if (!WormGameState->CurrentPlayerName.IsEmpty())
        {
            PlayerName = WormGameState->CurrentPlayerName;
            UE_LOG(LogTemp, Verbose, TEXT("Current player name: %s"), *PlayerName);
        }
        else if (WormGameState->PlayerNames.IsValidIndex(WormGameState->CurrentPlayerIndex))
        {
            // Fallback au cas où CurrentPlayerName n'est pas défini
            PlayerName = WormGameState->PlayerNames[WormGameState->CurrentPlayerIndex];
            UE_LOG(LogTemp, Warning, TEXT("Using fallback player name: %s"), *PlayerName);
        }
        else
        {
            PlayerName = TEXT("Aucun joueur actif");
            UE_LOG(LogTemp, Warning, TEXT("No valid player name found, using default"));
        }
        
        CurrentPlayerName->SetText(FText::FromString(FString::Printf(TEXT("Joueur actif: %s"), *PlayerName)));
    }
    
    // Mise à jour du temps restant
    if (RemainingTime)
    {
        int32 Seconds = FMath::FloorToInt(WormGameState->RemainingTurnTime);
        int32 Minutes = Seconds / 60;
        Seconds = Seconds % 60;
        
        FString TimeString = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
        RemainingTime->SetText(FText::FromString(TimeString));
    }
    
    // Mise à jour de la barre de progression du temps
    if (TimeProgressBar && WormGameState->TurnDuration > 0)
    {
        float Percent = FMath::Clamp(WormGameState->RemainingTurnTime / WormGameState->TurnDuration, 0.0f, 1.0f);
        TimeProgressBar->SetPercent(Percent);
    }
    
    // Mise à jour du nombre de joueurs restants - utiliser la fonction améliorée
    if (PlayersRemaining)
    {
        int32 AliveCount = WormGameState->GetRemainingPlayersCount();
        PlayersRemaining->SetText(FText::FromString(FString::Printf(TEXT("Joueurs: %d"), AliveCount)));
    }
    APlayerController* PC = GetOwningPlayer();
    if (PC && WormGameState)
    {
        APawn* ControlledPawn = PC->GetPawn();
        if (ControlledPawn)
        {
            // Essayer une approche basée sur l'index du controller plutôt que le nom
            // Obtenir l'index actuel du PlayerController dans le jeu
            int32 MyControllerIndex = -1;
            AController* MyController = PC;
            
            // Obtenir le GameMode pour trouver tous les controllers
            AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
            AWormGameMode* WormGameMode = Cast<AWormGameMode>(GameMode);
            
            // Si nous ne sommes pas sur le serveur, on ne peut pas accéder directement au GameMode
            if (!WormGameMode && ControlledPawn)
            {
                // Sur un client, essayons de déterminer notre index d'une autre façon
                AWormCharacter* MyWorm = Cast<AWormCharacter>(ControlledPawn);
                if (MyWorm)
                {
                    // Vérifier si c'est notre tour via le flag bIsMyTurn du personnage
                    bool IsMyTurn = MyWorm->IsMyTurn();
                    
                    if (YourTurnText)
                    {
                        YourTurnText->SetText(FText::FromString(IsMyTurn ? TEXT("C'EST VOTRE TOUR!") : TEXT("")));
                    }
                    
                    return;  // Nous avons déjà mis à jour l'interface, pas besoin de continuer
                }
            }
            
            // Fallback à l'approche précédente si tout le reste échoue
            FString MyName = ControlledPawn->GetName();
            
            
            bool IsMyTurn = (WormGameState->CurrentPlayerName == MyName);
            
            if (YourTurnText)
            {
                YourTurnText->SetText(FText::FromString(IsMyTurn ? TEXT("C'EST VOTRE TOUR!") : TEXT("")));
                
                UE_LOG(LogTemp, Log, TEXT("Is my turn? By name: %s, Final: %s"), 
                    IsMyTurn ? TEXT("Yes") : TEXT("No"),
                    IsMyTurn ? TEXT("Yes") : TEXT("No"));
            }
        }
    }
}

void UWormGameUI::EnsureDelegateBinding()
{
    // Si nous n'avons pas de GameState, essayons d'en obtenir un
    if (!WormGameState)
    {
        WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(this));
    }
    
    // Si nous avons maintenant un GameState, assurons-nous que le delegate est lié
    if (WormGameState)
    {
        // Supprimer la liaison existante pour éviter les doublons
        WormGameState->OnCurrentPlayerChanged.RemoveDynamic(this, &UWormGameUI::OnCurrentPlayerChangedCallback);
        
        // Ajouter une nouvelle liaison
        WormGameState->OnCurrentPlayerChanged.AddDynamic(this, &UWormGameUI::OnCurrentPlayerChangedCallback);
        
    }
}