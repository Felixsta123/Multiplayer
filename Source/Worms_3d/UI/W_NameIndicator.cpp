#include "W_NameIndicator.h"
void UWNameIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
    
	// Set default values
	if (TeamInfo)
	{
		TeamInfo->SetText(FText::FromString(TEXT("Team")));
	}
    
	if (PlayerName)
	{
		PlayerName->SetText(FText::FromString(TEXT("Character")));
	}
}

void UWNameIndicatorWidget::SetNameInfo(int32 InTeamId, const FString& InCharacterName)
{
	// Set team info with color
	if (TeamInfo)
	{
		FString TeamText = FString::Printf(TEXT("[Team %d]"), InTeamId + 1);
		TeamInfo->SetText(FText::FromString(TeamText));
        
		// Set team color
		FLinearColor TeamColor = GetTeamColor(InTeamId);
		TeamInfo->SetColorAndOpacity(FSlateColor(TeamColor));
	}
    
	// Set character name
	if (PlayerName)
	{
		PlayerName->SetText(FText::FromString(InCharacterName));
	}
}

FLinearColor UWNameIndicatorWidget::GetTeamColor(int32 TeamId) const
{
	switch (TeamId)
	{
	case 0: return FLinearColor(0.0f, 0.5f, 1.0f); // Blue
	case 1: return FLinearColor(1.0f, 0.2f, 0.2f); // Red
	case 2: return FLinearColor(0.2f, 0.8f, 0.2f); // Green
	case 3: return FLinearColor(1.0f, 0.8f, 0.0f); // Yellow
	default: return FLinearColor(0.7f, 0.7f, 0.7f); // Gray
	}
}