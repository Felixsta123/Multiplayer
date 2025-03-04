#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelBuilding.generated.h"

/**
 * Un acteur qui représente un bâtiment composé de voxels (cubes)
 * Pour le moment, il ne peut créer qu'un simple rectangle allongé
 */
UCLASS()
class WORMS_3D_API AVoxelBuilding : public AActor
{
	GENERATED_BODY()
    
public:    
	// Constructeur par défaut
	AVoxelBuilding();

	// Crée un rectangle de voxels
	UFUNCTION(BlueprintCallable, Category = "VoxelBuilding")
	void CreateRectangle(int32 Width, int32 Height, int32 Depth);

	// Taille d'un voxel individuel
	UPROPERTY(EditDefaultsOnly, Category = "VoxelBuilding")
	float VoxelSize;

	// Le mesh à utiliser pour chaque voxel
	UPROPERTY(EditDefaultsOnly, Category = "VoxelBuilding")
	UStaticMesh* VoxelMesh;

	// Le matériau par défaut pour les voxels
	UPROPERTY(EditDefaultsOnly, Category = "VoxelBuilding")
	UMaterialInterface* VoxelMaterial;

	// Activer/désactiver la physique pour les voxels individuels
	UPROPERTY(EditDefaultsOnly, Category = "VoxelBuilding")
	bool bEnableVoxelPhysics;

protected:
	// Appelé quand le jeu commence
	virtual void BeginPlay() override;

	// Tableau contenant tous les composants de voxel
	UPROPERTY()
	TArray<UStaticMeshComponent*> VoxelComponents;

	// Nettoyage des voxels existants
	void ClearVoxels();

	// Fonction pour créer un seul voxel
	UStaticMeshComponent* CreateVoxel(const FVector& Location);
};