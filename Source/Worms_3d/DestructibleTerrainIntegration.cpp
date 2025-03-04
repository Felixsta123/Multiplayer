#include "DestructibleTerrainIntegration.h"
#include "TestWormGameMode.h"
#include "ADestructibleTerrain.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UDestructibleTerrainIntegration::UDestructibleTerrainIntegration()
{
    PrimaryComponentTick.bCanEverTick = false;
    bUseChunkBasedTerrain = true;
    OldTerrain = nullptr;
}

void UDestructibleTerrainIntegration::BeginPlay()
{
    Super::BeginPlay();

    // Get the game mode
    ATestWormGameMode* GameMode = Cast<ATestWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("DestructibleTerrainIntegration: Game mode not found or not a TestWormGameMode"));
        return;
    }
    
    // Store reference to old terrain if it exists
    OldTerrain = GameMode->OldDestructibleTerrain;
    
    // Spawn new terrain if needed
    if (bUseChunkBasedTerrain)
    {
        AChunkBasedDestructibleTerrain* Terrain = SpawnTerrain();
        
        if (Terrain)
        {
            UE_LOG(LogTemp, Warning, TEXT("DestructibleTerrainIntegration: Using new chunk-based terrain system"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("DestructibleTerrainIntegration: Failed to spawn new terrain, falling back to old system"));
            bUseChunkBasedTerrain = false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DestructibleTerrainIntegration: Using original terrain system"));
    }
}

void UDestructibleTerrainIntegration::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    
    // Clean up if needed
    if (ChunkBasedTerrain && EndPlayReason == EEndPlayReason::Destroyed)
    {
        ChunkBasedTerrain->Destroy();
        ChunkBasedTerrain = nullptr;
    }
}

AChunkBasedDestructibleTerrain* UDestructibleTerrainIntegration::SpawnTerrain()
{
    if (ChunkBasedTerrain)
    {
        return ChunkBasedTerrain;
    }
    
    // Make sure we have a valid class to spawn
    if (!ChunkBasedTerrainClass)
    {
        UE_LOG(LogTemp, Error, TEXT("DestructibleTerrainIntegration: ChunkBasedTerrainClass not set"));
        return nullptr;
    }
    
    // Spawn new terrain actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    ChunkBasedTerrain = GetWorld()->SpawnActor<AChunkBasedDestructibleTerrain>(
        ChunkBasedTerrainClass,
        FVector(0, 0, 0),
        FRotator::ZeroRotator,
        SpawnParams
    );
    
    if (ChunkBasedTerrain)
    {
        // Configure the terrain
        ChunkBasedTerrain->TerrainSize = TerrainSize;
        ChunkBasedTerrain->VoxelSize = VoxelSize;
        ChunkBasedTerrain->ChunkSize = ChunkSize;
        ChunkBasedTerrain->TerrainType = TerrainType;
        
        if (TerrainMaterial)
        {
            ChunkBasedTerrain->TerrainMaterial = TerrainMaterial;
        }
        
        // Move to a suitable location (same as old terrain)
        if (OldTerrain)
        {
            ChunkBasedTerrain->SetActorLocation(OldTerrain->GetActorLocation());
        }
        else
        {
            // Default position if old terrain not available
            ChunkBasedTerrain->SetActorLocation(FVector(-1000.0f, -100.0f, -2250.0f));
        }
        
        // Generate the terrain
        ChunkBasedTerrain->GenerateTerrain();
    }
    
    return ChunkBasedTerrain;
}

void UDestructibleTerrainIntegration::RequestDestroyTerrainAt(const FVector2D& Position, const FVector2D& Size)
{
    if (bUseChunkBasedTerrain && ChunkBasedTerrain)
    {
        // Calculate center of the destruction area
        FVector Center = FVector(
            Position.X + Size.X * 0.5f,
            Position.Y + Size.Y * 0.5f,
            0.0f // Use 0 for Z as this is 2D input
        );
        
        // Estimate a radius that would cover the area
        float Radius = FMath::Max(Size.X, Size.Y) * 0.5f;
        
        // Apply to the new terrain system
        ChunkBasedTerrain->ApplyTerrainModification(Center, Radius);
    }
    else if (OldTerrain)
    {
        // Fall back to old system
        OldTerrain->RequestDestroyTerrainAt(Position, Size);
    }
}

void UDestructibleTerrainIntegration::ApplyExplosionAt(const FVector& Position, float Radius, float Falloff)
{
    if (bUseChunkBasedTerrain && ChunkBasedTerrain)
    {
        // Apply to the new terrain system
        ChunkBasedTerrain->ApplyTerrainModification(Position, Radius, Falloff);
    }
    else if (OldTerrain)
    {
        // For the old system, convert to 2D
        FVector2D Position2D(Position.X, Position.Z); // Old system uses X,Z as primary plane
        FVector2D Size2D(Radius * 2.0f, Radius * 2.0f);
        
        // Apply to old system
        OldTerrain->RequestDestroyTerrainAt(Position2D, Size2D);
    }
}

AActor* UDestructibleTerrainIntegration::GetActiveTerrain() const
{
    if (bUseChunkBasedTerrain && ChunkBasedTerrain)
    {
        return ChunkBasedTerrain;
    }
    else
    {
        return OldTerrain;
    }
}

void UDestructibleTerrainIntegration::ToggleTerrainSystem()
{
    // Toggle the system
    bUseChunkBasedTerrain = !bUseChunkBasedTerrain;
    
    // Get the game mode
    ATestWormGameMode* GameMode = Cast<ATestWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GameMode)
    {
        return;
    }
    
    if (bUseChunkBasedTerrain)
    {
        // Spawn the new terrain system if needed
        if (!ChunkBasedTerrain)
        {
            SpawnTerrain();
        }
        
        // Hide the old terrain
        if (OldTerrain)
        {
            OldTerrain->SetActorHiddenInGame(true);
            OldTerrain->SetActorEnableCollision(false);
        }
        
        // Show the new terrain
        if (ChunkBasedTerrain)
        {
            ChunkBasedTerrain->SetActorHiddenInGame(false);
            ChunkBasedTerrain->SetActorEnableCollision(true);
        }
    }
    else
    {
        // Hide the new terrain
        if (ChunkBasedTerrain)
        {
            ChunkBasedTerrain->SetActorHiddenInGame(true);
            ChunkBasedTerrain->SetActorEnableCollision(false);
        }
        
        // Recreate the old terrain if needed
        if (!OldTerrain)
        {
            GameMode->SpawnDestructibleTerrain();
            OldTerrain = GameMode->OldDestructibleTerrain;
        }
        else
        {
            // Show the old terrain
            OldTerrain->SetActorHiddenInGame(false);
            OldTerrain->SetActorEnableCollision(true);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Terrain system toggled to: %s"), 
        bUseChunkBasedTerrain ? TEXT("Chunk-based") : TEXT("Original"));
}

FString UDestructibleTerrainIntegration::GetActiveTerrainSystemName() const
{
    return bUseChunkBasedTerrain ? FString(TEXT("Chunk-based Marching Cubes")) : FString(TEXT("Original"));
}