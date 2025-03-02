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
    if (!bGenerateInternalStructure || InternalLayerCount <= 0)
    {
        return;
    }

    // Vider les tableaux existants
    InternalVertices.Empty();
    InternalTriangles.Empty();
    InternalUVs.Empty();
    InternalNormals.Empty();
    InternalVertexColors.Empty();

    // Calculer le pas entre chaque point comme dans la génération de terrain standard
    float HStep = TerrainWidth / (HorizontalResolution - 1);
    float VStep = TerrainHeight / (VerticalResolution - 1);

    // Calculer l'épaisseur de chaque couche interne
    float TotalDepth = TerrainDepth - (2 * InternalLayerThickness); // Soustraire l'épaisseur des parois avant/arrière
    float LayerDepth = TotalDepth / InternalLayerCount;

    // Nous allons créer des couches internes parallèles à la face avant/arrière
    for (int32 layerIndex = 0; layerIndex < InternalLayerCount; ++layerIndex)
    {
        // Position Y de cette couche interne à partir de la face avant
        float LayerPosition = InternalLayerThickness + (layerIndex * LayerDepth);
        
        // Index de départ pour les vertices de cette couche
        int32 LayerVertexStartIndex = InternalVertices.Num();
        
        // Générer un plan interne similaire à la face avant/arrière
        for (int32 y = 0; y < VerticalResolution; ++y)
        {
            for (int32 x = 0; x < HorizontalResolution; ++x)
            {
                // Calculer la position de ce vertex
                float PosX = x * HStep;
                float PosZ = y * VStep;
                
                // Ajouter le vertex avec une composante Y correspondant à la profondeur de la couche
                InternalVertices.Add(FVector(PosX, LayerPosition, PosZ));
                
                // Ajouter les UV correspondants (normalisés de 0 à 1)
                InternalUVs.Add(FVector2D(
                    static_cast<float>(x) / (HorizontalResolution - 1), 
                    static_cast<float>(y) / (VerticalResolution - 1)
                ));
                
                // Sélectionner la couleur de cette couche interne
                FLinearColor LayerColor;
                if (InternalLayerColors.IsValidIndex(layerIndex))
                {
                    LayerColor = InternalLayerColors[layerIndex];
                }
                else if (InternalLayerColors.Num() > 0)
                {
                    // Fallback à la première couleur si l'index n'est pas valide
                    LayerColor = InternalLayerColors[0];
                }
                else
                {
                    // Fallback à une couleur marron si aucune couleur n'est définie
                    LayerColor = FLinearColor(0.5f, 0.25f, 0.0f, 1.0f);
                }
                
                // Ajouter une variation aléatoire subtile à la couleur
                float ColorVariation = FMath::RandRange(-0.1f, 0.1f);
                LayerColor.R = FMath::Clamp(LayerColor.R + ColorVariation, 0.0f, 1.0f);
                LayerColor.G = FMath::Clamp(LayerColor.G + ColorVariation, 0.0f, 1.0f);
                LayerColor.B = FMath::Clamp(LayerColor.B + ColorVariation, 0.0f, 1.0f);
                
                // Convertir en FColor
                InternalVertexColors.Add(LayerColor.ToFColor(true));
            }
        }
        
        // Créer les triangles pour cette couche interne
        for (int32 y = 0; y < VerticalResolution - 1; ++y)
        {
            for (int32 x = 0; x < HorizontalResolution - 1; ++x)
            {
                int32 Current = LayerVertexStartIndex + y * HorizontalResolution + x;
                int32 Next = Current + 1;
                int32 Bottom = Current + HorizontalResolution;
                int32 BottomNext = Bottom + 1;
                
                // Premier triangle (avec orientation correcte)
                InternalTriangles.Add(Current);
                InternalTriangles.Add(Next);
                InternalTriangles.Add(Bottom);
                
                // Second triangle (avec orientation correcte)
                InternalTriangles.Add(Next);
                InternalTriangles.Add(BottomNext);
                InternalTriangles.Add(Bottom);
            }
        }
    }
    
    // Initialiser les normales pour toutes les couches
    InternalNormals.Init(FVector::ZeroVector, InternalVertices.Num());
    
    // Calculer les normales pour chaque triangle et les ajouter aux normales des vertices
    for (int32 i = 0; i < InternalTriangles.Num(); i += 3)
    {
        // Obtenir les indices des vertices du triangle
        int32 Index0 = InternalTriangles[i];
        int32 Index1 = InternalTriangles[i + 1];
        int32 Index2 = InternalTriangles[i + 2];
        
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
    
    // Normaliser toutes les normales
    for (int32 i = 0; i < InternalNormals.Num(); i++)
    {
        InternalNormals[i] = InternalNormals[i].GetSafeNormal();
    }
    
    UE_LOG(LogTemp, Log, TEXT("Generated internal structure with %d vertices and %d triangles"), 
        InternalVertices.Num(), InternalTriangles.Num() / 3);
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
    
    // S'assurer que toutes les propriétés sont correctement définies
    TArray<FVector> EmptyVertices;
    TArray<int32> EmptyTriangles;
    
    // Créer la section de mesh
    // IMPORTANT: Nous utilisons ClearMeshSection pour éviter tout problème
    TerrainMesh->ClearMeshSection(0);
    
    // Vérification supplémentaire pour éviter des crashs
    if (InMeshData.Vertices.Num() > 0 && InMeshData.Triangles.Num() > 0 && 
        InMeshData.Normals.Num() == InMeshData.Vertices.Num() && 
        InMeshData.UVs.Num() == InMeshData.Vertices.Num() &&
        LinearColors.Num() == InMeshData.Vertices.Num())
    {
        // Créer la section avec les données fournies
        TerrainMesh->CreateMeshSection_LinearColor(
            0, 
            InMeshData.Vertices, 
            InMeshData.Triangles, 
            InMeshData.Normals, 
            InMeshData.UVs, 
            LinearColors, 
            Tangents, 
            true  // Génère une collision
        );
        
        // Activer les collisions
        TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid mesh data dimensions"));
        return;
    }
    
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
    // Obtenir la position locale du vertex par rapport à l'acteur
    FVector LocalVertex = Vertex;
    
    // Log pour déboguer
    //UE_LOG(LogTemp, Log, TEXT("Checking vertex at (%f, %f, %f) against modification at (%f, %f) with size (%f, %f)"),LocalVertex.X, LocalVertex.Y, LocalVertex.Z,Modification.Position.X, Modification.Position.Y,Modification.Size.X, Modification.Size.Y);
    
    // Considérer que le vertex est en 2D (X, Z)
    float VertexX = LocalVertex.X;
    float VertexZ = LocalVertex.Z;
    
    // Vérifier si le vertex est dans le rectangle de modification
    bool isInside = (VertexX >= Modification.Position.X && 
            VertexX <= Modification.Position.X + Modification.Size.X &&
            VertexZ >= Modification.Position.Y && 
            VertexZ <= Modification.Position.Y + Modification.Size.Y);
    
    if (isInside)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Vertex is inside modification area!"));
    }
    
    return isInside;
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
    
    // Liste des triangles à conserver (non affectés par les modifications)
    TArray<bool> TriangleToKeep;
    TriangleToKeep.Init(true, MeshData.Triangles.Num() / 3);
    
