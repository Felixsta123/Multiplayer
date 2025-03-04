#pragma once

#include "CoreMinimal.h"
#include "MarchingCubes.generated.h"

/**
 * Utilitaire pour l'algorithme Marching Cubes utilisé pour générer le maillage du terrain
 */
UCLASS()
class WORMS_3D_API UMarchingCubesUtils : public UObject
{
    GENERATED_BODY()

public:
    // Tables de recherche pour l'algorithme Marching Cubes
    // Ces tables sont des données précalculées pour optimiser la génération de maillage
    
    // Table des triangles
    static const int32 TriTable[256][16];

    // Table des bords
    static const int32 EdgeTable[256];

    // Positions des sommets d'un cube (relatif à l'origine en bas à gauche avant)
    static const FVector CubeVertices[8];

    // Arêtes du cube (paires d'indices dans CubeVertices)
    static const int32 CubeEdges[12][2];

    /**
     * Calcule l'indice du cas Marching Cubes à partir des densités aux sommets du cube
     * @param Densities Tableau de 8 densités aux sommets du cube
     * @return Indice Marching Cubes
     */
    static int32 CalculateCubeIndex(const float Densities[8])
    {
        int32 CubeIndex = 0;
        for (int32 i = 0; i < 8; i++)
        {
            if (Densities[i] < 0.5f)
            {
                CubeIndex |= 1 << i;
            }
        }
        return CubeIndex;
    }

    /**
     * Calcule la position d'intersection entre l'isosurface et une arête du cube
     * @param P1 Premier sommet de l'arête
     * @param P2 Second sommet de l'arête
     * @param V1 Densité au premier sommet
     * @param V2 Densité au second sommet
     * @param IsoLevel Valeur d'isosurface (généralement 0.5)
     * @return Position d'intersection
     */
    static FVector VertexInterp(const FVector& P1, const FVector& P2, float V1, float V2, float IsoLevel = 0.5f)
    {
        if (FMath::Abs(IsoLevel - V1) < 0.00001f)
            return P1;
        if (FMath::Abs(IsoLevel - V2) < 0.00001f)
            return P2;
        if (FMath::Abs(V1 - V2) < 0.00001f)
            return P1;

        float Mu = (IsoLevel - V1) / (V2 - V1);
        return P1 + Mu * (P2 - P1);
    }

    /**
     * Calcule la normale à l'isosurface à partir des gradients de densité
     * @param Gradients Tableau de 8 gradients aux sommets du cube
     * @param Point Position à laquelle calculer la normale
     * @param P1 Premier sommet du cube
     * @return Normale interpolée au point
     */
    static FVector InterpolateNormal(const FVector Gradients[8], const FVector& Point, const FVector& P1)
    {
        // Déterminer les coordonnées relatives dans le cube [0..1]
        FVector RelPos = (Point - P1);
        
        float X = RelPos.X;
        float Y = RelPos.Y;
        float Z = RelPos.Z;
        
        // Coefficients d'interpolation trilinéaire
        float C000 = (1-X) * (1-Y) * (1-Z);
        float C001 = (1-X) * (1-Y) * Z;
        float C010 = (1-X) * Y * (1-Z);
        float C011 = (1-X) * Y * Z;
        float C100 = X * (1-Y) * (1-Z);
        float C101 = X * (1-Y) * Z;
        float C110 = X * Y * (1-Z);
        float C111 = X * Y * Z;
        
        // Interpolation trilinéaire des gradients
        FVector InterpolatedGradient = 
            C000 * Gradients[0] + C001 * Gradients[1] +
            C010 * Gradients[2] + C011 * Gradients[3] +
            C100 * Gradients[4] + C101 * Gradients[5] +
            C110 * Gradients[6] + C111 * Gradients[7];
        
        // La normale est l'opposé du gradient normalisé
        return -InterpolatedGradient.GetSafeNormal();
    }

    /**
     * Calcule une couleur interpolée à partir des couleurs aux sommets du cube
     * @param Colors Tableau de 8 couleurs aux sommets du cube
     * @param Point Position à laquelle calculer la couleur
     * @param P1 Premier sommet du cube
     * @return Couleur interpolée au point
     */
    static FColor InterpolateColor(const FColor Colors[8], const FVector& Point, const FVector& P1)
    {
        // Déterminer les coordonnées relatives dans le cube [0..1]
        FVector RelPos = (Point - P1);
        
        float X = RelPos.X;
        float Y = RelPos.Y;
        float Z = RelPos.Z;
        
        // Coefficients d'interpolation trilinéaire
        float C000 = (1-X) * (1-Y) * (1-Z);
        float C001 = (1-X) * (1-Y) * Z;
        float C010 = (1-X) * Y * (1-Z);
        float C011 = (1-X) * Y * Z;
        float C100 = X * (1-Y) * (1-Z);
        float C101 = X * (1-Y) * Z;
        float C110 = X * Y * (1-Z);
        float C111 = X * Y * Z;
        
        // Interpolation trilinéaire des composantes de couleur
        float R = 
            C000 * Colors[0].R + C001 * Colors[1].R +
            C010 * Colors[2].R + C011 * Colors[3].R +
            C100 * Colors[4].R + C101 * Colors[5].R +
            C110 * Colors[6].R + C111 * Colors[7].R;
            
        float G = 
            C000 * Colors[0].G + C001 * Colors[1].G +
            C010 * Colors[2].G + C011 * Colors[3].G +
            C100 * Colors[4].G + C101 * Colors[5].G +
            C110 * Colors[6].G + C111 * Colors[7].G;
            
        float B = 
            C000 * Colors[0].B + C001 * Colors[1].B +
            C010 * Colors[2].B + C011 * Colors[3].B +
            C100 * Colors[4].B + C101 * Colors[5].B +
            C110 * Colors[6].B + C111 * Colors[7].B;
            
        float A = 
            C000 * Colors[0].A + C001 * Colors[1].A +
            C010 * Colors[2].A + C011 * Colors[3].A +
            C100 * Colors[4].A + C101 * Colors[5].A +
            C110 * Colors[6].A + C111 * Colors[7].A;
        
        return FColor(
            FMath::Clamp<int32>(FMath::RoundToInt(R), 0, 255),
            FMath::Clamp<int32>(FMath::RoundToInt(G), 0, 255),
            FMath::Clamp<int32>(FMath::RoundToInt(B), 0, 255),
            FMath::Clamp<int32>(FMath::RoundToInt(A), 0, 255)
        );
    }

