// WeaponWheelWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponUIData.h"
#include "Engine/DataTable.h"
#include "WeaponWheelWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UImage;
class AWormCharacter;
class UWeaponButtonWidget;
class AWormWeapon; // Add forward declaration for AWormWeapon

/**
 * Widget for the weapon selection wheel
 */
UCLASS()
class WORMS_3D_API UWeaponWheelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UWeaponWheelWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Grid", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxButtonsPerRow = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Grid", meta = (ClampMin = "0", ClampMax = "100"))
    float ButtonPadding = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Grid")
    FVector2D ButtonSize = FVector2D(100.0f, 100.0f);

    // Grid Position Adjustments
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Grid")
    float GridOffsetX = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Grid")
    float GridOffsetY = 0.0f;
    
    // Create the weapon wheel buttons
    void CreateWeaponWheel();
    
    // Set the highlighted weapon
    void SetHighlightedWeapon(int32 WeaponIndex);
    
    // Select a weapon and close the wheel
    void SelectWeapon(int32 WeaponIndex);
    
    // Set the data table to use for weapon UI data
    void SetWeaponDataTable(UDataTable* InDataTable) { WeaponDataTable = InDataTable; }

protected:
    // The canvas panel containing the buttons
    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* WheelCanvas;
    
    // The background image
    UPROPERTY(meta = (BindWidget))
    UImage* WheelBackground;
    
    // The text displaying the selected weapon name
    UPROPERTY(meta = (BindWidget))
    UTextBlock* SelectedWeaponName;
    
    // The text displaying the weapon description
    UPROPERTY(meta = (BindWidget))
    UTextBlock* WeaponDescription;
    
    // Array of weapon buttons
    UPROPERTY()
    TArray<UWeaponButtonWidget*> WeaponButtons;
    
    // The currently highlighted weapon index
    UPROPERTY()
    int32 HighlightedIndex;
    
    // The owning character
    UPROPERTY()
    AWormCharacter* OwningCharacter;
    
    // Data table containing weapon UI data
    UPROPERTY()
    UDataTable* WeaponDataTable;
    
    // Radius of the wheel
    UPROPERTY(EditAnywhere, Category = "Appearance")
    float WheelRadius = 200.0f;
    
    // Button class to use
    UPROPERTY(EditAnywhere, Category = "Appearance")
    TSubclassOf<UWeaponButtonWidget> WeaponButtonClass;
    
    // Calculate weapon index from mouse position
    int32 GetWeaponIndexFromMousePosition(FVector2D MousePosition);
    
    // Load weapon data from data table
    FWeaponUIData GetWeaponUIData(TSubclassOf<AWormWeapon> WeaponClass);
    
    // Update center display with weapon info
    void UpdateCenterDisplay();

    
};