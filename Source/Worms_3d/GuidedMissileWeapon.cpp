#include "GuidedMissileWeapon.h"
#include "GuidedMissileProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "AWormCharacter.h"
#include "GameFramework/PlayerController.h"

AGuidedMissileWeapon::AGuidedMissileWeapon()
{
    // Ensure Tick is enabled
    PrimaryActorTick.bCanEverTick = true;
    
    // Configure weapon properties
    MaxAmmo = 3;
    AmmoCount = MaxAmmo;
    ReloadTime = 5.0f;
    
    // No active missile at start
    ActiveMissile = nullptr;
}

void AGuidedMissileWeapon::Fire()
{
    UE_LOG(LogTemp, Warning, TEXT("GuidedMissileWeapon::Fire() called - HasAuthority: %s"), 
        HasAuthority() ? TEXT("Yes") : TEXT("No"));
    
    // Skip if standard checks fail
    if (!HasAuthority() || !ProjectileClass || AmmoCount <= 0 || bIsReloading)
    {
        UE_LOG(LogTemp, Warning, TEXT("Fire conditions not met - Authority: %s, ProjectileClass: %s, AmmoCount: %d, IsReloading: %s"),
            HasAuthority() ? TEXT("Yes") : TEXT("No"),
            ProjectileClass ? TEXT("Valid") : TEXT("NULL"),
            AmmoCount,
            bIsReloading ? TEXT("Yes") : TEXT("No"));
        return;
    }
    
    // Make sure we don't already have an active missile
    if (ActiveMissile != nullptr && IsValid(ActiveMissile))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot fire: Active missile already exists"));
        return;
    }
    
    // Reduce ammo
    AmmoCount--;
    
    // Get spawn location and direction
    FVector MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
    FRotator MuzzleRotation = WeaponMesh->GetSocketRotation(MuzzleSocketName);
    FVector LaunchDirection = MuzzleRotation.Vector();
    
    // Offset to avoid collision with weapon
    FVector SpawnLocation = MuzzleLocation + (LaunchDirection * 250.0f); // Increased offset
    
    UE_LOG(LogTemp, Warning, TEXT("Spawning missile at %s with rotation %s"),
        *SpawnLocation.ToString(), *MuzzleRotation.ToString());
    
    // Ensure we're using the right class from ProjectileClass
    TSubclassOf<AGuidedMissileProjectile> MissileClass = nullptr;
    
    // Try to cast the base projectile class to our specific missile class
    if (ProjectileClass->IsChildOf(AGuidedMissileProjectile::StaticClass()))
    {
        MissileClass = *reinterpret_cast<TSubclassOf<AGuidedMissileProjectile>*>(&ProjectileClass);
        UE_LOG(LogTemp, Warning, TEXT("Using valid missile class: %s"), *MissileClass->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ProjectileClass is not a AGuidedMissileProjectile!"));
        return;
    }
    
    // Spawn parameters
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = Cast<APawn>(GetOwner());
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    // Spawn the missile
    ActiveMissile = GetWorld()->SpawnActor<AGuidedMissileProjectile>(
        MissileClass,
        SpawnLocation,
        MuzzleRotation,
        SpawnParams
    );
    
    if (ActiveMissile)
    {
        UE_LOG(LogTemp, Warning, TEXT("Missile spawned successfully: %s"), *ActiveMissile->GetName());
        
        // Initialize the missile
        ActiveMissile->InitializeProjectile(LaunchDirection, FirePower);
        
        // Draw a debug sphere at the missile's location
        // DrawDebugSphere(
        //     GetWorld(),
        //     SpawnLocation,
        //     20.0f,
        //     8,
        //     FColor::Yellow,
        //     false,
        //     3.0f
        // );
        
        // Try to possess the missile immediately
        APawn* OwnerPawn = Cast<APawn>(GetOwner());
        if (OwnerPawn)
        {
            APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
            if (PC)
            {
                UE_LOG(LogTemp, Warning, TEXT("Possessing missile immediately with controller: %s"), *PC->GetName());
                ActiveMissile->PossessMissile(PC);
                
                // Notify the character that it's now controlling a missile
                AWormCharacter* Character = Cast<AWormCharacter>(OwnerPawn);
                if (Character)
                {
                    Character->bIsGuidingMissile = true;
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("No valid player controller found!"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("No valid owner pawn!"));
        }
        
        // Set a backup timer in case immediate possession fails
        FTimerHandle BackupPossessionTimer;
        GetWorld()->GetTimerManager().SetTimer(
            BackupPossessionTimer,
            [this]()
            {
                UE_LOG(LogTemp, Warning, TEXT("Backup possession timer triggered"));
                
                if (ActiveMissile && IsValid(ActiveMissile))
                {
                    // Only try to possess if not already possessed
                    if (!ActiveMissile->IsPossessed())
                    {
                        APawn* OwnerPawn = Cast<APawn>(GetOwner());
                        if (OwnerPawn)
                        {
                            APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
                            if (PC)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("Using backup possession with controller: %s"), *PC->GetName());
                                ActiveMissile->PossessMissile(PC);
                            }
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Backup timer: Missile already possessed"));
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Backup timer: No valid ActiveMissile to possess!"));
                }
            },
            0.2f,
            false
        );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn missile!"));
    }
    
    // Hide trajectory
    ShowTrajectory(false);
    
    // Play effects
    Multicast_OnFire();
    
    // Start reload if needed
    if (AmmoCount <= 0)
    {
        bIsReloading = true;
        GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AWormWeapon::OnReloadComplete, ReloadTime, false);
    }
}

void AGuidedMissileWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // ONLY clear the ActiveMissile reference if it's invalid
    // Previously this code was incorrectly written
    if (ActiveMissile && !IsValid(ActiveMissile))
    {
        UE_LOG(LogTemp, Warning, TEXT("Active missile is no longer valid, clearing reference"));
        ActiveMissile = nullptr;
        
        // Notify the owner that guidance is over
        APawn* OwnerPawn = Cast<APawn>(GetOwner());
        if (OwnerPawn)
        {
            AWormCharacter* Character = Cast<AWormCharacter>(OwnerPawn);
            if (Character)
            {
                Character->bIsGuidingMissile = false;
            }
        }
    }
}

void AGuidedMissileWeapon::ProcessMissileMovementInput(float YawInput, float PitchInput)
{
    if (ActiveMissile && IsValid(ActiveMissile))
    {
        // Pass inputs to the missile
        ActiveMissile->ApplyYawInput(YawInput);
        ActiveMissile->ApplyPitchInput(PitchInput);
    }
}

void AGuidedMissileWeapon::AbortGuidance()
{
    if (ActiveMissile && IsValid(ActiveMissile))
    {
        // Release control
        ActiveMissile->ReleaseMissile();
        
        // Trigger explosion
        ActiveMissile->Explode();
        
        // Clear reference
        ActiveMissile = nullptr;
        
        // Notify the owner pawn that missile guidance is over
        APawn* OwnerPawn = Cast<APawn>(GetOwner());
        if (OwnerPawn)
        {
            AWormCharacter* Character = Cast<AWormCharacter>(OwnerPawn);
            if (Character)
            {
                Character->bIsGuidingMissile = false;
            }
        }
    }
}
