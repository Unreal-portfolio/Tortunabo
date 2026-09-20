#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/TN_GridTerrainTypes.h"
#include "TN_GridTerrainTile.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
namespace TNGridTerrain { struct FTerrainContext; }

/**
 * ATN_GridTerrainTile
 *
 * Trozo de terreno de una celda del mapa en grid. La forma la decide
 * TN_GridTerrainDecisions.h (lógica pura y testeada); este actor solo convierte esa
 * malla en geometría con colisión.
 *
 * Red: lo spawnea el servidor (ATN_GridMapGenerator) y se replica como los chunks de
 * ATN_ChunkManager — sin movimiento y siempre relevante. Por la red viaja únicamente
 * FTNGridTileInit, una sola vez; cada máquina construye su malla a partir de él. Como
 * la construcción es determinista, servidor y clientes pisan exactamente el mismo suelo.
 *
 * La basura (TN_GridJunkDecisions.h) va en componentes de instancias: uno por forma
 * básica y por clase de colisión. Son subobjetos por defecto —con nombre estable— para
 * que sigan siendo direccionables por red cuando un personaje se sube a un objeto.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_GridTerrainTile : public AActor
{
	GENERATED_BODY()

public:
	ATN_GridTerrainTile();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Fija los datos del tile. Solo servidor, antes de FinishSpawning. */
	void InitializeTile(const FTNGridTileInit& InInit);

	/** Construye la malla. Idempotente. El generador la llama a mano en mundo de editor,
	 *  donde los actores spawneados no ejecutan BeginPlay. */
	void BuildTerrain();

	const FTNGridTerrainSettings& GetSettings() const { return Settings; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** Parámetros del terreno. Deben ser idénticos en todas las máquinas: viven en los
	 *  Class Defaults y no se replican. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Terrain")
	FTNGridTerrainSettings Settings;

	/** Material del terreno. Debe usar el color de vértice como BaseColor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** Material de la basura. Debe leer el color de PerInstanceCustomData (3 floats). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> JunkMaterial;

	/** Instancias con colisión, indexadas por TNGridJunk::EJunkShape. */
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> SolidJunk;

	/** Instancias de decorado, sin colisión, indexadas por TNGridJunk::EJunkShape. */
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> DecorJunk;

private:
	void BuildJunk(const TNGridTerrain::FTerrainContext& Context);

	UFUNCTION()
	void OnRep_Init();

	UPROPERTY(ReplicatedUsing = OnRep_Init)
	FTNGridTileInit Init;

	bool bTerrainBuilt = false;
};
