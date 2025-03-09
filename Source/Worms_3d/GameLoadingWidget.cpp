#include "GameLoadingWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

UGameLoadingWidget::UGameLoadingWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsActive = false;
}

void UGameLoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Set default values
    if (LoadingProgressBar)
    {
        LoadingProgressBar->SetPercent(0.0f);
    }
    
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("Loading game...")));
    }
    
    // Play loading animation if available
    if (LoadingAnimation)
    {
        PlayAnimation(LoadingAnimation, 0.0f, 0, EUMGSequencePlayMode::Forward, 1.0f);
    }
    
    // Play show animation if available
    if (ShowAnimation)
    {
        PlayAnimation(ShowAnimation);
    }
    
    // Block input
    if (GetOwningPlayer())
    {
        UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(GetOwningPlayer());
        GetOwningPlayer()->SetShowMouseCursor(false);
    }
    
    bIsActive = true;
    
    // Log for debugging
    UE_LOG(LogTemp, Warning, TEXT("Game loading widget constructed and active"));
}

void UGameLoadingWidget::NativeDestruct()
{
    Super::NativeDestruct();
    
    // Restore input mode to game and UI when the widget is removed
    if (GetOwningPlayer())
    {
        UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(GetOwningPlayer());
        GetOwningPlayer()->SetShowMouseCursor(true);
    }
    
    bIsActive = false;
    
    // Log for debugging
    UE_LOG(LogTemp, Warning, TEXT("Game loading widget destroyed"));
}

void UGameLoadingWidget::SetLoadingProgress(float Progress, const FString& NewStatusText)
{
    // Update progress bar
    if (LoadingProgressBar)
    {
        LoadingProgressBar->SetPercent(FMath::Clamp(Progress, 0.0f, 1.0f));
    }
    
    // Update status text
    if (StatusText && !NewStatusText.IsEmpty())
    {
        StatusText->SetText(FText::FromString(NewStatusText));
    }
}

void UGameLoadingWidget::SetStatusText(const FString& NewStatusText)
{
    // Update just the status text
    if (StatusText && !NewStatusText.IsEmpty())
    {
        StatusText->SetText(FText::FromString(NewStatusText));
    }
}

void UGameLoadingWidget::SetProgress(float NewProgress)
{
    // Update just the progress bar
    if (LoadingProgressBar)
    {
        LoadingProgressBar->SetPercent(FMath::Clamp(NewProgress, 0.0f, 1.0f));
    }
}

void UGameLoadingWidget::ShowLoadingScreen(float Duration)
{
    // Clear any existing dismiss timer
    if (GetWorld()->GetTimerManager().IsTimerActive(DismissTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(DismissTimerHandle);
    }
    
    // Set new timer if duration is greater than 0
    if (Duration > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            DismissTimerHandle,
            this,
            &UGameLoadingWidget::OnDismissTimerComplete,
            Duration,
            false
        );
        
        UE_LOG(LogTemp, Warning, TEXT("Loading screen will auto-dismiss in %.1f seconds"), Duration);
    }
    
    // Make the widget visible in case it wasn't
    if (!IsInViewport())
    {
        AddToViewport(9999); // High Z-order to ensure it's on top
    }
    
    // Block input
    if (GetOwningPlayer())
    {
        UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(GetOwningPlayer());
        GetOwningPlayer()->SetShowMouseCursor(false);
    }
    
    bIsActive = true;
    
    // Play show animation if available
    if (ShowAnimation)
    {
        PlayAnimation(ShowAnimation);
    }
}

void UGameLoadingWidget::DismissLoadingScreen()
{
    // Clear any existing dismiss timer
    if (GetWorld()->GetTimerManager().IsTimerActive(DismissTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(DismissTimerHandle);
    }
    
    // Call the blueprint event
    OnLoadingComplete();
    
    // Play hide animation if available
    if (HideAnimation)
    {
        PlayAnimation(HideAnimation);
        
        // Set a timer to remove the widget after the animation completes
        FTimerHandle RemovalTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            RemovalTimerHandle,
            [this]()
            {
                // Restore game input mode
                if (GetOwningPlayer())
                {
                    UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(GetOwningPlayer());
                    GetOwningPlayer()->SetShowMouseCursor(true);
                }
                
                RemoveFromParent();
                bIsActive = false;
                
                UE_LOG(LogTemp, Warning, TEXT("Loading screen dismissed after animation"));
            },
            HideAnimation->GetEndTime(),
            false
        );
        
        return;
    }
    
    // If no hide animation, remove immediately with a slight delay
    FTimerHandle RemovalTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        RemovalTimerHandle,
        [this]()
        {
            // Restore game input mode
            if (GetOwningPlayer())
            {
                UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(GetOwningPlayer());
                GetOwningPlayer()->SetShowMouseCursor(true);
            }
            
            RemoveFromParent();
            bIsActive = false;
            
            UE_LOG(LogTemp, Warning, TEXT("Loading screen dismissed"));
        },
        0.5f,
        false
    );
}

void UGameLoadingWidget::OnDismissTimerComplete()
{
    // Dismiss loading screen when timer completes
    DismissLoadingScreen();
}