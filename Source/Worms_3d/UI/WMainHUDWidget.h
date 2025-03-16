#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTeamStatusWidget.h"
#include "WTurnTimerWidget.h"
#include "WPlayerStatusWidget.h"
#include "WActiveCharacterInfoWidget.h"
#include "WMainHUDWidget.generated.h"

/**
 * Widget principal qui contient tous les autres widgets de l'UI
 */
UCLASS()
class WORMS_3D_API UWMainHUDWidget : public UUserWidget
{
	GENERATED_BODY()
    
public:
	UWMainHUDWidget(const FObjectInitializer& ObjectInitializer);
    
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
    
protected:
	// Les widgets enfants qui composent l'UI
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWTeamStatusWidget* TeamStatusWidget;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWTurnTimerWidget* TurnTimerWidget;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWPlayerStatusWidget* PlayerStatusWidget;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWActiveCharacterInfoWidget* ActiveCharacterInfoWidget;
    
	// Référence au GameState pour les mises à jour
	UPROPERTY()
	class AWormGameState* WormGameState;
};