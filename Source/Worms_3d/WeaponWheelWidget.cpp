// WeaponWheelWidget.cpp
#include "WeaponWheelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "AWormCharacter.h"
#include "WeaponButtonWidget.h"
#include "WormWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

UWeaponWheelWidget::UWeaponWheelWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    HighlightedIndex = -1;
    OwningCharacter = nullptr;
    WeaponDataTable = nullptr;
}

void UWeaponWheelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Get the owning character
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        OwningCharacter = Cast<AWormCharacter>(PC->GetPawn());
    }
    
    if (OwningCharacter)
    {
        // Create the weapon wheel
        CreateWeaponWheel();
        
        // Set the initial highlighted weapon to the current weapon if it's accessible
        // Let's use a public method to get the current weapon index
        if (OwningCharacter)
        {
            if (OwningCharacter->AvailableWeapons.Num() > 0)
            {
                SetHighlightedWeapon(0); // Default to first weapon
            }
        }
    }
}

void UWeaponWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // You could add animation or other continuous updates here
}

void UWeaponWheelWidget::CreateWeaponWheel()
{
    UE_LOG(LogTemp, Warning, TEXT("CreateWeaponWheel with grid layout on right side"));
    
    // Clear any existing buttons
    for (UWeaponButtonWidget* Button : WeaponButtons)
    {
        if (Button)
        {
            Button->RemoveFromParent();
        }
    }
    WeaponButtons.Empty();
    
    if (!OwningCharacter || !WheelCanvas || !WeaponButtonClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing required components..."));
        return;
    }
    
    // Get the available weapons
    const TArray<TSubclassOf<AWormWeapon>>& AvailableWeapons = OwningCharacter->AvailableWeapons;
    int32 WeaponCount = AvailableWeapons.Num();
    
    if (WeaponCount == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No weapons available to display"));
        return;
    }

    // Get viewport size
    FVector2D ViewportSize;
    GEngine->GameViewport->GetViewportSize(ViewportSize);
    
    UE_LOG(LogTemp, Warning, TEXT("Screen dimensions: %.1f x %.1f"), ViewportSize.X, ViewportSize.Y);
    
    // Button size (consistent for all buttons)
    FVector2D ButtonSize(100.0f, 100.0f);
    
    // Grid parameters
    float RightMargin = 150.0f; // Distance from right edge of screen
    float TopMargin = 100.0f;   // Distance from top of screen
    float ButtonPadding = 20.0f; // Space between buttons
    int32 MaxButtonsPerRow = 2;  // Number of buttons per row
    
    // Calculate positions in a grid layout on the right side
    for (int32 i = 0; i < WeaponCount; ++i)
    {
        // Calculate row and column for grid layout
        int32 Row = i / MaxButtonsPerRow;
        int32 Col = i % MaxButtonsPerRow;
        
        // Calculate position (from top-right corner of screen)
        float X = ViewportSize.X - RightMargin - ((Col + 1) * (ButtonSize.X + ButtonPadding));
        float Y = TopMargin + (Row * (ButtonSize.Y + ButtonPadding));
        
        UE_LOG(LogTemp, Warning, TEXT("Button %d: grid pos=(%d,%d), screen pos=(%.1f, %.1f)"), 
            i, Row, Col, X, Y);
        
        // Create button widget
        UWeaponButtonWidget* ButtonWidget = CreateWidget<UWeaponButtonWidget>(this, WeaponButtonClass);
        if (ButtonWidget)
        {
            // Add to canvas
            UCanvasPanelSlot* CanvasSlot = WheelCanvas->AddChildToCanvas(ButtonWidget);
            if (CanvasSlot)
            {
                // Set button size
                CanvasSlot->SetSize(ButtonSize);
                
                // Position on screen
                CanvasSlot->SetPosition(FVector2D(X, Y));
                
                // Use absolute positioning (no alignment)
                CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
                
                // Get weapon data
                FWeaponUIData WeaponData = GetWeaponUIData(AvailableWeapons[i]);
                
                // Initialize button
                ButtonWidget->SetupWeaponButton(i, WeaponData, this);
                
                // Add to our array
                WeaponButtons.Add(ButtonWidget);
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("CreateWeaponWheel completed with %d buttons created"), WeaponButtons.Num());
}

void UWeaponWheelWidget::SelectWeapon(int32 WeaponIndex)
{
    if (OwningCharacter)
    {
        OwningCharacter->SelectWeaponFromWheel(WeaponIndex);
    }
}

FWeaponUIData UWeaponWheelWidget::GetWeaponUIData(TSubclassOf<AWormWeapon> WeaponClass)
{
    FWeaponUIData WeaponData;
    
    // Try to get data from the data table first
    if (WeaponDataTable)
    {
        // Debug: List all rows in the data table
        TArray<FName> RowNames = WeaponDataTable->GetRowNames();
        UE_LOG(LogTemp, Warning, TEXT("Data table has %d rows:"), RowNames.Num());
        for (const FName& Name : RowNames)
        {
            UE_LOG(LogTemp, Warning, TEXT("  Row name: %s"), *Name.ToString());
        }
    
        FString RowName = WeaponClass->GetName();
        UE_LOG(LogTemp, Warning, TEXT("Looking for weapon UI data with row name: %s"), *RowName);
        FWeaponUIData* FoundData = WeaponDataTable->FindRow<FWeaponUIData>(FName(*RowName), "");
        if (FoundData)
        {
            WeaponData = *FoundData;
            return WeaponData;
        }
    }
    
    // Fallback: Generate basic data from the class
    if (WeaponClass)
    {
        // Get default object to access properties
        AWormWeapon* DefaultWeapon = WeaponClass.GetDefaultObject();
        if (DefaultWeapon)
        {
            // Try to get display name from the weapon class (if you've added such properties)
            // This assumes you've added DisplayName and WeaponIcon properties to AWormWeapon
            // Otherwise, just use the class name as a fallback
            
            // For this example, we'll just use the class name as a fallback
            FString ClassName = WeaponClass->GetName();
            
            // Remove common prefixes/suffixes for cleaner display
            ClassName.ReplaceInline(TEXT("BP_"), TEXT(""));
            ClassName.ReplaceInline(TEXT("_C"), TEXT(""));
            ClassName.ReplaceInline(TEXT("Weapon"), TEXT(""));
            
            WeaponData.Name = ClassName;
            WeaponData.Description = FString::Printf(TEXT("%s Weapon"), *ClassName);
            
            // Note: Icon will remain null
        }
    }
    
    return WeaponData;
}

void UWeaponWheelWidget::UpdateCenterDisplay()
{
    if (HighlightedIndex >= 0 && HighlightedIndex < WeaponButtons.Num() && OwningCharacter)
    {
        // Get the weapon class
        TSubclassOf<AWormWeapon> WeaponClass = OwningCharacter->AvailableWeapons[HighlightedIndex];
        
        // Get the weapon data
        FWeaponUIData WeaponData = GetWeaponUIData(WeaponClass);
        
        // Update the text displays
        if (SelectedWeaponName)
        {
            SelectedWeaponName->SetText(FText::FromString(WeaponData.Name));
        }
        
        if (WeaponDescription)
        {
            WeaponDescription->SetText(FText::FromString(WeaponData.Description));
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Updated center display to: %s - %s"), 
            *WeaponData.Name, *WeaponData.Description);
    }
}
int32 UWeaponWheelWidget::GetWeaponIndexFromMousePosition(FVector2D MousePosition)
{
    if (WeaponButtons.Num() == 0 || !WheelCanvas)
    {
        return -1;
    }
    
    // Check if mouse is directly over any button
    for (int32 i = 0; i < WeaponButtons.Num(); ++i)
    {
        if (WeaponButtons[i])
        {
            FGeometry ButtonGeometry = WeaponButtons[i]->GetCachedGeometry();
            
            // Convert mouse position to button's local space
            FVector2D LocalMousePos = ButtonGeometry.AbsoluteToLocal(
                WheelCanvas->GetCachedGeometry().LocalToAbsolute(MousePosition));
            
            FVector2D ButtonSize = ButtonGeometry.GetLocalSize();
            
            // Check if mouse is within button bounds
            if (LocalMousePos.X >= 0 && LocalMousePos.X <= ButtonSize.X &&
                LocalMousePos.Y >= 0 && LocalMousePos.Y <= ButtonSize.Y)
            {
                return i; // Mouse is directly over this button
            }
        }
    }
    
    // No button is directly hovered
    return -1;
}

FReply UWeaponWheelWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Get the local mouse position
    FVector2D MousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    
    // Calculate which weapon should be highlighted based on mouse position
    int32 Index = GetWeaponIndexFromMousePosition(MousePosition);
    
    // Update highlighted weapon if needed
    if (Index != HighlightedIndex)
    {
        if (Index >= 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Highlighting weapon index: %d"), Index);
            SetHighlightedWeapon(Index);
        }
        else if (HighlightedIndex >= 0)
        {
            // Clear highlighting when mouse isn't over any button
            UE_LOG(LogTemp, Warning, TEXT("No button hovered, clearing highlight"));
            
            // Option 1: Leave the last highlighted button visible
            // (do nothing here)
            
            // Option 2: Clear the highlight entirely (use this if you want no highlight when not over buttons)
            // HighlightedIndex = -1;
            // for (auto* Button : WeaponButtons)
            // {
            //     if (Button)
            //     {
            //         Button->SetHighlighted(false);
            //     }
            // }
        }
    }
    
    return FReply::Handled();
}

void UWeaponWheelWidget::SetHighlightedWeapon(int32 WeaponIndex)
{
    if (WeaponIndex < 0 || WeaponIndex >= WeaponButtons.Num())
    {
        return;
    }
    
    // Debug logging
    if (HighlightedIndex != WeaponIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("Changing highlighted weapon from %d to %d"), 
            HighlightedIndex, WeaponIndex);
    }
    
    // Update the highlighted index
    HighlightedIndex = WeaponIndex;
    
    // Update button visuals
    for (int32 i = 0; i < WeaponButtons.Num(); ++i)
    {
        if (WeaponButtons[i])
        {
            WeaponButtons[i]->SetHighlighted(i == HighlightedIndex);
        }
    }
    
    // Update the center display
    UpdateCenterDisplay();
}