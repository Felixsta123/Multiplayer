// WormTutorialCharacter.h - Enhanced implementation

#pragma once

#include "CoreMinimal.h"
#include "../AWormCharacter.h"
#include "WormTutorialCharacter.generated.h"

/**
 * Extended character class for tutorial with event dispatchers
 */
UCLASS()
class WORMS_3D_API AWormTutorialCharacter : public AWormCharacter
{
	GENERATED_BODY()

public:
	AWormTutorialCharacter();
    
	virtual void BeginPlay() override;

	// Event dispatchers for tutorial tracking
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterMovedSignature);
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnCharacterMovedSignature OnCharacterMoved;
    
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterJumpedSignature);
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnCharacterJumpedSignature OnCharacterJumped;
    
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterFiredSignature);
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnCharacterFiredSignature OnCharacterFired;
    
	// Override movement to track tutorial progress
	virtual void MoveForward(float Value) override;
	virtual void MoveRight(float Value) override;
	virtual void Jump() override;
	virtual void FireWeapon() override;
    
	// Override movement points for tutorial
	virtual void UpdateMovementPoints() override;

protected:
	// Track if player has performed tutorial actions
	bool bHasMovedInTutorial;
	bool bHasJumpedInTutorial;
	bool bHasFiredInTutorial;
    
	// Timer handle for movement check
	FTimerHandle MovementCheckTimerHandle;
    
	// Additional method to detect movement
	UFUNCTION()
	void CheckInitialMovement();
};