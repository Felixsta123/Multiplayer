#include "Worms_3d/UI/WMainHUDWidget.h"
#include "Worms_3d/UI/WTeamStatusWidget.h"
#include "Worms_3d/UI/WTurnTimerWidget.h"
#include "Worms_3d/UI/WPlayerStatusWidget.h"
#include "Worms_3d/UI/WActiveCharacterInfoWidget.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"

UWMainHUDWidget::UWMainHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Rien à initialiser ici
}

void UWMainHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
    
	// Récupérer le GameState pour les mises à jour
	WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    
	// Vérifions que tous les widgets enfants sont correctement liés
	if (!TeamStatusWidget || !TurnTimerWidget || !PlayerStatusWidget || !ActiveCharacterInfoWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("WMainHUDWidget: Un ou plusieurs widgets enfants ne sont pas liés correctement"));
	}
    
	UE_LOG(LogTemp, Log, TEXT("WMainHUDWidget construit avec succès"));
}

void UWMainHUDWidget::NativeDestruct()
{
	// Nettoyer les références
	WormGameState = nullptr;
    
	Super::NativeDestruct();
}