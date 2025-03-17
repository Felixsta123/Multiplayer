#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTutorialWidget.generated.h"

/**
 * Widget for displaying tutorial instructions
 */
UCLASS()
class WORMS_3D_API UWTutorialWidget : public UUserWidget
{
	GENERATED_BODY()
    
public:
	virtual void NativeConstruct() override;
    
	// Update instruction text
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SetInstructionText(const FString& NewText);
    
	// Show objective complete message
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void ShowObjectiveComplete();
    
	// Update progress indicators
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void UpdateProgressIndicator(int32 CurrentStep, int32 TotalSteps);
    
protected:
	// UI Elements (to be bound in Blueprint)
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* InstructionText;
    
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ObjectiveText;
    
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* ProgressBar;
    
	// Animation for when objective completes
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	class UWidgetAnimation* ObjectiveCompleteAnimation;
};