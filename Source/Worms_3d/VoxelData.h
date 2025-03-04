#pragma once

#include "CoreMinimal.h"
#include "VoxelData.generated.h"

/**
 * Type d'état du voxel
 * - Plein: Voxel solide
 * - Vide: Voxel complètement vide
 * - Partiel: Voxel partiellement détruit (utilisé pour les effets de lissage)
 */
UENUM(BlueprintType)
enum class EVoxelState : uint8
{
    Full = 0,   // Voxel solide
    Empty = 1,  // Voxel vide
    Partial = 2 // Voxel partiellement détruit
};

/**
 * Type de matériau du voxel - utilisé pour les effets visuels
 */
UENUM(BlueprintType)
enum class EVoxelMaterial : uint8
{
    Dirt = 0,    // Terre
    Stone = 1,   // Pierre
    Sand = 2,    // Sable
    Grass = 3,   // Herbe (surface)
    Custom1 = 4, // Matériau personnalisé 1
    Custom2 = 5  // Matériau personnalisé 2
};

/**
 * Données d'un voxel individuel - optimisé pour l'espace mémoire
 */
USTRUCT(BlueprintType)
struct WORMS_3D_API FVoxel
{
    GENERATED_BODY()
    // Add these methods to the FVoxel struct in VoxelData.h
    friend FArchive& operator<<(FArchive& Ar, FVoxel& Voxel)
    {
        uint8 StateValue = static_cast<uint8>(Voxel.State);
        uint8 MaterialValue = static_cast<uint8>(Voxel.Material);
    
        Ar << StateValue;
        Ar << MaterialValue;
        Ar << Voxel.Density;
        Ar << Voxel.Color;
    
        if (Ar.IsLoading())
        {
            Voxel.State = static_cast<EVoxelState>(StateValue);
            Voxel.Material = static_cast<EVoxelMaterial>(MaterialValue);
        }
    
        return Ar;
    }
    // État du voxel (plein, vide, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    EVoxelState State = EVoxelState::Full;
    
    // Matériau du voxel
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    EVoxelMaterial Material = EVoxelMaterial::Dirt;

    // Densité du voxel (utilisée pour l'algorithme Marching Cubes)
    // 1.0 = complètement solide, 0.0 = complètement vide
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    float Density = 1.0f;

    // Couleur du voxel (utilisée pour les variations visuelles)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FColor Color = FColor(120, 80, 40, 255); // Terre par défaut

    // Constructeur par défaut
    FVoxel() = default;

    // Constructeur avec état
    FVoxel(EVoxelState InState, float InDensity = 1.0f)
        : State(InState), Density(InDensity)
    {
        UpdateColorFromMaterial();
    }

    // Constructeur avec état et matériau
    FVoxel(EVoxelState InState, EVoxelMaterial InMaterial, float InDensity = 1.0f)
        : State(InState), Material(InMaterial), Density(InDensity)
    {
        UpdateColorFromMaterial();
    }

    // Mettre à jour la couleur en fonction du matériau
    void UpdateColorFromMaterial()
    {
        switch (Material)
        {
        case EVoxelMaterial::Dirt:
            Color = FColor(120, 80, 40, 255);
            break;
        case EVoxelMaterial::Stone:
            Color = FColor(100, 100, 100, 255);
            break;
        case EVoxelMaterial::Sand:
            Color = FColor(240, 220, 160, 255);
            break;
        case EVoxelMaterial::Grass:
            Color = FColor(80, 150, 60, 255);
            break;
        default:
            Color = FColor(120, 80, 40, 255);
            break;
        }

        // Ajouter une légère variation aléatoire pour plus de réalisme
        int32 Variation = FMath::RandRange(-15, 15);
        Color.R = FMath::Clamp(Color.R + Variation, 0, 255);
        Color.G = FMath::Clamp(Color.G + Variation, 0, 255);
        Color.B = FMath::Clamp(Color.B + Variation, 0, 255);
    }
};

