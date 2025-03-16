#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTurnTimerWidget.generated.h"

/**
 * Widget qui affiche le temps restant pour le tour actuel
 */
UCLASS()
class WORMS_3D_API UWTurnTimerWidget : public UUserWidget
{
	GENERATED_BODY()
    
public:
	UWTurnTimerWidget(const FObjectInitializer& ObjectInitializer);
    
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
    
	// Mettre à jour l'affichage du timer
	UFUNCTION()
	void UpdateTimer();
    
protected:
	// Éléments UI
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* TimerText;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UProgressBar* TimerProgressBar;
    
	// Référence au GameState
	UPROPERTY()
	class AWormGameState* WormGameState;
    
	// Durée totale du tour pour calculer le pourcentage
	float TurnDuration;
};