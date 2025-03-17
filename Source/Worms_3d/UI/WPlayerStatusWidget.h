#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WPlayerStatusWidget.generated.h"

/**
 * Widget qui indique si c'est le tour du joueur ou s'il attend
 */
UCLASS()
class WORMS_3D_API UWPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()
    
public:
	UWPlayerStatusWidget(const FObjectInitializer& ObjectInitializer);
    
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
    
	// Mettre à jour l'affichage du statut
	UFUNCTION()
	void UpdatePlayerStatus();
    
protected:
	// Éléments UI
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* StatusText;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UImage* StatusBackground;
    
	// Référence au GameState
	UPROPERTY()
	class AWormGameState* WormGameState;
    
	// Couleurs pour les différents états
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Appearance")
	FLinearColor ActiveColor = FLinearColor::Green;
    
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Appearance")
	FLinearColor WaitingColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f); // Gris
};