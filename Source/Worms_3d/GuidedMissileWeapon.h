#pragma once

#include "CoreMinimal.h"
#include "WormWeapon.h"
#include "GuidedMissileWeapon.generated.h"

UCLASS()
class WORMS_3D_API AGuidedMissileWeapon : public AWormWeapon
{
	GENERATED_BODY()

public:
	AGuidedMissileWeapon();
    
	// Override the base Fire function
	virtual void Fire() override;
    
	// Function to process movement input while guiding
	UFUNCTION(BlueprintCallable, Category = "Missile")
	void ProcessMissileMovementInput(float YawInput, float PitchInput);
    
	// Function to abort guidance
	UFUNCTION(BlueprintCallable, Category = "Missile")
	void AbortGuidance();
    
	// Check if currently guiding a missile
	UFUNCTION(BlueprintPure, Category = "Missile")
	bool IsGuidingMissile() const { return ActiveMissile != nullptr; }
    
protected:
	// Reference to the active missile
	UPROPERTY(BlueprintReadOnly, Category = "Missile")
	class AGuidedMissileProjectile* ActiveMissile;
    
	// This will handle player input during missile guidance
	virtual void Tick(float DeltaTime) override;
};