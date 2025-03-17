#include "Worms_3d/UI/WCharacterStatusWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Worms_3d/AWormCharacter.h"
#include "WormGameState.h"


UWCharacterStatusWidget::UWCharacterStatusWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UWCharacterStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Initial update
    UpdateHealth();
}

void UWCharacterStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Update health bar if needed
    if (Character)
    {
        UpdateHealth();
    }
}

void UWCharacterStatusWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

void UWCharacterStatusWidget::SetCharacter(AWormCharacter* InCharacter)
{
    Character = InCharacter;
    
    // Update information
    if (Character)
    {
        UE_LOG(LogTemp, Verbose, TEXT("CharacterStatusWidget: Setting character to %s"), *Character->GetName());
        
        // Set character name
        if (CharacterNameText)
        {
            FString DisplayName = !Character->InGameName.IsEmpty() ? 
                Character->InGameName : Character->GetName();
                
            CharacterNameText->SetText(FText::FromString(DisplayName));
            UE_LOG(LogTemp, Verbose, TEXT("CharacterStatusWidget: Set name to %s"), *DisplayName);
        }
        
        // Set team color
        if (TeamColorImage)
        {
            // Get team color from GameState
            AWormGameState* GameState = Cast<AWormGameState>(GetWorld()->GetGameState());
            if (GameState && GameState->Teams.IsValidIndex(Character->TeamId))
            {
                FLinearColor TeamColor = GameState->Teams[Character->TeamId].TeamColor;
                TeamColorImage->SetColorAndOpacity(TeamColor);
                UE_LOG(LogTemp, Verbose, TEXT("CharacterStatusWidget: Set team color for team %d"), Character->TeamId);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("CharacterStatusWidget: Could not find team %d"), Character->TeamId);
                // Fallback color based on team ID
                FLinearColor FallbackColor;
                switch (Character->TeamId % 4)
                {
                    case 0: FallbackColor = FLinearColor::Blue; break;
                    case 1: FallbackColor = FLinearColor::Red; break;
                    case 2: FallbackColor = FLinearColor::Green; break;
                    case 3: FallbackColor = FLinearColor::Yellow; break;
                    default: FallbackColor = FLinearColor::White; break;
                }
                TeamColorImage->SetColorAndOpacity(FallbackColor);
            }
        }
    }
}

void UWCharacterStatusWidget::UpdateHealth()
{
    if (!Character || !HealthBar)
    {
        return;
    }
    
    // Update health bar
    float HealthPercent = Character->GetHealth() / Character->MaxHealth;
    HealthBar->SetPercent(HealthPercent);
    
    // Change color based on health
    FLinearColor HealthColor;
    if (HealthPercent > 0.6f)
    {
        HealthColor = FLinearColor::Green;
    }
    else if (HealthPercent > 0.3f)
    {
        HealthColor = FLinearColor::Yellow;
    }
    else
    {
        HealthColor = FLinearColor::Red;
    }
    HealthBar->SetFillColorAndOpacity(HealthColor);
}