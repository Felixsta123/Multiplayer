#include "Worms_3d/UI/WTeamStatusWidget.h"
#include "Worms_3d/UI/WCharacterStatusWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/VerticalBox.h"
#include "WormGameState.h"

UWTeamStatusWidget::UWTeamStatusWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UWTeamStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Get GameState
    WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    
    if (WormGameState)
    {
        // Subscribe to team status update event
        WormGameState->OnTeamStatusUpdated.AddDynamic(this, &UWTeamStatusWidget::UpdateTeamStatus);
        
        // Also subscribe to active player changes since that can affect team display
        WormGameState->OnActivePlayerChanged.AddDynamic(this, &UWTeamStatusWidget::UpdateTeamStatus);
        
        // Initial update
        UpdateTeamStatus();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WTeamStatusWidget: GameState not found"));
    }
}

void UWTeamStatusWidget::NativeDestruct()
{
    // Unsubscribe from events
    if (WormGameState)
    {
        WormGameState->OnTeamStatusUpdated.RemoveDynamic(this, &UWTeamStatusWidget::UpdateTeamStatus);
        WormGameState->OnActivePlayerChanged.RemoveDynamic(this, &UWTeamStatusWidget::UpdateTeamStatus);
    }
    
    Super::NativeDestruct();
}

void UWTeamStatusWidget::UpdateTeamStatus()
{
    if (!WormGameState || !TeamsContainer || !CharacterStatusWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTeamStatusWidget: Missing resources for UpdateTeamStatus"));
        return;
    }
    
    // Log current state for debugging
    UE_LOG(LogTemp, Warning, TEXT("TeamStatusWidget: Updating teams, count=%d"), WormGameState->Teams.Num());
    
    // Clear current container
    TeamsContainer->ClearChildren();
    
    // Process all teams
    for (const FTeamInfo& Team : WormGameState->Teams)
    {
        UE_LOG(LogTemp, Warning, TEXT("TeamStatusWidget: Processing Team %d with %d members"), 
            Team.TeamId, Team.TeamMembers.Num());
            
        // Create a widget for each character in the team
        for (AWormCharacter* Character : Team.TeamMembers)
        {
            if (Character)
            {
                UE_LOG(LogTemp, Verbose, TEXT("TeamStatusWidget: Creating status for %s"), *Character->GetName());
                
                UWCharacterStatusWidget* CharWidget = CreateCharacterStatusWidget(Character);
                if (CharWidget)
                {
                    TeamsContainer->AddChild(CharWidget);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("TeamStatusWidget: NULL character in team %d"), Team.TeamId);
            }
        }
    }
}

UWCharacterStatusWidget* UWTeamStatusWidget::CreateCharacterStatusWidget(AWormCharacter* Character)
{
    if (!Character || !CharacterStatusWidgetClass)
    {
        return nullptr;
    }
    
    // Create widget instance
    UWCharacterStatusWidget* Widget = CreateWidget<UWCharacterStatusWidget>(this, CharacterStatusWidgetClass);
    if (Widget)
    {
        // Initialize widget with character
        Widget->SetCharacter(Character);
    }
    
    return Widget;
}