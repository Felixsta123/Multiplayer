#include "Worms_3d/UI/WTurnTimerWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"

UWTurnTimerWidget::UWTurnTimerWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Valeur par défaut
    TurnDuration = 30.0f;
}

void UWTurnTimerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Récupérer le GameState
    WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    
    if (WormGameState)
    {
        // Stocker la durée totale du tour
        TurnDuration = WormGameState->TurnDuration;
        
        // S'abonner à l'événement de mise à jour du timer
        WormGameState->OnTurnTimerUpdated.AddDynamic(this, &UWTurnTimerWidget::UpdateTimer);
        
        // Mise à jour initiale
        UpdateTimer();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WTurnTimerWidget: GameState non trouvé"));
    }
}

void UWTurnTimerWidget::NativeDestruct()
{
    // Se désabonner des événements
    if (WormGameState)
    {
        WormGameState->OnTurnTimerUpdated.RemoveDynamic(this, &UWTurnTimerWidget::UpdateTimer);
    }
    
    Super::NativeDestruct();
}

void UWTurnTimerWidget::UpdateTimer()
{
    if (!WormGameState)
    {
        return;
    }
    
    // Mettre à jour le texte du timer
    if (TimerText)
    {
        int32 Seconds = FMath::FloorToInt(WormGameState->RemainingTurnTime);
        FString TimerString = FString::Printf(TEXT("%d"), Seconds);
        TimerText->SetText(FText::FromString(TimerString));
    }
    
    // Mettre à jour la barre de progression
    if (TimerProgressBar && TurnDuration > 0)
    {
        float ProgressPercent = WormGameState->RemainingTurnTime / TurnDuration;
        TimerProgressBar->SetPercent(ProgressPercent);
        
        // Changer la couleur en fonction du temps restant
        FLinearColor TimerColor;
        if (ProgressPercent > 0.5f)
        {
            TimerColor = FLinearColor::Green;
        }
        else if (ProgressPercent > 0.25f)
        {
            TimerColor = FLinearColor::Yellow;
        }
        else
        {
            TimerColor = FLinearColor::Red;
        }
        
        TimerProgressBar->SetFillColorAndOpacity(TimerColor);
    }
}