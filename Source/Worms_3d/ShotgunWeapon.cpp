// ShotgunWeapon.cpp
#include "ShotgunWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "AWormsProjectile.h"

AShotgunWeapon::AShotgunWeapon()
{
    // Default values
    PelletCount = 8;
    SpreadAngle = 15.0f;
    PelletDamageMultiplier = 0.5f;
    
    // Set reduced ammo for balance
    MaxAmmo = 3;
    AmmoCount = MaxAmmo;
    
    // Longer reload time
    ReloadTime = 3.0f;
}

void AShotgunWeapon::Fire()
{
    // Skip if checks from base class fail
    if (!HasAuthority() || !ProjectileClass || AmmoCount <= 0 || bIsReloading)
    {
        return;
    }
    
    // Reduce ammo
    AmmoCount--;
    
    // Get muzzle position and rotation
    FVector MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
    FRotator MuzzleRotation = WeaponMesh->GetSocketRotation(MuzzleSocketName);
    FVector LaunchDirection = MuzzleRotation.Vector();
    
    // Spawn position offset to avoid collisions with the weapon
    FVector SpawnLocation = MuzzleLocation + (LaunchDirection * 100.0f);
    
    // Spawn params
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = Cast<APawn>(GetOwner());
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    // Spawn multiple pellets
    for (int32 i = 0; i < PelletCount; ++i)
    {
        // Calculate random spread for this pellet
        float HorizontalSpread = FMath::RandRange(-SpreadAngle, SpreadAngle);
        float VerticalSpread = FMath::RandRange(-SpreadAngle, SpreadAngle);
        
        // Apply spread to rotation
        FRotator PelletRotation = MuzzleRotation;
        PelletRotation.Pitch += VerticalSpread;
        PelletRotation.Yaw += HorizontalSpread;
        
        // Get the adjusted direction
        FVector PelletDirection = PelletRotation.Vector();
        
        // Spawn the pellet
        AWormProjectile* Projectile = GetWorld()->SpawnActor<AWormProjectile>(
            ProjectileClass,
            SpawnLocation,
            PelletRotation,
            SpawnParams
        );
        
        if (Projectile)
        {
            // Initialize with reduced damage
            Projectile->ExplosionDamage *= PelletDamageMultiplier;
            // Smaller explosion radius
            Projectile->ExplosionRadius *= 0.7f;
            // Add some random velocity variation
            float PowerVariation = FMath::RandRange(0.9f, 1.1f);
            Projectile->InitializeProjectile(PelletDirection, FirePower * PowerVariation);
        }
    }
    
    // Hide trajectory now that we've fired
    ShowTrajectory(false);
    
    // Play effects on all clients
    Multicast_OnFire();
    
    // Start reload if needed
    if (AmmoCount <= 0)
    {
        bIsReloading = true;
        GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AWormWeapon::OnReloadComplete, ReloadTime, false);
    }
}