#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "AVoxelBuilding.generated.h"

// Structure to store voxel data
USTRUCT(BlueprintType)
struct FVoxelData
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    bool bIsActive;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FColor Color;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    int32 MaterialIndex;
    
    FVoxelData()
    {
        bIsActive = true;
        Color = FColor::White;
        MaterialIndex = 0;
    }
};

/**
 * Class that generates a procedural voxel-based building
 */
UCLASS()
class WORMS_3D_API AImprovedVoxelBuilding : public AActor
{
    GENERATED_BODY()
    
public:    
    AImprovedVoxelBuilding();

    virtual void OnConstruction(const FTransform& Transform) override;
    
    // Properties to configure the building
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
    int32 GridSizeX;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
    int32 GridSizeY;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
    int32 GridSizeZ;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
    float VoxelSize;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
    float SmoothingFactor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
    bool bUseRandomColors;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building", meta = (EditCondition = "!bUseRandomColors"))
    FLinearColor BuildingColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
    bool bGenerateOnBeginPlay;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
    TArray<UMaterialInterface*> Materials;
    
    // Procedural mesh configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Rendering")
    bool bUseDoubleSidedGeometry;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Rendering")
    bool bEnableCollision;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Rendering")
    float CubeMargin;



    // Fonction statique pour localiser tous les buildings voxel
    UFUNCTION(BlueprintCallable, Category = "Building", meta = (WorldContext = "WorldContextObject"))
    static TArray<AImprovedVoxelBuilding*> FindAllVoxelBuildings(const UObject* WorldContextObject);

    // Function to generate the building
    UFUNCTION(BlueprintCallable, Category = "Building")
    void GenerateBuilding();
    
    // Function to destroy part of the building
    UFUNCTION(BlueprintCallable, Category = "Building")
    void DestroyVoxelsAt(FVector Location, FVector ImpactNormal, float Radius);
    // RPC serveur pour détruire des voxels (appelé par les clients)
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_DestroyVoxelsAt(FVector Location, FVector ImpactNormal, float Radius);

    // RPC multicast pour synchroniser la destruction sur tous les clients
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DestroyVoxelsAt(FVector Location, FVector ImpactNormal, float Radius);

protected:
    virtual void BeginPlay() override;
    
    // Procedural mesh component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProceduralMeshComponent* BuildingMesh;
    
    // Voxel data
    TArray<TArray<TArray<FVoxelData>>> VoxelGrid;
    
    // Initialize voxel grid
    void InitializeVoxelGrid();
    
    // Create mesh from voxel grid
    void CreateMesh();
    void GenerateOptimizedRayDirections(TArray<FVector>& RayDirections, const FVector& ImpactNormal, int32 NumRays);

    // Add only visible faces to reduce polygon count
    void AddVisibleFacesToMesh(int32 X, int32 Y, int32 Z, TArray<FVector>& Vertices, TArray<int32>& Triangles, 
                          TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FColor>& Colors, TArray<FProcMeshTangent>& Tangents,
                          bool bBottomFaceVisible, bool bTopFaceVisible, bool bLeftFaceVisible, bool bRightFaceVisible,
                          bool bBackFaceVisible, bool bFrontFaceVisible);   
    // Helper functions for mesh creation
    void AddFaceTriangles(TArray<int32>& Triangles, int32 BaseIndex, bool bReversed);
    void AddFaceNormals(TArray<FVector>& Normals, FVector Normal, int32 Count);
    void AddFaceUVs(TArray<FVector2D>& UVs);
    void AddFaceColors(TArray<FColor>& Colors, FColor Color, int32 Count);
    void AddFaceTangents(TArray<FProcMeshTangent>& Tangents, FVector Tangent, int32 Count);
    
    // Function to get a random color
    FColor GetRandomColor();
    
    // Function to apply smoothing to vertices
    void SmoothVertices(TArray<FVector>& Vertices, TArray<int32>& Triangles);
};