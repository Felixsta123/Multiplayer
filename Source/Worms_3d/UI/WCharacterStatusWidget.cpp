#include "Worms_3d/UI/WCharacterStatusWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Worms_3d/AWormCharacter.h"
#include "WormGameState.h"

UWCharacterStatusWidget::UWCharacterStatusWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Activer le tick pour les mises à jour en temps réel
}


void UWCharacterStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Mise à jour initiale
    UpdateHealth();
}

void UWCharacterStatusWidget::NativeDestruct()
{
    // Nettoyer les références
    Character = nullptr;
    
    Super::NativeDestruct();
}

void UWCharacterStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Mettre à jour la barre de vie si nécessaire
    // Note: cela peut être optimisé pour ne pas mettre à jour à chaque tick
    if (Character)
    {
        UpdateHealth();
    }
}

void UWCharacterStatusWidget::SetCharacter(AWormCharacter* InCharacter)
{
    Character = InCharacter;
    
    // Mettre à jour les informations
    if (Character)
    {
        // Définir le nom du personnage
        if (CharacterNameText)
        {
            CharacterNameText->SetText(FText::FromString(Character->InGameName));
        }
        
        // Définir la couleur de l'équipe
        if (TeamColorImage)
        {
            // Récupérer la couleur de l'équipe depuis le GameState
            AWormGameState* GameState = Cast<AWormGameState>(GetWorld()->GetGameState());
            if (GameState && GameState->Teams.IsValidIndex(Character->TeamId))
            {
                FLinearColor TeamColor = GameState->Teams[Character->TeamId].TeamColor;
                TeamColorImage->SetColorAndOpacity(TeamColor);
            }
        }
        
        // Mettre à jour la barre de vie
        UpdateHealth();
    }
}

void UWCharacterStatusWidget::UpdateHealth()
{
    if (Character && HealthBar)
    {
        // Calculer le pourcentage de vie
        float HealthPercent = Character->GetHealth() / 100.0f; // Supposant 100 comme vie max
        HealthBar->SetPercent(HealthPercent);
        
        // Changer la couleur en fonction de la santé
        FLinearColor HealthColor;
        if (HealthPercent > 0.6f)
        {
            HealthColor = FLinearColor::Green;
        }
        else if (HealthPercent > 0.3f)
        {
            HealthColor = FLinearColor::Yellow;
        }
        else
        {
            HealthColor = FLinearColor::Red;
        }
        
        HealthBar->SetFillColorAndOpacity(HealthColor);
    }
}