// WTutorialWidget.cpp - Enhanced implementation

#include "WTutorialWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/WidgetAnimation.h"

void UWTutorialWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Initialize default values
    if (InstructionText)
    {
        InstructionText->SetText(FText::FromString("Welcome to the Worms 3D Tutorial! Follow the instructions to learn the basics."));
    }
    
    if (ObjectiveText)
    {
        ObjectiveText->SetText(FText::FromString("Current Objective: Move using WASD keys"));
    }
    
    if (ProgressBar)
    {
        ProgressBar->SetPercent(0.0f);
    }
    
}

void UWTutorialWidget::SetInstructionText(const FString& NewText)
{
    if (InstructionText)
    {
        InstructionText->SetText(FText::FromString(NewText));
    }
    
    // Update objective text to match with "Current Objective: " prefix
    if (ObjectiveText)
    {
        ObjectiveText->SetText(FText::FromString(FString::Printf(TEXT("Current Objective: %s"), *NewText)));
    }
    
    // Play text change animation if available
    if (TextChangeAnimation)
    {
        PlayAnimation(TextChangeAnimation);
    }
}

void UWTutorialWidget::ShowObjectiveComplete()
{
    // Play animation if available
    if (ObjectiveCompleteAnimation)
    {
        PlayAnimation(ObjectiveCompleteAnimation);
    }
    
    // Update objective text
    if (ObjectiveText)
    {
        ObjectiveText->SetText(FText::FromString("Objective Complete!"));
    }
    
    // Show checkmark or success image if available
    if (SuccessImage)
    {
        SuccessImage->SetVisibility(ESlateVisibility::Visible);
        
        // Hide it after a delay
        FTimerHandle HideSuccessImageTimer;
        GetWorld()->GetTimerManager().SetTimer(
            HideSuccessImageTimer,
            [this]()
            {
                if (SuccessImage)
                {
                    SuccessImage->SetVisibility(ESlateVisibility::Hidden);
                }
            },
            2.0f,
            false
        );
    }
}

void UWTutorialWidget::UpdateProgressIndicator(int32 CurrentStep, int32 TotalSteps)
{
    if (ProgressBar)
    {
        float Progress = FMath::Clamp((float)CurrentStep / (float)TotalSteps, 0.0f, 1.0f);
        ProgressBar->SetPercent(Progress);
    }
    
    // Update step text if available
    if (StepText)
    {
        StepText->SetText(FText::FromString(FString::Printf(TEXT("Step %d of %d"), CurrentStep + 1, TotalSteps + 1)));
    }
}

void UWTutorialWidget::ShowTutorialComplete()
{
    // Play completion animation if available
    if (TutorialCompleteAnimation)
    {
        PlayAnimation(TutorialCompleteAnimation);
    }
    
    // Update text
    if (InstructionText)
    {
        InstructionText->SetText(FText::FromString("Tutorial Complete! You're now ready to play Worms 3D!"));
    }
    
    if (ObjectiveText)
    {
        ObjectiveText->SetText(FText::FromString("Congratulations!"));
    }
    

}
