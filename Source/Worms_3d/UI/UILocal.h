#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "UILocal.generated.h"

/**
 * Widget UI pour afficher les informations du personnage Worm pour le joueur local
 */
UCLASS()
class WORMS_3D_API UWormPlayerUI : public UUserWidget
{
	GENERATED_BODY()
    
public:
	// Fonctions du cycle de vie
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
protected:
	// Éléments d'UI liés via le système de binding UMG
    
	// Nom du personnage
	UPROPERTY(meta = (BindWidget))
	UTextBlock* CharacterNameText;
    
	// Arme actuelle
	UPROPERTY(meta = (BindWidget))
	UTextBlock* CurrentWeaponText;
    
	// Points de mouvement (texte)
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MovementPointsText;
    
	// Barre de progression pour les points de mouvement
	UPROPERTY(meta = (BindWidget))
	UProgressBar* MovementPointsBar;
    
	// Santé (texte)
	UPROPERTY(meta = (BindWidget))
	UTextBlock* HealthText;
    
	// Barre de progression pour la santé
	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthBar;
    
	// Indicateur de tour actif
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TurnStatusText;
    
	// Image de l'arme actuelle (optionnel)
	UPROPERTY(meta = (BindWidget))
	UImage* WeaponImage;
    
	// Fonction pour mettre à jour l'UI avec les données du personnage local
	UFUNCTION()
	void UpdatePlayerInfo();
    
	// Référence au personnage du joueur
	UPROPERTY()
	class AWormCharacter* PlayerCharacter;
};