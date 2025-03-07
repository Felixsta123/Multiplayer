#include "AVoxelBuilding.h"

AVoxelBuilding::AVoxelBuilding()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Créer le composant de mesh procédural
    BuildingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BuildingMesh"));
    RootComponent = BuildingMesh;
    BuildingMesh->bUseAsyncCooking = true;
    
    // Valeurs par défaut
    GridSizeX = 10;
    GridSizeY = 10;
    GridSizeZ = 10;
    VoxelSize = 100.0f;
    SmoothingFactor = 0.01f;
    bUseRandomColors = false;
    BuildingColor = FLinearColor(0.5f, 0.5f, 1.0f, 1.0f);
    bGenerateOnBeginPlay = true;
    bUseDoubleSidedGeometry = true;
    bEnableCollision = true;
    CubeMargin = 0.02f;
    
    // Rendre l'acteur réplicable
    bReplicates = true;
    BuildingMesh->SetIsReplicated(true);
}

void AVoxelBuilding::BeginPlay()
{
    Super::BeginPlay();
    
    if (bGenerateOnBeginPlay)
    {
        GenerateBuilding();
    }
}

void AVoxelBuilding::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    // Générer le bâtiment dans l'éditeur pour prévisualisation
    #if WITH_EDITOR
        GenerateBuilding();
    #endif
}

void AVoxelBuilding::GenerateBuilding()
{
    // Initialiser la grille de voxels
    InitializeVoxelGrid();
    
    // Créer le mesh du bâtiment
    CreateMesh();
}

void AVoxelBuilding::InitializeVoxelGrid()
{
    VoxelGrid.Empty();
    VoxelGrid.SetNum(GridSizeX);
    
    for (int32 X = 0; X < GridSizeX; X++)
    {
        VoxelGrid[X].SetNum(GridSizeY);
        
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            VoxelGrid[X][Y].SetNum(GridSizeZ);
            
            for (int32 Z = 0; Z < GridSizeZ; Z++)
            {
                FVoxelData& Voxel = VoxelGrid[X][Y][Z];
                
                // Tous les voxels sont actifs par défaut pour créer un cube plein
                Voxel.bIsActive = true;
                
                // Définir la couleur du voxel
                if (bUseRandomColors)
                {
                    Voxel.Color = GetRandomColor();
                }
                else
                {
                    Voxel.Color = BuildingColor.ToFColor(true);
                }
                
                // Ajouter un peu de variation pour les matériaux
                Voxel.MaterialIndex = FMath::RandRange(0, FMath::Max(0, Materials.Num() - 1));
            }
        }
    }
}

