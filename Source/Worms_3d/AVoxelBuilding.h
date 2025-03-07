#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "AVoxelBuilding.generated.h"

// Structure pour stocker les informations d'un voxel
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
 * Classe qui génère un bâtiment procédural basé sur des voxels
 */
UCLASS()
class WORMS_3D_API AVoxelBuilding : public AActor
{
    GENERATED_BODY()
    
public:    
    AVoxelBuilding();

    virtual void OnConstruction(const FTransform& Transform) override;
    
    // Propriétés pour configurer le bâtiment
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
    
    // Configuration du mesh procédural
    // Pour résoudre les problèmes de rendu des faces
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Rendering")
    bool bUseDoubleSidedGeometry = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Rendering")
    bool bEnableCollision = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Rendering")
    float CubeMargin = 0.02f;
    
    // Fonction pour générer le bâtiment
    UFUNCTION(BlueprintCallable, Category = "Building")
    void GenerateBuilding();
    
    // Fonction pour détruire une partie du bâtiment
    UFUNCTION(BlueprintCallable, Category = "Building")
    void DestroyVoxelsAt(FVector Location, float Radius);
    
protected:
    virtual void BeginPlay() override;
    
    // Composant de mesh procédural
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProceduralMeshComponent* BuildingMesh;
    
    // Données des voxels
    TArray<TArray<TArray<FVoxelData>>> VoxelGrid;
    
    // Initialiser la grille de voxels
    void InitializeVoxelGrid();
    
    // Créer le mesh pour la grille de voxels
    void CreateMesh();
    
    // Fonction pour ajouter un cube à la position spécifiée
    void AddVoxelToMesh(int32 X, int32 Y, int32 Z, TArray<FVector>& Vertices, TArray<int32>& Triangles, 
                        TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FColor>& Colors, TArray<FProcMeshTangent>& Tangents);
    
    // Fonction pour vérifier si un voxel est visible (a des faces exposées)
    bool IsVoxelVisible(int32 X, int32 Y, int32 Z);
    
    // Fonction pour obtenir une couleur aléatoire
    FColor GetRandomColor();
    
    // Fonction pour appliquer un lissage aux sommets
    void SmoothVertices(TArray<FVector>& Vertices, TArray<int32>& Triangles);
};