#include "Worms_3d/UI/WPlayerStatusWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"

UWPlayerStatusWidget::UWPlayerStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Rien de spécial à initialiser
}

void UWPlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
    
	// Récupérer le GameState
	WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    
	if (WormGameState)
	{
		// S'abonner à l'événement de changement de joueur actif
		WormGameState->OnActivePlayerChanged.AddDynamic(this, &UWPlayerStatusWidget::UpdatePlayerStatus);
        
		// Mise à jour initiale
		UpdatePlayerStatus();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("WPlayerStatusWidget: GameState non trouvé"));
	}
}

void UWPlayerStatusWidget::NativeDestruct()
{
	// Se désabonner des événements
	if (WormGameState)
	{
		WormGameState->OnActivePlayerChanged.RemoveDynamic(this, &UWPlayerStatusWidget::UpdatePlayerStatus);
	}
    
	Super::NativeDestruct();
}

void UWPlayerStatusWidget::UpdatePlayerStatus()
{
	if (!WormGameState)
	{
		return;
	}
    
	bool bIsPlayerTurn = WormGameState->IsLocalPlayerTurn();
    
	// Mettre à jour le texte
	if (StatusText)
	{
		FText StatusMessage = bIsPlayerTurn 
			? FText::FromString("C'EST VOTRE TOUR !") 
			: FText::FromString("En attente...");
        
		StatusText->SetText(StatusMessage);
	}
    
	// Mettre à jour la couleur de fond
	if (StatusBackground)
	{
		StatusBackground->SetColorAndOpacity(bIsPlayerTurn ? ActiveColor : WaitingColor);
	}
    
	// Log pour debug
	UE_LOG(LogTemp, Verbose, TEXT("WPlayerStatusWidget: Statut mis à jour - IsPlayerTurn: %s"), 
		bIsPlayerTurn ? TEXT("true") : TEXT("false"));
}