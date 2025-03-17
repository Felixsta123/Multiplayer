#include "MeleeBatWeapon.h"
#include "AWormCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"

AMeleeBatWeapon::AMeleeBatWeapon()
{
    // Set default values
    HitDistance = 150.0f;
    HitAngle = 60.0f;
    BaseDamage = 30.0f;
    KnockbackForce = 1500.0f;
    BatTipSocketName = "MuzzleSocket"; // Using the existing socket
    bDrawDebugTrace = false;

    // Animation defaults
    SwingDuration = 0.5f;          // Increased from 0.3f to make animation slower and more visible
    SwingAngle = 180.0f;           // Increased from 120.0f for more dramatic movement
    SwingAxis = FVector(1, 0, 0);  // Changed to X-axis which might be more visible for your bat mesh
    SwingReturnDelay = 0.2f;       // Increased delay before return
    bIsSwinging = false;
    bIsReturning = false;
    SwingTime = 0.0f;

    // Melee weapons don't need a projectile class
    ProjectileClass = nullptr;

    // Melee weapons typically have quicker cooldowns
    ReloadTime = 0.5f;
    MaxAmmo = 99999; // Effectively unlimited ammo for a melee weapon
    AmmoCount = MaxAmmo;

    // Make sure Tick is enabled for the animation
    PrimaryActorTick.bCanEverTick = true;
}

void AMeleeBatWeapon::BeginPlay()
{
    Super::BeginPlay();
}

void AMeleeBatWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update swing animation
    if (bIsSwinging && !bIsReturning)
    {
        UpdateSwingAnimation(DeltaTime);
    }

    // Handle swing timing and hit detection
    if (bIsSwinging)
    {
        SwingTime += DeltaTime;

        // Hit trace during the middle of the swing (when the bat would be moving fastest)
        if (SwingTime >= SwingDuration * 0.3f && SwingTime <= SwingDuration * 0.7f)
        {
            PerformMeleeTrace();
        }

        // End swing
        if (SwingTime >= SwingDuration && !bIsReturning)
        {
            FinishSwingAnimation();
        }
    }
}

void AMeleeBatWeapon::Fire()
{
    // Check if we can fire
    if (HasAuthority() && AmmoCount > 0 && !bIsReloading && !bIsSwinging)
    {
        // Reduce ammo (optional for a melee weapon)
        AmmoCount--;

        // Start swinging
        bIsSwinging = true;
        bIsReturning = false;
        SwingTime = 0.0f;
        AlreadyHitActors.Empty();

        // Play swing effects
        Multicast_Swing();

        // Perform initial trace (for very fast swings or if Tick rate is low)
        PerformMeleeTrace();

        // Hide trajectory when swinging
        ShowTrajectory(false);

        // Start reload timer if needed (for cooldown)
        if (AmmoCount <= 0)
        {
            bIsReloading = true;
            GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AMeleeBatWeapon::OnReloadComplete, ReloadTime, false);
        }
    }
}

void AMeleeBatWeapon::Multicast_Swing_Implementation()
{
    // Store the original rotation
    if (WeaponMesh)
    {
        OriginalRotation = WeaponMesh->GetRelativeRotation();
        
        // Create a more dramatic swing by using specific rotator values
        // rather than just adding to the original rotation
        if (SwingAxis.X > 0)
        {
            // X-axis swing (pitch)
            TargetRotation = FRotator(OriginalRotation.Pitch - SwingAngle, OriginalRotation.Yaw, OriginalRotation.Roll);
        }
        else if (SwingAxis.Y > 0)
        {
            // Y-axis swing (yaw)
            TargetRotation = FRotator(OriginalRotation.Pitch, OriginalRotation.Yaw - SwingAngle, OriginalRotation.Roll);
        }
        else
        {
            // Z-axis swing (roll)
            TargetRotation = FRotator(OriginalRotation.Pitch, OriginalRotation.Yaw, OriginalRotation.Roll - SwingAngle);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Swing Animation Started: Original=%s, Target=%s"), 
               *OriginalRotation.ToString(), *TargetRotation.ToString());
    }

    // Play swing sound
    if (SwingSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, SwingSound, GetActorLocation());
    }

    // Start the swing animation
    StartSwingAnimation();

    // Create swing trail effect
    if (SwingTrailEffect && WeaponMesh && WeaponMesh->DoesSocketExist(BatTipSocketName))
    {
        TrailParticleComponent = UGameplayStatics::SpawnEmitterAttached(
            SwingTrailEffect,
            WeaponMesh,
            BatTipSocketName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            FVector(1.0f),
            EAttachLocation::SnapToTarget,
            true
        );
        
        // Set lifetime to match swing duration
        if (TrailParticleComponent)
        {
            // This will automatically destroy the trail after the swing
            TrailParticleComponent->bAutoDestroy = true;
            
            // Schedule destruction using a timer
            FTimerHandle DestroyTimerHandle;
            GetWorldTimerManager().SetTimer(
                DestroyTimerHandle,
                FTimerDelegate::CreateLambda([this]() {
                    if (TrailParticleComponent)
                    {
                        TrailParticleComponent->DeactivateSystem();
                        TrailParticleComponent = nullptr;
                    }
                }),
                SwingDuration * 1.5f,
                false
            );
        }
    }
}

