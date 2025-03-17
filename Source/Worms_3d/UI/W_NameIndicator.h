#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "W_NameIndicator.generated.h"

/**
 * Widget that displays a character's team and name above their head
 */
UCLASS()
class WORMS_3D_API UWNameIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()
    
public:
	// Function to set the widget's displayed information
	UFUNCTION(BlueprintCallable, Category = "Name Indicator")
	void SetNameInfo(int32 InTeamId, const FString& InCharacterName);
    
protected:
	virtual void NativeConstruct() override;
    
	// Text blocks for displaying info
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* TeamInfo;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* PlayerName;
    
	// Get color for team
	FLinearColor GetTeamColor(int32 TeamId) const;
};