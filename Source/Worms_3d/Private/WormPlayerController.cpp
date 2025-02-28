
// WormPlayerController.cpp
#include "WormPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UWormGameUI.h"
#include "WormGameState.h"

AWormPlayerController::AWormPlayerController()
{
    // Constructeur vide
}

void AWormPlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    // Démarrer un timer qui va vérifier périodiquement si l'UI peut être créée
    GetWorldTimerManager().SetTimer(
        UICheckTimerHandle,
        this,
        &AWormPlayerController::CheckAndCreateUI,
        0.5f,
        true // Répéter jusqu'à ce que l'UI soit créée
    );
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