#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TestWormUI.generated.h"

/**
 * Interface utilisateur simplifiée pour le mode test
 */
UCLASS()
class WORMS_3D_API UTestWormUI : public UUserWidget
{
	GENERATED_BODY()
    
public:
	// Fonctions de cycle de vie
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
	// Références aux éléments UI (liés via le système de binding UMG)
	UPROPERTY(meta = (BindWidget))
	class UButton* EndTurnButton;
    
	UPROPERTY(meta = (BindWidget))
	class UButton* TestExplosionButton;
    
	UPROPERTY(meta = (BindWidget))
	class UButton* ResetTerrainButton;
    
	UPROPERTY(meta = (BindWidget))
	class UButton* ToggleDestructionSystemButton;
    
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TimeRemainingText;
    
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* PlayerInfoText;
    
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DestructionSystemText;
    
	// Callbacks pour les boutons
	UFUNCTION()
	void OnEndTurnClicked();
    
	UFUNCTION()
	void OnTestExplosionClicked();
    
	UFUNCTION()
	void OnResetTerrainClicked();
    
	UFUNCTION()
	void OnToggleDestructionSystemClicked();
    
protected:
	// Fonction pour mettre à jour les informations affichées
	void UpdateTestInfo();
};