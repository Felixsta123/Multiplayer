// #pragma once
//
// #include "CoreMinimal.h"
// #include "AWormsProjectile.h"
// #include "GuidedMissileProjectile.generated.h"
//
// UCLASS()
// class WORMS_3D_API AGuidedMissileProjectile : public AWormProjectile
// {
// 	GENERATED_BODY()
//
// public:
// 	AGuidedMissileProjectile();
// 	bool IsPossessed() const { return bIsPossessed; }
//
// 	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
// 				FVector NormalImpulse, const FHitResult& Hit) override;
// 	
// 	virtual void Tick(float DeltaTime) override;
// 	virtual void BeginPlay() override;
// 	virtual void Destroyed() override;
// 	
// 	// Function to enable collisions after delay
// 	virtual void EnableCollisions() override;
// 	
// 	// Functions to control the missile
// 	UFUNCTION(BlueprintCallable, Category = "Missile")
// 	void ApplyYawInput(float Value);
//     
// 	UFUNCTION(BlueprintCallable, Category = "Missile")
// 	void ApplyPitchInput(float Value);
//
// 	// Functions to handle possession
// 	UFUNCTION(BlueprintCallable, Category = "Missile")
// 	void PossessMissile(APlayerController* NewController);
//     
// 	UFUNCTION(BlueprintCallable, Category = "Missile")
// 	void ReleaseMissile();
//
// protected:
// 	// Camera component for missile view
// 	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
// 	class UCameraComponent* MissileCamera;
//
// 	// Control properties
// 	UPROPERTY(EditDefaultsOnly, Category = "Missile")
// 	float TurnRate;
//     
// 	UPROPERTY(EditDefaultsOnly, Category = "Missile")
// 	float MaxTurnAnglePerFrame;
//     
// 	UPROPERTY(EditDefaultsOnly, Category = "Missile")
// 	float FuelDuration;
//     
// 	// Store the original controller and character
// 	UPROPERTY()
// 	APlayerController* OwningController;
//     
// 	UPROPERTY()
// 	APawn* OriginalPawn;
//     
// 	// Fuel timer
// 	FTimerHandle FuelTimerHandle;
//     
// 	// Flag to track if missile is being controlled
// 	bool bIsPossessed;
// 	
// 	// Flag to track if missile can collide
// 	bool bCanCollide;
//     
// 	// Functions for fuel expiration
// 	UFUNCTION()
// 	void OnFuelExpired();
// };

#pragma once

#include "CoreMinimal.h"
#include "AWormsProjectile.h"
#include "GuidedMissileProjectile.generated.h"

UCLASS()
class WORMS_3D_API AGuidedMissileProjectile : public AWormProjectile
{
    GENERATED_BODY()

public:
    AGuidedMissileProjectile();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;
    virtual void Destroyed() override;
    virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                FVector NormalImpulse, const FHitResult& Hit) override;

    // Function to enable collisions (removing override since it's not virtual in parent)
    virtual void EnableCollisions();
    
    // Functions to control the missile
    UFUNCTION(BlueprintCallable, Category = "Missile")
    void ApplyYawInput(float Value);
    
    UFUNCTION(BlueprintCallable, Category = "Missile")
    void ApplyPitchInput(float Value);

    // Functions to handle possession
    UFUNCTION(BlueprintCallable, Category = "Missile")
    void PossessMissile(APlayerController* NewController);
    
    UFUNCTION(BlueprintCallable, Category = "Missile")
    void ReleaseMissile();
    
    // Is the missile being controlled?
    UFUNCTION(BlueprintPure, Category = "Missile")
    bool IsPossessed() const { return bIsPossessed; }

    FRotator CurrentMissileRotation;

protected:
    // Camera component for missile view
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UCameraComponent* MissileCamera;

    // Control properties
    UPROPERTY(EditDefaultsOnly, Category = "Missile")
    float TurnRate;
    
    UPROPERTY(EditDefaultsOnly, Category = "Missile")
    float MaxTurnAnglePerFrame;
    
    UPROPERTY(EditDefaultsOnly, Category = "Missile")
    float FuelDuration;
    
    // Store the original controller and character
    UPROPERTY()
    APlayerController* OwningController;
    
    UPROPERTY()
    APawn* OriginalPawn;
    
    // Fuel timer
    FTimerHandle FuelTimerHandle;
    
    // Flag to track if missile is being controlled
    bool bIsPossessed;
    
    // Flag to track if missile can collide
    bool bCanCollide;
    
    // Functions for fuel expiration
    UFUNCTION()
    void OnFuelExpired();
    
    // Function to explode after backup timer
    UFUNCTION()
    void ForceExplodeAfterDelay();
};