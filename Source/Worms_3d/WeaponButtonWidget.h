// WeaponButtonWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponUIData.h"
#include "Components/Overlay.h"
#include "WeaponButtonWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWeaponWheelWidget;

/**
 * Widget for a single weapon button in the weapon wheel
 */
UCLASS()
class WORMS_3D_API UWeaponButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWeaponButtonWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
    
	// Initialize the button with data (renamed to avoid conflict with UUserWidget::Initialize)
	void SetupWeaponButton(int32 InWeaponIndex, const FWeaponUIData& InWeaponData, UWeaponWheelWidget* InParentWheel);
    
	// Set the button's highlighted state
	void SetHighlighted(bool bHighlighted);
    
	// Get the weapon index
	int32 GetWeaponIndex() const { return WeaponIndex; };

	UFUNCTION(BlueprintCallable, Category = "Button")
	void OnButtonHovered();
	
protected:
	// The button component
	UPROPERTY(meta = (BindWidget))
	UButton* WeaponButton;
    
	// Container for icon and text
	UPROPERTY(meta = (BindWidget))
	UOverlay* ButtonContent;  // Or UVerticalBox
    
	// The icon image
	UPROPERTY(meta = (BindWidget))
	UImage* WeaponIcon;
    
	// The weapon name text
	UPROPERTY(meta = (BindWidget))
	UTextBlock* WeaponName;
    
	// The weapon index in the character's weapon array
	UPROPERTY()
	int32 WeaponIndex;
    
	// The weapon data
	UPROPERTY()
	FWeaponUIData WeaponData;
    
	// Reference to parent wheel widget
	UPROPERTY()
	UWeaponWheelWidget* ParentWheel;
    
	// Called when button is clicked
	UFUNCTION()
	void OnButtonClicked();
    
	// // Called when button is hovered
	// UFUNCTION()
	// void OnButtonHovered();
};