#include "TestWormPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "AWormCharacter.h"
#include "TestWormGameMode.h"

ATestWormPlayerController::ATestWormPlayerController()
{
    // Rien à initialiser dans le constructeur pour l'instant
}

void ATestWormPlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    if (IsLocalPlayerController())
    {
        // Créer l'interface utilisateur
        CreateTestUI();
        
        // On peut aussi ajouter d'autres initialisations spécifiques au mode test ici
        UE_LOG(LogTemp, Log, TEXT("TestWormPlayerController: BeginPlay pour le contrôleur local"));
    }
}

void ATestWormPlayerController::CreateTestUI()
{
    // Vérifier que la classe UI est définie
    if (!TestUIWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("TestUIWidgetClass non défini dans TestWormPlayerController"));
        return;
    }
    
    // Créer le widget si ce n'est pas déjà fait
    if (!TestUIWidget)
    {
        TestUIWidget = CreateWidget<UUserWidget>(this, TestUIWidgetClass);
        
        if (TestUIWidget)
        {
            TestUIWidget->AddToViewport();
            UE_LOG(LogTemp, Log, TEXT("Interface utilisateur de test créée avec succès"));
            
            // Configurer l'input mode
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputMode.SetHideCursorDuringCapture(false);
            SetInputMode(InputMode);
            
            // Montrer le curseur pour faciliter les tests
            bShowMouseCursor = true;
        }
    }
}

void ATestWormPlayerController::EndTurn()
{
    // Référence au GameMode
    ATestWormGameMode* TestGameMode = Cast<ATestWormGameMode>(GetWorld()->GetAuthGameMode());
    if (TestGameMode)
    {
        // Réinitialiser le tour dans le mode test
        TestGameMode->ResetTurn();
        
        UE_LOG(LogTemp, Log, TEXT("Tour terminé par le joueur"));
    }
    
    // Dans un vrai jeu, on passerait le tour au prochain joueur
}

void ATestWormPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // On pourrait ajouter ici des mises à jour périodiques spécifiques au mode test
}