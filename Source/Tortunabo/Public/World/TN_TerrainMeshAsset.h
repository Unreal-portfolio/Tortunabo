#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TN_TerrainMeshAsset.generated.h"

/** Alga del bosque: forma (TNTerrainBiome::EFoliageShape), transformada local y color. */
USTRUCT(BlueprintType)
struct FTNTerrainMeshFoliage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage")
	uint8 Shape = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage")
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage")
	FLinearColor Color = FLinearColor::Green;
};

/**
 * Trozo del mapa volumétrico fijo: la malla ya hecha (marching cubes, fuera del editor con
 * Scripts/gen_terrain_volume.py) y sus algas. Lo pinta ATN_TerrainMeshTile. Túneles,
 * voladizos y arcos forman parte de la propia malla.
 */
UCLASS(BlueprintType)
class TORTUNABO_API UTN_TerrainMeshAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Posiciones en uu, locales al centro del trozo. */
	UPROPERTY()
	TArray<FVector3f> Vertices;

	UPROPERTY()
	TArray<FVector3f> Normals;

	/** Color LINEAL * 255 por vértice (no sRGB): ver TNTerrainMesh::ToTileMesh. */
	UPROPERTY()
	TArray<FColor> Colors;

	/** Índices de triángulo, con la cara visible según Unreal. */
	UPROPERTY()
	TArray<int32> Triangles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	TArray<FTNTerrainMeshFoliage> Foliage;

	/** Carga un trozo TNTM1 (Scripts/terrain_vol/export.py). Solo lo usa el importador. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	bool LoadFromFile(const FString& Path);

	bool IsValidMesh() const
	{
		return Vertices.Num() > 0 && Normals.Num() == Vertices.Num() && Colors.Num() == Vertices.Num()
			&& Triangles.Num() > 0 && Triangles.Num() % 3 == 0;
	}
};
