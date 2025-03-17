#include "Worms_3d/UI/WActiveCharacterInfoWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"
#include "Worms_3d/AWormCharacter.h"
#include "Worms_3d/WormWeapon.h"

UWActiveCharacterInfoWidget::UWActiveCharacterInfoWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Enable ticking for real-time updates
}

void UWActiveCharacterInfoWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

void UWActiveCharacterInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Update movement points every tick
    UpdateMovementPoints();
}

void UWActiveCharacterInfoWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Get GameState
    WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    
    if (WormGameState)
    {
        // Subscribe to active player changed event
        WormGameState->OnActivePlayerChanged.AddDynamic(this, &UWActiveCharacterInfoWidget::OnActivePlayerChanged);
        
        // Initial update
        OnActivePlayerChanged();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WActiveCharacterInfoWidget: GameState not found"));
    }
}

void UWActiveCharacterInfoWidget::OnActivePlayerChanged()
{
    // Only update if it's the local player's turn
    bool bLocalPlayerTurn = false;
    
    if (WormGameState)
    {
        bLocalPlayerTurn = WormGameState->IsLocalPlayerTurn();
        
        // Log current state for debugging
        UE_LOG(LogTemp, Warning, TEXT("ActiveCharacterInfo: Player Turn=%s, CurrentIndex=%d"), 
            bLocalPlayerTurn ? TEXT("true") : TEXT("false"),
            WormGameState->CurrentPlayerIndex);
    }
    
    if (bLocalPlayerTurn)
    {
        // Get active character
        ActiveCharacter = nullptr;
        if (WormGameState)
        {
            ActiveCharacter = WormGameState->GetActiveCharacter();
        }
        
        if (ActiveCharacter)
        {
            UE_LOG(LogTemp, Warning, TEXT("ActiveCharacterInfo: Found active character %s"), *ActiveCharacter->GetName());
            
            // Update character name
            if (CharacterNameText)
            {
                FString DisplayName = !ActiveCharacter->InGameName.IsEmpty() ? 
                    ActiveCharacter->InGameName : ActiveCharacter->GetName();
                    
                CharacterNameText->SetText(FText::FromString(DisplayName));
                UE_LOG(LogTemp, Warning, TEXT("ActiveCharacterInfo: Set character name to %s"), *DisplayName);
            }
            
            // Update other information
            UpdateMovementPoints();
            UpdateWeapon();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ActiveCharacterInfo: No active character found"));
            
            // No active character, clear information
            if (CharacterNameText)
            {
                CharacterNameText->SetText(FText::FromString(TEXT("No Active Character")));
            }
            
            if (WeaponNameText)
            {
                WeaponNameText->SetText(FText::FromString(TEXT("No Weapon")));
            }
            
            if (MovementPointsBar)
            {
                MovementPointsBar->SetPercent(0.0f);
            }
            
            if (MovementPointsText)
            {
                MovementPointsText->SetText(FText::FromString(TEXT("0 / 0")));
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ActiveCharacterInfo: Not local player's turn"));
        ActiveCharacter = nullptr;
        
        // Not the local player's turn, show appropriate message
        if (CharacterNameText)
        {
            CharacterNameText->SetText(FText::FromString(TEXT("Waiting...")));
        }
    }
}

void UWActiveCharacterInfoWidget::UpdateMovementPoints()
{
    if (!ActiveCharacter)
    {
        return;
    }
    
    // Update progress bar
    if (MovementPointsBar)
    {
        float Percent = ActiveCharacter->MovementPoints / ActiveCharacter->MaxMovementPoints;
        MovementPointsBar->SetPercent(Percent);
        
        // Change color based on remaining points
        FLinearColor MovementColor;
        if (Percent > 0.6f)
        {
            MovementColor = FLinearColor::Green;
        }
        else if (Percent > 0.3f)
        {
            MovementColor = FLinearColor::Yellow;
        }
        else
        {
            MovementColor = FLinearColor::Red;
        }
        
        MovementPointsBar->SetFillColorAndOpacity(MovementColor);
        
        UE_LOG(LogTemp, Verbose, TEXT("ActiveCharacterInfo: Updated movement points bar to %.2f%%"), Percent * 100.0f);
    }
    
    // Update text
    if (MovementPointsText)
    {
        int32 Current = FMath::FloorToInt(ActiveCharacter->MovementPoints);
        int32 Max = FMath::FloorToInt(ActiveCharacter->MaxMovementPoints);
        FString MovementString = FString::Printf(TEXT("%d / %d"), Current, Max);
        MovementPointsText->SetText(FText::FromString(MovementString));
        
        UE_LOG(LogTemp, Verbose, TEXT("ActiveCharacterInfo: Updated movement points text to %s"), *MovementString);
    }
}

void UWActiveCharacterInfoWidget::UpdateWeapon()
{
    if (!ActiveCharacter)
    {
        return;
    }
    
    // Update weapon name
    if (WeaponNameText)
    {
        if (ActiveCharacter->CurrentWeapon)
        {
            FString WeaponName = ActiveCharacter->CurrentWeapon->GetClass()->GetName();
            
            // Clean up class name for better display
            WeaponName.ReplaceInline(TEXT("WormWeapon_"), TEXT(""));
            WeaponName.ReplaceInline(TEXT("BP_"), TEXT(""));
            WeaponName.ReplaceInline(TEXT("_C"), TEXT(""));
            
            WeaponNameText->SetText(FText::FromString(WeaponName));
            UE_LOG(LogTemp, Verbose, TEXT("ActiveCharacterInfo: Updated weapon name to %s"), *WeaponName);
        }
        else
        {
            WeaponNameText->SetText(FText::FromString(TEXT("No Weapon")));
        }
    }
}