void AMeleeBatWeapon::StartSwingAnimation()
{
    // Initialize animation state
    SwingTime = 0.0f;
    bIsSwinging = true;
    bIsReturning = false;
    
    // Clear any existing timers
    GetWorldTimerManager().ClearTimer(SwingTimerHandle);
    GetWorldTimerManager().ClearTimer(ReturnTimerHandle);
}

void AMeleeBatWeapon::UpdateSwingAnimation(float DeltaTime)
{
    if (!WeaponMesh || !bIsSwinging)
        return;
    
    // Calculate animation progress
    float Alpha = FMath::Clamp(SwingTime / SwingDuration, 0.0f, 1.0f);
    
    // Use a sine curve for smoother, more natural motion
    float SmoothAlpha = FMath::Sin(Alpha * PI * 0.5f);
    
    // Interpolate between original and target rotation
    FRotator NewRotation = FMath::Lerp(OriginalRotation, TargetRotation, SmoothAlpha);
    
    // Apply the new rotation
    WeaponMesh->SetRelativeRotation(NewRotation);
    
    // Debug visualization
    if (bDrawDebugTrace && GetWorld())
    {
        // Draw debug info every few frames to avoid spam
        static int32 DebugFrameCount = 0;
        if (++DebugFrameCount % 10 == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Bat Animation: Alpha=%.2f, Rotation=%s"), 
                   SmoothAlpha, *NewRotation.ToString());
            
            // Draw the swing arc
            FVector TipLocation = WeaponMesh->GetSocketLocation(BatTipSocketName);
            DrawDebugSphere(GetWorld(), TipLocation, 5.0f, 8, FColor::Yellow, false, 0.1f);
        }
    }
}

void AMeleeBatWeapon::FinishSwingAnimation()
{
    if (!WeaponMesh)
        return;
    
    // Mark as returning
    bIsReturning = true;
    
    // Wait a short delay before returning to the original position
    GetWorldTimerManager().SetTimer(ReturnTimerHandle, this, &AMeleeBatWeapon::ReturnToOriginalPosition, SwingReturnDelay, false);
}

void AMeleeBatWeapon::ReturnToOriginalPosition()
{
    if (!WeaponMesh)
        return;
    
    // Create a timer to smoothly return to original position
    float ReturnDuration = SwingDuration * 0.5f; // Return faster than the swing
    float ElapsedTime = 0.0f;
    FRotator CurrentRotation = WeaponMesh->GetRelativeRotation();
    
    // Clear any existing timers
    GetWorldTimerManager().ClearTimer(SwingTimerHandle);
    
    // Setup a timer to animate the return
    GetWorldTimerManager().SetTimer(SwingTimerHandle, [this, CurrentRotation, ReturnDuration, ElapsedTime]() mutable {
        if (!WeaponMesh)
        {
            GetWorldTimerManager().ClearTimer(SwingTimerHandle);
            return;
        }
        
        ElapsedTime += 0.016f; // Approximately 60fps
        float Alpha = FMath::Clamp(ElapsedTime / ReturnDuration, 0.0f, 1.0f);
        
        // Use ease-out curve for smooth return
        float SmoothAlpha = 1.0f - FMath::Pow(1.0f - Alpha, 2.0f);
        
        // Interpolate back to original rotation
        FRotator NewRotation = FMath::Lerp(CurrentRotation, OriginalRotation, SmoothAlpha);
        WeaponMesh->SetRelativeRotation(NewRotation);
        
        // Check if we're done
        if (Alpha >= 1.0f)
        {
            // Reset state
            bIsSwinging = false;
            bIsReturning = false;
            SwingTime = 0.0f;
            
            // Clear the timer
            GetWorldTimerManager().ClearTimer(SwingTimerHandle);
        }
    }, 0.016f, true); // Update approximately every frame
}