void AVoxelBuilding::CreateMesh()
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    
    // Nettoyer d'abord toutes les sections de mesh existantes
    BuildingMesh->ClearAllMeshSections();
    
    // Parcourir tous les voxels et ajouter ceux qui sont visibles
    for (int32 X = 0; X < GridSizeX; X++)
    {
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            for (int32 Z = 0; Z < GridSizeZ; Z++)
            {
                if (VoxelGrid[X][Y][Z].bIsActive && IsVoxelVisible(X, Y, Z))
                {
                    AddVoxelToMesh(X, Y, Z, Vertices, Triangles, Normals, UVs, Colors, Tangents);
                }
            }
        }
    }
    
    // Appliquer le lissage si nécessaire
    if (SmoothingFactor > 0.0f)
    {
        SmoothVertices(Vertices, Triangles);
    }
    
    // Vérifier que nous avons des données à ajouter
    if (Vertices.Num() > 0 && Triangles.Num() > 0)
    {
        // Configurer les propriétés du mesh procédural
        BuildingMesh->bUseComplexAsSimpleCollision = false;
        BuildingMesh->bReceivesDecals = true;
        
        // Activer la géométrie double face
        if (bUseDoubleSidedGeometry)
        {
            BuildingMesh->SetMaterial(0, Materials.IsValidIndex(0) ? Materials[0] : nullptr);
        }
        
        // Créer la section de mesh
        BuildingMesh->CreateMeshSection_LinearColor(
            0,                  // Section index
            Vertices,           // Vertices
            Triangles,          // Triangles
            Normals,            // Normals
            UVs,                // UV0
            TArray<FVector2D>(), // UV1
            TArray<FVector2D>(), // UV2
            TArray<FVector2D>(), // UV3
            TArray<FLinearColor>(), // Vertex colors (empty, will set later)
            Tangents,           // Tangents
            bEnableCollision    // Enable collision
        );
        
        // Définir les propriétés de la section
        BuildingMesh->SetMeshSectionVisible(0, true);
//        BuildingMesh->SetMeshSectionVertexColor(0, Colors);

        // Appliquer le matériau
        if (Materials.Num() > 0 && Materials[0] != nullptr)
        {
            BuildingMesh->SetMaterial(0, Materials[0]);
        }
        
        // S'assurer que les collisions sont correctement configurées
        if (bEnableCollision)
        {
            BuildingMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            BuildingMesh->SetCollisionObjectType(ECC_WorldStatic);
            BuildingMesh->SetCollisionResponseToAllChannels(ECR_Block);
        }
        else
        {
            BuildingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
}

bool AVoxelBuilding::IsVoxelVisible(int32 X, int32 Y, int32 Z)
{
    // Un voxel est visible s'il a au moins une face exposée
    
    // Vérifier les limites de la grille
    bool bXMinBorder = (X == 0);
    bool bXMaxBorder = (X == GridSizeX - 1);
    bool bYMinBorder = (Y == 0);
    bool bYMaxBorder = (Y == GridSizeY - 1);
    bool bZMinBorder = (Z == 0);
    bool bZMaxBorder = (Z == GridSizeZ - 1);
    
    // Si le voxel est sur la bordure, il est visible
    if (bXMinBorder || bXMaxBorder || bYMinBorder || bYMaxBorder || bZMinBorder || bZMaxBorder)
    {
        return true;
    }
    
    // Vérifier les six voisins
    bool bXMinEmpty = !VoxelGrid[X-1][Y][Z].bIsActive;
    bool bXMaxEmpty = !VoxelGrid[X+1][Y][Z].bIsActive;
    bool bYMinEmpty = !VoxelGrid[X][Y-1][Z].bIsActive;
    bool bYMaxEmpty = !VoxelGrid[X][Y+1][Z].bIsActive;
    bool bZMinEmpty = !VoxelGrid[X][Y][Z-1].bIsActive;
    bool bZMaxEmpty = !VoxelGrid[X][Y][Z+1].bIsActive;
    
    // Si au moins un voisin est vide, le voxel est visible
    return bXMinEmpty || bXMaxEmpty || bYMinEmpty || bYMaxEmpty || bZMinEmpty || bZMaxEmpty;
}

void AVoxelBuilding::AddVoxelToMesh(int32 X, int32 Y, int32 Z, TArray<FVector>& Vertices, TArray<int32>& Triangles, 
                         TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FColor>& Colors, TArray<FProcMeshTangent>& Tangents)
{
    // Position du centre du voxel
    FVector Center = FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
    
    // Taille du demi-voxel - légère réduction pour créer une séparation visuelle entre les cubes
    float HalfSize = VoxelSize * (0.5f - CubeMargin);
    
    // Couleur du voxel
    FColor VoxelColor = VoxelGrid[X][Y][Z].Color;
    
    // Index de base pour ce voxel
    int32 BaseIndex = Vertices.Num();
    
    // Définir les 8 sommets du cube
    Vertices.Add(Center + FVector(-HalfSize, -HalfSize, -HalfSize)); // 0: Bas Gauche Arrière
    Vertices.Add(Center + FVector(HalfSize, -HalfSize, -HalfSize));  // 1: Bas Droite Arrière
    Vertices.Add(Center + FVector(HalfSize, HalfSize, -HalfSize));   // 2: Bas Droite Avant
    Vertices.Add(Center + FVector(-HalfSize, HalfSize, -HalfSize));  // 3: Bas Gauche Avant
    Vertices.Add(Center + FVector(-HalfSize, -HalfSize, HalfSize));  // 4: Haut Gauche Arrière
    Vertices.Add(Center + FVector(HalfSize, -HalfSize, HalfSize));   // 5: Haut Droite Arrière
    Vertices.Add(Center + FVector(HalfSize, HalfSize, HalfSize));    // 6: Haut Droite Avant
    Vertices.Add(Center + FVector(-HalfSize, HalfSize, HalfSize));   // 7: Haut Gauche Avant
    
    // Ajouter les couleurs pour chaque sommet
    for (int32 i = 0; i < 8; i++)
    {
        Colors.Add(VoxelColor);
    }
    
    // Pour ce rendu, nous allons créer toutes les faces pour chaque cube
    // Face bas (Z-)
    Triangles.Add(BaseIndex + 0);
    Triangles.Add(BaseIndex + 2);
    Triangles.Add(BaseIndex + 1);
    
    Triangles.Add(BaseIndex + 0);
    Triangles.Add(BaseIndex + 3);
    Triangles.Add(BaseIndex + 2);
    
    // Ajouter les normales pour la face du bas (toutes vers le bas)
    for (int32 i = 0; i < 2; i++)
    {
        Normals.Add(FVector(0, 0, -1));
        Normals.Add(FVector(0, 0, -1));
        Normals.Add(FVector(0, 0, -1));
    }
    
    // Ajouter les coordonnées UV pour la face du bas
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 1));
    UVs.Add(FVector2D(1, 0));
    
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(0, 1));
    UVs.Add(FVector2D(1, 1));
    
    // Ajouter les tangentes
    for (int32 i = 0; i < 6; i++)
    {
        Tangents.Add(FProcMeshTangent(1, 0, 0));
    }
    
    // Face haut (Z+)
    Triangles.Add(BaseIndex + 4);
    Triangles.Add(BaseIndex + 5);
    Triangles.Add(BaseIndex + 6);
    
    Triangles.Add(BaseIndex + 4);
    Triangles.Add(BaseIndex + 6);
    Triangles.Add(BaseIndex + 7);
    
    // Ajouter les normales pour la face du haut (toutes vers le haut)
    for (int32 i = 0; i < 2; i++)
    {
        Normals.Add(FVector(0, 0, 1));
        Normals.Add(FVector(0, 0, 1));
        Normals.Add(FVector(0, 0, 1));
    }
    
    // Ajouter les coordonnées UV pour la face du haut
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 0));
    UVs.Add(FVector2D(1, 1));
    
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 1));
    UVs.Add(FVector2D(0, 1));
    
    // Ajouter les tangentes
    for (int32 i = 0; i < 6; i++)
    {
        Tangents.Add(FProcMeshTangent(1, 0, 0));
    }
    
    // Face gauche (X-)
    Triangles.Add(BaseIndex + 0);
    Triangles.Add(BaseIndex + 4);
    Triangles.Add(BaseIndex + 7);
    
    Triangles.Add(BaseIndex + 0);
    Triangles.Add(BaseIndex + 7);
    Triangles.Add(BaseIndex + 3);
    
    // Ajouter les normales pour la face gauche (toutes vers la gauche)
    for (int32 i = 0; i < 2; i++)
    {
        Normals.Add(FVector(-1, 0, 0));
        Normals.Add(FVector(-1, 0, 0));
        Normals.Add(FVector(-1, 0, 0));
    }
    
    // Ajouter les coordonnées UV pour la face gauche
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 0));
    UVs.Add(FVector2D(1, 1));
    
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 1));
    UVs.Add(FVector2D(0, 1));
    
    // Ajouter les tangentes
    for (int32 i = 0; i < 6; i++)
    {
        Tangents.Add(FProcMeshTangent(0, 1, 0));
    }
    
    // Face droite (X+)
    Triangles.Add(BaseIndex + 1);
    Triangles.Add(BaseIndex + 2);
    Triangles.Add(BaseIndex + 6);
    
    Triangles.Add(BaseIndex + 1);
    Triangles.Add(BaseIndex + 6);
    Triangles.Add(BaseIndex + 5);
    
    // Ajouter les normales pour la face droite (toutes vers la droite)
    for (int32 i = 0; i < 2; i++)
    {
        Normals.Add(FVector(1, 0, 0));
        Normals.Add(FVector(1, 0, 0));
        Normals.Add(FVector(1, 0, 0));
    }
    
    // Ajouter les coordonnées UV pour la face droite
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 0));
    UVs.Add(FVector2D(1, 1));
    
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 1));
    UVs.Add(FVector2D(0, 1));
    
    // Ajouter les tangentes
    for (int32 i = 0; i < 6; i++)
    {
        Tangents.Add(FProcMeshTangent(0, -1, 0));
    }
    
    // Face arrière (Y-)
    Triangles.Add(BaseIndex + 0);
    Triangles.Add(BaseIndex + 1);
    Triangles.Add(BaseIndex + 5);
    
    Triangles.Add(BaseIndex + 0);
    Triangles.Add(BaseIndex + 5);
    Triangles.Add(BaseIndex + 4);
    
    // Ajouter les normales pour la face arrière (toutes vers l'arrière)
    for (int32 i = 0; i < 2; i++)
    {
        Normals.Add(FVector(0, -1, 0));
        Normals.Add(FVector(0, -1, 0));
        Normals.Add(FVector(0, -1, 0));
    }
    
    // Ajouter les coordonnées UV pour la face arrière
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 0));
    UVs.Add(FVector2D(1, 1));
    
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 1));
    UVs.Add(FVector2D(0, 1));
    
    // Ajouter les tangentes
    for (int32 i = 0; i < 6; i++)
    {
        Tangents.Add(FProcMeshTangent(1, 0, 0));
    }
    
    // Face avant (Y+)
    Triangles.Add(BaseIndex + 3);
    Triangles.Add(BaseIndex + 7);
    Triangles.Add(BaseIndex + 6);
    
    Triangles.Add(BaseIndex + 3);
    Triangles.Add(BaseIndex + 6);
    Triangles.Add(BaseIndex + 2);
    
    // Ajouter les normales pour la face avant (toutes vers l'avant)
    for (int32 i = 0; i < 2; i++)
    {
        Normals.Add(FVector(0, 1, 0));
        Normals.Add(FVector(0, 1, 0));
        Normals.Add(FVector(0, 1, 0));
    }
    
    // Ajouter les coordonnées UV pour la face avant
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 0));
    UVs.Add(FVector2D(1, 1));
    
    UVs.Add(FVector2D(0, 0));
    UVs.Add(FVector2D(1, 1));
    UVs.Add(FVector2D(0, 1));
    
    // Ajouter les tangentes
    for (int32 i = 0; i < 6; i++)
    {
        Tangents.Add(FProcMeshTangent(-1, 0, 0));
    }
}

