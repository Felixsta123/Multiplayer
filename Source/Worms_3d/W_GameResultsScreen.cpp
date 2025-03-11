// W_GameResultsScreen.cpp
#include "W_GameResultsScreen.h"

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
        RestartButton->OnClicked.AddDynamic(this, &UW_GameResultsScreen::RestartGame);
    }
    
    if (MainMenuButton)
    {
        MainMenuButton->OnClicked.AddDynamic(this, &UW_GameResultsScreen::ReturnToMainMenu);
    }
    
    // Get game results from the World
    // This will depend on how you pass data between levels
    // You could use GameInstance or temporary UObject to store results
}

void UW_GameResultsScreen::DisplayResults(const FString& WinnerName, const TArray<FPlayerDamageInfo>& DamageStats)
{
    // Display winner
    if (WinnerText)
    {
        WinnerText->SetText(FText::FromString(FString::Printf(TEXT("Winner: %s"), *WinnerName)));
    }
    
    // Display damage stats
    if (DamageStatsContainer && DamageStatEntryClass)
    {
        // Clear previous entries
        DamageStatsContainer->ClearChildren();
        
        // Sort players by damage
        TArray<FPlayerDamageInfo> SortedStats = DamageStats;
        SortedStats.Sort([](const FPlayerDamageInfo& A, const FPlayerDamageInfo& B) {
            return A.DamageValue > B.DamageValue;
        });
        
        // Create entries for each player
        for (const auto& Stat : SortedStats)
        {
            UUserWidget* Entry = CreateWidget<UUserWidget>(this, DamageStatEntryClass);
            if (Entry)
            {
                // Set damage stat data - this assumes your entry widget has these properties
                UTextBlock* NameText = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("PlayerNameText")));
                UTextBlock* DamageText = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("DamageText")));
                
                if (NameText)
                {
                    NameText->SetText(FText::FromString(Stat.PlayerName));
                }
                
                if (DamageText)
                {
                    DamageText->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Stat.DamageValue)));
                }
                
                DamageStatsContainer->AddChild(Entry);
            }
        }
    }
}

void UW_GameResultsScreen::RestartGame()
{
    // Restart the game
    UGameplayStatics::OpenLevel(GetWorld(), FName(*UGameplayStatics::GetCurrentLevelName(GetWorld())));
}

void UW_GameResultsScreen::ReturnToMainMenu()
{
    // Return to main menu
    UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("MainMenu")));
}