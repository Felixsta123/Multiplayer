#include "ADestructibleTerrain.h"
#include "MaterialDomain.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ADestructibleTerrain::ADestructibleTerrain()
{
    PrimaryActorTick.bCanEverTick = true; // Modifié à true pour supporter le LOD
    
    // Création du mesh procédural
    TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
    RootComponent = TerrainMesh;
    
    // Configuration de la réplication
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicatingMovement(true);
    
    // Pour le mesh procédural - NE PAS répliquer le composant lui-même
    // C'est important : nous allons gérer manuellement la réplication des données de mesh
    TerrainMesh->SetIsReplicated(false);
    
    // Configuration des collisions
    TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    
    // Valeurs par défaut
    TerrainWidth = 2000.0f;
    TerrainHeight = 2000.0f;
    TerrainDepth = 1000.0f;
    HorizontalResolution = 15; // 15 subdivisions en largeur
    VerticalResolution = 15; // 15 subdivisions en hauteur
    bIsInitialized = false;
    bModificationsApplied = false;
    
    // Augmenter la fréquence de mise à jour réseau
    NetUpdateFrequency = 10.0f;
    MinNetUpdateFrequency = 5.0f;
    
    // Rendre le terrain visible par défaut
    TerrainMesh->SetVisibility(true);
    TerrainMesh->SetCastShadow(true);
    TerrainMesh->bCastDynamicShadow = true;
    
    // Initialisation de la structure interne
    bGenerateInternalStructure = true;
    InternalLayerCount = 3;
    InternalLayerThickness = 50.0f;

    // Couleurs des couches internes par défaut
    InternalLayerColors.Add(FLinearColor(0.5f, 0.3f, 0.1f, 1.0f));  // Terre
    InternalLayerColors.Add(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f));  // Pierre
    InternalLayerColors.Add(FLinearColor(0.3f, 0.2f, 0.1f, 1.0f));  // Roche sombre
    
    // Initialisation de l'optimisation par sections
    bUseTerrainSections = true;
    SectionSizeX = 500.0f;
    SectionSizeY = 500.0f;
    
    // Initialisation du système de LOD
    bUseLOD = true;
    LODDistanceThreshold = 3000.0f;
    LODHorizontalResolution = 8;
    LODVerticalResolution = 8;
    bIsUsingLOD = false;
}

void ADestructibleTerrain::BeginPlay()
{
    Super::BeginPlay();
    
    // Configurer les matériaux
    SetupMaterials();
    
    // Générer le terrain initial après un court délai
    if (HasAuthority())
    {
        // Retarder légèrement la génération du terrain pour s'assurer que tous les systèmes sont prêts
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle, 
            [this]() { 
                if (!bIsInitialized) {
                    UE_LOG(LogTemp, Warning, TEXT("Initializing terrain in BeginPlay with delayed timer"));
                    InitializeTerrain(TerrainWidth, TerrainHeight, TerrainDepth);
                }
            }, 
            0.5f, 
            false
        );
    }
}

void ADestructibleTerrain::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Mettre à jour le système de LOD (à intervalle réduit pour optimiser)
    static float LODUpdateTimer = 0.0f;
    LODUpdateTimer += DeltaTime;
    
    if (LODUpdateTimer >= 1.0f) // Vérifier une fois par seconde
    {
        UpdateLOD();
        LODUpdateTimer = 0.0f;
    }
}

void ADestructibleTerrain::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    // Dans l'éditeur, générer le terrain pour pouvoir le visualiser
    if (GIsEditor && !HasAuthority())
    {
        GenerateTerrain();
    }
}

void ADestructibleTerrain::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    
    // S'assurer que le matériau est correctement appliqué
    if (TerrainMaterial && TerrainMesh)
    {
        TerrainMesh->SetMaterial(0, TerrainMaterial);
    }
}

void ADestructibleTerrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Répliquer les propriétés importantes
    DOREPLIFETIME(ADestructibleTerrain, TerrainModifications);
    DOREPLIFETIME(ADestructibleTerrain, TerrainWidth);
    DOREPLIFETIME(ADestructibleTerrain, TerrainHeight);
    DOREPLIFETIME(ADestructibleTerrain, TerrainDepth);
    DOREPLIFETIME(ADestructibleTerrain, bIsInitialized);
    DOREPLIFETIME(ADestructibleTerrain, bModificationsApplied);
    DOREPLIFETIME(ADestructibleTerrain, MeshData);
}

void ADestructibleTerrain::InitializeTerrain(float Width, float Height, float Depth)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeTerrain called on client, ignoring"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Initializing terrain on server: %fx%fx%f"), Width, Height, Depth);
    
    TerrainWidth = Width;
    TerrainHeight = Height;
    TerrainDepth = Depth;
    
    // Marquer comme initialisé
    bIsInitialized = true;
    
    // Initialiser les sections si activées
    if (bUseTerrainSections)
    {
        InitializeSections();
    }
    
    // Générer le terrain
    GenerateTerrain();
    
    // Informer tous les clients
    Multicast_NotifyInitialized(Width, Height, Depth);
}

