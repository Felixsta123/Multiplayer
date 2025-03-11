#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WormGameState.h"
#include "W_GameResultsScreen.generated.h"

UCLASS()
class WORMS_3D_API UW_GameResultsScreen : public UUserWidget
{
	GENERATED_BODY()
    
public:
	virtual void NativeConstruct() override;
    
	UFUNCTION(BlueprintCallable, Category = "Results")
	void DisplayResults(const FString& Winner, const TArray<FPlayerDamageInfo>& DamageStats);
    
	// Legacy methods - converted to new OnRestart/OnReturnToMenu
	UFUNCTION(BlueprintCallable, Category = "Results")
	void RestartGame();
    
	UFUNCTION(BlueprintCallable, Category = "Results")
	void ReturnToMainMenu();
    
	// New button handlers for in-place restart
	UFUNCTION(BlueprintCallable, Category = "Results")
	void OnRestartClicked();
    
	UFUNCTION(BlueprintCallable, Category = "Results")
	void OnReturnToMenuClicked();
    
protected:
	// UI bindings - bind these in your BP widget 
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* WinnerText;
    
	UPROPERTY(meta = (BindWidget))
	class UVerticalBox* DamageContainer;
    
	UPROPERTY(meta = (BindWidget))
	class UButton* RestartButton;
    
	UPROPERTY(meta = (BindWidget))
	class UButton* MainMenuButton;
    
	// Animation played when showing results
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	class UWidgetAnimation* ShowAnimation;
    
	// Store the game results locally
	UPROPERTY()
	FString WinnerName;
    
	UPROPERTY()
	TArray<FPlayerDamageInfo> PlayerDamageDealt;
};