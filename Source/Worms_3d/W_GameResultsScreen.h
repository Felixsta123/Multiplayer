#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_GameResultsScreen.generated.h"

UCLASS()
class WORMS_3D_API UW_GameResultsScreen : public UUserWidget
{
	GENERATED_BODY()
    
public:
	virtual void NativeConstruct() override;
    
	UFUNCTION(BlueprintCallable, Category = "Results")
	void DisplayResults(const FString& WinnerName, const TArray<FPlayerDamageInfo>& DamageStats);
	UFUNCTION(BlueprintCallable, Category = "Results")
	void RestartGame();
    
	UFUNCTION(BlueprintCallable, Category = "Results")
	void ReturnToMainMenu();
    
protected:
	// UI bindings - bind these in your BP widget 
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* WinnerText;
    
	UPROPERTY(meta = (BindWidget))
	class UVerticalBox* DamageStatsContainer;
    
	UPROPERTY(meta = (BindWidget))
	class UButton* RestartButton;
    
	UPROPERTY(meta = (BindWidget))
	class UButton* MainMenuButton;
    
	// Reference to the damage stat entry widget class
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UUserWidget> DamageStatEntryClass;
};