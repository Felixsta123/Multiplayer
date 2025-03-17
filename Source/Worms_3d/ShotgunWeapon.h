// ShotgunWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "WormWeapon.h"
#include "ShotgunWeapon.generated.h"

UCLASS()
class WORMS_3D_API AShotgunWeapon : public AWormWeapon
{
	GENERATED_BODY()

public:
	AShotgunWeapon();
    
	// Override the base Fire function
	virtual void Fire() override;
    
protected:
	// Number of pellets to fire
	UPROPERTY(EditDefaultsOnly, Category = "Shotgun")
	int32 PelletCount;
    
	// Maximum spread angle in degrees
	UPROPERTY(EditDefaultsOnly, Category = "Shotgun")
	float SpreadAngle;
    
	// Optional: Pellet damage multiplier (compared to base projectile)
	UPROPERTY(EditDefaultsOnly, Category = "Shotgun")
	float PelletDamageMultiplier;
};