// 1. Première passe: identifier les triangles touchés par les modifications
    for (int32 TriIdx = 0; TriIdx < MeshData.Triangles.Num() / 3; TriIdx++)
    {
        int32 Index1 = MeshData.Triangles[TriIdx * 3];
        int32 Index2 = MeshData.Triangles[TriIdx * 3 + 1];
        int32 Index3 = MeshData.Triangles[TriIdx * 3 + 2];
        
        // Vérifier si le triangle est affecté par l'une des nouvelles modifications
        for (const FTerrainModification& Mod : NewModifications)
        {
            if (IsVertexInModification(MeshData.Vertices[Index1], Mod) ||
                IsVertexInModification(MeshData.Vertices[Index2], Mod) ||
                IsVertexInModification(MeshData.Vertices[Index3], Mod))
            {
                TriangleToKeep[TriIdx] = false;
                break;
            }
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
    
    // 3. Mettre à jour les données de mesh
    MeshData.Triangles = NewTriangles;
    
    // 4. Recalculer les normales
    MeshData.Normals.Init(FVector::ZeroVector, NewVertices.Num());
    
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
    
    // 5. Appliquer les données au mesh
    CreateMeshFromData(MeshData);
    
    // 6. Ajouter les nouvelles modifications à la liste des modifications appliquées
    for (const FTerrainModification& Mod : NewModifications)
    {
        AppliedModifications.Add(Mod);
    }
    
    // Marquer que les modifications ont été appliquées
    bModificationsApplied = true;
    
    // Si nous sommes sur le serveur, répliquer ces données vers tous les clients
    if (HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("Replicating updated mesh to clients"));
        Multicast_UpdateTerrainMesh(MeshData);
    }
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
