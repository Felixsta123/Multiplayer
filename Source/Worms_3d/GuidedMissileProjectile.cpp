#include "GuidedMissileProjectile.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "AWormCharacter.h"

AGuidedMissileProjectile::AGuidedMissileProjectile()
{
    // Create camera component
    MissileCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MissileCamera"));
    MissileCamera->SetupAttachment(RootComponent);
    MissileCamera->SetRelativeLocation(FVector(-50.0f, 0.0f, 20.0f)); // Position behind missile
    MissileCamera->bUsePawnControlRotation = false;
    
    // Set default values
    TurnRate = 5.0f;
    MaxTurnAnglePerFrame = 5.0f;
    FuelDuration = 15.0f;
    bIsPossessed = false;
    bCanCollide = false;
    OwningController = nullptr;
    OriginalPawn = nullptr;
    
    // Configure projectile movement for guided missiles
    if (ProjectileMovement)
    {
        // Set gravity to zero for better control
        ProjectileMovement->ProjectileGravityScale = 0.0f;
        
        // Make rotation follow velocity for better visual feedback
        ProjectileMovement->bRotationFollowsVelocity = false; // testing with false
        
        // Disable bouncing
        ProjectileMovement->bShouldBounce = false;
        
        // Adjust speed for better control
        ProjectileMovement->InitialSpeed = 1500.0f;
        ProjectileMovement->MaxSpeed = 2000.0f;
        
        // Disable homing (we'll handle guiding manually)
        ProjectileMovement->bIsHomingProjectile = false;
        
        // Set up velocity inheritance
        ProjectileMovement->bInitialVelocityInLocalSpace = true;
    }
    
    // Extend lifetime
    InitialLifeSpan = 20.0f;
    
    // Enhanced collision detection - but initially disabled
    if (CollisionComp)
    {
        CollisionComp->SetCollisionProfileName("NoCollision"); // Start with no collision
        CollisionComp->SetNotifyRigidBodyCollision(true);
    }
    
    // Set damage and radius
    ExplosionDamage = 80.0f;
    ExplosionRadius = 300.0f;
}

void AGuidedMissileProjectile::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("GuidedMissileProjectile::BeginPlay() - %s"), *GetName());
    
    // Ensure MissileCamera exists and is properly set up
    if (!MissileCamera)
    {
        UE_LOG(LogTemp, Error, TEXT("MissileCamera is NULL! Creating one now"));
        
        // Create a camera if it doesn't exist
        MissileCamera = NewObject<UCameraComponent>(this, TEXT("MissileCamera"));
        MissileCamera->RegisterComponent();
        MissileCamera->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
        MissileCamera->SetRelativeLocation(FVector(-50.0f, 0.0f, 20.0f));
        MissileCamera->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
    }
    
    // Make sure CollisionComp is properly set up
    if (CollisionComp)
    {
        // Start with NoCollision and then enable after a delay
        CollisionComp->SetCollisionProfileName("NoCollision");
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CollisionComp is NULL!"));
    }
    
    // Start fuel timer
    GetWorldTimerManager().SetTimer(
        FuelTimerHandle,
        this,
        &AGuidedMissileProjectile::OnFuelExpired,
        FuelDuration,
        false
    );
    
    // Enable collisions after a delay to avoid hitting the firing player
    FTimerHandle EnableCollisionTimer;
    GetWorld()->GetTimerManager().SetTimer(
        EnableCollisionTimer,
        this,
        &AGuidedMissileProjectile::EnableCollisions,
        1.0f, // Delay to ensure possession happens first
        false
    );
    
    // Debug to confirm the missile exists and is ready
    UE_LOG(LogTemp, Warning, TEXT("Missile %s is ready for possession"), *GetName());

    CurrentMissileRotation = GetActorRotation();
}

void AGuidedMissileProjectile::EnableCollisions()
{
    bCanCollide = true;
    
    if (CollisionComp)
    {
        CollisionComp->SetCollisionProfileName("BlockAllDynamic");
        UE_LOG(LogTemp, Warning, TEXT("Missile collisions enabled"));
    }
}

void AGuidedMissileProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (bIsPossessed && ProjectileMovement)
    {
        // Get the current forward vector from the actor's rotation
        FVector Direction = GetActorForwardVector();
        
        // Set the velocity to match our forward direction
        float CurrentSpeed = FMath::Clamp(ProjectileMovement->Velocity.Size(), 1500.0f, 2000.0f);
        ProjectileMovement->Velocity = Direction * CurrentSpeed;
        
        // Disabling rotation following velocity is now done in the constructor
        
        // Debug logging
        static float TimeSinceLastLog = 0.0f;
        TimeSinceLastLog += DeltaTime;
        if (TimeSinceLastLog > 1.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("Missile guided - Speed: %.1f, Direction: %s, Location: %s"), 
                CurrentSpeed, *Direction.ToString(), *GetActorLocation().ToString());
            TimeSinceLastLog = 0.0f;
        }
        
        // Draw debug line showing missile direction
        // DrawDebugLine(
        //     GetWorld(),
        //     GetActorLocation(),
        //     GetActorLocation() + Direction * 100.0f,
        //     FColor::Red,
        //     false,
        //     -1.0f,
        //     0,
        //     2.0f
        // );
    }
}

void AGuidedMissileProjectile::ApplyYawInput(float Value)
{
    if (!bIsPossessed)
        return;
    
    if (!FMath::IsNearlyZero(Value, 0.01f))
    {
        // Log current rotation
        UE_LOG(LogTemp, Warning, TEXT("Missile rotation - Before: %s"), *CurrentMissileRotation.ToString());
        
        // Calculate rotation change
        float DeltaYaw = FMath::Clamp(Value * TurnRate * 2.0f, -MaxTurnAnglePerFrame, MaxTurnAnglePerFrame);
        
        // Update our stored rotation
        CurrentMissileRotation.Yaw += DeltaYaw;
        
        // Apply the rotation to the actor
        SetActorRotation(CurrentMissileRotation);
        
        // Log the new rotation
        UE_LOG(LogTemp, Warning, TEXT("Missile rotation - After: %s"), *CurrentMissileRotation.ToString());
        
        // Debug visualization
        // DrawDebugLine(
        //     GetWorld(),
        //     GetActorLocation(),
        //     GetActorLocation() + GetActorRightVector() * (Value * 100.0f),
        //     FColor::Blue,
        //     false,
        //     0.1f,
        //     0,
        //     3.0f
        // );
    }
}

void AGuidedMissileProjectile::ApplyPitchInput(float Value)
{
    if (!bIsPossessed)
        return;
    
    if (!FMath::IsNearlyZero(Value, 0.01f))
    {
        // Log current rotation
        UE_LOG(LogTemp, Warning, TEXT("Missile rotation - Before: %s"), *CurrentMissileRotation.ToString());
        
        // Calculate rotation change
        float DeltaPitch = FMath::Clamp(Value * TurnRate * 2.0f, -MaxTurnAnglePerFrame, MaxTurnAnglePerFrame);
        
        // Update our stored rotation
        CurrentMissileRotation.Pitch += DeltaPitch;
        
        // Apply the rotation to the actor
        SetActorRotation(CurrentMissileRotation);
        
        // Log the new rotation
        UE_LOG(LogTemp, Warning, TEXT("Missile rotation - After: %s"), *CurrentMissileRotation.ToString());
        
        // Debug visualization
        // DrawDebugLine(
        //     GetWorld(),
        //     GetActorLocation(),
        //     GetActorLocation() + GetActorUpVector() * (Value * 100.0f),
        //     FColor::Green,  // Green is often used for up/down
        //     false,
        //     0.1f,
        //     0,
        //     3.0f
        // );
    }
}

