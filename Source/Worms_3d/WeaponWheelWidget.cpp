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

FReply UWeaponWheelWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Get the local mouse position
    FVector2D MousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    
    // Calculate which weapon should be highlighted based on mouse position
    int32 Index = GetWeaponIndexFromMousePosition(MousePosition);
    
    // Update highlighted weapon if needed
    if (Index != HighlightedIndex && Index >= 0)
    {
        SetHighlightedWeapon(Index);
    }
    
    return FReply::Handled();
}

// void UWeaponWheelWidget::CreateWeaponWheel()
// {
//     UE_LOG(LogTemp, Warning, TEXT("CreateWeaponWheel called"));
//
//     if (WeaponDataTable)
//     {
//         TArray<FName> RowNames = WeaponDataTable->GetRowNames();
//         UE_LOG(LogTemp, Warning, TEXT("Data table has %d rows:"), RowNames.Num());
//         for (const FName& Name : RowNames)
//         {
//             UE_LOG(LogTemp, Warning, TEXT("  Row name: %s"), *Name.ToString());
//         }
//     }
//     else
//     {
//         UE_LOG(LogTemp, Error, TEXT("WeaponDataTable is null in WeaponWheelWidget!"));
//     }
//     
//     // Clear any existing buttons
//     for (UWeaponButtonWidget* Button : WeaponButtons)
//     {
//         if (Button)
//         {
//             Button->RemoveFromParent();
//         }
//     }
//     WeaponButtons.Empty();
//     
//     if (!OwningCharacter || !WheelCanvas || !WeaponButtonClass)
//     {
//         return;
//     }
//     
//     // Get the available weapons
//     const TArray<TSubclassOf<AWormWeapon>>& AvailableWeapons = OwningCharacter->AvailableWeapons;
//     int32 WeaponCount = AvailableWeapons.Num();
//     
//     if (WeaponCount == 0)
//     {
//         return;
//     }
//     
//     // Calculate the center of the canvas
//     FVector2D CanvasSize = WheelCanvas->GetCachedGeometry().GetLocalSize();
//     FVector2D Center = CanvasSize * 0.5f;
//     
//     // Create a button for each weapon
//     for (int32 i = 0; i < WeaponCount; ++i)
//     {
//         // Calculate the position for this button
//         float Angle = (2.0f * PI * i) / WeaponCount;
//         float X = Center.X + WheelRadius * FMath::Cos(Angle);
//         float Y = Center.Y + WheelRadius * FMath::Sin(Angle);
//         
//         // Create the button
//         UWeaponButtonWidget* ButtonWidget = CreateWidget<UWeaponButtonWidget>(this, WeaponButtonClass);
//         if (ButtonWidget)
//         {
//             // Add to canvas
//             UCanvasPanelSlot* CanvasSlot = WheelCanvas->AddChildToCanvas(ButtonWidget);
//             if (CanvasSlot)
//             {
//                 // Button size (100x100)
//                 CanvasSlot->SetSize(FVector2D(100.0f, 100.0f));
//                 
//                 // Center the button on its position
//                 CanvasSlot->SetPosition(FVector2D(X - 50.0f, Y - 50.0f));
//                 
//                 // Get weapon data
//                 FWeaponUIData WeaponData = GetWeaponUIData(AvailableWeapons[i]);
//                 
//                 // Initialize the button (using our renamed function)
//                 ButtonWidget->SetupWeaponButton(i, WeaponData, this);
//                 
//                 // Add to our array
//                 WeaponButtons.Add(ButtonWidget);
//             }
//         }
//     }
// }

