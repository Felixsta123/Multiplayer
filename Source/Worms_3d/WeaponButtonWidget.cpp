// WeaponButtonWidget.cpp
#include "WeaponButtonWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "WeaponWheelWidget.h"
#include "AWormCharacter.h"

UWeaponButtonWidget::UWeaponButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponIndex = -1;
	ParentWheel = nullptr;
}

void UWeaponButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
    
	// Bind button events
	if (WeaponButton)
	{
		WeaponButton->OnClicked.AddDynamic(this, &UWeaponButtonWidget::OnButtonClicked);
		WeaponButton->OnHovered.AddDynamic(this, &UWeaponButtonWidget::OnButtonHovered);
	}
}

void UWeaponButtonWidget::SetupWeaponButton(int32 InWeaponIndex, const FWeaponUIData& InWeaponData, UWeaponWheelWidget* InParentWheel)
{
	WeaponIndex = InWeaponIndex;
	WeaponData = InWeaponData;
	ParentWheel = InParentWheel;
    
	// Set up the button visuals
	if (WeaponIcon && WeaponData.Icon)
	{
		WeaponIcon->SetBrushFromTexture(WeaponData.Icon);
	}
    
	if (WeaponName)
	{
		WeaponName->SetText(FText::FromString(WeaponData.Name));
	}
}

void UWeaponButtonWidget::SetHighlighted(bool bHighlighted)
{
	if (WeaponButton)
	{
		// Scale the button up when highlighted
		SetRenderScale(bHighlighted ? FVector2D(1.2f, 1.2f) : FVector2D(1.0f, 1.0f));
        
		// You could also change colors or other visual properties here
	}
}

void UWeaponButtonWidget::OnButtonClicked()
{
	if (ParentWheel)
	{
		ParentWheel->SelectWeapon(WeaponIndex);
	}
}

void UWeaponButtonWidget::OnButtonHovered()
{
    if (ParentWheel)
    {
        UE_LOG(LogTemp, Warning, TEXT("Button %d directly hovered"), WeaponIndex);
        ParentWheel->SetHighlightedWeapon(WeaponIndex);
    }
}

// void UWeaponButtonWidget::OnButtonHovered()
// {
// 	if (ParentWheel)
// 	{
// 		ParentWheel->SetHighlightedWeapon(WeaponIndex);
// 	}
// }