void AGuidedMissileProjectile::PossessMissile(APlayerController* NewController)
{
    if (!NewController)
    {
        UE_LOG(LogTemp, Error, TEXT("PossessMissile: NewController is NULL!"));
        return;
    }
    
    if (bIsPossessed)
    {
        UE_LOG(LogTemp, Warning, TEXT("Missile already possessed!"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("PossessMissile called for %s with controller %s"), 
        *GetName(), *NewController->GetName());
    
    // Store the controller and original pawn
    OwningController = NewController;
    OriginalPawn = OwningController->GetPawn();
    
    if (OriginalPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("Original pawn: %s"), *OriginalPawn->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Original pawn is NULL!"));
    }
    
    // For a pure camera-based approach, don't unpossess the character
    // Instead, just set the view target to the missile
    
    // Ensure the MissileCamera exists and is active
    if (MissileCamera)
    {
        MissileCamera->Activate(true);
        MissileCamera->SetActive(true);
        UE_LOG(LogTemp, Warning, TEXT("Missile camera activated"));
        
        // Draw debug to show camera position
        // DrawDebugSphere(
        //     GetWorld(),
        //     MissileCamera->GetComponentLocation(),
        //     10.0f,
        //     8,
        //     FColor::Blue,
        //     false,
        //     5.0f
        // );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MissileCamera is NULL!"));
    }
    
    // Set the view target with a small blend time
    OwningController->SetViewTargetWithBlend(this, 0.2f);
    
    // Set a flag to indicate successful possession
    bIsPossessed = true;
    
    // Notify the pawn that it's guiding a missile now
    if (OriginalPawn)
    {
        AWormCharacter* Character = Cast<AWormCharacter>(OriginalPawn);
        if (Character)
        {
            // Set the flag for missile control in the character
            Character->bIsGuidingMissile = true;
        }
    }
    
    // Log success
    UE_LOG(LogTemp, Warning, TEXT("Missile possessed successfully!"));
}

void AGuidedMissileProjectile::ReleaseMissile()
{
    if (!bIsPossessed || !OwningController)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot release missile: Not possessed or no controller"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Releasing missile control"));
    
    // Re-enable look input
    OwningController->SetIgnoreLookInput(false);
    
    // Return view to original pawn
    if (OriginalPawn && IsValid(OriginalPawn))
    {
        OwningController->SetViewTargetWithBlend(OriginalPawn, 0.5f);
        UE_LOG(LogTemp, Warning, TEXT("Returned view to original pawn: %s"), *OriginalPawn->GetName());
        
        // Notify the pawn that it's no longer guiding a missile
        AWormCharacter* Character = Cast<AWormCharacter>(OriginalPawn);
        if (Character)
        {
            Character->bIsGuidingMissile = false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Original pawn invalid, cannot return view"));
    }
    
    // Deactivate camera
    if (MissileCamera)
    {
        MissileCamera->Activate(false);
    }
    
    // Reset possession flags
    bIsPossessed = false;
    OwningController = nullptr;
    OriginalPawn = nullptr;
    
    UE_LOG(LogTemp, Warning, TEXT("Missile released"));
}

void AGuidedMissileProjectile::OnFuelExpired()
{
    UE_LOG(LogTemp, Warning, TEXT("Missile fuel expired! Adding gravity."));
    
    // Release control first
    if (bIsPossessed)
    {
        ReleaseMissile();
    }
    
    // Add gravity - this causes the missile to fall naturally
    if (ProjectileMovement)
    {
        ProjectileMovement->ProjectileGravityScale = 1.0f;
        UE_LOG(LogTemp, Warning, TEXT("Gravity applied to missile"));
    }
    
    // Don't explode immediately - let it fall and hit something
    // Set a backup timer to explode after 5 seconds if it hasn't hit anything
    FTimerHandle BackupExplosionTimer;
    GetWorld()->GetTimerManager().SetTimer(
        BackupExplosionTimer,
        this,
        &AGuidedMissileProjectile::ForceExplodeAfterDelay,
        5.0f,
        false
    );
}

void AGuidedMissileProjectile::ForceExplodeAfterDelay()
{
    // Only explode if we're still alive
    if (IsValid(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("Backup timer expired, forcing missile explosion"));
        Explode();
    }
}

void AGuidedMissileProjectile::Destroyed()
{
    // Make sure to release control before destruction
    if (bIsPossessed)
    {
        ReleaseMissile();
    }
    
    Super::Destroyed();
}

void AGuidedMissileProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, 
                            UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // If we're not supposed to collide yet, ignore hits
    if (!bCanCollide)
    {
        UE_LOG(LogTemp, Warning, TEXT("Missile hit ignored - collisions not enabled yet"));
        return;
    }
    
    // Log the hit information
    UE_LOG(LogTemp, Warning, TEXT("Guided Missile Hit: %s at %s"), 
        OtherActor ? *OtherActor->GetName() : TEXT("NULL"),
        *Hit.Location.ToString());
    
    // Release the missile first if it's being controlled
    if (bIsPossessed)
    {
        ReleaseMissile();
    }
    
    // Call the base class implementation which will handle the explosion
    Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
    
    // Draw debug for the hit
    DrawDebugSphere(
        GetWorld(), 
        Hit.Location, 
        20.0f, 
        12, 
        FColor::Red, 
        false, 
        5.0f
    );
}