    /**
     * Génère un maillage d'isosurface à partir d'un champ de densité
     * @param Vertices Sortie: Tableau de vertices
     * @param Triangles Sortie: Tableau d'indices de triangles
     * @param Normals Sortie: Tableau de normales
     * @param VertexColors Sortie: Tableau de couleurs de vertex
     * @param GetDensity Fonction pour obtenir la densité à une position
     * @param GetGradient Fonction pour obtenir le gradient à une position
     * @param GetColor Fonction pour obtenir la couleur à une position
     * @param Start Position de départ (coin min)
     * @param End Position de fin (coin max)
     * @param IsoLevel Valeur d'isosurface
     */
    template<typename F1, typename F2, typename F3>
    static void GenerateIsosurfaceMesh(
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector>& Normals,
        TArray<FColor>& VertexColors,
        F1 GetDensity,
        F2 GetGradient,
        F3 GetColor,
        const FIntVector& Start,
        const FIntVector& End,
        float IsoLevel = 0.5f)
    {
        // Parcourir le volume voxel par voxel
        for (int32 z = Start.Z; z < End.Z; z++)
        {
            for (int32 y = Start.Y; y < End.Y; y++)
            {
                for (int32 x = Start.X; x < End.X; x++)
                {
                    // Position du coin du cube (bas, gauche, avant)
                    FVector CubePos(x, y, z);

                    // Densités aux sommets du cube
                    float Densities[8];
                    FVector Gradients[8];
                    FColor Colors[8];

                    // Récupérer les valeurs pour chaque sommet du cube
                    for (int32 i = 0; i < 8; i++)
                    {
                        FVector VertexPos = CubePos + CubeVertices[i];
                        Densities[i] = GetDensity(VertexPos.X, VertexPos.Y, VertexPos.Z);
                        Gradients[i] = GetGradient(VertexPos.X, VertexPos.Y, VertexPos.Z);
                        Colors[i] = GetColor(VertexPos.X, VertexPos.Y, VertexPos.Z);
                    }

                    // Déterminer le cas Marching Cubes
                    int32 CubeIndex = CalculateCubeIndex(Densities);

                    // Ignorer les cas où le cube est entièrement à l'intérieur ou à l'extérieur
                    if (CubeIndex == 0 || CubeIndex == 255)
                        continue;

                    // Positions des intersections avec l'isosurface sur les arêtes du cube
                    FVector VertexList[12];
                    
                    // Calculer les positions des intersections
                    for (int32 i = 0; i < 12; i++)
                    {
                        if (EdgeTable[CubeIndex] & (1 << i))
                        {
                            int32 Edge0 = CubeEdges[i][0];
                            int32 Edge1 = CubeEdges[i][1];
                            
                            FVector Pos0 = CubePos + CubeVertices[Edge0];
                            FVector Pos1 = CubePos + CubeVertices[Edge1];
                            
                            float Density0 = Densities[Edge0];
                            float Density1 = Densities[Edge1];
                            
                            VertexList[i] = VertexInterp(Pos0, Pos1, Density0, Density1, IsoLevel);
                        }
                    }

                    // Créer les triangles pour ce cube
                    for (int32 i = 0; TriTable[CubeIndex][i] != -1; i += 3)
                    {
                        // Indices des intersections formant ce triangle
                        int32 Edge0 = TriTable[CubeIndex][i];
                        int32 Edge1 = TriTable[CubeIndex][i + 1];
                        int32 Edge2 = TriTable[CubeIndex][i + 2];
                        
                        // Positions des vertices du triangle
                        FVector Pos0 = VertexList[Edge0];
                        FVector Pos1 = VertexList[Edge1];
                        FVector Pos2 = VertexList[Edge2];
                        
                        // Normales aux vertices du triangle
                        FVector Normal0 = InterpolateNormal(Gradients, Pos0, CubePos);
                        FVector Normal1 = InterpolateNormal(Gradients, Pos1, CubePos);
                        FVector Normal2 = InterpolateNormal(Gradients, Pos2, CubePos);
                        
                        // Couleurs des vertices du triangle
                        FColor Color0 = InterpolateColor(Colors, Pos0, CubePos);
                        FColor Color1 = InterpolateColor(Colors, Pos1, CubePos);
                        FColor Color2 = InterpolateColor(Colors, Pos2, CubePos);
                        
                        // Ajouter les vertices au maillage
                        int32 BaseIndex = Vertices.Num();
                        
                        Vertices.Add(Pos0);
                        Vertices.Add(Pos1);
                        Vertices.Add(Pos2);
                        
                        Normals.Add(Normal0);
                        Normals.Add(Normal1);
                        Normals.Add(Normal2);
                        
                        VertexColors.Add(Color0);
                        VertexColors.Add(Color1);
                        VertexColors.Add(Color2);
                        
                        // Ajouter les indices de triangle
                        Triangles.Add(BaseIndex);
                        Triangles.Add(BaseIndex + 1);
                        Triangles.Add(BaseIndex + 2);
                    }
                }
            }
        }
    }
};