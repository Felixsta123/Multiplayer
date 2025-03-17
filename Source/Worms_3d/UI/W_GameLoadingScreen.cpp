#include "W_GameLoadingScreen.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Throbber.h"

UW_GameLoadingScreen::UW_GameLoadingScreen(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Default values
    LoadingScreenDuration = 10.0f;
    TitleText = FText::FromString(TEXT("LOADING GAME"));
    bIsNetworkSynchronized = true;
    bAutoDismiss = false; // Changed to false - we will rely on network coordination
}

void UW_GameLoadingScreen::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Initialize UI elements if they're bound in the UMG designer
    if (LoadingTitle)
    {
        LoadingTitle->SetText(TitleText);
    }
    // Play show animation if available
    if (ShowAnimation)
    {
        PlayAnimation(ShowAnimation);
    }
    if (LoadingTitle)
    {
        LoadingTitle->SetText(TitleText);
    }
    
    if (LoadingThrobber)
    {
        LoadingThrobber->SetVisibility(ESlateVisibility::Visible);
    }
    // Make sure the widget is fully opaque
    SetRenderOpacity(1.0f);
    
    // Start with the default status message
    UpdateStatusMessage(FText::FromString(TEXT("Initializing game...")));
    
    // Start auto-dismiss timer if duration is set AND NOT network synchronized
    if (LoadingScreenDuration > 0.0f && bAutoDismiss && !bIsNetworkSynchronized)
    {
        GetWorld()->GetTimerManager().SetTimer(
            DismissTimerHandle,
            this,
            &UW_GameLoadingScreen::HandleDismissed,
            LoadingScreenDuration,
            false
        );
    }
    
    UE_LOG(LogTemp, Log, TEXT("W_GameLoadingScreen constructed - Network synchronized: %s"), 
           bIsNetworkSynchronized ? TEXT("Yes") : TEXT("No"));
}

void UW_GameLoadingScreen::UpdateProgress(float ProgressPercent)
{
    // Call base method to update progress bar
    SetProgress(ProgressPercent);
}


void UW_GameLoadingScreen::UpdateStatusMessage(const FText& NewStatus)
{
    // Update status text if it exists
    SetStatusText(NewStatus.ToString());
}

void UW_GameLoadingScreen::HandleDismissed()
{
    // Call the blueprint event
    UE_LOG(LogTemp, Log, TEXT("Loading screen auto-dismissed after %.1f seconds"), LoadingScreenDuration);
    OnLoadingScreenDismissed.Broadcast();
    
    // Remove from parent
    RemoveFromParent();
}