FColor AVoxelBuilding::GetRandomColor()
{
    // Générer une couleur aléatoire plus vive
    float Hue = FMath::FRand() * 360.0f;
    float Saturation = 0.8f + FMath::FRand() * 0.2f; // Plus saturé
    float Value = 0.7f + FMath::FRand() * 0.3f;      // Plus lumineux
    
    // Convertir HSV en RGB
    FLinearColor LinearColor = FLinearColor::MakeFromHSV8(Hue, Saturation * 255.0f, Value * 255.0f);
    
    // Assurer une couleur opaque
    LinearColor.A = 1.0f;
    
    // Convertir en FColor avec opacité complète
    return LinearColor.ToFColor(true);
}

void AVoxelBuilding::SmoothVertices(TArray<FVector>& Vertices, TArray<int32>& Triangles)
{
    // Créer une copie des sommets originaux
    TArray<FVector> OriginalVertices = Vertices;
    
    // Créer une structure pour stocker les sommets connectés
    TArray<TArray<int32>> VertexConnections;
    VertexConnections.SetNum(Vertices.Num());
    
    // Parcourir tous les triangles et construire les connexions
    for (int32 i = 0; i < Triangles.Num(); i += 3)
    {
        int32 V1 = Triangles[i];
        int32 V2 = Triangles[i + 1];
        int32 V3 = Triangles[i + 2];
        
        VertexConnections[V1].AddUnique(V2);
        VertexConnections[V1].AddUnique(V3);
        
        VertexConnections[V2].AddUnique(V1);
        VertexConnections[V2].AddUnique(V3);
        
        VertexConnections[V3].AddUnique(V1);
        VertexConnections[V3].AddUnique(V2);
    }
    
    // Appliquer le lissage
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        if (VertexConnections[i].Num() > 0)
        {
            // Calculer la position moyenne des sommets connectés
            FVector AveragePosition = FVector::ZeroVector;
            for (int32 j = 0; j < VertexConnections[i].Num(); j++)
            {
                AveragePosition += OriginalVertices[VertexConnections[i][j]];
            }
            AveragePosition /= VertexConnections[i].Num();
            
            // Appliquer le lissage avec la pondération du facteur
            Vertices[i] = FMath::Lerp(OriginalVertices[i], AveragePosition, SmoothingFactor);
        }
    }
}

