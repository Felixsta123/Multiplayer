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
    
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetDestroyedSignature);
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnTargetDestroyedSignature OnTargetDestroyed;

protected:
	// Override movement and action functions to detect tutorial progress
	virtual void MoveForward(float Value);
	virtual void MoveRight(float Value);
	virtual void Jump() override;
	virtual void FireWeapon() override;
    
	// Track if player has moved in tutorial
	bool bHasMovedInTutorial;
	bool bHasJumpedInTutorial;
	bool bHasFiredInTutorial;
};