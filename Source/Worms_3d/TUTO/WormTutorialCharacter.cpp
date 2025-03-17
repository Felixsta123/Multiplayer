// WormTutorialCharacter.cpp - Enhanced implementation

#include "WormTutorialCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

AWormTutorialCharacter::AWormTutorialCharacter()
{
    // Initialize tracking variables
    bHasMovedInTutorial = false;
    bHasJumpedInTutorial = false;
    bHasFiredInTutorial = false;
    
    // Set up properties for tutorial
    MaxMovementPoints = 10000; // Effectively unlimited for tutorial
    Health = 100.0f;
    
    // Enhance movement for tutorial
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->JumpZVelocity = 700.0f; // Higher jump
        GetCharacterMovement()->AirControl = 0.9f;      // Better air control
    }
}

void AWormTutorialCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // Force movement points for tutorial
    MovementPoints = MaxMovementPoints;
    
    // Ensure turn is active
    bIsMyTurn = true;
    
    // Log for debugging
    UE_LOG(LogTemp, Warning, TEXT("Tutorial character initialized: %s"), *GetName());
    
    // Set a timer to trigger movement checks
    GetWorldTimerManager().SetTimer(
        MovementCheckTimerHandle, 
        this, 
        &AWormTutorialCharacter::CheckInitialMovement, 
        0.5f, 
        true
    );
}

void AWormTutorialCharacter::MoveForward(float Value)
{
    // Call parent implementation for actual movement
    Super::MoveForward(Value);
    
    // Check if this is a significant movement
    if (FMath::Abs(Value) > 0.1f && !bHasMovedInTutorial)
    {
        bHasMovedInTutorial = true;
        OnCharacterMoved.Broadcast();
        UE_LOG(LogTemp, Warning, TEXT("Tutorial Character: Forward Movement Detected!"));
        
        // Cancel the timer once movement is detected
        GetWorldTimerManager().ClearTimer(MovementCheckTimerHandle);
    }
}

void AWormTutorialCharacter::MoveRight(float Value)
{
    // Call parent implementation for actual movement
    Super::MoveRight(Value);
    
    // Check if this is a significant movement
    if (FMath::Abs(Value) > 0.1f && !bHasMovedInTutorial)
    {
        bHasMovedInTutorial = true;
        OnCharacterMoved.Broadcast();
        UE_LOG(LogTemp, Warning, TEXT("Tutorial Character: Right Movement Detected!"));
        
        // Cancel the timer once movement is detected
        GetWorldTimerManager().ClearTimer(MovementCheckTimerHandle);
    }
}

void AWormTutorialCharacter::Jump()
{
    // Call parent implementation for actual jump
    Super::Jump();
    
    // Track jump for tutorial
    if (!bHasJumpedInTutorial)
    {
        bHasJumpedInTutorial = true;
        OnCharacterJumped.Broadcast();
        UE_LOG(LogTemp, Warning, TEXT("Tutorial Character: Jump Detected!"));
    }
}

void AWormTutorialCharacter::FireWeapon()
{
    // Call parent implementation for actual firing
    Super::FireWeapon();
    
    // Track firing for tutorial
    if (!bHasFiredInTutorial)
    {
        bHasFiredInTutorial = true;
        OnCharacterFired.Broadcast();
        UE_LOG(LogTemp, Warning, TEXT("Tutorial Character: Weapon Fired!"));
    }
}

void AWormTutorialCharacter::CheckInitialMovement()
{
    // Special check for movement detection - checks if the character has moved from its initial position
    if (!bHasMovedInTutorial)
    {
        static FVector InitialLocation = FVector::ZeroVector;
        
        // Initialize on first call
        if (InitialLocation.IsZero())
        {
            InitialLocation = GetActorLocation();
            return;
        }
        
        // Check if we've moved significantly
        float MovedDistance = FVector::Distance(InitialLocation, GetActorLocation());
        if (MovedDistance > 50.0f) // If moved more than 50 units
        {
            bHasMovedInTutorial = true;
            OnCharacterMoved.Broadcast();
            UE_LOG(LogTemp, Warning, TEXT("Tutorial Character: Movement Detected (distance: %.2f)!"), MovedDistance);
            
            // Cancel the timer
            GetWorldTimerManager().ClearTimer(MovementCheckTimerHandle);
        }
    }
}

// Override to ensure we don't consume movement points in tutorial
void AWormTutorialCharacter::UpdateMovementPoints()
{
    // Tutorial doesn't consume movement points
    MovementPoints = MaxMovementPoints;
}