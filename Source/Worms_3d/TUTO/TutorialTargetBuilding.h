#pragma once

#include "CoreMinimal.h"
#include "Worms_3d/Building/AVoxelBuilding.h"
#include "TutorialTargetBuilding.generated.h"

/**
 * Special destructible building for tutorial targets that emit events when damaged
 */
UCLASS()
class WORMS_3D_API ATutorialTargetBuilding : public AImprovedVoxelBuilding
{
	GENERATED_BODY()
    
public:
	ATutorialTargetBuilding();
    
	// Delegate to broadcast when target is damaged
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetDamagedSignature);
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnTargetDamagedSignature OnTargetDamaged;
    
	// Delegate to broadcast when target is destroyed significantly
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetDestroyedSignature);
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnTargetDestroyedSignature OnTargetDestroyed;
    
	// Track damage for tutorial
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
	int32 DamageCount;
    
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial")
	bool bIsTargetDestroyed;
    
	// Threshold for "destroyed" state (percentage of voxels destroyed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	float DestroyedThreshold;
    
	// Override destruction function to track for tutorial
	virtual void DestroyVoxelsAt(FVector Location, FVector ImpactNormal, float Radius) override;
    
protected:
	// Total number of voxels at start
	int32 TotalVoxelCount;
    
	// Number of destroyed voxels
	int32 DestroyedVoxelCount;
    
	// Initialize voxel counts
	virtual void BeginPlay() override;
    
	// Function to count active voxels
	int32 CountActiveVoxels() const;
};