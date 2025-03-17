#include "WTutorialWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

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
}

void UWTutorialWidget::UpdateProgressIndicator(int32 CurrentStep, int32 TotalSteps)
{
	if (ProgressBar)
	{
		float Progress = FMath::Clamp((float)CurrentStep / (float)TotalSteps, 0.0f, 1.0f);
		ProgressBar->SetPercent(Progress);
	}
}