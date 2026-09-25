#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_TerrainMeshTile.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTN_TerrainMeshAsset;

/**
 * Trozo del mapa volumétrico fijo, colocado a mano en el nivel (no lo genera el
 * GridMapGenerator). Pinta la malla del asset con colisión y siembra sus algas. El mapa es
 * el mismo en todas las máquinas porque viene del nivel: no se replica nada.
 */
UCLASS()
class TORTUNABO_API ATN_TerrainMeshTile : public AActor
{
	GENERATED_BODY()

public:
	ATN_TerrainMeshTile();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Reconstruye malla, colisión y algas desde MeshAsset. */
	void BuildTile();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UTN_TerrainMeshAsset> MeshAsset;

	/** Material del terreno: color de vértice por grano triplanar (M_GridTerrain). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** Material de las algas: color de PerInstanceCustomData (3 floats), p. ej. M_GridJunk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> FoliageMaterial;

	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** Algas, indexadas por TNTerrainBiome::EFoliageShape. Sin colisión. */
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Foliage;

private:
	void BuildFoliage();

	/** Asset con el que se construyó la malla actual, para no reconstruir en balde. */
	TWeakObjectPtr<const UTN_TerrainMeshAsset> BuiltFromAsset;
};
