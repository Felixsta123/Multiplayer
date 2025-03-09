#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameLoadingWidget.generated.h"

/**
 * Widget displayed during game initialization to block input and prevent early interactions
 */
UCLASS()
class WORMS_3D_API UGameLoadingWidget : public UUserWidget
{
    GENERATED_BODY()
    
public:
    UGameLoadingWidget(const FObjectInitializer& ObjectInitializer);
    
    // Called when added to viewport
    virtual void NativeConstruct() override;
    
    // Called when removed from viewport
    virtual void NativeDestruct() override;
    
    // Updates the loading progress
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void SetLoadingProgress(float Progress, const FString& StatusText);
    
    // Displays the loading screen for a specified duration
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void ShowLoadingScreen(float Duration);
    
    // Dismiss the loading screen
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void DismissLoadingScreen();
    
    // Event called when loading is complete (can be overridden in Blueprints)
    UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
    void OnLoadingComplete();
    
    // Whether the loading widget is currently active
    UFUNCTION(BlueprintPure, Category = "Loading")
    bool IsActive() const { return bIsActive; }
    
protected:
    // Text block for status message
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* StatusText;
    
    // Progress bar for loading progress
    UPROPERTY(meta = (BindWidget))
    class UProgressBar* LoadingProgressBar;
    
    // Animation for the loading screen
    UPROPERTY(Transient, meta = (BindWidgetAnim))
    class UWidgetAnimation* LoadingAnimation;
    
    // Timer handle for auto-dismiss
    FTimerHandle DismissTimerHandle;
    
    // Flag indicating if the loading screen is active
    bool bIsActive;
    
    // Called when the dismiss timer expires
    UFUNCTION()
    void OnDismissTimerComplete();
};