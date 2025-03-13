#include "UILocal.h"
#include "AWormCharacter.h"
#include "WormPlayerController.h"
#include "WormWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"

void UWormPlayerUI::NativeConstruct()
{
    Super::NativeConstruct();
    
    UE_LOG(LogTemp, Log, TEXT("UWormPlayerUI::NativeConstruct - Initializing player UI"));
    
    // Initialiser avec des valeurs par défaut
    if (CharacterNameText)
        CharacterNameText->SetText(FText::FromString(TEXT("...")));
    
    if (CurrentWeaponText)
        CurrentWeaponText->SetText(FText::FromString(TEXT("...")));
    
    if (MovementPointsText)
        MovementPointsText->SetText(FText::FromString(TEXT("Points: --/--")));
    
    if (MovementPointsBar)
        MovementPointsBar->SetPercent(1.0f);
    
    if (HealthText)
        HealthText->SetText(FText::FromString(TEXT("Santé: --")));
    
    if (HealthBar)
        HealthBar->SetPercent(1.0f);
    
    if (TurnStatusText)
        TurnStatusText->SetText(FText::FromString(TEXT("")));
    
    // Tenter d'obtenir une référence vers le personnage du joueur
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    {
        PlayerCharacter = Cast<AWormCharacter>(PC->GetPawn());
        if (PlayerCharacter)
        {
            UE_LOG(LogTemp, Log, TEXT("Found player character: %s"), *PlayerCharacter->GetName());
            
            // Mettre à jour l'UI immédiatement
            UpdatePlayerInfo();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Player character not available in NativeConstruct, will retry in NativeTick"));
        }
    }
}

void UWormPlayerUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Mettre à jour l'UI à chaque tick pour les valeurs qui changent fréquemment
    UpdatePlayerInfo();
}

void UWormPlayerUI::UpdatePlayerInfo()
{
    // Si nous n'avons pas le personnage, essayer de l'obtenir
    if (!PlayerCharacter)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
        if (PC)
        {
            PlayerCharacter = Cast<AWormCharacter>(PC->GetPawn());
            if (!PlayerCharacter)
            {
                return; // Toujours pas de personnage, on quitte
            }
        }
        else
        {
            return;
        }
    }
    
    // Mise à jour du nom du personnage
    if (CharacterNameText)
    {
        FString PlayerName;
        APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
        AWormPlayerController* WPC = Cast<AWormPlayerController>(PC);
        
        if (WPC && !WPC->PlayerSettings.MyPlayerName.IsEmpty())
        {
            PlayerName = WPC->PlayerSettings.MyPlayerName.ToString();
        }
        else if (PlayerCharacter)
        {
            // Fallback au nom du Pawn
            PlayerName = PlayerCharacter->GetName();
            // Simplifier le nom en enlevant le préfixe BP_ et autres nomenclatures
            PlayerName.ReplaceInline(TEXT("BP_"), TEXT(""));
            PlayerName.ReplaceInline(TEXT("Character"), TEXT(""));
            PlayerName.ReplaceInline(TEXT("_C"), TEXT(""));
        }
        
        CharacterNameText->SetText(FText::FromString(PlayerName));
    }
    
    // Mise à jour de l'arme actuelle
    if (CurrentWeaponText && PlayerCharacter->CurrentWeapon)
    {
        FString WeaponName = PlayerCharacter->CurrentWeapon->GetName();
        // Simplifier le nom de l'arme
        WeaponName.ReplaceInline(TEXT("BP_"), TEXT(""));
        WeaponName.ReplaceInline(TEXT("Weapon"), TEXT(""));
        WeaponName.ReplaceInline(TEXT("_C"), TEXT(""));
        CurrentWeaponText->SetText(FText::FromString(WeaponName));
    }
    else if (CurrentWeaponText)
    {
        CurrentWeaponText->SetText(FText::FromString(TEXT("Aucune arme")));
    }
    
    // Mise à jour des points de mouvement
    if (MovementPointsText)
    {
        MovementPointsText->SetText(FText::FromString(
            FString::Printf(TEXT("Points: %.0f/%.0f"), 
            PlayerCharacter->MovementPoints,
            PlayerCharacter->MaxMovementPoints)
        ));
    }
    
    if (MovementPointsBar)
    {
        float MovementPercent = PlayerCharacter->MovementPoints / PlayerCharacter->MaxMovementPoints;
        MovementPointsBar->SetPercent(FMath::Clamp(MovementPercent, 0.0f, 1.0f));
        
        // Changer la couleur de la barre selon le niveau de points restants
        FLinearColor BarColor;
        if (MovementPercent > 0.7f)
            BarColor = FLinearColor(0.0f, 0.8f, 0.2f); // Vert
        else if (MovementPercent > 0.3f)
            BarColor = FLinearColor(1.0f, 0.8f, 0.0f); // Jaune
        else
            BarColor = FLinearColor(1.0f, 0.2f, 0.0f); // Rouge
        
        MovementPointsBar->SetFillColorAndOpacity(BarColor);
    }
    
    // Mise à jour de la santé
    if (HealthText)
    {
        HealthText->SetText(FText::FromString(
            FString::Printf(TEXT("Santé: %.0f"), PlayerCharacter->GetHealth())
        ));
    }
    
    if (HealthBar)
    {
        float HealthPercent = PlayerCharacter->GetHealth() / 100.0f; // Assumant que la santé max est 100
        HealthBar->SetPercent(FMath::Clamp(HealthPercent, 0.0f, 1.0f));
        
        // Changer la couleur de la barre selon le niveau de santé
        FLinearColor BarColor;
        if (HealthPercent > 0.7f)
            BarColor = FLinearColor(0.0f, 0.8f, 0.2f); // Vert
        else if (HealthPercent > 0.3f)
            BarColor = FLinearColor(1.0f, 0.8f, 0.0f); // Jaune
        else
            BarColor = FLinearColor(1.0f, 0.2f, 0.0f); // Rouge
        
        HealthBar->SetFillColorAndOpacity(BarColor);
    }
    
    // Mise à jour du statut de tour
    if (TurnStatusText)
    {
        if (PlayerCharacter->IsMyTurn())
        {
            TurnStatusText->SetText(FText::FromString(TEXT("À VOTRE TOUR!")));
            TurnStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.0f))); // Jaune vif
        }
        else
        {
            TurnStatusText->SetText(FText::FromString(TEXT("En attente...")));
            TurnStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f))); // Gris
        }
    }
}