/**
 * Structure pour stocker et manipuler un tableau 3D de voxels de manière efficace
 * Utilise un stockage linéaire pour de meilleures performances
 */
USTRUCT(BlueprintType)
struct WORMS_3D_API FVoxelGrid
{
    GENERATED_BODY()
    // Obtenir un voxel par coordonnées

private:
    // Dimensions du tableau de voxels
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VoxelGrid", meta = (AllowPrivateAccess = "true"))
    FIntVector Size;

    // Données de voxels stockées de manière linéaire pour la performance
    UPROPERTY()
    TArray<FVoxel> Voxels;

    // Taille des voxels en unités du monde
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VoxelGrid", meta = (AllowPrivateAccess = "true"))
    float VoxelSize = 100.0f;

public:
    // Constructeur par défaut
    FVoxelGrid()
        : Size(FIntVector(0, 0, 0))
    {
    }

    // Constructeur avec dimensions
    FVoxelGrid(const FIntVector& InSize, float InVoxelSize = 100.0f)
        : Size(InSize), VoxelSize(InVoxelSize)
    {
        // Initialiser le tableau avec des voxels pleins
        Voxels.SetNum(Size.X * Size.Y * Size.Z);
        for (int32 i = 0; i < Voxels.Num(); ++i)
        {
            Voxels[i] = FVoxel(EVoxelState::Full);
        }
    }

    // Redimensionner le tableau de voxels
    void Resize(const FIntVector& NewSize)
    {
        // Stocker les anciennes dimensions
        FIntVector OldSize = Size;
        
        // Mettre à jour les dimensions
        Size = NewSize;
        
        // Créer un nouveau tableau
        TArray<FVoxel> NewVoxels;
        NewVoxels.SetNum(Size.X * Size.Y * Size.Z);
        
        // Copier les données existantes si possible
        for (int32 z = 0; z < FMath::Min(OldSize.Z, Size.Z); ++z)
        {
            for (int32 y = 0; y < FMath::Min(OldSize.Y, Size.Y); ++y)
            {
                for (int32 x = 0; x < FMath::Min(OldSize.X, Size.X); ++x)
                {
                    const int32 OldIndex = GetIndex(x, y, z, OldSize);
                    const int32 NewIndex = GetIndex(x, y, z, Size);
                    
                    if (Voxels.IsValidIndex(OldIndex) && NewVoxels.IsValidIndex(NewIndex))
                    {
                        NewVoxels[NewIndex] = Voxels[OldIndex];
                    }
                }
            }
        }
        
        // Initialiser le reste des voxels
        for (int32 i = 0; i < NewVoxels.Num(); ++i)
        {
            if (NewVoxels[i].State != EVoxelState::Full && NewVoxels[i].State != EVoxelState::Empty)
            {
                NewVoxels[i] = FVoxel(EVoxelState::Full);
            }
        }
        
        // Remplacer l'ancien tableau
        Voxels = MoveTemp(NewVoxels);
    }

    // Obtenir la taille du tableau de voxels
    const FIntVector& GetSize() const { return Size; }

    // Obtenir la taille d'un voxel
    float GetVoxelSize() const { return VoxelSize; }

    // Définir la taille d'un voxel
    void SetVoxelSize(float NewVoxelSize) { VoxelSize = NewVoxelSize; }

    // Vérifier si des coordonnées sont valides
    bool IsValidCoordinate(int32 X, int32 Y, int32 Z) const
    {
        return X >= 0 && X < Size.X && Y >= 0 && Y < Size.Y && Z >= 0 && Z < Size.Z;
    }

