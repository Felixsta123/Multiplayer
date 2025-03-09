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
    LoadingScreenDuration = 5.0f;
    TitleText = FText::FromString(TEXT("LOADING GAME"));
}

void UW_GameLoadingScreen::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Initialize UI elements if they're bound in the UMG designer
    if (LoadingTitle)
    {
        LoadingTitle->SetText(TitleText);
    }
    
    if (LoadingProgress)
    {
        LoadingProgress->SetPercent(0.0f);
    }
    
    // Play show animation if available
    if (ShowAnimation)
    {
        PlayAnimation(ShowAnimation);
    }
    
    // Make sure the widget is fully opaque
    SetRenderOpacity(1.0f);
    
    // Start with the default status message
    UpdateStatusMessage(FText::FromString(TEXT("Initializing game...")));
    
    // Start auto-dismiss timer if duration is set
    if (LoadingScreenDuration > 0.0f && bAutoDismiss)
    {
        GetWorld()->GetTimerManager().SetTimer(
            DismissTimerHandle,
            this,
            &UW_GameLoadingScreen::HandleDismissed,
            LoadingScreenDuration,
            false
        );
    }
}

void UW_GameLoadingScreen::UpdateProgress(float ProgressPercent)
{
    // Update progress bar if it exists
    if (LoadingProgress)
    {
        LoadingProgress->SetPercent(FMath::Clamp(ProgressPercent, 0.0f, 1.0f));
    }
}

void UW_GameLoadingScreen::UpdateStatusMessage(const FText& NewStatus)
{
    // Update status text if it exists
    if (StatusText)
    {
        StatusText->SetText(NewStatus);
    }
}

void UW_GameLoadingScreen::DismissLoadingScreen()
{
    // Clear any existing auto-dismiss timer
    if (GetWorld()->GetTimerManager().IsTimerActive(DismissTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(DismissTimerHandle);
    }
    
    // Play hide animation if available
    if (HideAnimation)
    {
        PlayAnimationForward(HideAnimation);
        
        // Set up a timer to remove from parent after animation completes
        FTimerHandle RemoveTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            RemoveTimerHandle,
            this,
            &UW_GameLoadingScreen::HandleDismissed,
            HideAnimation->GetEndTime(),
            false
        );
    }
    else
    {
        // No animation, just handle dismissed immediately
        HandleDismissed();
    }
}

void UW_GameLoadingScreen::HandleDismissed()
{
    // Call the blueprint event
    OnLoadingScreenDismissed.Broadcast();
    
    // Remove from parent
    RemoveFromParent();
}