void AVoxelBuilding::DestroyVoxelsAt(FVector Location, float Radius)
{
    // Cette fonction sera implémentée dans une future mise à jour
    // Elle permettra de détruire les voxels dans un rayon donné
    
    // Convertir la position du monde en coordonnées de la grille
    FVector LocalPosition = GetActorTransform().InverseTransformPosition(Location);
    int32 GridX = FMath::Floor(LocalPosition.X / VoxelSize) + GridSizeX / 2;
    int32 GridY = FMath::Floor(LocalPosition.Y / VoxelSize) + GridSizeY / 2;
    int32 GridZ = FMath::Floor(LocalPosition.Z / VoxelSize) + GridSizeZ / 2;
    
    // Rayon en unités de grille
    int32 GridRadius = FMath::CeilToInt(Radius / VoxelSize);
    
    // Vérifier chaque voxel dans la zone d'explosion
    bool bAnyVoxelDestroyed = false;
    
    for (int32 X = FMath::Max(0, GridX - GridRadius); X <= FMath::Min(GridSizeX - 1, GridX + GridRadius); X++)
    {
        for (int32 Y = FMath::Max(0, GridY - GridRadius); Y <= FMath::Min(GridSizeY - 1, GridY + GridRadius); Y++)
        {
            for (int32 Z = FMath::Max(0, GridZ - GridRadius); Z <= FMath::Min(GridSizeZ - 1, GridZ + GridRadius); Z++)
            {
                // Calculer la distance au centre de l'explosion
                FVector VoxelCenter = FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
                float Distance = FVector::Dist(VoxelCenter, LocalPosition);
                
                // Si le voxel est dans le rayon et qu'il est actif
                if (Distance <= Radius && VoxelGrid[X][Y][Z].bIsActive)
                {
                    // Désactiver le voxel
                    VoxelGrid[X][Y][Z].bIsActive = false;
                    bAnyVoxelDestroyed = true;
                }
            }
        }
    }
    
    // Si des voxels ont été détruits, recréer le mesh
    if (bAnyVoxelDestroyed)
    {
        CreateMesh();
    }
}