    // Vérifier si des coordonnées sont valides
    bool IsValidCoordinate(const FIntVector& Coordinate) const
    {
        return IsValidCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z);
    }

    // Convertir des coordonnées 3D en indice linéaire
    FORCEINLINE int32 GetIndex(int32 X, int32 Y, int32 Z) const
    {
        return X + Y * Size.X + Z * Size.X * Size.Y;
    }

    // Convertir des coordonnées 3D en indice linéaire avec taille personnalisée
    FORCEINLINE static int32 GetIndex(int32 X, int32 Y, int32 Z, const FIntVector& GridSize)
    {
        return X + Y * GridSize.X + Z * GridSize.X * GridSize.Y;
    }

    // Convertir un indice linéaire en coordonnées 3D
    FORCEINLINE FIntVector GetCoordinate(int32 Index) const
    {
        const int32 Z = Index / (Size.X * Size.Y);
        const int32 Remainder = Index - Z * Size.X * Size.Y;
        const int32 Y = Remainder / Size.X;
        const int32 X = Remainder % Size.X;
        return FIntVector(X, Y, Z);
    }

    // Obtenir un voxel par coordonnées
    FVoxel& GetVoxel(int32 X, int32 Y, int32 Z)
    {
        const int32 Index = GetIndex(X, Y, Z);
        return Voxels[Index];
    }

    // Obtenir un voxel par coordonnées (const)
    const FVoxel& GetVoxel(int32 X, int32 Y, int32 Z) const
    {
        const int32 Index = GetIndex(X, Y, Z);
        return Voxels[Index];
    }

    // Obtenir un voxel par coordonnées vectorielles
    FVoxel& GetVoxel(const FIntVector& Coordinate)
    {
        return GetVoxel(Coordinate.X, Coordinate.Y, Coordinate.Z);
    }

    // Obtenir un voxel par coordonnées vectorielles (const)
    const FVoxel& GetVoxel(const FIntVector& Coordinate) const
    {
        return GetVoxel(Coordinate.X, Coordinate.Y, Coordinate.Z);
    }

    // Définir un voxel par coordonnées
    void SetVoxel(int32 X, int32 Y, int32 Z, const FVoxel& Voxel)
    {
        if (IsValidCoordinate(X, Y, Z))
        {
            const int32 Index = GetIndex(X, Y, Z);
            Voxels[Index] = Voxel;
        }
    }

    // Définir un voxel par coordonnées vectorielles
    void SetVoxel(const FIntVector& Coordinate, const FVoxel& Voxel)
    {
        SetVoxel(Coordinate.X, Coordinate.Y, Coordinate.Z, Voxel);
    }

    // Obtenir les voxels bruts
    TArray<FVoxel>& GetRawVoxels() { return Voxels; }
    
    // Obtenir les voxels bruts (const)
    const TArray<FVoxel>& GetRawVoxels() const { return Voxels; }

    // Convertir des coordonnées du monde en coordonnées de voxel
    FIntVector WorldToVoxel(const FVector& WorldPosition, const FVector& Origin) const
    {
        const FVector RelativePosition = WorldPosition - Origin;
        const int32 X = FMath::FloorToInt(RelativePosition.X / VoxelSize);
        const int32 Y = FMath::FloorToInt(RelativePosition.Y / VoxelSize);
        const int32 Z = FMath::FloorToInt(RelativePosition.Z / VoxelSize);
        return FIntVector(X, Y, Z);
    }

    // Convertir des coordonnées de voxel en coordonnées du monde
    FVector VoxelToWorld(const FIntVector& VoxelPosition, const FVector& Origin) const
    {
        return FVector(
            VoxelPosition.X * VoxelSize + Origin.X + VoxelSize * 0.5f,
            VoxelPosition.Y * VoxelSize + Origin.Y + VoxelSize * 0.5f,
            VoxelPosition.Z * VoxelSize + Origin.Z + VoxelSize * 0.5f
        );
    }

    // Carver une sphère dans le volume de voxels
    void CarveSphere(const FVector& WorldCenter, float Radius, float Falloff = 0.5f, const FVector& Origin = FVector::ZeroVector)
    {
        // Convertir le centre en coordonnées de voxel
        const FIntVector Center = WorldToVoxel(WorldCenter, Origin);
        
        // Rayon en unités de voxel
        const float VoxelRadius = Radius / VoxelSize;
        
        // Calculer les limites de la boîte englobante pour optimiser
        const int32 MinX = FMath::Max(0, Center.X - FMath::CeilToInt(VoxelRadius) - 1);
        const int32 MinY = FMath::Max(0, Center.Y - FMath::CeilToInt(VoxelRadius) - 1);
        const int32 MinZ = FMath::Max(0, Center.Z - FMath::CeilToInt(VoxelRadius) - 1);
        const int32 MaxX = FMath::Min(Size.X - 1, Center.X + FMath::CeilToInt(VoxelRadius) + 1);
        const int32 MaxY = FMath::Min(Size.Y - 1, Center.Y + FMath::CeilToInt(VoxelRadius) + 1);
        const int32 MaxZ = FMath::Min(Size.Z - 1, Center.Z + FMath::CeilToInt(VoxelRadius) + 1);
        
        // Parcourir tous les voxels dans la boîte englobante
        for (int32 z = MinZ; z <= MaxZ; ++z)
        {
            for (int32 y = MinY; y <= MaxY; ++y)
            {
                for (int32 x = MinX; x <= MaxX; ++x)
                {
                    // Calculer la distance au centre
                    const FVector VoxelWorldPos = VoxelToWorld(FIntVector(x, y, z), Origin);
                    const float Distance = FVector::Dist(VoxelWorldPos, WorldCenter);
                    
                    // Si le voxel est dans la sphère ou proche de sa surface
                    if (Distance <= Radius)
                    {
                        // Calculer la densité en fonction de la distance
                        const float NormalizedDist = Distance / Radius;
                        
                        // Plus la distance est grande, plus la densité diminue
                        // Si la distance est <= (1 - Falloff) * Radius, alors le voxel est complètement vide
                        // Sinon, la densité varie de 0 à 1 en fonction de la distance
                        float NewDensity = 0.0f;
                        
                        if (NormalizedDist > (1.0f - Falloff))
                        {
                            // Transition douce
                            NewDensity = FMath::Clamp((NormalizedDist - (1.0f - Falloff)) / Falloff, 0.0f, 1.0f);
                        }
                        
                        // Mettre à jour le voxel
                        FVoxel& Voxel = GetVoxel(x, y, z);
                        
                        // Utiliser la densité minimale entre l'actuelle et la nouvelle
                        Voxel.Density = FMath::Min(Voxel.Density, NewDensity);
                        
                        // Mettre à jour l'état du voxel
                        if (Voxel.Density <= 0.01f)
                        {
                            Voxel.State = EVoxelState::Empty;
                        }
                        else if (Voxel.Density < 0.99f)
                        {
                            Voxel.State = EVoxelState::Partial;
                        }
                    }
                }
            }
        }
    }

    // Effectuer une opération de lissage sur le volume de voxels
    void SmoothVolume(int32 Iterations = 1)
    {
        for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
        {
            // Créer une copie des données actuelles
            TArray<FVoxel> TempVoxels = Voxels;
            
            // Parcourir le volume (exclure les bords)
            for (int32 z = 1; z < Size.Z - 1; ++z)
            {
                for (int32 y = 1; y < Size.Y - 1; ++y)
                {
                    for (int32 x = 1; x < Size.X - 1; ++x)
                    {
                        const int32 Index = GetIndex(x, y, z);
                        
                        // Ne pas lisser les voxels complètement vides ou pleins
                        if (Voxels[Index].State != EVoxelState::Partial)
                        {
                            continue;
                        }
                        
                        // Calculer la densité moyenne des voxels voisins
                        float TotalDensity = 0.0f;
                        int32 Count = 0;
                        
                        // 6 voisins directs
                        const FIntVector Neighbors[6] = {
                            FIntVector(x + 1, y, z),
                            FIntVector(x - 1, y, z),
                            FIntVector(x, y + 1, z),
                            FIntVector(x, y - 1, z),
                            FIntVector(x, y, z + 1),
                            FIntVector(x, y, z - 1)
                        };
                        
                        for (const FIntVector& Neighbor : Neighbors)
                        {
                            if (IsValidCoordinate(Neighbor))
                            {
                                const int32 NeighborIndex = GetIndex(Neighbor.X, Neighbor.Y, Neighbor.Z);
                                TotalDensity += Voxels[NeighborIndex].Density;
                                Count++;
                            }
                        }
                        
                        // Calculer la nouvelle densité
                        if (Count > 0)
                        {
                            const float AverageDensity = TotalDensity / Count;
                            TempVoxels[Index].Density = FMath::Lerp(Voxels[Index].Density, AverageDensity, 0.5f);
                            
                            // Mettre à jour l'état en fonction de la densité
                            if (TempVoxels[Index].Density <= 0.01f)
                            {
                                TempVoxels[Index].State = EVoxelState::Empty;
                            }
                            else if (TempVoxels[Index].Density >= 0.99f)
                            {
                                TempVoxels[Index].State = EVoxelState::Full;
                            }
                            else
                            {
                                TempVoxels[Index].State = EVoxelState::Partial;
                            }
                        }
                    }
                }
            }
            
            // Mettre à jour le tableau de voxels
            Voxels = MoveTemp(TempVoxels);
        }
    }

    // Générer un terrain aléatoire
    void GenerateRandomTerrain(float HeightScale = 1000.0f, float NoiseScale = 0.01f, int32 SeedOffset = 0)
    {
        // Génération de terrain simple avec bruit de Perlin
        for (int32 x = 0; x < Size.X; ++x)
        {
            for (int32 y = 0; y < Size.Y; ++y)
            {
                // Calculer la hauteur à cette position
                float NoiseValue = FMath::PerlinNoise2D(FVector2D(x + SeedOffset, y + SeedOffset) * NoiseScale);
                int32 Height = FMath::FloorToInt((NoiseValue * 0.5f + 0.5f) * HeightScale / VoxelSize);
                Height = FMath::Clamp(Height, 0, Size.Z - 1);
                
                // Remplir jusqu'à la hauteur calculée
                for (int32 z = 0; z < Height; ++z)
                {
                    FVoxel& Voxel = GetVoxel(x, y, z);
                    Voxel.State = EVoxelState::Full;
                    Voxel.Density = 1.0f;
                    
                    // Définir le matériau en fonction de la profondeur
                    if (z == Height - 1)
                    {
                        Voxel.Material = EVoxelMaterial::Grass;
                    }
                    else if (z > Height - 4)
                    {
                        Voxel.Material = EVoxelMaterial::Dirt;
                    }
                    else
                    {
                        Voxel.Material = EVoxelMaterial::Stone;
                    }
                    
                    Voxel.UpdateColorFromMaterial();
                }
                
                // Vider au-delà de la hauteur calculée
                for (int32 z = Height; z < Size.Z; ++z)
                {
                    FVoxel& Voxel = GetVoxel(x, y, z);
                    Voxel.State = EVoxelState::Empty;
                    Voxel.Density = 0.0f;
                }
            }
        }
    }

    // Générer un volume plein avec surface d'herbe
    void GenerateTerrainWithSurface(int32 SurfaceHeight)
    {
        // Limiter SurfaceHeight aux limites du volume
        SurfaceHeight = FMath::Clamp(SurfaceHeight, 0, Size.Z - 1);
        
        // Remplir tous les voxels jusqu'à SurfaceHeight
        for (int32 z = 0; z < Size.Z; ++z)
        {
            for (int32 y = 0; y < Size.Y; ++y)
            {
                for (int32 x = 0; x < Size.X; ++x)
                {
                    FVoxel& Voxel = GetVoxel(x, y, z);
                    
                    // Vider au-dessus de la surface
                    if (z > SurfaceHeight)
                    {
                        Voxel.State = EVoxelState::Empty;
                        Voxel.Density = 0.0f;
                    }
                    // Remplir sous la surface
                    else
                    {
                        Voxel.State = EVoxelState::Full;
                        Voxel.Density = 1.0f;
                        
                        // Définir le matériau en fonction de la profondeur
                        if (z == SurfaceHeight)
                        {
                            Voxel.Material = EVoxelMaterial::Grass;
                        }
                        else if (z > SurfaceHeight - 3)
                        {
                            Voxel.Material = EVoxelMaterial::Dirt;
                        }
                        else
                        {
                            Voxel.Material = EVoxelMaterial::Stone;
                        }
                        
                        Voxel.UpdateColorFromMaterial();
                    }
                }
            }
        }
    }

    // Obtenir la densité à une position interpolée
    float GetInterpolatedDensity(float X, float Y, float Z) const
    {
        const int32 X0 = FMath::FloorToInt(X);
        const int32 Y0 = FMath::FloorToInt(Y);
        const int32 Z0 = FMath::FloorToInt(Z);
        const int32 X1 = X0 + 1;
        const int32 Y1 = Y0 + 1;
        const int32 Z1 = Z0 + 1;
        
        // Facteurs d'interpolation
        const float Fx = X - X0;
        const float Fy = Y - Y0;
        const float Fz = Z - Z0;
        
        // Valeurs aux coins du cube
        const float D000 = IsValidCoordinate(X0, Y0, Z0) ? GetVoxel(X0, Y0, Z0).Density : 1.0f;
        const float D001 = IsValidCoordinate(X0, Y0, Z1) ? GetVoxel(X0, Y0, Z1).Density : 1.0f;
        const float D010 = IsValidCoordinate(X0, Y1, Z0) ? GetVoxel(X0, Y1, Z0).Density : 1.0f;
        const float D011 = IsValidCoordinate(X0, Y1, Z1) ? GetVoxel(X0, Y1, Z1).Density : 1.0f;
        const float D100 = IsValidCoordinate(X1, Y0, Z0) ? GetVoxel(X1, Y0, Z0).Density : 1.0f;
        const float D101 = IsValidCoordinate(X1, Y0, Z1) ? GetVoxel(X1, Y0, Z1).Density : 1.0f;
        const float D110 = IsValidCoordinate(X1, Y1, Z0) ? GetVoxel(X1, Y1, Z0).Density : 1.0f;
        const float D111 = IsValidCoordinate(X1, Y1, Z1) ? GetVoxel(X1, Y1, Z1).Density : 1.0f;
        
        // Interpolation trilinéaire
        const float D00 = FMath::Lerp(D000, D001, Fz);
        const float D01 = FMath::Lerp(D010, D011, Fz);
        const float D10 = FMath::Lerp(D100, D101, Fz);
        const float D11 = FMath::Lerp(D110, D111, Fz);
        
        const float D0 = FMath::Lerp(D00, D01, Fy);
        const float D1 = FMath::Lerp(D10, D11, Fy);
        
        return FMath::Lerp(D0, D1, Fx);
    }

    // Calculer la normale à une position
    FVector CalculateNormal(float X, float Y, float Z, float Delta = 0.5f) const
    {
        // Échantillonner la densité aux points voisins
        const float DX1 = GetInterpolatedDensity(X + Delta, Y, Z);
        const float DX2 = GetInterpolatedDensity(X - Delta, Y, Z);
        const float DY1 = GetInterpolatedDensity(X, Y + Delta, Z);
        const float DY2 = GetInterpolatedDensity(X, Y - Delta, Z);
        const float DZ1 = GetInterpolatedDensity(X, Y, Z + Delta);
        const float DZ2 = GetInterpolatedDensity(X, Y, Z - Delta);
        
        // Calculer la normale en utilisant les différences de densité
        FVector Normal(DX2 - DX1, DY2 - DY1, DZ2 - DZ1);
        return Normal.GetSafeNormal();
    }
};