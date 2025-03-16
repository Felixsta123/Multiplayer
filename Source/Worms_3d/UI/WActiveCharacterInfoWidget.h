#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WActiveCharacterInfoWidget.generated.h"

/**
 * Widget qui affiche les informations du personnage actif (PA, arme équipée, nom)
 */
UCLASS()
class WORMS_3D_API UWActiveCharacterInfoWidget : public UUserWidget
{
	GENERATED_BODY()
    
public:
	UWActiveCharacterInfoWidget(const FObjectInitializer& ObjectInitializer);
    
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
        
	// Mettre à jour les informations
	UFUNCTION()
	void OnActivePlayerChanged();
    
protected:
	// Éléments UI
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* CharacterNameText;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* WeaponNameText;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UProgressBar* MovementPointsBar;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* MovementPointsText;
    
	// Référence au GameState et au personnage actif
	UPROPERTY()
	class AWormGameState* WormGameState;
    
	UPROPERTY()
	class AWormCharacter* ActiveCharacter;

	// Mettre à jour les points de mouvement
	void UpdateMovementPoints();
    
	// Mettre à jour l'arme équipée
	void UpdateWeapon();
};