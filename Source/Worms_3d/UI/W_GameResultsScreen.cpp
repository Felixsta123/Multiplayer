#include "W_GameResultsScreen.h"

#include "WormGameMode.h"
#include "WormGameState.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

void UW_GameResultsScreen::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Bind button events
    if (RestartButton)
    {
        RestartButton->OnClicked.AddDynamic(this, &UW_GameResultsScreen::OnRestartClicked);
    }
    
    if (MainMenuButton)
    {
        MainMenuButton->OnClicked.AddDynamic(this, &UW_GameResultsScreen::OnReturnToMenuClicked);
    }
}

void UW_GameResultsScreen::DisplayResults(const FString& Winner, const TArray<FPlayerDamageInfo>& DamageStats)
{
    // Store the results locally for use in the widget
    WinnerName = Winner;
    PlayerDamageDealt = DamageStats;
    
    // Update the winner text
    if (WinnerText)
    {
        WinnerText->SetText(FText::FromString(FString::Printf(TEXT("Winner: %s"), *WinnerName)));
    }
    
    // Clear any existing damage entries
    if (DamageContainer)
    {
        DamageContainer->ClearChildren();
        
        // Sort damage stats by value (highest first)
        TArray<FPlayerDamageInfo> SortedStats = PlayerDamageDealt;
        SortedStats.Sort([](const FPlayerDamageInfo& A, const FPlayerDamageInfo& B) {
            return A.DamageValue > B.DamageValue;
        });
        
        // Create simple text entries for each player's damage
        for (const FPlayerDamageInfo& DamageInfo : SortedStats)
        {
            UTextBlock* DamageText = NewObject<UTextBlock>(this);
            if (DamageText)
            {
                FString DamageString = FString::Printf(TEXT("%s - %.1f damage"), 
                    *DamageInfo.PlayerName, DamageInfo.DamageValue);
                DamageText->SetText(FText::FromString(DamageString));
                DamageContainer->AddChild(DamageText);
            }
        }
    }
    
    // Play show animation if available
    if (ShowAnimation)
    {
        PlayAnimation(ShowAnimation);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Game results displayed: Winner=%s, Damage Stats=%d entries"), 
        *WinnerName, PlayerDamageDealt.Num());
}

// Legacy method - keeping for compatibility
void UW_GameResultsScreen::RestartGame()
{
    OnRestartClicked();
}

// Legacy method - keeping for compatibility
void UW_GameResultsScreen::ReturnToMainMenu()
{
    OnReturnToMenuClicked();
}

void UW_GameResultsScreen::OnRestartClicked()
{
    // Get game mode to handle restart
    AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    
    if (GameMode && GameMode->GameInitManager)
    {
        // First remove this widget
        RemoveFromParent();
        
        // Unpause the game
        UGameplayStatics::SetGamePaused(GetWorld(), false);
        
        // Reset game and start initialization sequence
        GameMode->StartRestartSequence();
    }
}

void UW_GameResultsScreen::OnReturnToMenuClicked()
{
    // Get player controller
    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        // First remove this widget
        RemoveFromParent();
        
        // Unpause the game
        UGameplayStatics::SetGamePaused(GetWorld(), false);
        
        // Return to main menu
        UGameplayStatics::OpenLevel(GetWorld(), FName("MainMenuMap"));
    }
}