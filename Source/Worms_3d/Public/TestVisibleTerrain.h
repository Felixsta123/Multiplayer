#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TestVisibleTerrain.generated.h"

UCLASS()
class WORMS_3D_API ATestVisibleTerrain : public AActor
{
	GENERATED_BODY()
    
	public:    
	ATestVisibleTerrain();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TerrainMesh;
    
	UPROPERTY(EditDefaultsOnly, Category = "Test")
	UMaterialInterface* TerrainMaterial;
    
	UFUNCTION(BlueprintCallable, Category = "Test")
	void SetDimensions(float Width, float Height, float Depth);

	protected:
	virtual void BeginPlay() override;
};