void ADestructibleTerrain::SetupMaterials()
{
    // Vérifier si nous avons déjà des matériaux assignés
    if (TerrainMaterial)
    {
        // Créer une instance dynamique du matériau principal si ce n'est pas déjà fait
        if (!TerrainMaterialInstance)
        {
            TerrainMaterialInstance = UMaterialInstanceDynamic::Create(TerrainMaterial, this);
            
            if (TerrainMaterialInstance)
            {
                // Appliquer l'instance de matériau au mesh
                TerrainMesh->SetMaterial(0, TerrainMaterialInstance);
                
                // Mettre à jour les paramètres du matériau
                UpdateMaterialParameters();
                
                UE_LOG(LogTemp, Log, TEXT("Material instance created and applied successfully"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to create dynamic material instance"));
            }
        }
    }
    else
    {
        // Si aucun matériau n'est défini, utiliser un matériau par défaut
        TerrainMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        TerrainMesh->SetMaterial(0, TerrainMaterial);
        
        UE_LOG(LogTemp, Warning, TEXT("No terrain material assigned, using default material"));
    }
}

void ADestructibleTerrain::UpdateMaterialParameters()
{
    if (TerrainMaterialInstance)
    {
        // Passer les dimensions du terrain au matériau
        TerrainMaterialInstance->SetScalarParameterValue(TEXT("TerrainWidth"), TerrainWidth);
        TerrainMaterialInstance->SetScalarParameterValue(TEXT("TerrainHeight"), TerrainHeight);
        TerrainMaterialInstance->SetScalarParameterValue(TEXT("TerrainDepth"), TerrainDepth);
        
        // Passer les informations sur les couches internes
        if (bGenerateInternalStructure && InternalLayerCount > 0)
        {
            TerrainMaterialInstance->SetScalarParameterValue(TEXT("InternalLayerCount"), InternalLayerCount);
            
            // Passer les couleurs des couches internes (jusqu'à 4 couches max)
            for (int32 i = 0; i < FMath::Min(InternalLayerColors.Num(), 4); ++i)
            {
                FString ParamName = FString::Printf(TEXT("InternalLayerColor%d"), i);
                TerrainMaterialInstance->SetVectorParameterValue(FName(*ParamName), InternalLayerColors[i]);
            }
        }
        
        // Ajouter un effet de bord au matériau pour les cratères
        TerrainMaterialInstance->SetScalarParameterValue(TEXT("CraterEdgeThickness"), 2.0f);
        TerrainMaterialInstance->SetVectorParameterValue(TEXT("CraterEdgeColor"), FLinearColor(0.2f, 0.2f, 0.2f, 1.0f));
        
        // Ajouter une texture de noise pour la variation
        TerrainMaterialInstance->SetScalarParameterValue(TEXT("NoiseIntensity"), 0.1f);
        
        UE_LOG(LogTemp, Verbose, TEXT("Material parameters updated"));
    }
}

void ADestructibleTerrain::InitializeSections()
{
    if (!bUseTerrainSections)
    {
        return;
    }

    // Vider la map de sections existante
    SectionModifications.Empty();
    
    // Calculer combien de sections nous avons en X et Y
    int32 SectionsX = FMath::CeilToInt(TerrainWidth / SectionSizeX);
    int32 SectionsY = FMath::CeilToInt(TerrainHeight / SectionSizeY);
    
    // Initialiser chaque section avec une liste vide de modifications
    for (int32 y = 0; y < SectionsY; ++y)
    {
        for (int32 x = 0; x < SectionsX; ++x)
        {
            FIntPoint SectionCoord(x, y);
            SectionModifications.Add(SectionCoord, FTerrainModificationArray());
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Initialized %d x %d terrain sections"), SectionsX, SectionsY);
}

void ADestructibleTerrain::Multicast_NotifyInitialized_Implementation(float Width, float Height, float Depth)
{
    // Ne pas exécuter sur le serveur, il a déjà fait cette opération
    if (HasAuthority())
    {
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Client received terrain initialization: %fx%fx%f"), Width, Height, Depth);
    
    TerrainWidth = Width;
    TerrainHeight = Height;
    TerrainDepth = Depth;
    
    // Sur les clients, on ne génère pas le terrain ici, car les données viendront via la réplication de MeshData
    
    // Mais on peut préparer le matériau
    if (TerrainMaterial && TerrainMesh)
    {
        TerrainMesh->SetMaterial(0, TerrainMaterial);
    }
    
    // Marquer comme initialisé
    bIsInitialized = true;
}

void ADestructibleTerrain::GenerateInternalStructure()
{
    if (!bGenerateInternalStructure)
    {
        return;
    }

    // Vider les tableaux existants
    InternalVertices.Empty();
    InternalTriangles.Empty();
    InternalUVs.Empty();
    InternalNormals.Empty();
    InternalVertexColors.Empty();
    TerrainCells.Empty();

    // Calculer combien de cellules nous pouvons avoir dans chaque dimension
    CellCountX = FMath::Max(1, FMath::FloorToInt(TerrainWidth / CellSizeX));
    CellCountY = FMath::Max(1, FMath::FloorToInt(TerrainDepth / CellSizeY));
    CellCountZ = FMath::Max(1, FMath::FloorToInt(TerrainHeight / CellSizeZ));

    // Ajuster les tailles de cellules pour qu'elles couvrent exactement le terrain
    float AdjustedCellSizeX = TerrainWidth / CellCountX;
    float AdjustedCellSizeY = TerrainDepth / CellCountY;
    float AdjustedCellSizeZ = TerrainHeight / CellCountZ;

    UE_LOG(LogTemp, Log, TEXT("Generating internal grid structure: %d x %d x %d cells"), 
        CellCountX, CellCountY, CellCountZ);

    // Créer un générateur de nombres aléatoires pour la variation
    FRandomStream Random(FMath::Rand());

    // Création des cellules et stockage dans le tableau
    for (int32 z = 0; z < CellCountZ; ++z)
    {
        for (int32 y = 0; y < CellCountY; ++y)
        {
            for (int32 x = 0; x < CellCountX; ++x)
            {
                // Position de base de cette cellule
                float MinX = x * AdjustedCellSizeX;
                float MinY = y * AdjustedCellSizeY;
                float MinZ = z * AdjustedCellSizeZ;
                
                // Position max de cette cellule
                float MaxX = (x + 1) * AdjustedCellSizeX;
                float MaxY = (y + 1) * AdjustedCellSizeY;
                float MaxZ = (z + 1) * AdjustedCellSizeZ;
                
                // Créer la cellule
                FTerrainCell Cell(
                    FVector(MinX, MinY, MinZ),
                    FVector(MaxX, MaxY, MaxZ),
                    x, y, z
                );
                
                // Ajouter au tableau
                TerrainCells.Add(Cell);
                
                // Génération des parois de la cellule
                GenerateCellWalls(Cell, Random);
            }
        }
    }

    // Initialiser les normales pour toutes les faces
    CalculateInternalNormals();

    UE_LOG(LogTemp, Log, TEXT("Generated internal structure with %d vertices, %d triangles, and %d cells"), 
        InternalVertices.Num(), InternalTriangles.Num() / 3, TerrainCells.Num());
}

// Nouvelle fonction pour générer les parois d'une cellule spécifique
void ADestructibleTerrain::GenerateCellWalls(const FTerrainCell& Cell, FRandomStream& Random)
{
    // Extraction des coordonnées pour lisibilité
    float MinX = Cell.Min.X;
    float MinY = Cell.Min.Y;
    float MinZ = Cell.Min.Z;
    float MaxX = Cell.Max.X;
    float MaxY = Cell.Max.Y;
    float MaxZ = Cell.Max.Z;
    
    // Calculer l'épaisseur des parois en fonction de la taille des cellules
    float WallThicknessX = FMath::Min(WallThickness, (MaxX - MinX) * 0.25f);
    float WallThicknessY = FMath::Min(WallThickness, (MaxY - MinY) * 0.25f);
    float WallThicknessZ = FMath::Min(WallThickness, (MaxZ - MinZ) * 0.25f);
    
    // Couleur de cette cellule (varier par couche Z)
    FLinearColor CellColor = GetInternalLayerColor(Cell.GridZ % InternalLayerColors.Num());
    
    // Ajouter un peu de variation à la couleur selon la position X et Y
    if (bAddNoiseToStructure)
    {
        float ColorVariation = Random.FRandRange(-0.1f, 0.1f);
        CellColor.R = FMath::Clamp(CellColor.R + ColorVariation, 0.0f, 1.0f);
        CellColor.G = FMath::Clamp(CellColor.G + ColorVariation, 0.0f, 1.0f);
        CellColor.B = FMath::Clamp(CellColor.B + ColorVariation, 0.0f, 1.0f);
    }
    
    // Six faces pour la cellule (négatif et positif selon X, Y, Z)
    
    // Face X- (gauche)
    GenerateInternalWall(
        FVector(MinX, MinY, MinZ),
        FVector(MinX, MaxY, MinZ),
        FVector(MinX, MinY, MaxZ),
        FVector(MinX, MaxY, MaxZ),
        WallThicknessX,
        CellColor,
        Random
    );
    
    // Face X+ (droite)
    GenerateInternalWall(
        FVector(MaxX, MinY, MinZ),
        FVector(MaxX, MaxY, MinZ),
        FVector(MaxX, MinY, MaxZ),
        FVector(MaxX, MaxY, MaxZ),
        WallThicknessX,
        CellColor,
        Random
    );
    
    // Face Y- (avant)
    GenerateInternalWall(
        FVector(MinX, MinY, MinZ),
        FVector(MaxX, MinY, MinZ),
        FVector(MinX, MinY, MaxZ),
        FVector(MaxX, MinY, MaxZ),
        WallThicknessY,
        CellColor,
        Random
    );
    
    // Face Y+ (arrière)
    GenerateInternalWall(
        FVector(MinX, MaxY, MinZ),
        FVector(MaxX, MaxY, MinZ),
        FVector(MinX, MaxY, MaxZ),
        FVector(MaxX, MaxY, MaxZ),
        WallThicknessY,
        CellColor,
        Random
    );
    
    // Face Z- (bas)
    GenerateInternalWall(
        FVector(MinX, MinY, MinZ),
        FVector(MaxX, MinY, MinZ),
        FVector(MinX, MaxY, MinZ),
        FVector(MaxX, MaxY, MinZ),
        WallThicknessZ,
        CellColor,
        Random
    );
    
    // Face Z+ (haut)
    GenerateInternalWall(
        FVector(MinX, MinY, MaxZ),
        FVector(MaxX, MinY, MaxZ),
        FVector(MinX, MaxY, MaxZ),
        FVector(MaxX, MaxY, MaxZ),
        WallThicknessZ,
        CellColor,
        Random
    );
}
void ADestructibleTerrain::GenerateInternalWall(
    const FVector& BottomLeft, 
    const FVector& BottomRight, 
    const FVector& TopLeft, 
    const FVector& TopRight, 
    float Thickness,
    const FLinearColor& Color,
    FRandomStream& Random)
{
    // Bilan vertices: 8 vertices par paroi (face avant + face arrière)
    int32 VertexStartIndex = InternalVertices.Num();

    // Calculer le vecteur normal à la face (pour l'extrusion)
    FVector Edge1 = BottomRight - BottomLeft;
    FVector Edge2 = TopLeft - BottomLeft;
    FVector Normal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

    // Si la normale n'est pas valide, utiliser une direction par défaut
    if (Normal.IsNearlyZero())
    {
        Normal = FVector(0, 1, 0);
    }

    // Direction d'extrusion pour l'épaisseur
    FVector ExtrusionDir = Normal * Thickness;

    // Ajouter une variation à l'épaisseur si activé
    if (bAddNoiseToStructure)
    {
        float ThicknessVariation = 1.0f + Random.FRandRange(-0.2f, 0.2f);
        ExtrusionDir *= ThicknessVariation;
    }

    // Face avant
    InternalVertices.Add(BottomLeft);                                // 0
    InternalVertices.Add(BottomRight);                               // 1
    InternalVertices.Add(TopLeft);                                   // 2
    InternalVertices.Add(TopRight);                                  // 3

    // Face arrière (extrudée)
    InternalVertices.Add(BottomLeft + ExtrusionDir);                 // 4
    InternalVertices.Add(BottomRight + ExtrusionDir);                // 5
    InternalVertices.Add(TopLeft + ExtrusionDir);                    // 6
    InternalVertices.Add(TopRight + ExtrusionDir);                   // 7

    // Triangles pour la face avant
    InternalTriangles.Add(VertexStartIndex + 0);
    InternalTriangles.Add(VertexStartIndex + 2);
    InternalTriangles.Add(VertexStartIndex + 1);

    InternalTriangles.Add(VertexStartIndex + 1);
    InternalTriangles.Add(VertexStartIndex + 2);
    InternalTriangles.Add(VertexStartIndex + 3);

    // Triangles pour la face arrière (orientation inversée)
    InternalTriangles.Add(VertexStartIndex + 4);
    InternalTriangles.Add(VertexStartIndex + 5);
    InternalTriangles.Add(VertexStartIndex + 6);

    InternalTriangles.Add(VertexStartIndex + 5);
    InternalTriangles.Add(VertexStartIndex + 7);
    InternalTriangles.Add(VertexStartIndex + 6);

    // Triangles pour les côtés (4 côtés à relier)
    
    // Côté bas
    InternalTriangles.Add(VertexStartIndex + 0);
    InternalTriangles.Add(VertexStartIndex + 1);
    InternalTriangles.Add(VertexStartIndex + 4);

    InternalTriangles.Add(VertexStartIndex + 1);
    InternalTriangles.Add(VertexStartIndex + 5);
    InternalTriangles.Add(VertexStartIndex + 4);

    // Côté droit
    InternalTriangles.Add(VertexStartIndex + 1);
    InternalTriangles.Add(VertexStartIndex + 3);
    InternalTriangles.Add(VertexStartIndex + 5);

    InternalTriangles.Add(VertexStartIndex + 3);
    InternalTriangles.Add(VertexStartIndex + 7);
    InternalTriangles.Add(VertexStartIndex + 5);

    // Côté haut
    InternalTriangles.Add(VertexStartIndex + 2);
    InternalTriangles.Add(VertexStartIndex + 6);
    InternalTriangles.Add(VertexStartIndex + 3);

    InternalTriangles.Add(VertexStartIndex + 3);
    InternalTriangles.Add(VertexStartIndex + 6);
    InternalTriangles.Add(VertexStartIndex + 7);

    // Côté gauche
    InternalTriangles.Add(VertexStartIndex + 0);
    InternalTriangles.Add(VertexStartIndex + 4);
    InternalTriangles.Add(VertexStartIndex + 2);

    InternalTriangles.Add(VertexStartIndex + 2);
    InternalTriangles.Add(VertexStartIndex + 4);
    InternalTriangles.Add(VertexStartIndex + 6);

    // Ajouter des UVs et des couleurs pour tous les vertices
    for (int32 i = 0; i < 8; ++i)
    {
        // UVs simplement répartis sur la paroi
        float U = (i == 1 || i == 3 || i == 5 || i == 7) ? 1.0f : 0.0f;
        float V = (i == 2 || i == 3 || i == 6 || i == 7) ? 1.0f : 0.0f;
        InternalUVs.Add(FVector2D(U, V));

        // Ajouter une légère variation à la couleur
        FLinearColor ModifiedColor = Color;
        if (bAddNoiseToStructure)
        {
            float ColorVariation = Random.FRandRange(-0.1f, 0.1f);
            ModifiedColor.R = FMath::Clamp(ModifiedColor.R + ColorVariation, 0.0f, 1.0f);
            ModifiedColor.G = FMath::Clamp(ModifiedColor.G + ColorVariation, 0.0f, 1.0f);
            ModifiedColor.B = FMath::Clamp(ModifiedColor.B + ColorVariation, 0.0f, 1.0f);
        }
        
        InternalVertexColors.Add(ModifiedColor.ToFColor(true));
    }
}

// Helper pour calculer les normales
void ADestructibleTerrain::CalculateInternalNormals()
{
    // Initialiser les normales
    InternalNormals.Init(FVector::ZeroVector, InternalVertices.Num());
    
    // Calculer les normales pour chaque triangle et les ajouter aux normales des vertices
    for (int32 i = 0; i < InternalTriangles.Num(); i += 3)
    {
        // Obtenir les indices des vertices du triangle
        int32 Index0 = InternalTriangles[i];
        int32 Index1 = InternalTriangles[i + 1];
        int32 Index2 = InternalTriangles[i + 2];
        
        // Vérifier que les indices sont valides
        if (InternalVertices.IsValidIndex(Index0) && InternalVertices.IsValidIndex(Index1) && InternalVertices.IsValidIndex(Index2))
        {
            // Calculer les vecteurs des côtés du triangle
            FVector Side1 = InternalVertices[Index1] - InternalVertices[Index0];
            FVector Side2 = InternalVertices[Index2] - InternalVertices[Index0];
            
            // Calculer la normale du triangle (produit vectoriel)
            FVector Normal = FVector::CrossProduct(Side1, Side2).GetSafeNormal();
            
            // Ajouter la normale à chaque vertex du triangle
            InternalNormals[Index0] += Normal;
            InternalNormals[Index1] += Normal;
            InternalNormals[Index2] += Normal;
        }
    }
    
    // Normaliser toutes les normales
    for (int32 i = 0; i < InternalNormals.Num(); i++)
    {
        if (!InternalNormals[i].IsZero())
        {
            InternalNormals[i] = InternalNormals[i].GetSafeNormal();
        }
        else
        {
            // Par défaut, on met une normale vers l'extérieur
            InternalNormals[i] = FVector(0.0f, -1.0f, 0.0f);
        }
    }
}

// Helper pour obtenir une couleur pour une couche
FLinearColor ADestructibleTerrain::GetInternalLayerColor(int32 LayerIndex)
{
    if (InternalLayerColors.IsValidIndex(LayerIndex))
    {
        return InternalLayerColors[LayerIndex];
    }
    else if (InternalLayerColors.Num() > 0)
    {
        // Fallback à la première couleur
        return InternalLayerColors[0];
    }
    
    // Couleur par défaut si aucune n'est définie
    return FLinearColor(0.5f, 0.3f, 0.1f, 1.0f);
}
void ADestructibleTerrain::GenerateTerrain()
{
    // Vider les tableaux
    MeshData.Vertices.Empty();
    MeshData.Triangles.Empty();
    MeshData.UVs.Empty();
    MeshData.Normals.Empty();
    MeshData.VertexColors.Empty();
    
    // Initialiser les tangentes une seule fois
    if (Tangents.Num() == 0)
    {
        InitializeTangents();
    }
    
    // Vérifier que les résolutions sont valides
    HorizontalResolution = FMath::Max(HorizontalResolution, 2);
    VerticalResolution = FMath::Max(VerticalResolution, 2);
    
    // Calculer le pas entre chaque point
    float HStep = TerrainWidth / (HorizontalResolution - 1);
    float VStep = TerrainHeight / (VerticalResolution - 1);
    
    // 1. Génération de la face avant (vue principale du terrain)
    for (int32 y = 0; y < VerticalResolution; ++y)
    {
        for (int32 x = 0; x < HorizontalResolution; ++x)
        {
            // Calculer la position de ce vertex
            float PosX = x * HStep;
            float PosZ = y * VStep;
            
            // Ajouter le vertex
            MeshData.Vertices.Add(FVector(PosX, 0.0f, PosZ));
            
            // Ajouter les UV correspondants (normalisés de 0 à 1)
            MeshData.UVs.Add(FVector2D(
                static_cast<float>(x) / (HorizontalResolution - 1), 
                static_cast<float>(y) / (VerticalResolution - 1)
            ));
            
            // Couleur verte pour le terrain
            MeshData.VertexColors.Add(FColor(75, 150, 75, 255));
        }
    }
    
    // 2. Génération de la face arrière (derrière le terrain)
    for (int32 y = 0; y < VerticalResolution; ++y)
    {
        for (int32 x = 0; x < HorizontalResolution; ++x)
        {
            // Calculer la position de ce vertex
            float PosX = x * HStep;
            float PosZ = y * VStep;
            
            // Ajouter le vertex
            MeshData.Vertices.Add(FVector(PosX, TerrainDepth, PosZ));
            
            // Ajouter les UV correspondants (normalisés de 0 à 1)
            MeshData.UVs.Add(FVector2D(
                static_cast<float>(x) / (HorizontalResolution - 1), 
                static_cast<float>(y) / (VerticalResolution - 1)
            ));
            
            // Couleur verte pour le terrain
            MeshData.VertexColors.Add(FColor(75, 150, 75, 255));
        }
    }
    
    // 3. Création des triangles pour la face avant
    int32 VerticesPerFace = HorizontalResolution * VerticalResolution;
    for (int32 y = 0; y < VerticalResolution - 1; ++y)
    {
        for (int32 x = 0; x < HorizontalResolution - 1; ++x)
        {
            int32 Current = y * HorizontalResolution + x;
            int32 Next = Current + 1;
            int32 Bottom = Current + HorizontalResolution;
            int32 BottomNext = Bottom + 1;
            
            // Premier triangle
            MeshData.Triangles.Add(Current);
            MeshData.Triangles.Add(Bottom);
            MeshData.Triangles.Add(Next);
            
            // Second triangle
            MeshData.Triangles.Add(Next);
            MeshData.Triangles.Add(Bottom);
            MeshData.Triangles.Add(BottomNext);
        }
    }
    
    // 4. Création des triangles pour la face arrière (inversés)
    for (int32 y = 0; y < VerticalResolution - 1; ++y)
    {
        for (int32 x = 0; x < HorizontalResolution - 1; ++x)
        {
            int32 Current = VerticesPerFace + y * HorizontalResolution + x;
            int32 Next = Current + 1;
            int32 Bottom = Current + HorizontalResolution;
            int32 BottomNext = Bottom + 1;
            
            // Premier triangle (inversé)
            MeshData.Triangles.Add(Next);
            MeshData.Triangles.Add(Bottom);
            MeshData.Triangles.Add(Current);
            
            // Second triangle (inversé)
            MeshData.Triangles.Add(BottomNext);
            MeshData.Triangles.Add(Bottom);
            MeshData.Triangles.Add(Next);
        }
    }
    
    // 5. Ajouter les triangles pour les faces latérales
    // Face inférieure (bas)
    for (int32 x = 0; x < HorizontalResolution - 1; ++x)
    {
        int32 FrontLeft = x;
        int32 FrontRight = x + 1;
        int32 BackLeft = VerticesPerFace + x;
        int32 BackRight = VerticesPerFace + x + 1;
        
        MeshData.Triangles.Add(FrontLeft);
        MeshData.Triangles.Add(FrontRight);
        MeshData.Triangles.Add(BackLeft);
        
        MeshData.Triangles.Add(BackLeft);
        MeshData.Triangles.Add(FrontRight);
        MeshData.Triangles.Add(BackRight);
    }
    
    // Face supérieure (haut)
    for (int32 x = 0; x < HorizontalResolution - 1; ++x)
    {
        int32 FrontLeft = (VerticalResolution - 1) * HorizontalResolution + x;
        int32 FrontRight = FrontLeft + 1;
        int32 BackLeft = VerticesPerFace + (VerticalResolution - 1) * HorizontalResolution + x;
        int32 BackRight = BackLeft + 1;
        
        MeshData.Triangles.Add(FrontRight);
        MeshData.Triangles.Add(FrontLeft);
        MeshData.Triangles.Add(BackLeft);
        
        MeshData.Triangles.Add(BackRight);
        MeshData.Triangles.Add(FrontRight);
        MeshData.Triangles.Add(BackLeft);
    }
    
    // Face gauche
    for (int32 y = 0; y < VerticalResolution - 1; ++y)
    {
        int32 FrontBottom = y * HorizontalResolution;
        int32 FrontTop = FrontBottom + HorizontalResolution;
        int32 BackBottom = VerticesPerFace + y * HorizontalResolution;
        int32 BackTop = BackBottom + HorizontalResolution;
        
        MeshData.Triangles.Add(FrontBottom);
        MeshData.Triangles.Add(BackBottom);
        MeshData.Triangles.Add(FrontTop);
        
        MeshData.Triangles.Add(FrontTop);
        MeshData.Triangles.Add(BackBottom);
        MeshData.Triangles.Add(BackTop);
    }
    
    // Face droite
    for (int32 y = 0; y < VerticalResolution - 1; ++y)
    {
        int32 FrontBottom = y * HorizontalResolution + (HorizontalResolution - 1);
        int32 FrontTop = FrontBottom + HorizontalResolution;
        int32 BackBottom = VerticesPerFace + y * HorizontalResolution + (HorizontalResolution - 1);
        int32 BackTop = BackBottom + HorizontalResolution;
        
        MeshData.Triangles.Add(BackBottom);
        MeshData.Triangles.Add(FrontBottom);
        MeshData.Triangles.Add(FrontTop);
        
        MeshData.Triangles.Add(BackTop);
        MeshData.Triangles.Add(BackBottom);
        MeshData.Triangles.Add(FrontTop);
    }
    
    // Initialiser les normales
    MeshData.Normals.Init(FVector::ZeroVector, MeshData.Vertices.Num());
    
    // Calculer les normales pour chaque triangle et les ajouter aux normales des vertices
    for (int32 i = 0; i < MeshData.Triangles.Num(); i += 3)
    {
        // Obtenir les indices des vertices du triangle
        int32 Index0 = MeshData.Triangles[i];
        int32 Index1 = MeshData.Triangles[i + 1];
        int32 Index2 = MeshData.Triangles[i + 2];
        
        // Calculer les vecteurs des côtés du triangle
        FVector Side1 = MeshData.Vertices[Index1] - MeshData.Vertices[Index0];
        FVector Side2 = MeshData.Vertices[Index2] - MeshData.Vertices[Index0];
        
        // Calculer la normale du triangle (produit vectoriel)
        FVector Normal = FVector::CrossProduct(Side1, Side2).GetSafeNormal();
        
        // Ajouter la normale à chaque vertex du triangle
        MeshData.Normals[Index0] += Normal;
        MeshData.Normals[Index1] += Normal;
        MeshData.Normals[Index2] += Normal;
    }
    
    // Normaliser toutes les normales
    for (int32 i = 0; i < MeshData.Normals.Num(); i++)
    {
        MeshData.Normals[i] = MeshData.Normals[i].GetSafeNormal();
    }
    
    // Élargir le tableau des tangentes si nécessaire
    if (Tangents.Num() < MeshData.Vertices.Num())
    {
        int32 OldSize = Tangents.Num();
        Tangents.AddDefaulted(MeshData.Vertices.Num() - OldSize);
        
        for (int32 i = OldSize; i < Tangents.Num(); ++i)
        {
            Tangents[i] = FProcMeshTangent(1.0f, 0.0f, 0.0f);
        }
    }
    
    // 6. Générer la structure interne si activée
    if (bGenerateInternalStructure)
    {
        GenerateInternalStructure();
        
        // Ajouter la structure interne aux données du mesh principal
        
        // Sauvegarder les indices de départ pour les références
        int32 VertexStartIndex = MeshData.Vertices.Num();
        
        // Ajouter les vertices internes
        MeshData.Vertices.Append(InternalVertices);
        MeshData.UVs.Append(InternalUVs);
        MeshData.VertexColors.Append(InternalVertexColors);
        MeshData.Normals.Append(InternalNormals);
        
        // Ajouter les triangles internes (en ajustant les indices)
        for (int32 i = 0; i < InternalTriangles.Num(); ++i)
        {
            MeshData.Triangles.Add(InternalTriangles[i] + VertexStartIndex);
        }
        
        // Log pour débogage
        UE_LOG(LogTemp, Log, TEXT("Added internal structure: total mesh now has %d vertices and %d triangles"), 
            MeshData.Vertices.Num(), MeshData.Triangles.Num() / 3);
    }
    
    // Marquer les données comme valides
    MeshData.bIsValid = true;
    
    // Créer le mesh à partir des données
    CreateMeshFromData(MeshData);
    
    // Si nous sommes sur le serveur, répliquer ces données vers tous les clients
    if (HasAuthority())
    {
        Multicast_UpdateTerrainMesh(MeshData);
    }
    
    // Log pour débogage
    UE_LOG(LogTemp, Log, TEXT("%s: Terrain généré avec %d vertices et %d triangles"), 
        HasAuthority() ? TEXT("Server") : TEXT("Client"),
        MeshData.Vertices.Num(), 
        MeshData.Triangles.Num() / 3);
}

void ADestructibleTerrain::InitializeTangents()
{
    // Initialiser les tangentes (une seule fois)
    Tangents.Empty();
    
    // Nous utilisons une tangente par défaut
    for (int32 i = 0; i < 8; i++)  // 8 vertices pour notre cube
    {
        Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
    }
}
void ADestructibleTerrain::CreateMeshFromData(const FTerrainMeshData& InMeshData)
{
    if (!InMeshData.bIsValid)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateMeshFromData called with invalid mesh data"));
        return;
    }
    
    // Convertir les couleurs
    TArray<FLinearColor> LinearColors = ConvertColorsToLinear(InMeshData.VertexColors);
    
    // Vérification des dimensions de données pour éviter les crashs
    if (InMeshData.Vertices.Num() == 0 || InMeshData.Triangles.Num() == 0 || 
        InMeshData.Normals.Num() != InMeshData.Vertices.Num() || 
        InMeshData.UVs.Num() != InMeshData.Vertices.Num() ||
        LinearColors.Num() != InMeshData.Vertices.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid mesh data dimensions - V:%d, T:%d, N:%d, UV:%d, C:%d"), 
            InMeshData.Vertices.Num(), InMeshData.Triangles.Num(), 
            InMeshData.Normals.Num(), InMeshData.UVs.Num(), LinearColors.Num());
        return;
    }
    
    // IMPORTANT: Nettoyer complètement la section avant de la recréer
    // pour garantir que les données de collision sont également nettoyées
    TerrainMesh->ClearMeshSection(0);
    
    // Recréer la section avec collision
    TerrainMesh->CreateMeshSection_LinearColor(
        0, 
        InMeshData.Vertices, 
        InMeshData.Triangles, 
        InMeshData.Normals, 
        InMeshData.UVs, 
        LinearColors, 
        Tangents, 
        true  // Générer une collision - CRUCIAL
    );
    
    // Forcer une mise à jour complète des données de collision
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    
    // Activation explicite de la collision à la fin
    TerrainMesh->ContainsPhysicsTriMeshData(true);
    
    // Explicitement créer les convex hulls pour une meilleure collision
    TerrainMesh->bUseComplexAsSimpleCollision = false;
    
    // Utiliser RecreatePhysicsState pour forcer la mise à jour de la collision
    TerrainMesh->RecreatePhysicsState();
    
    // Forcer l'application du matériau
    if (TerrainMaterialInstance)
    {
        TerrainMesh->SetMaterial(0, TerrainMaterialInstance);
    }
    else if (TerrainMaterial)
    {
        TerrainMesh->SetMaterial(0, TerrainMaterial);
    }
    else
    {
        // Utiliser un matériau par défaut si aucun n'est assigné
        UE_LOG(LogTemp, Warning, TEXT("Aucun matériau assigné au terrain. Utilisation d'un matériau par défaut."));
        UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        TerrainMesh->SetMaterial(0, DefaultMaterial);
    }
    
    // S'assurer que le mesh est visible
    TerrainMesh->SetVisibility(true);
    
    // Forcer une mise à jour du rendu
    TerrainMesh->MarkRenderStateDirty();
}


TArray<FLinearColor> ADestructibleTerrain::ConvertColorsToLinear(const TArray<FColor>& Colors)
{
    TArray<FLinearColor> LinearColors;
    LinearColors.Reserve(Colors.Num());
    
    for (const FColor& Color : Colors)
    {
        LinearColors.Add(FLinearColor(Color));
    }
    
    return LinearColors;
}

void ADestructibleTerrain::Multicast_UpdateTerrainMesh_Implementation(const FTerrainMeshData& InMeshData)
{
    // Ne pas exécuter sur le serveur, il a déjà fait cette opération
    if (HasAuthority())
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Client received terrain mesh update with %d vertices and %d triangles"), 
        InMeshData.Vertices.Num(), InMeshData.Triangles.Num() / 3);
    
    // Mettre à jour les données locales
    this->MeshData = InMeshData;
    
    // Créer le mesh à partir des données
    CreateMeshFromData(InMeshData);
}

void ADestructibleTerrain::OnRep_MeshData()
{
    UE_LOG(LogTemp, Log, TEXT("OnRep_MeshData called on client"));
    
    // Lorsque MeshData est répliqué, recréer le mesh
    if (MeshData.bIsValid)
    {
        CreateMeshFromData(MeshData);
    }
}

void ADestructibleTerrain::RequestDestroyTerrainAt(FVector2D Position, FVector2D Size)
{
    // Appeler la fonction serveur pour valider et appliquer la destruction
    if (GetLocalRole() < ROLE_Authority)
    {
        Server_DestroyTerrainAt(Position, Size);
    }
    else
    {
        // Si déjà sur le serveur, appliquer directement
        Server_DestroyTerrainAt(Position, Size);
    }
}

bool ADestructibleTerrain::Server_DestroyTerrainAt_Validate(FVector2D Position, FVector2D Size)
{
    // Validation simple
    return true;
}

void ADestructibleTerrain::Server_DestroyTerrainAt_Implementation(FVector2D Position, FVector2D Size)
{
    UE_LOG(LogTemp, Warning, TEXT("Destroying terrain at position (%f, %f) with size (%f, %f)"), 
        Position.X, Position.Y, Size.X, Size.Y);
    
    // Créer une nouvelle modification
    FTerrainModification NewMod(Position, Size);
    
    // Ajouter à la liste globale des modifications
    TerrainModifications.Add(NewMod);
    
    // Afficher le nombre total de modifications
    UE_LOG(LogTemp, Warning, TEXT("Total modifications: %d"), TerrainModifications.Num());
    
    if (bUseTerrainSections)
    {
        // Assigner cette modification aux sections appropriées et les mettre à jour
        AssignModificationToSections(NewMod);
    }
    else
    {
        // Approche classique : appliquer toutes les modifications sur le mesh entier
        ApplyTerrainModifications();
    }
}

void ADestructibleTerrain::AssignModificationToSections(const FTerrainModification& Modification)
{
    if (!bUseTerrainSections)
    {
        return;
    }
    
    // Déterminer quelles sections sont affectées par cette modification
    TArray<FIntPoint> AffectedSections = GetAffectedSections(Modification);
    
    // Ajouter la modification à chaque section affectée
    for (const FIntPoint& SectionCoord : AffectedSections)
    {
        if (SectionModifications.Contains(SectionCoord))
        {
            SectionModifications[SectionCoord].Modifications.Add(Modification);
            UE_LOG(LogTemp, Verbose, TEXT("Added modification to section (%d, %d)"), SectionCoord.X, SectionCoord.Y);
        }
    }
    
    // Reconstruire uniquement les sections affectées
    RegenerateSections(AffectedSections);
}

TArray<FIntPoint> ADestructibleTerrain::GetAffectedSections(const FTerrainModification& Modification)
{
    TArray<FIntPoint> AffectedSections;
    
    if (!bUseTerrainSections)
    {
        return AffectedSections;
    }
    
    // Convertir les coordonnées de la modification en coordonnées de sections
    float ModStartX = Modification.Position.X;
    float ModStartY = Modification.Position.Y;
    float ModEndX = ModStartX + Modification.Size.X;
    float ModEndY = ModStartY + Modification.Size.Y;
    
    // Déterminer les indices de section de début et de fin
    int32 StartSectionX = FMath::Max(0, FMath::FloorToInt(ModStartX / SectionSizeX));
    int32 StartSectionY = FMath::Max(0, FMath::FloorToInt(ModStartY / SectionSizeY));
    int32 EndSectionX = FMath::Min(FMath::CeilToInt(TerrainWidth / SectionSizeX) - 1, FMath::CeilToInt(ModEndX / SectionSizeX));
    int32 EndSectionY = FMath::Min(FMath::CeilToInt(TerrainHeight / SectionSizeY) - 1, FMath::CeilToInt(ModEndY / SectionSizeY));
    
    // Ajouter toutes les sections entre les indices de début et de fin
    for (int32 y = StartSectionY; y <= EndSectionY; ++y)
    {
        for (int32 x = StartSectionX; x <= EndSectionX; ++x)
        {
            AffectedSections.Add(FIntPoint(x, y));
        }
    }
    
    UE_LOG(LogTemp, Verbose, TEXT("Modification affects %d sections"), AffectedSections.Num());
    return AffectedSections;
}

void ADestructibleTerrain::RegenerateSections(const TArray<FIntPoint>& SectionCoords)
{
    if (!bUseTerrainSections || SectionCoords.Num() == 0)
    {
        return;
    }
    
    // Pour chaque section affectée, nous allons reconstruire son mesh
    for (const FIntPoint& SectionCoord : SectionCoords)
    {
        // Obtenir les modifications pour cette section
        FTerrainModificationArray* SectionMods = SectionModifications.Find(SectionCoord);
        if (SectionMods)
        {
            // Calculer les limites de cette section
            float SectionStartX = SectionCoord.X * SectionSizeX;
            float SectionStartY = SectionCoord.Y * SectionSizeY;
            float SectionEndX = FMath::Min(SectionStartX + SectionSizeX, TerrainWidth);
            float SectionEndY = FMath::Min(SectionStartY + SectionSizeY, TerrainHeight);
            
            UE_LOG(LogTemp, Verbose, TEXT("Regenerating section (%d, %d) with %d modifications"), 
                SectionCoord.X, SectionCoord.Y, SectionMods->Modifications.Num());
        }
    }
    
    // Pour l'instant, comme solution simplifiée, reconstruisons le mesh entier
    // en appliquant toutes les modifications
    ApplyTerrainModifications();
}

bool ADestructibleTerrain::IsVertexInSection(const FVector& Vertex, const FIntPoint& SectionCoord)
{
    if (!bUseTerrainSections)
    {
        return false;
    }
    
    // Calculer les limites de cette section
    float SectionStartX = SectionCoord.X * SectionSizeX;
    float SectionStartY = SectionCoord.Y * SectionSizeY;
    float SectionEndX = FMath::Min(SectionStartX + SectionSizeX, TerrainWidth);
    float SectionEndY = FMath::Min(SectionStartY + SectionSizeY, TerrainHeight);
    
    // Vérifier si le vertex est dans cette section
    return (Vertex.X >= SectionStartX && Vertex.X < SectionEndX && 
            Vertex.Z >= SectionStartY && Vertex.Z < SectionEndY);
}

void ADestructibleTerrain::OnRep_TerrainModifications()
{
    // Appelé sur les clients quand TerrainModifications est répliqué
    ApplyTerrainModifications();
}

bool ADestructibleTerrain::IsVertexInModification(const FVector& Vertex, const FTerrainModification& Modification)
{
    // Rechercher d'abord à quelle cellule appartient ce vertex
    for (const FTerrainCell& Cell : TerrainCells)
    {
        if (Cell.ContainsPoint(Vertex) && Cell.IsAffectedByModification(Modification))
        {
            // Le vertex est dans une cellule affectée par la modification
            return true;
        }
    }
    
    // Fallback à l'ancien test si vertex n'est dans aucune cellule connue
    // Considérer que le vertex est en 2D (X, Z)
    float VertexX = Vertex.X;
    float VertexZ = Vertex.Z;
    
    // Si c'est une modification circulaire
    if (Modification.bIsCircular)
    {
        // Calculer la distance du vertex au centre du cercle
        float DistanceSquared = FMath::Square(VertexX - Modification.CircleCenter.X) + 
                               FMath::Square(VertexZ - Modification.CircleCenter.Y);
        
        // Comparer au carré du rayon
        return DistanceSquared <= FMath::Square(Modification.CircleRadius);
    }
    else
    {
        // Pour une modification rectangulaire
        return (VertexX >= Modification.Position.X && 
                VertexX <= Modification.Position.X + Modification.Size.X &&
                VertexZ >= Modification.Position.Y && 
                VertexZ <= Modification.Position.Y + Modification.Size.Y);
    }
}

// Fonction pour détecter les cellules affectées par une modification
TArray<FTerrainCell*> ADestructibleTerrain::GetAffectedCells(const FTerrainModification& Modification)
{
    TArray<FTerrainCell*> AffectedCells;
    
    for (int32 i = 0; i < TerrainCells.Num(); ++i)
    {
        if (TerrainCells[i].IsAffectedByModification(Modification))
        {
            AffectedCells.Add(&TerrainCells[i]);
        }
    }
    
    // Si aucune cellule n'est trouvée avec la nouvelle méthode, essayer l'ancienne
    if (AffectedCells.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No cells found with cell-based detection, falling back to legacy method"));
        
        // Méthode traditionnelle - estimer les cellules touchées
        if (Modification.bIsCircular)
        {
            // Pour les modifications circulaires
            float CenterX = Modification.CircleCenter.X;
            float CenterZ = Modification.CircleCenter.Y;
            float Radius = Modification.CircleRadius;
            
            // Calculer les limites de la zone affectée
            int32 MinGridX = FMath::Max(0, FMath::FloorToInt((CenterX - Radius) / CellSizeX));
            int32 MaxGridX = FMath::Min(CellCountX - 1, FMath::CeilToInt((CenterX + Radius) / CellSizeX));
            int32 MinGridZ = FMath::Max(0, FMath::FloorToInt((CenterZ - Radius) / CellSizeZ));
            int32 MaxGridZ = FMath::Min(CellCountZ - 1, FMath::CeilToInt((CenterZ + Radius) / CellSizeZ));
            
            // Parcourir les cellules potentiellement touchées
            for (int32 z = MinGridZ; z <= MaxGridZ; ++z)
            {
                for (int32 y = 0; y < CellCountY; ++y) // Inclure toute la profondeur
                {
                    for (int32 x = MinGridX; x <= MaxGridX; ++x)
                    {
                        int32 CellIndex = (z * CellCountY + y) * CellCountX + x;
                        if (TerrainCells.IsValidIndex(CellIndex) && !TerrainCells[CellIndex].bIsDestroyed)
                        {
                            AffectedCells.Add(&TerrainCells[CellIndex]);
                        }
                    }
                }
            }
        }
        else
        {
            // Pour les modifications rectangulaires
            float MinX = Modification.Position.X;
            float MinZ = Modification.Position.Y;
            float MaxX = MinX + Modification.Size.X;
            float MaxZ = MinZ + Modification.Size.Y;
            
            // Calculer les limites de la zone affectée
            int32 MinGridX = FMath::Max(0, FMath::FloorToInt(MinX / CellSizeX));
            int32 MaxGridX = FMath::Min(CellCountX - 1, FMath::CeilToInt(MaxX / CellSizeX));
            int32 MinGridZ = FMath::Max(0, FMath::FloorToInt(MinZ / CellSizeZ));
            int32 MaxGridZ = FMath::Min(CellCountZ - 1, FMath::CeilToInt(MaxZ / CellSizeZ));
            
            // Parcourir les cellules potentiellement touchées
            for (int32 z = MinGridZ; z <= MaxGridZ; ++z)
            {
                for (int32 y = 0; y < CellCountY; ++y) // Inclure toute la profondeur
                {
                    for (int32 x = MinGridX; x <= MaxGridX; ++x)
                    {
                        int32 CellIndex = (z * CellCountY + y) * CellCountX + x;
                        if (TerrainCells.IsValidIndex(CellIndex) && !TerrainCells[CellIndex].bIsDestroyed)
                        {
                            AffectedCells.Add(&TerrainCells[CellIndex]);
                        }
                    }
                }
            }
        }
    }
    
    return AffectedCells;
}

// Fonction pour créer une modification circulaire
UFUNCTION(BlueprintCallable, Category = "Terrain")
void ADestructibleTerrain::RequestDestroyTerrainCircle(FVector2D Center, float Radius)
{
    // Créer une modification circulaire
    FTerrainModification Mod = FTerrainModification::MakeCircular(Center, Radius);
    
    // Appeler la fonction serveur pour valider et appliquer la destruction
    if (GetLocalRole() < ROLE_Authority)
    {
        Server_DestroyTerrainCircle(Center, Radius);
    }
    else
    {
        // Si déjà sur le serveur, appliquer directement
        Server_DestroyTerrainCircle(Center, Radius);
    }
}

// RPC serveur pour les destructions circulaires
void ADestructibleTerrain::Server_DestroyTerrainCircle(FVector2D Center, float Radius)
{
    UE_LOG(LogTemp, Warning, TEXT("Destroying terrain in circle at position (%f, %f) with radius %f"), 
        Center.X, Center.Y, Radius);
    
    // Créer une nouvelle modification circulaire
    FTerrainModification NewMod = FTerrainModification::MakeCircular(Center, Radius);
    
    // Ajouter à la liste globale des modifications
    TerrainModifications.Add(NewMod);
    
    // Afficher le nombre total de modifications
    UE_LOG(LogTemp, Warning, TEXT("Total modifications: %d"), TerrainModifications.Num());
    
    if (bUseTerrainSections)
    {
        // Assigner cette modification aux sections appropriées et les mettre à jour
        AssignModificationToSections(NewMod);
    }
    else
    {
        // Approche classique : appliquer toutes les modifications sur le mesh entier
        ApplyTerrainModifications();
    }
}

bool ADestructibleTerrain::Server_DestroyTerrainCircle_Validate(FVector2D Center, float Radius)
{
    // Validation simple: s'assurer que le rayon est positif
    return Radius > 0.0f;
}

// Fonction pour ajuster les paramètres de la structure interne à l'exécution
UFUNCTION(BlueprintCallable, Category = "Terrain|Internal")
void ADestructibleTerrain::SetInternalStructureParameters(
    float NewCellSizeX, 
    float NewCellSizeY, 
    float NewCellSizeZ, 
    float NewWallThickness, 
    bool bRegenerate)
{
    // Mettre à jour les paramètres
    CellSizeX = FMath::Max(10.0f, NewCellSizeX);
    CellSizeY = FMath::Max(10.0f, NewCellSizeY);
    CellSizeZ = FMath::Max(10.0f, NewCellSizeZ);
    WallThickness = FMath::Max(1.0f, NewWallThickness);
    
    UE_LOG(LogTemp, Log, TEXT("Updated internal structure parameters: Cell Size = (%f, %f, %f), Wall Thickness = %f"),
        CellSizeX, CellSizeY, CellSizeZ, WallThickness);
    
    // Si demandé, régénérer le terrain avec les nouveaux paramètres
    if (bRegenerate && HasAuthority())
    {
        // Regenerate internal structure only
        InternalVertices.Empty();
        InternalTriangles.Empty();
        InternalUVs.Empty();
        InternalNormals.Empty();
        InternalVertexColors.Empty();
        
        // Générer la nouvelle structure interne
        GenerateInternalStructure();
        
        // Mettre à jour le mesh
        MeshData.Vertices.Empty();
        MeshData.Triangles.Empty();
        MeshData.UVs.Empty();
        MeshData.Normals.Empty();
        MeshData.VertexColors.Empty();
        
        // Recréer tout le terrain
        GenerateTerrain();
        
        // Réappliquer les modifications existantes
        ApplyTerrainModifications();
    }
}

void ADestructibleTerrain::ApplyTerrainModifications()
{
    // Si aucune modification, ne rien faire
    if (TerrainModifications.Num() == 0)
    {
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Applying %d terrain modifications"), TerrainModifications.Num());
    
    // On ne traite que les modifications non appliquées
    TArray<FTerrainModification> NewModifications;
    for (const FTerrainModification& Mod : TerrainModifications)
    {
        if (!AppliedModifications.Contains(Mod))
        {
            NewModifications.Add(Mod);
        }
    }
    
    if (NewModifications.Num() == 0)
    {
        return; // Toutes les modifications ont déjà été appliquées
    }
    
    // Nous allons travailler avec des copies des données existantes
    TArray<FVector> NewVertices = MeshData.Vertices;
    TArray<int32> NewTriangles; // Sera rempli avec les triangles conservés et modifiés
    TArray<FVector2D> NewUVs = MeshData.UVs;
    TArray<FColor> NewVertexColors = MeshData.VertexColors;
    
    // Lister les cellules affectées par les nouvelles modifications
    TArray<FTerrainCell*> AllAffectedCells;
    
    // Utiliser une approche par lot pour traiter toutes les modifications d'un coup
    for (const FTerrainModification& Mod : NewModifications)
    {
        TArray<FTerrainCell*> CellsAffectedByThisMod = GetAffectedCells(Mod);
        
        for (FTerrainCell* Cell : CellsAffectedByThisMod)
        {
            if (Cell && !Cell->bIsDestroyed)
            {
                // Marquer la cellule comme détruite
                Cell->bIsDestroyed = true;
                AllAffectedCells.AddUnique(Cell);
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Found %d affected cells"), AllAffectedCells.Num());
    
    if (AllAffectedCells.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No cells affected, no changes to make"));
        
        // Ajouter quand même les modifications à la liste des appliquées
        for (const FTerrainModification& Mod : NewModifications)
        {
            AppliedModifications.Add(Mod);
        }
        
        return;
    }
    
    // 1. Première étape: identifier les triangles touchés par les cellules détruites
    TBitArray<> TriangleToKeep;
    TriangleToKeep.Init(true, MeshData.Triangles.Num() / 3);
    
    for (int32 TriIdx = 0; TriIdx < MeshData.Triangles.Num() / 3; TriIdx++)
    {
        int32 Index1 = MeshData.Triangles[TriIdx * 3];
        int32 Index2 = MeshData.Triangles[TriIdx * 3 + 1];
        int32 Index3 = MeshData.Triangles[TriIdx * 3 + 2];
        
        // Vérifier que les indices sont valides
        if (NewVertices.IsValidIndex(Index1) && NewVertices.IsValidIndex(Index2) && NewVertices.IsValidIndex(Index3))
        {
            // Récupérer les vertices du triangle
            FVector Vertex1 = NewVertices[Index1];
            FVector Vertex2 = NewVertices[Index2];
            FVector Vertex3 = NewVertices[Index3];
            
            // Vérifier si au moins un des vertices est dans une cellule détruite
            bool bTriangleAffected = false;
            
            for (FTerrainCell* Cell : AllAffectedCells)
            {
                if (Cell && (Cell->ContainsPoint(Vertex1) || Cell->ContainsPoint(Vertex2) || Cell->ContainsPoint(Vertex3)))
                {
                    bTriangleAffected = true;
                    break;
                }
            }
            
            if (bTriangleAffected)
            {
                TriangleToKeep[TriIdx] = false;
            }
        }
        else
        {
            // Si les indices sont invalides, ne pas garder ce triangle
            TriangleToKeep[TriIdx] = false;
            UE_LOG(LogTemp, Warning, TEXT("Invalid triangle index detected: %d, %d, %d"), Index1, Index2, Index3);
        }
    }
    
    // 2. Construire le nouveau tableau de triangles en ne gardant que ceux qui ne sont pas touchés
    for (int32 TriIdx = 0; TriIdx < MeshData.Triangles.Num() / 3; TriIdx++)
    {
        if (TriangleToKeep[TriIdx])
        {
            // Ajouter les indices du triangle
            NewTriangles.Add(MeshData.Triangles[TriIdx * 3]);
            NewTriangles.Add(MeshData.Triangles[TriIdx * 3 + 1]);
            NewTriangles.Add(MeshData.Triangles[TriIdx * 3 + 2]);
        }
    }
    
    // 3. Créer des faces propres aux bords des cellules détruites
    CreateCleanCellCutFaces(AllAffectedCells, NewVertices, NewTriangles, NewUVs, NewVertexColors);
    
    // 4. Nettoyer les vertices orphelins
    CleanupOrphanVertices(NewVertices, NewTriangles, NewUVs, NewVertexColors, MeshData.Normals);
    
    // 5. Mettre à jour les données de mesh
    MeshData.Vertices = NewVertices;
    MeshData.Triangles = NewTriangles;
    MeshData.UVs = NewUVs;
    MeshData.VertexColors = NewVertexColors;
    
    // 6. Recalculer les normales
    MeshData.Normals.Init(FVector::ZeroVector, MeshData.Vertices.Num());
    
    for (int32 i = 0; i < NewTriangles.Num(); i += 3)
    {
        // Obtenir les indices des vertices du triangle
        int32 Index0 = NewTriangles[i];
        int32 Index1 = NewTriangles[i + 1];
        int32 Index2 = NewTriangles[i + 2];
        
        // Vérifier si les indices sont valides
        if (NewVertices.IsValidIndex(Index0) && NewVertices.IsValidIndex(Index1) && NewVertices.IsValidIndex(Index2))
        {
            // Calculer les vecteurs des côtés du triangle
            FVector Side1 = NewVertices[Index1] - NewVertices[Index0];
            FVector Side2 = NewVertices[Index2] - NewVertices[Index0];
            
            // Calculer la normale du triangle (produit vectoriel)
            FVector Normal = FVector::CrossProduct(Side1, Side2).GetSafeNormal();
            
            // Ajouter la normale à chaque vertex du triangle
            MeshData.Normals[Index0] += Normal;
            MeshData.Normals[Index1] += Normal;
            MeshData.Normals[Index2] += Normal;
        }
    }
    
    // Normaliser toutes les normales
    for (int32 i = 0; i < MeshData.Normals.Num(); i++)
    {
        if (!MeshData.Normals[i].IsZero())
        {
            MeshData.Normals[i] = MeshData.Normals[i].GetSafeNormal();
        }
        else
        {
            // Par défaut, on met une normale vers l'extérieur
            MeshData.Normals[i] = FVector(0.0f, -1.0f, 0.0f);
        }
    }
    
    // Marquer les données comme valides
    MeshData.bIsValid = true;
    
    // 7. Appliquer les données au mesh
    CreateMeshFromData(MeshData);
    
    // 8. Ajouter les nouvelles modifications à la liste des modifications appliquées
    for (const FTerrainModification& Mod : NewModifications)
    {
        AppliedModifications.Add(Mod);
    }
    
    // Marquer que les modifications ont été appliquées
    bModificationsApplied = true;
    
    // 9. Spawner les effets de destruction APRÈS avoir mis à jour le mesh
    SpawnDestructionEffects(AllAffectedCells);
    
    // 10. Si nous sommes sur le serveur, répliquer ces données vers tous les clients
    if (HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("Replicating updated mesh to clients"));
        Multicast_UpdateTerrainMesh(MeshData);
    }
}


// Fonction pour créer des faces propres aux bords des zones détruites
void ADestructibleTerrain::CreateCleanCutFaces(
    const TArray<FTerrainModification>& Modifications,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors)
{
    // Pour chaque modification, créer des faces de "coupe" propres
    for (const FTerrainModification& Mod : Modifications)
    {
        if (Mod.bIsCircular)
        {
            CreateCircularCutFaces(Mod, Vertices, Triangles, UVs, VertexColors);
        }
        else
        {
            CreateRectangularCutFaces(Mod, Vertices, Triangles, UVs, VertexColors);
        }
    }
}

void ADestructibleTerrain::CreateRectangularCutFaces(
    const FTerrainModification& Mod,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors)
{
    // Obtenir les coins du rectangle de modification
    float MinX = Mod.Position.X;
    float MinZ = Mod.Position.Y;
    float MaxX = MinX + Mod.Size.X;
    float MaxZ = MinZ + Mod.Size.Y;
    
    // Créer des faces pour les 4 plans de coupe (avant/arrière de chaque côté)
    
    // Pour la couleur des faces de coupe
    FColor CutFaceColor = FColor(100, 80, 60, 255); // Marron pour les coupes
    
    // 1. Face de coupe gauche
    CreateCutFace(
        FVector(MinX, 0.0f, MinZ),             // Bas avant
        FVector(MinX, TerrainDepth, MinZ),     // Bas arrière
        FVector(MinX, 0.0f, MaxZ),             // Haut avant
        FVector(MinX, TerrainDepth, MaxZ),     // Haut arrière
        Vertices, Triangles, UVs, VertexColors, CutFaceColor
    );
    
    // 2. Face de coupe droite
    CreateCutFace(
        FVector(MaxX, 0.0f, MinZ),             // Bas avant
        FVector(MaxX, TerrainDepth, MinZ),     // Bas arrière
        FVector(MaxX, 0.0f, MaxZ),             // Haut avant
        FVector(MaxX, TerrainDepth, MaxZ),     // Haut arrière
        Vertices, Triangles, UVs, VertexColors, CutFaceColor
    );
    
    // 3. Face de coupe inférieure
    CreateCutFace(
        FVector(MinX, 0.0f, MinZ),             // Gauche avant
        FVector(MinX, TerrainDepth, MinZ),     // Gauche arrière
        FVector(MaxX, 0.0f, MinZ),             // Droite avant
        FVector(MaxX, TerrainDepth, MinZ),     // Droite arrière
        Vertices, Triangles, UVs, VertexColors, CutFaceColor
    );
    
    // 4. Face de coupe supérieure
    CreateCutFace(
        FVector(MinX, 0.0f, MaxZ),             // Gauche avant
        FVector(MinX, TerrainDepth, MaxZ),     // Gauche arrière
        FVector(MaxX, 0.0f, MaxZ),             // Droite avant
        FVector(MaxX, TerrainDepth, MaxZ),     // Droite arrière
        Vertices, Triangles, UVs, VertexColors, CutFaceColor
    );
}

void ADestructibleTerrain::CreateCircularCutFaces(
    const FTerrainModification& Mod,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors)
{
    // Obtenir le centre et le rayon du cercle
    FVector2D Center = Mod.CircleCenter;
    float Radius = Mod.CircleRadius;
    
    // Nombre de segments pour approximer le cercle
    const int32 NumSegments = 24; // Plus de segments = meilleure qualité
    
    // Couleur pour les faces de coupe
    FColor CutFaceColor = FColor(100, 80, 60, 255);
    
    // Créer un cylindre de coupe
    TArray<FVector> CircleVerticesFront;
    TArray<FVector> CircleVerticesBack;
    
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle = 2.0f * PI * i / NumSegments;
        float X = Center.X + Radius * FMath::Cos(Angle);
        float Z = Center.Y + Radius * FMath::Sin(Angle);
        
        // Créer les vertices avant et arrière
        CircleVerticesFront.Add(FVector(X, 0.0f, Z));
        CircleVerticesBack.Add(FVector(X, TerrainDepth, Z));
    }
    
    // Index de départ pour les nouveaux vertices
    int32 BaseVertexIndex = Vertices.Num();
    
    // Ajouter tous les vertices au mesh
    for (int32 i = 0; i < NumSegments; ++i)
    {
        // Vertices avant
        Vertices.Add(CircleVerticesFront[i]);
        UVs.Add(FVector2D(static_cast<float>(i) / NumSegments, 0.0f));
        VertexColors.Add(CutFaceColor);
        
        // Vertices arrière
        Vertices.Add(CircleVerticesBack[i]);
        UVs.Add(FVector2D(static_cast<float>(i) / NumSegments, 1.0f));
        VertexColors.Add(CutFaceColor);
    }
    
    // Créer les triangles pour les faces de coupe
    for (int32 i = 0; i < NumSegments; ++i)
    {
        int32 NextI = (i + 1) % NumSegments;
        
        // Indices des vertices actuels
        int32 CurFront = BaseVertexIndex + i * 2;
        int32 CurBack = BaseVertexIndex + i * 2 + 1;
        
        // Indices des vertices suivants
        int32 NextFront = BaseVertexIndex + NextI * 2;
        int32 NextBack = BaseVertexIndex + NextI * 2 + 1;
        
        // Créer la face du côté
        Triangles.Add(CurFront);
        Triangles.Add(NextFront);
        Triangles.Add(CurBack);
        
        Triangles.Add(NextFront);
        Triangles.Add(NextBack);
        Triangles.Add(CurBack);
    }
}

void ADestructibleTerrain::CreateCutFace(
    const FVector& Corner1,
    const FVector& Corner2,
    const FVector& Corner3,
    const FVector& Corner4,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors,
    const FColor& BaseColor)
{
    // Index de départ pour les nouveaux vertices
    int32 BaseIndex = Vertices.Num();
    
    // Ajouter les 4 vertices
    Vertices.Add(Corner1);
    Vertices.Add(Corner2);
    Vertices.Add(Corner3);
    Vertices.Add(Corner4);
    
    // Ajouter les coordonnées UV
    // Utiliser une projection adaptée à l'orientation de la face
    FVector FaceNormal = FVector::CrossProduct(Corner3 - Corner1, Corner2 - Corner1).GetSafeNormal();
    
    // Déterminer l'orientation principale de la face
    float AbsX = FMath::Abs(FaceNormal.X);
    float AbsY = FMath::Abs(FaceNormal.Y);
    float AbsZ = FMath::Abs(FaceNormal.Z);
    
    if (AbsX >= AbsY && AbsX >= AbsZ)
    {
        // Face orientée selon X, utiliser Y et Z pour les UV
        UVs.Add(FVector2D(Corner1.Y / 100.0f, Corner1.Z / 100.0f));
        UVs.Add(FVector2D(Corner2.Y / 100.0f, Corner2.Z / 100.0f));
        UVs.Add(FVector2D(Corner3.Y / 100.0f, Corner3.Z / 100.0f));
        UVs.Add(FVector2D(Corner4.Y / 100.0f, Corner4.Z / 100.0f));
    }
    else if (AbsY >= AbsX && AbsY >= AbsZ)
    {
        // Face orientée selon Y, utiliser X et Z pour les UV
        UVs.Add(FVector2D(Corner1.X / 100.0f, Corner1.Z / 100.0f));
        UVs.Add(FVector2D(Corner2.X / 100.0f, Corner2.Z / 100.0f));
        UVs.Add(FVector2D(Corner3.X / 100.0f, Corner3.Z / 100.0f));
        UVs.Add(FVector2D(Corner4.X / 100.0f, Corner4.Z / 100.0f));
    }
    else
    {
        // Face orientée selon Z, utiliser X et Y pour les UV
        UVs.Add(FVector2D(Corner1.X / 100.0f, Corner1.Y / 100.0f));
        UVs.Add(FVector2D(Corner2.X / 100.0f, Corner2.Y / 100.0f));
        UVs.Add(FVector2D(Corner3.X / 100.0f, Corner3.Y / 100.0f));
        UVs.Add(FVector2D(Corner4.X / 100.0f, Corner4.Y / 100.0f));
    }
    
    // Ajouter un peu de variation à la couleur en fonction de la position
    for (int32 i = 0; i < 4; ++i)
    {
        FVector CornerPosition = Vertices[BaseIndex + i];
        
        // Variation de couleur basée sur la profondeur (axe Y)
        float DepthFactor = FMath::Clamp((CornerPosition.Y / TerrainDepth) * 0.3f, 0.0f, 0.3f);
        
        // Variation de couleur basée sur la hauteur (axe Z)
        float HeightFactor = FMath::Clamp((CornerPosition.Z / TerrainHeight) * 0.2f, 0.0f, 0.2f);
        
        // Créer une couleur avec variation
        FColor ModifiedColor = BaseColor;
        ModifiedColor.R = FMath::Clamp(int32(BaseColor.R * (1.0f - DepthFactor + HeightFactor)), 0, 255);
        ModifiedColor.G = FMath::Clamp(int32(BaseColor.G * (1.0f - DepthFactor + HeightFactor)), 0, 255);
        ModifiedColor.B = FMath::Clamp(int32(BaseColor.B * (1.0f - DepthFactor + HeightFactor)), 0, 255);
        
        VertexColors.Add(ModifiedColor);
    }
    
    // Ajouter les deux triangles qui forment le quad
    Triangles.Add(BaseIndex);
    Triangles.Add(BaseIndex + 2);
    Triangles.Add(BaseIndex + 1);
    
    Triangles.Add(BaseIndex + 1);
    Triangles.Add(BaseIndex + 2);
    Triangles.Add(BaseIndex + 3);
}

// Fonction pour ajouter des effets de débris et particules lors de la destruction
void ADestructibleTerrain::SpawnDestructionEffects(const TArray<FTerrainCell*>& DestroyedCells)
{
    // Ne rien faire s'il n'y a pas de cellules détruites
    if (DestroyedCells.Num() == 0)
    {
        return;
    }
    
    // Calculer le centre de la zone détruite
    FVector AveragePosition = FVector::ZeroVector;
    int32 CellsUsedForAverage = 0;
    
    for (const FTerrainCell* Cell : DestroyedCells)
    {
        if (Cell)
        {
            AveragePosition += (Cell->Min + Cell->Max) * 0.5f;
            CellsUsedForAverage++;
        }
    }
    
    if (CellsUsedForAverage > 0)
    {
        AveragePosition /= CellsUsedForAverage;
    }
    
    // Si nous avons un effet de destruction, le jouer à cette position
    if (DestructionEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            DestructionEffect,
            AveragePosition,
            FRotator::ZeroRotator,
            true
        );
    }
    
    // Si nous avons un son de destruction, le jouer à cette position
    if (DestructionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            DestructionSound,
            AveragePosition
        );
    }
    
    // Réduire la quantité de débris pour de meilleures performances
    int32 MaxDebrisToSpawn = FMath::Min(DestroyedCells.Num() * NumDebrisPerCell, 50);
    int32 DebrisSpawned = 0;
    
    // Créer une copie du tableau pour le tri
    TArray<FTerrainCell*> PrioritizedCells = DestroyedCells;
    
    // Trouver la caméra du joueur si disponible
    FVector CameraLocation = FVector::ZeroVector;
    FRotator CameraRotation = FRotator::ZeroRotator;
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
        
        // Tri manuel plutôt que d'utiliser la méthode Sort()
        for (int32 i = 0; i < PrioritizedCells.Num() - 1; i++)
        {
            for (int32 j = 0; j < PrioritizedCells.Num() - i - 1; j++)
            {
                if (PrioritizedCells[j] && PrioritizedCells[j+1])
                {
                    FVector CenterA = (PrioritizedCells[j]->Min + PrioritizedCells[j]->Max) * 0.5f;
                    FVector CenterB = (PrioritizedCells[j+1]->Min + PrioritizedCells[j+1]->Max) * 0.5f;
                    
                    if (FVector::DistSquared(CenterA, CameraLocation) > FVector::DistSquared(CenterB, CameraLocation))
                    {
                        // Échanger les cellules
                        FTerrainCell* Temp = PrioritizedCells[j];
                        PrioritizedCells[j] = PrioritizedCells[j+1];
                        PrioritizedCells[j+1] = Temp;
                    }
                }
            }
        }
    }
    
    // Spawn de débris pour chaque cellule, en commençant par les plus prioritaires
    for (FTerrainCell* Cell : PrioritizedCells)
    {
        if (!Cell || DebrisSpawned >= MaxDebrisToSpawn)
            break;
            
        // Calculer le centre de la cellule
        FVector CellCenter = (Cell->Min + Cell->Max) * 0.5f;
        
        // Si nous avons un blueprint de débris
        if (DebrisClass)
        {
            // Déterminer combien de débris spawner pour cette cellule
            int32 DebrisForThisCell = FMath::Min(NumDebrisPerCell, MaxDebrisToSpawn - DebrisSpawned);
            
            // Spawner plusieurs débris avec des positions et rotations aléatoires
            for (int32 i = 0; i < DebrisForThisCell; ++i)
            {
                // Position aléatoire dans la cellule, mais ajustée pour être plus proche du centre
                FVector RandomOffset(
                    FMath::RandRange(-0.3f, 0.3f) * (Cell->Max.X - Cell->Min.X),
                    FMath::RandRange(-0.3f, 0.3f) * (Cell->Max.Y - Cell->Min.Y),
                    FMath::RandRange(-0.3f, 0.3f) * (Cell->Max.Z - Cell->Min.Z)
                );
                
                FVector SpawnLocation = CellCenter + RandomOffset;
                
                // Rotation aléatoire
                FRotator SpawnRotation(
                    FMath::RandRange(0.0f, 360.0f),
                    FMath::RandRange(0.0f, 360.0f),
                    FMath::RandRange(0.0f, 360.0f)
                );
                
                // Taille aléatoire
                float DebrisScale = FMath::RandRange(0.3f, 0.8f);
                
                // Paramètres de spawn
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
                
                // Spawner le débris
                AActor* Debris = GetWorld()->SpawnActor<AActor>(
                    DebrisClass,
                    SpawnLocation,
                    SpawnRotation,
                    SpawnParams
                );
                
                // Ajuster la taille
                if (Debris)
                {
                    Debris->SetActorScale3D(FVector(DebrisScale));
                    
                    // Si le débris a un composant de mesh, lui donner une impulsion
                    UStaticMeshComponent* MeshComp = Debris->FindComponentByClass<UStaticMeshComponent>();
                    if (MeshComp && MeshComp->IsSimulatingPhysics())
                    {
                        // Direction d'impulsion aléatoire, principalement vers l'extérieur
                        FVector ImpulseDir = (SpawnLocation - CellCenter).GetSafeNormal();
                        
                        // Ajouter une composante verticale pour éviter que les débris ne tombent directement
                        ImpulseDir += FVector(
                            FMath::RandRange(-0.1f, 0.1f),
                            FMath::RandRange(-0.1f, 0.1f),
                            FMath::RandRange(0.2f, 0.5f)
                        );
                        ImpulseDir = ImpulseDir.GetSafeNormal();
                        
                        // Force aléatoire
                        float ImpulseStrength = FMath::RandRange(300.0f, 700.0f);
                        
                        // Appliquer l'impulsion
                        MeshComp->AddImpulse(ImpulseDir * ImpulseStrength);
                        
                        // Ajouter une rotation aléatoire
                        MeshComp->AddAngularImpulseInDegrees(
                            FVector(
                                FMath::RandRange(-300.0f, 300.0f),
                                FMath::RandRange(-300.0f, 300.0f),
                                FMath::RandRange(-300.0f, 300.0f)
                            )
                        );
                        
                        // Définir une durée de vie limitée pour les débris
                        FTimerHandle DestroyTimerHandle;
                        float DebrisLifetime = FMath::RandRange(5.0f, 10.0f);
                        GetWorld()->GetTimerManager().SetTimer(
                            DestroyTimerHandle,
                            FTimerDelegate::CreateLambda([Debris]() {
                                if (Debris && Debris->IsValidLowLevel())
                                {
                                    Debris->Destroy();
                                }
                            }),
                            DebrisLifetime,
                            false
                        );
                    }
                    
                    DebrisSpawned++;
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Spawned %d debris for %d destroyed cells"), DebrisSpawned, DestroyedCells.Num());
}


void ADestructibleTerrain::Multicast_ForceVisualUpdate_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("ForceVisualUpdate called on %s"), HasAuthority() ? TEXT("server") : TEXT("client"));
    
    if (MeshData.bIsValid)
    {
        // Recréer le mesh à partir des données actuelles
        CreateMeshFromData(MeshData);
    }
    else if (bIsInitialized)
    {
        // Si les données ne sont pas valides mais que le terrain est initialisé, régénérer
        GenerateTerrain();
    }
}

void ADestructibleTerrain::UpdateLOD()
{
    if (!bUseLOD || !bIsInitialized)
    {
        return;
    }
    
    // Calculer la distance au joueur le plus proche
    float DistanceToPlayer = GetDistanceToNearestPlayer();
    
    // Déterminer si nous devons utiliser le niveau de détail bas
    bool bShouldUseLOD = (DistanceToPlayer > LODDistanceThreshold);
    
    // Si l'état du LOD a changé, mettre à jour le mesh
    if (bShouldUseLOD != bIsUsingLOD)
    {
        UE_LOG(LogTemp, Verbose, TEXT("LOD state changed: distance = %.1f, using LOD = %s"), 
            DistanceToPlayer, bShouldUseLOD ? TEXT("true") : TEXT("false"));
        
        // Basculer la résolution
        SwitchResolution(bShouldUseLOD);
        
        // Mémoriser le nouvel état
        bIsUsingLOD = bShouldUseLOD;
    }
}

float ADestructibleTerrain::GetDistanceToNearestPlayer()
{
    // Obtenir tous les joueurs
    TArray<AActor*> PlayerPawns;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), PlayerPawns);
    
    if (PlayerPawns.Num() == 0)
    {
        return TNumericLimits<float>::Max(); // Aucun joueur trouvé
    }
    
    // Calculer la distance au joueur le plus proche
    float MinDistance = TNumericLimits<float>::Max();
    
    for (AActor* PlayerActor : PlayerPawns)
    {
        float Distance = FVector::Distance(GetActorLocation(), PlayerActor->GetActorLocation());
        MinDistance = FMath::Min(MinDistance, Distance);
    }
    
    return MinDistance;
}

void ADestructibleTerrain::SwitchResolution(bool bUseLowResolution)
{
    if (!HasAuthority() || !bUseLOD)
    {
        // Ne changer la résolution que sur le serveur
        return;
    }
    
    // Sauvegarder les résolutions actuelles
    static int32 HighHorizontalResolution = HorizontalResolution;
    static int32 HighVerticalResolution = VerticalResolution;
    
    if (bUseLowResolution)
    {
        // Sauvegarder la haute résolution si ce n'est pas déjà fait
        if (HorizontalResolution > LODHorizontalResolution)
        {
            HighHorizontalResolution = HorizontalResolution;
            HighVerticalResolution = VerticalResolution;
        }
        
        // Passer à la basse résolution
        HorizontalResolution = LODHorizontalResolution;
        VerticalResolution = LODVerticalResolution;
    }
    else
    {
        // Restaurer la résolution normale
        HorizontalResolution = HighHorizontalResolution;
        VerticalResolution = HighVerticalResolution;
    }
    
    // Réappliquer toutes les modifications avec la nouvelle résolution
    GenerateTerrain();
    
    // Log pour le débogage
    UE_LOG(LogTemp, Log, TEXT("Switched terrain resolution to %s (%d x %d)"), 
        bUseLowResolution ? TEXT("LOD") : TEXT("normal"),
        HorizontalResolution, VerticalResolution);
}
void ADestructibleTerrain::CreateCleanCellCutFaces(
    const TArray<FTerrainCell*>& AffectedCells,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors)
{
    // Couleur pour les faces de coupe
    FColor CutFaceColor = FColor(100, 80, 60, 255); // Marron pour les coupes
    
    // Pour éviter de créer des faces dupliquées, nous allons garder une trace des faces déjà créées
    TSet<TPair<FIntPoint, FIntPoint>> CreatedFaces;
    
    // Pour chaque cellule affectée, créer des faces de coupe aux bords de la cellule
    for (FTerrainCell* Cell : AffectedCells)
    {
        if (!Cell)
            continue;
            
        // Récupérer les coordonnées de la cellule
        float MinX = Cell->Min.X;
        float MinY = Cell->Min.Y;
        float MinZ = Cell->Min.Z;
        float MaxX = Cell->Max.X;
        float MaxY = Cell->Max.Y;
        float MaxZ = Cell->Max.Z;
        
        // Pour chaque face de la cellule, vérifier si c'est une face extérieure
        // Face X- (gauche)
        if (Cell->GridX == 0 || !IsCellDestroyed(Cell->GridX - 1, Cell->GridY, Cell->GridZ))
        {
            // Clé unique pour identifier cette face (combinaison des coordonnées de grille)
            TPair<FIntPoint, FIntPoint> FaceKey(
                FIntPoint(Cell->GridX * 10 - 1, Cell->GridY * 10),
                FIntPoint(Cell->GridZ * 10, 0) // Second élément pour rendre la clé unique
            );
            
            if (!CreatedFaces.Contains(FaceKey))
            {
                CreatedFaces.Add(FaceKey);
                CreateCutFace(
                    FVector(MinX, MinY, MinZ), // Coin bas avant
                    FVector(MinX, MaxY, MinZ), // Coin bas arrière
                    FVector(MinX, MinY, MaxZ), // Coin haut avant
                    FVector(MinX, MaxY, MaxZ), // Coin haut arrière
                    Vertices, Triangles, UVs, VertexColors, CutFaceColor
                );
            }
        }
        
        // Face X+ (droite)
        if (Cell->GridX == CellCountX - 1 || !IsCellDestroyed(Cell->GridX + 1, Cell->GridY, Cell->GridZ))
        {
            // Clé unique pour cette face
            TPair<FIntPoint, FIntPoint> FaceKey(
                FIntPoint(Cell->GridX * 10 + 1, Cell->GridY * 10),
                FIntPoint(Cell->GridZ * 10, 1) // Second élément pour rendre la clé unique
            );
            
            if (!CreatedFaces.Contains(FaceKey))
            {
                CreatedFaces.Add(FaceKey);
                CreateCutFace(
                    FVector(MaxX, MinY, MinZ), // Coin bas avant
                    FVector(MaxX, MaxY, MinZ), // Coin bas arrière
                    FVector(MaxX, MinY, MaxZ), // Coin haut avant
                    FVector(MaxX, MaxY, MaxZ), // Coin haut arrière
                    Vertices, Triangles, UVs, VertexColors, CutFaceColor
                );
            }
        }
        
        // Face Y- (avant)
        if (Cell->GridY == 0 || !IsCellDestroyed(Cell->GridX, Cell->GridY - 1, Cell->GridZ))
        {
            // Clé unique pour cette face
            TPair<FIntPoint, FIntPoint> FaceKey(
                FIntPoint(Cell->GridX * 10, Cell->GridY * 10 - 1),
                FIntPoint(Cell->GridZ * 10, 2) // Second élément pour rendre la clé unique
            );
            
            if (!CreatedFaces.Contains(FaceKey))
            {
                CreatedFaces.Add(FaceKey);
                CreateCutFace(
                    FVector(MinX, MinY, MinZ), // Coin bas gauche
                    FVector(MaxX, MinY, MinZ), // Coin bas droite
                    FVector(MinX, MinY, MaxZ), // Coin haut gauche
                    FVector(MaxX, MinY, MaxZ), // Coin haut droite
                    Vertices, Triangles, UVs, VertexColors, CutFaceColor
                );
            }
        }
        
        // Face Y+ (arrière)
        if (Cell->GridY == CellCountY - 1 || !IsCellDestroyed(Cell->GridX, Cell->GridY + 1, Cell->GridZ))
        {
            // Clé unique pour cette face
            TPair<FIntPoint, FIntPoint> FaceKey(
                FIntPoint(Cell->GridX * 10, Cell->GridY * 10 + 1),
                FIntPoint(Cell->GridZ * 10, 3) // Second élément pour rendre la clé unique
            );
            
            if (!CreatedFaces.Contains(FaceKey))
            {
                CreatedFaces.Add(FaceKey);
                CreateCutFace(
                    FVector(MinX, MaxY, MinZ), // Coin bas gauche
                    FVector(MaxX, MaxY, MinZ), // Coin bas droite
                    FVector(MinX, MaxY, MaxZ), // Coin haut gauche
                    FVector(MaxX, MaxY, MaxZ), // Coin haut droite
                    Vertices, Triangles, UVs, VertexColors, CutFaceColor
                );
            }
        }
        
        // Face Z- (bas)
        if (Cell->GridZ == 0 || !IsCellDestroyed(Cell->GridX, Cell->GridY, Cell->GridZ - 1))
        {
            // Clé unique pour cette face
            TPair<FIntPoint, FIntPoint> FaceKey(
                FIntPoint(Cell->GridX * 10, Cell->GridY * 10),
                FIntPoint(Cell->GridZ * 10 - 1, 4) // Second élément pour rendre la clé unique
            );
            
            if (!CreatedFaces.Contains(FaceKey))
            {
                CreatedFaces.Add(FaceKey);
                CreateCutFace(
                    FVector(MinX, MinY, MinZ), // Coin avant gauche
                    FVector(MaxX, MinY, MinZ), // Coin avant droite
                    FVector(MinX, MaxY, MinZ), // Coin arrière gauche
                    FVector(MaxX, MaxY, MinZ), // Coin arrière droite
                    Vertices, Triangles, UVs, VertexColors, CutFaceColor
                );
            }
        }
        
        // Face Z+ (haut)
        if (Cell->GridZ == CellCountZ - 1 || !IsCellDestroyed(Cell->GridX, Cell->GridY, Cell->GridZ + 1))
        {
            // Clé unique pour cette face
            TPair<FIntPoint, FIntPoint> FaceKey(
                FIntPoint(Cell->GridX * 10, Cell->GridY * 10),
                FIntPoint(Cell->GridZ * 10 + 1, 5) // Second élément pour rendre la clé unique
            );
            
            if (!CreatedFaces.Contains(FaceKey))
            {
                CreatedFaces.Add(FaceKey);
                CreateCutFace(
                    FVector(MinX, MinY, MaxZ), // Coin avant gauche
                    FVector(MaxX, MinY, MaxZ), // Coin avant droite
                    FVector(MinX, MaxY, MaxZ), // Coin arrière gauche
                    FVector(MaxX, MaxY, MaxZ), // Coin arrière droite
                    Vertices, Triangles, UVs, VertexColors, CutFaceColor
                );
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Created %d unique cut faces"), CreatedFaces.Num());
}

// Fonction pour vérifier si une cellule est détruite
bool ADestructibleTerrain::IsCellDestroyed(int32 GridX, int32 GridY, int32 GridZ)
{
    // Vérifier que les indices sont valides
    if (GridX < 0 || GridX >= CellCountX ||
        GridY < 0 || GridY >= CellCountY ||
        GridZ < 0 || GridZ >= CellCountZ)
    {
        // Hors limites, considérée comme non détruite
        return false;
    }
    
    // Trouver l'index de la cellule dans le tableau
    int32 CellIndex = (GridZ * CellCountY + GridY) * CellCountX + GridX;
    
    // Vérifier si cette cellule existe et est détruite
    if (TerrainCells.IsValidIndex(CellIndex))
    {
        return TerrainCells[CellIndex].bIsDestroyed;
    }
    
    return false;
}
void ADestructibleTerrain::CleanupOrphanVertices(
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors,
    TArray<FVector>& Normals)
{
    // Vérifier que tous les tableaux ont des dimensions cohérentes
    if (Vertices.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("CleanupOrphanVertices: No vertices to clean up!"));
        return;
    }
    
    if (Vertices.Num() != UVs.Num() || Vertices.Num() != VertexColors.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("CleanupOrphanVertices: Array size mismatch! V=%d, UV=%d, Color=%d"),
               Vertices.Num(), UVs.Num(), VertexColors.Num());
               
        // Ajuster les tableaux pour qu'ils aient la même taille
        UVs.SetNum(Vertices.Num());
        VertexColors.SetNum(Vertices.Num());
    }
    
    // Ajuster les normales si nécessaire
    if (Normals.Num() != Vertices.Num())
    {
        Normals.SetNum(Vertices.Num());
        
        // Initialiser les nouvelles normales
        for (int32 i = Normals.Num() - 1; i >= 0; --i)
        {
            if (Normals[i].IsZero())
            {
                Normals[i] = FVector(0.0f, -1.0f, 0.0f);
            }
        }
    }
    
    // Marquer tous les vertices comme non utilisés
    TArray<bool> VertexIsUsed;
    VertexIsUsed.Init(false, Vertices.Num());
    
    // Parcourir tous les triangles et marquer les vertices utilisés
    TArray<int32> InvalidIndices;
    bool bHasInvalidIndices = false;
    
    for (int32 i = 0; i < Triangles.Num(); ++i)
    {
        int32 VertexIndex = Triangles[i];
        if (VertexIndex >= 0 && VertexIndex < VertexIsUsed.Num())
        {
            VertexIsUsed[VertexIndex] = true;
        }
        else
        {
            bHasInvalidIndices = true;
            InvalidIndices.Add(i);
            UE_LOG(LogTemp, Warning, TEXT("Invalid vertex index %d (max: %d) in triangle %d"),
                VertexIndex, VertexIsUsed.Num() > 0 ? VertexIsUsed.Num() - 1 : -1, i / 3);
        }
    }
    
    // Si des indices invalides ont été trouvés, les corriger
    if (bHasInvalidIndices)
    {
        // Corriger les indices en partant de la fin pour ne pas perturber les indices précédents
        for (int32 i = InvalidIndices.Num() - 1; i >= 0; --i)
        {
            int32 TriangleIdx = InvalidIndices[i] / 3;
            
            // Si c'est un triangle entier à supprimer
            if (TriangleIdx * 3 + 2 < Triangles.Num() && 
                (InvalidIndices.Contains(TriangleIdx * 3) ||
                 InvalidIndices.Contains(TriangleIdx * 3 + 1) ||
                 InvalidIndices.Contains(TriangleIdx * 3 + 2)))
            {
                // Supprimer le triangle entier en commençant par la fin
                if (TriangleIdx * 3 + 2 < Triangles.Num())
                    Triangles.RemoveAt(TriangleIdx * 3 + 2);
                if (TriangleIdx * 3 + 1 < Triangles.Num())
                    Triangles.RemoveAt(TriangleIdx * 3 + 1);
                if (TriangleIdx * 3 < Triangles.Num())
                    Triangles.RemoveAt(TriangleIdx * 3);
            }
            else
            {
                // Remplacer l'index invalide par 0 (valeur sûre)
                if (InvalidIndices[i] < Triangles.Num())
                {
                    Triangles[InvalidIndices[i]] = 0;
                }
            }
        }
    }
    
    // Compter combien de vertices sont encore utilisés
    int32 VerticesUsed = 0;
    for (bool bUsed : VertexIsUsed)
    {
        if (bUsed) VerticesUsed++;
    }
    
    // Si la majorité des vertices sont inutilisés, on procède au nettoyage
    const float OrphanThreshold = 0.25f; // Si plus de 75% des vertices sont orphelins, reconstruire
    
    if (VerticesUsed < Vertices.Num() * OrphanThreshold)
    {
        UE_LOG(LogTemp, Log, TEXT("Severe vertex orphaning detected: %d/%d used. Rebuilding mesh..."),
            VerticesUsed, Vertices.Num());
        
        // Créer une nouvelle map et de nouveaux tableaux
        TMap<int32, int32> OldToNewIndexMap;
        TArray<FVector> NewVertices;
        TArray<FVector2D> NewUVs;
        TArray<FColor> NewVertexColors;
        TArray<FVector> NewNormals;
        TArray<int32> NewTriangles;
        
        // Pour chaque indice dans le tableau de triangles
        for (int32 i = 0; i < Triangles.Num(); i += 3)
        {
            // Vérifier que nous avons un triangle complet
            if (i + 2 >= Triangles.Num())
                continue;
                
            int32 Index0 = Triangles[i];
            int32 Index1 = Triangles[i + 1];
            int32 Index2 = Triangles[i + 2];
            
            // Vérifier que tous les indices sont valides
            if (!Vertices.IsValidIndex(Index0) || !Vertices.IsValidIndex(Index1) || !Vertices.IsValidIndex(Index2))
                continue;
                
            // Ajouter un nouveau triangle avec des indices mis à jour
            int32 NewIndex0, NewIndex1, NewIndex2;
            
            // Pour chaque sommet du triangle
            if (!OldToNewIndexMap.Contains(Index0))
            {
                NewIndex0 = NewVertices.Num();
                OldToNewIndexMap.Add(Index0, NewIndex0);
                
                NewVertices.Add(Vertices[Index0]);
                
                // S'assurer que les tableaux d'attributs ont assez d'éléments
                if (Index0 < UVs.Num())
                    NewUVs.Add(UVs[Index0]);
                else
                    NewUVs.Add(FVector2D::ZeroVector);
                    
                if (Index0 < VertexColors.Num())
                    NewVertexColors.Add(VertexColors[Index0]);
                else
                    NewVertexColors.Add(FColor::White);
                    
                if (Index0 < Normals.Num())
                    NewNormals.Add(Normals[Index0]);
                else
                    NewNormals.Add(FVector(0.0f, -1.0f, 0.0f));
            }
            else
            {
                NewIndex0 = OldToNewIndexMap[Index0];
            }
            
            // Répéter pour les autres sommets
            if (!OldToNewIndexMap.Contains(Index1))
            {
                NewIndex1 = NewVertices.Num();
                OldToNewIndexMap.Add(Index1, NewIndex1);
                
                NewVertices.Add(Vertices[Index1]);
                
                if (Index1 < UVs.Num())
                    NewUVs.Add(UVs[Index1]);
                else
                    NewUVs.Add(FVector2D::ZeroVector);
                    
                if (Index1 < VertexColors.Num())
                    NewVertexColors.Add(VertexColors[Index1]);
                else
                    NewVertexColors.Add(FColor::White);
                    
                if (Index1 < Normals.Num())
                    NewNormals.Add(Normals[Index1]);
                else
                    NewNormals.Add(FVector(0.0f, -1.0f, 0.0f));
            }
            else
            {
                NewIndex1 = OldToNewIndexMap[Index1];
            }
            
            if (!OldToNewIndexMap.Contains(Index2))
            {
                NewIndex2 = NewVertices.Num();
                OldToNewIndexMap.Add(Index2, NewIndex2);
                
                NewVertices.Add(Vertices[Index2]);
                
                if (Index2 < UVs.Num())
                    NewUVs.Add(UVs[Index2]);
                else
                    NewUVs.Add(FVector2D::ZeroVector);
                    
                if (Index2 < VertexColors.Num())
                    NewVertexColors.Add(VertexColors[Index2]);
                else
                    NewVertexColors.Add(FColor::White);
                    
                if (Index2 < Normals.Num())
                    NewNormals.Add(Normals[Index2]);
                else
                    NewNormals.Add(FVector(0.0f, -1.0f, 0.0f));
            }
            else
            {
                NewIndex2 = OldToNewIndexMap[Index2];
            }
            
            // Ajouter le triangle avec les nouveaux indices
            NewTriangles.Add(NewIndex0);
            NewTriangles.Add(NewIndex1);
            NewTriangles.Add(NewIndex2);
        }
        
        // Mettre à jour les tableaux originaux
        Vertices = NewVertices;
        Triangles = NewTriangles;
        UVs = NewUVs;
        VertexColors = NewVertexColors;
        Normals = NewNormals;
        
        UE_LOG(LogTemp, Log, TEXT("Mesh rebuilt: V=%d, T=%d (from V=%d)"),
            Vertices.Num(), Triangles.Num() / 3, OldToNewIndexMap.Num());
    }
    else
    {
        // Approche classique moins radicale - créer une mapping des anciens indices vers les nouveaux
        TArray<int32> OldToNewIndexMap;
        OldToNewIndexMap.Init(-1, Vertices.Num());
        
        // Compter combien de vertices sont conservés
        int32 NewVertexCount = 0;
        for (int32 i = 0; i < VertexIsUsed.Num(); ++i)
        {
            if (VertexIsUsed[i])
            {
                OldToNewIndexMap[i] = NewVertexCount++;
            }
        }
        
        // Créer de nouveaux tableaux pour contenir seulement les vertices utilisés
        TArray<FVector> NewVertices;
        TArray<FVector2D> NewUVs;
        TArray<FColor> NewVertexColors;
        TArray<FVector> NewNormals;
        
        // Pré-allouer les tableaux pour de meilleures performances
        NewVertices.Reserve(NewVertexCount);
        NewUVs.Reserve(NewVertexCount);
        NewVertexColors.Reserve(NewVertexCount);
        NewNormals.Reserve(NewVertexCount);
        
        // Copier uniquement les données des vertices utilisés
        for (int32 i = 0; i < Vertices.Num(); ++i)
        {
            if (VertexIsUsed[i])
            {
                NewVertices.Add(Vertices[i]);
                
                // S'assurer que les autres tableaux ont des indices valides
                if (i < UVs.Num())
                    NewUVs.Add(UVs[i]);
                else
                    NewUVs.Add(FVector2D::ZeroVector);
                    
                if (i < VertexColors.Num())
                    NewVertexColors.Add(VertexColors[i]);
                else
                    NewVertexColors.Add(FColor::White);
                    
                if (i < Normals.Num())
                    NewNormals.Add(Normals[i]);
                else
                    NewNormals.Add(FVector::UpVector);
            }
        }
        
        // Mettre à jour les indices des triangles
        for (int32 i = 0; i < Triangles.Num(); ++i)
        {
            int32 OldIndex = Triangles[i];
            if (OldIndex >= 0 && OldIndex < OldToNewIndexMap.Num())
            {
                int32 NewIndex = OldToNewIndexMap[OldIndex];
                if (NewIndex >= 0)  // Vérifier que le vertex est utilisé
                {
                    Triangles[i] = NewIndex;
                }
                else
                {
                    // Si nous arrivons ici, c'est qu'il y a une incohérence dans les données
                    UE_LOG(LogTemp, Error, TEXT("Triangle references unused vertex %d"), OldIndex);
                    // Utiliser l'indice 0 comme fallback
                    Triangles[i] = 0;
                }
            }
            else
            {
                // Si nous arrivons ici, c'est qu'il y a une incohérence dans les données
                UE_LOG(LogTemp, Error, TEXT("Invalid triangle index %d (out of bounds)"), OldIndex);
                // Utiliser l'indice 0 comme fallback
                Triangles[i] = 0;
            }
        }
        
        // Remplacer les tableaux originaux par les nouveaux
        Vertices = NewVertices;
        UVs = NewUVs;
        VertexColors = NewVertexColors;
        Normals = NewNormals;
        
        UE_LOG(LogTemp, Log, TEXT("CleanupOrphanVertices: Removed %d unused vertices (from %d to %d)"),
            VertexIsUsed.Num() - NewVertexCount, VertexIsUsed.Num(), NewVertexCount);
    }
}