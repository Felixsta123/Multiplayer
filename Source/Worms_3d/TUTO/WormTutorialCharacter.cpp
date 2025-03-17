#include "WormTutorialCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

AWormTutorialCharacter::AWormTutorialCharacter()
{
	// Initialize tracking variables
	bHasMovedInTutorial = false;
	bHasJumpedInTutorial = false;
	bHasFiredInTutorial = false;
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
	}
}