void AMeleeBatWeapon::PerformMeleeTrace()
{
    if (!HasAuthority() || !GetOwner())
        return;

    // Get the owner of the weapon
    AWormCharacter* OwnerChar = Cast<AWormCharacter>(GetOwner());
    if (!OwnerChar)
        return;

    // Get the tip of the bat as the starting point for our trace
    FVector TraceStart;
    FRotator TraceRot;

    if (WeaponMesh && WeaponMesh->DoesSocketExist(BatTipSocketName))
    {
        TraceStart = WeaponMesh->GetSocketLocation(BatTipSocketName);
        TraceRot = WeaponMesh->GetSocketRotation(BatTipSocketName);
    }
    else
    {
        // Fallback to weapon location and rotation
        TraceStart = GetActorLocation();
        TraceRot = GetActorRotation();
    }

    // Create a set of traces in a cone pattern for better hit detection
    TArray<FVector> TraceDirections;
    
    // Forward direction is the main direction
    FVector ForwardVector = TraceRot.Vector();
    TraceDirections.Add(ForwardVector);
    
    // Add angled directions based on the HitAngle to form a cone
    const int32 NumExtraTraces = 4; // Number of extra traces around the main trace
    float AngleIncrement = HitAngle / NumExtraTraces;
    
    for (int32 i = 1; i <= NumExtraTraces; i++)
    {
        // Create trace directions in a cone pattern
        float CurrentAngle = i * AngleIncrement;
        FVector RightOffset = TraceRot.RotateVector(FVector::RightVector) * FMath::Sin(FMath::DegreesToRadians(CurrentAngle));
        FVector UpOffset = TraceRot.RotateVector(FVector::UpVector) * FMath::Sin(FMath::DegreesToRadians(CurrentAngle));
        
        // Add offset directions in different angles
        TraceDirections.Add((ForwardVector * FMath::Cos(FMath::DegreesToRadians(CurrentAngle))) + RightOffset);
        TraceDirections.Add((ForwardVector * FMath::Cos(FMath::DegreesToRadians(CurrentAngle))) - RightOffset);
        TraceDirections.Add((ForwardVector * FMath::Cos(FMath::DegreesToRadians(CurrentAngle))) + UpOffset);
        TraceDirections.Add((ForwardVector * FMath::Cos(FMath::DegreesToRadians(CurrentAngle))) - UpOffset);
    }
    
    // Collect actors to ignore
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);
    ActorsToIgnore.Add(GetOwner());
    
    // Collect hit results from all traces
    for (const FVector& Direction : TraceDirections)
    {
        FVector TraceEnd = TraceStart + Direction * HitDistance;
        
        // Debug visualization
        if (bDrawDebugTrace)
        {
            DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Red, false, 1.0f, 0, 2.0f);
        }
        
        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActors(ActorsToIgnore);
        QueryParams.bTraceComplex = true;
        
        // Perform the trace
        if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Pawn, QueryParams))
        {
            // We hit something!
            if (HitResult.GetActor())
            {
                AWormCharacter* HitChar = Cast<AWormCharacter>(HitResult.GetActor());
                
                // Check if it's a character and we haven't already hit it in this swing
                if (HitChar && !AlreadyHitActors.Contains(HitChar))
                {
                    // Add to already hit list to prevent multiple hits
                    AlreadyHitActors.Add(HitChar);
                    
                    // Process the hit
                    ProcessHit(HitChar, HitResult.ImpactPoint, Direction);
                    
                    // Debug visualization of hit
                    if (bDrawDebugTrace)
                    {
                        DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 10.0f, 8, FColor::Green, false, 1.0f);
                    }
                }
            }
        }
    }
}

void AMeleeBatWeapon::ProcessHit(AActor* HitActor, const FVector& HitLocation, const FVector& HitDirection)
{
    AWormCharacter* HitChar = Cast<AWormCharacter>(HitActor);
    if (!HitChar)
        return;
    
    // Get the normalized direction for knockback
    FVector KnockbackDirection = HitDirection.GetSafeNormal();
    
    // Add a slight upward component to the knockback for better gameplay feel
    KnockbackDirection.Z += 0.5f;
    KnockbackDirection.Normalize();
    
    // Calculate final knockback force
    FVector FinalKnockback = KnockbackDirection * KnockbackForce;
    
    // Apply damage and knockback to the hit character
    HitChar->ApplyDamageToWorm(BaseDamage, FinalKnockback);
    
    // Play hit effects
    if (HitSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), HitSound, HitLocation);
    }
    
    if (HitEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitEffect, HitLocation, KnockbackDirection.Rotation());
    }
}