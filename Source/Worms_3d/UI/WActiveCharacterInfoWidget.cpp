#include "Worms_3d/UI/WActiveCharacterInfoWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"
#include "Worms_3d/AWormCharacter.h"
#include "Worms_3d/WormWeapon.h"

UWActiveCharacterInfoWidget::UWActiveCharacterInfoWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Activer le tick pour les mises à jour en temps réel

}

void UWActiveCharacterInfoWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Récupérer le GameState
    WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    
    if (WormGameState)
    {
        // S'abonner à l'événement de changement de joueur actif
        WormGameState->OnActivePlayerChanged.AddDynamic(this, &UWActiveCharacterInfoWidget::OnActivePlayerChanged);
        
        // Mise à jour initiale
        OnActivePlayerChanged();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WActiveCharacterInfoWidget: GameState non trouvé"));
    }
}

void UWActiveCharacterInfoWidget::NativeDestruct()
{
    // Se désabonner des événements
    if (WormGameState)
    {
        WormGameState->OnActivePlayerChanged.RemoveDynamic(this, &UWActiveCharacterInfoWidget::OnActivePlayerChanged);
    }
    
    ActiveCharacter = nullptr;
    
    Super::NativeDestruct();
}

void UWActiveCharacterInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Mettre à jour les informations en temps réel si nécessaire
    if (ActiveCharacter && ActiveCharacter->IsMyTurn())
    {
        UpdateMovementPoints();
        UpdateWeapon();
    }
}

void UWActiveCharacterInfoWidget::OnActivePlayerChanged()
{
    // Ne mettre à jour que si c'est le tour du joueur local
    if (WormGameState && WormGameState->IsLocalPlayerTurn())
    {
        // Récupérer le personnage actif
        ActiveCharacter = WormGameState->GetActiveCharacter();
        
        if (ActiveCharacter)
        {
            // Mettre à jour le nom du personnage
            if (CharacterNameText)
            {
                CharacterNameText->SetText(FText::FromString(ActiveCharacter->InGameName));
            }
            
            // Mettre à jour les autres informations
            UpdateMovementPoints();
            UpdateWeapon();
        }
        else
        {
            // Aucun personnage actif, vider les informations
            if (CharacterNameText)
            {
                CharacterNameText->SetText(FText::FromString(TEXT("")));
            }
            
            if (WeaponNameText)
            {
                WeaponNameText->SetText(FText::FromString(TEXT("")));
            }
            
            if (MovementPointsBar)
            {
                MovementPointsBar->SetPercent(0.0f);
            }
            
            if (MovementPointsText)
            {
                MovementPointsText->SetText(FText::FromString(TEXT("0 / 0")));
            }
        }
    }
    else
    {
        ActiveCharacter = nullptr;
    }
}

void UWActiveCharacterInfoWidget::UpdateMovementPoints()
{
    if (!ActiveCharacter)
    {
        return;
    }
    
    // Mettre à jour la barre de progression
    if (MovementPointsBar)
    {
        float Percent = ActiveCharacter->MovementPoints / ActiveCharacter->MaxMovementPoints;
        MovementPointsBar->SetPercent(Percent);
        
        // Changer la couleur en fonction des points restants
        FLinearColor MovementColor;
        if (Percent > 0.6f)
        {
            MovementColor = FLinearColor::Green;
        }
        else if (Percent > 0.3f)
        {
            MovementColor = FLinearColor::Yellow;
        }
        else
        {
            MovementColor = FLinearColor::Red;
        }
        
        MovementPointsBar->SetFillColorAndOpacity(MovementColor);
    }
    
    // Mettre à jour le texte
    if (MovementPointsText)
    {
        int32 Current = FMath::FloorToInt(ActiveCharacter->MovementPoints);
        int32 Max = FMath::FloorToInt(ActiveCharacter->MaxMovementPoints);
        FString MovementString = FString::Printf(TEXT("%d / %d"), Current, Max);
        MovementPointsText->SetText(FText::FromString(MovementString));
    }
}

void UWActiveCharacterInfoWidget::UpdateWeapon()
{
    if (!ActiveCharacter)
    {
        return;
    }
    
    // Mettre à jour le nom de l'arme
    if (WeaponNameText && ActiveCharacter->CurrentWeapon)
    {
        FString WeaponName = ActiveCharacter->CurrentWeapon->GetClass()->GetName();
        
        // Nettoyage du nom de classe pour un affichage plus propre
        WeaponName.ReplaceInline(TEXT("WormWeapon_"), TEXT(""));
        WeaponName.ReplaceInline(TEXT("BP_"), TEXT(""));
        WeaponName.ReplaceInline(TEXT("_C"), TEXT(""));
        
        WeaponNameText->SetText(FText::FromString(WeaponName));
    }
    else if (WeaponNameText)
    {
        WeaponNameText->SetText(FText::FromString(TEXT("Aucune arme")));
    }
}