void UWeaponWheelWidget::CreateWeaponWheel()
{
    UE_LOG(LogTemp, Warning, TEXT("CreateWeaponWheel called"));
    
    if (WeaponDataTable)
    {
        TArray<FName> RowNames = WeaponDataTable->GetRowNames();
        UE_LOG(LogTemp, Warning, TEXT("Data table has %d rows:"), RowNames.Num());
        for (const FName& Name : RowNames)
        {
            UE_LOG(LogTemp, Warning, TEXT("  Row name: %s"), *Name.ToString());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("WeaponDataTable is null in WeaponWheelWidget!"));
    }
    
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

    // Get viewport size for centering
    FVector2D ViewportSize;
    if (GEngine && GEngine->GameViewport) {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }
    else {
        ViewportSize = FVector2D(1920, 1080);
    }
    
    // Use viewport center
    FVector2D ScreenCenter = ViewportSize * 0.5f;
    float ButtonRadius = FMath::Min(ViewportSize.X, ViewportSize.Y) * 0.2f;
    
    UE_LOG(LogTemp, Warning, TEXT("Screen size: %.1f x %.1f, Center: %.1f, %.1f, ButtonRadius: %.1f"), 
        ViewportSize.X, ViewportSize.Y, ScreenCenter.X, ScreenCenter.Y, ButtonRadius);
    
    // Create buttons around the circle
    for (int32 i = 0; i < WeaponCount; ++i)
    {
        // Calculate the position for this button (clockwise from top)
        float Angle = FMath::DegreesToRadians(270.0f + (360.0f * i) / WeaponCount);
    
        // For 2 weapons, place them left and right (170 degree separation)
        if (WeaponCount == 2) {
            Angle = FMath::DegreesToRadians(i == 0 ? 180.0f : 0.0f);
        }
    
        float X = ScreenCenter.X + ButtonRadius * FMath::Cos(Angle);
        float Y = ScreenCenter.Y + ButtonRadius * FMath::Sin(Angle);
    
        // Adjust radius based on screen size
        // float ButtonRadius = FMath::Min(ViewportSize.X, ViewportSize.Y) * 0.2f; // 20% of screen size
    
        // float X = ScreenCenter.X + ButtonRadius * FMath::Cos(Angle);
        // float Y = ScreenCenter.Y + ButtonRadius * FMath::Sin(Angle);
    
        // Create the button
        UWeaponButtonWidget* ButtonWidget = CreateWidget<UWeaponButtonWidget>(this, WeaponButtonClass);
        if (ButtonWidget)
        {
            // Add to canvas
            UCanvasPanelSlot* CanvasSlot = WheelCanvas->AddChildToCanvas(ButtonWidget);
            if (CanvasSlot)
            {
                // Button size (100x100)
                CanvasSlot->SetSize(FVector2D(100.0f, 100.0f));
            
                // Center the button on its position (subtract half the button size)
                CanvasSlot->SetPosition(FVector2D(X - 50.0f, Y - 50.0f));
            
                // Set alignment to center
                CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
            
                // Get weapon data
                FWeaponUIData WeaponData = GetWeaponUIData(AvailableWeapons[i]);
            
                // Initialize the button
                ButtonWidget->SetupWeaponButton(i, WeaponData, this);
            
                // Add to our array
                WeaponButtons.Add(ButtonWidget);
            }
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("CreateWeaponWheel completed with %d buttons created"), WeaponButtons.Num());
}

// void UWeaponWheelWidget::SetHighlightedWeapon(int32 WeaponIndex)
// {
//     if (WeaponIndex < 0 || WeaponIndex >= WeaponButtons.Num())
//     {
//         return;
//     }
//     
//     // Update the highlighted index
//     HighlightedIndex = WeaponIndex;
//     
//     // Update button visuals
//     for (int32 i = 0; i < WeaponButtons.Num(); ++i)
//     {
//         if (WeaponButtons[i])
//         {
//             WeaponButtons[i]->SetHighlighted(i == HighlightedIndex);
//         }
//     }
//     
//     // Update the center display
//     UpdateCenterDisplay();
// }

void UWeaponWheelWidget::SelectWeapon(int32 WeaponIndex)
{
    if (OwningCharacter)
    {
        OwningCharacter->SelectWeaponFromWheel(WeaponIndex);
    }
}

int32 UWeaponWheelWidget::GetWeaponIndexFromMousePosition(FVector2D MousePosition)
{
    if (WeaponButtons.Num() == 0 || !WheelCanvas)
    {
        return -1;
    }
    
    // Get the center of the canvas
    FVector2D CanvasSize = WheelCanvas->GetCachedGeometry().GetLocalSize();
    FVector2D Center = CanvasSize * 0.5f;
    
    // Calculate the direction vector from center to mouse
    FVector2D Direction = MousePosition - Center;
    
    // Calculate the angle (in radians)
    float Angle = FMath::Atan2(Direction.Y, Direction.X);
    
    // Convert to positive degrees (0-360)
    float Degrees = FMath::RadiansToDegrees(Angle);
    if (Degrees < 0.0f)
    {
        Degrees += 360.0f;
    }
    
    // Calculate the index based on the angle
    int32 WeaponCount = WeaponButtons.Num();
    float SegmentSize = 360.0f / WeaponCount;
    
    // Add half a segment to center the selection on the button
    int32 Index = FMath::FloorToInt((Degrees + (SegmentSize * 0.5f)) / SegmentSize) % WeaponCount;
    
    return Index;
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

// void UWeaponWheelWidget::UpdateCenterDisplay()
// {
//     if (HighlightedIndex >= 0 && HighlightedIndex < WeaponButtons.Num() && OwningCharacter)
//     {
//         // Get the weapon class
//         TSubclassOf<AWormWeapon> WeaponClass = OwningCharacter->AvailableWeapons[HighlightedIndex];
//         
//         // Get the weapon data
//         FWeaponUIData WeaponData = GetWeaponUIData(WeaponClass);
//         
//         // Update the text displays
//         if (SelectedWeaponName)
//         {
//             SelectedWeaponName->SetText(FText::FromString(WeaponData.Name));
//         }
//         
//         if (WeaponDescription)
//         {
//             WeaponDescription->SetText(FText::FromString(WeaponData.Description));
//         }
//     }
// }

void UWeaponWheelWidget::SetHighlightedWeapon(int32 WeaponIndex)
{
    if (WeaponIndex < 0 || WeaponIndex >= WeaponButtons.Num())
    {
        return;
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