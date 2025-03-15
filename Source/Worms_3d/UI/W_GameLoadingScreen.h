#pragma once

#include "CoreMinimal.h"
#include "Worms_3d/Init/GameLoadingWidget.h"
#include "W_GameLoadingScreen.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadingScreenDismissed);

/**
 * Blueprint-friendly implementation of the game loading screen widget
 */
UCLASS()
class WORMS_3D_API UW_GameLoadingScreen : public UGameLoadingWidget
{
    GENERATED_BODY()
    
public:
    UW_GameLoadingScreen(const FObjectInitializer& ObjectInitializer);
    
    // Called when this widget is constructed
    virtual void NativeConstruct() override;
    
    // Update loading progress
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void UpdateProgress(float ProgressPercent);
    
    // Update status message
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void UpdateStatusMessage(const FText& NewStatus);
    
    // Event fired when loading screen is dismissed
    UPROPERTY(BlueprintAssignable, Category = "Loading")
    FOnLoadingScreenDismissed OnLoadingScreenDismissed;
    
    // Duration before auto-dismissing the loading screen (0 for no auto-dismiss)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
    float LoadingScreenDuration;
    
    // Whether to automatically dismiss the loading screen after duration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
    bool bAutoDismiss = true;
    
    // Title text to display
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
    FText TitleText;
    // Whether this loading screen is network synchronized
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
    bool bIsNetworkSynchronized = true;
    
protected:
    // UI Elements (bind these in the UMG designer)
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* LoadingTitle;
    
    UPROPERTY(meta = (BindWidget))
    class UImage* BackgroundImage;
    
    UPROPERTY(meta = (BindWidget))
    class UThrobber* LoadingThrobber;

    // Timer for auto-dismiss
    FTimerHandle DismissTimerHandle;
    
    // Called when the loading screen should be removed
    UFUNCTION()
    void HandleDismissed();
};    