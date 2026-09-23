#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_TerrainModuleTile.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTN_TerrainModuleAsset;
enum class ETNTerrainModuleTopology : uint8;
namespace TNTerrainModule { struct FModuleColors; struct FModuleField; }
namespace TNGridTerrain { struct FTileMesh; }

/** Lados del módulo como bits, para marcar bocas bloqueadas (mismo orden que TNGridLogic). */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETNTerrainModuleSide : uint8
{
	None  = 0 UMETA(Hidden),
	North = 1 << 0,
	East  = 1 << 1,
	South = 1 << 2,
	West  = 1 << 3
};
ENUM_CLASS_FLAGS(ETNTerrainModuleSide);

/** Módulo vecino con el que el tile funde sus bordes (TNTerrainSeam). Lo fija el generador
 *  y viaja replicado una vez: cada máquina funde con los mismos datos. */
USTRUCT()
struct FTNSeamNeighbor
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTN_TerrainModuleAsset> Asset;

	/** Posición de la celda vecina en celdas, en el espacio local del tile (+X Norte, +Y Este). */
	UPROPERTY()
	int8 LocalX = 0;

	UPROPERTY()
	int8 LocalY = 0;

	/** Cuartos de vuelta del vecino respecto al tile. */
	UPROPERTY()
	int8 RelYawSteps = 0;

	UPROPERTY()
	bool bMirrored = false;

	/** Lados exteriores del vecino (su espacio local colocado) y su semilla: su costa. */
	UPROPERTY()
	uint8 OuterSides = 0;

	UPROPERTY()
	int32 Seed = 0;

	bool operator==(const FTNSeamNeighbor& Other) const
	{
		return Asset == Other.Asset && LocalX == Other.LocalX && LocalY == Other.LocalY && RelYawSteps == Other.RelYawSteps
			&& bMirrored == Other.bMirrored && OuterSides == Other.OuterSides && Seed == Other.Seed;
	}
};

/**
 * ATN_TerrainModuleTile
 *
 * Un módulo de terreno colocado en el mundo: convierte el heightfield de ModuleAsset en
 * geometría con colisión. Es la unidad con la que trabajan los diseñadores: cada
 * BP_Mod_* de /Game/Terrain/Modules es un hijo de esta clase con su asset asignado, y
 * se puede colocar a mano (snap a ModuleSize) o dejar que ATN_GridMapGenerator lo elija
 * por topología.
 *
 * Para bloquear una salida no se toca el heightfield: se añade al Blueprint un
 * componente (muro, montón de basura) sobre la boca del pasillo.
 *
 * Red: el servidor lo spawnea y se replica como los chunks (sin movimiento, siempre
 * relevante). No viaja ningún dato: el asset está en los Class Defaults, y cada máquina
 * construye la misma malla a partir de él.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_TerrainModuleTile : public AActor
{
	GENERATED_BODY()

public:
	ATN_TerrainModuleTile();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Construye (o reconstruye) la malla, los puentes y los muros de basura. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Module")
	void BuildModule();

	/** Fija las bocas bloqueadas y la semilla de sus muros. Solo servidor, antes de FinishSpawning. */
	void InitializeModule(uint8 InBlockedExits, int32 InWallSeed, bool bInMirrored = false, uint8 InOuterSides = 0);

	/** Fija los vecinos con los que se funden los bordes. Solo servidor, antes de FinishSpawning. */
	void SetSeamNeighbors(const TArray<FTNSeamNeighbor>& InNeighbors) { SeamNeighbors = InNeighbors; }

	uint8 GetBlockedExits() const { return BlockedExits; }

	UFUNCTION(BlueprintPure, Category = "Module")
	UTN_TerrainModuleAsset* GetModuleAsset() const { return ModuleAsset; }

	/** Topología del asset asignado, o Straight si no hay asset. */
	ETNTerrainModuleTopology GetTopology() const;

	float GetModuleSize() const { return ModuleSize; }

protected:
	/** Heightfield del módulo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UTN_TerrainModuleAsset> ModuleAsset;

	/** Lado del módulo en uu. Debe coincidir con CellSize del generador. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module", meta = (ClampMin = "100.0"))
	float ModuleSize = 40000.f;

	/** Material del terreno. Debe usar el color de vértice como BaseColor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** Cota del agua, en uu, relativa al módulo. Solo afecta al color de vértice. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module|Colors")
	float WaterLevel = -400.f;

	/** Material de los arcos de roca y los monolitos. Si falta, usan TerrainMaterial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UMaterialInterface> BridgeMaterial;

	/** Material del bosque de algas. Debe leer el color de PerInstanceCustomData (3 floats),
	 *  como JunkMaterial; si falta se usa JunkMaterial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module|Biome")
	TObjectPtr<UMaterialInterface> FoliageMaterial;

	/** Terreno en la sección 0, un arco de roca por puente del asset y un pilar por
	 *  monolito en las siguientes (TNTerrainModule::BuildArchMesh / BuildMonolithMesh).
	 *  Todas con colisión. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** Sin uso desde que los puentes son arcos de roca en TerrainMesh. Se conserva como
	 *  subobjeto vacío para no invalidar los Blueprints ya guardados. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UInstancedStaticMeshComponent> BridgeInstances;

	/** Bocas (en espacio local, sin rotar) tapadas con un muro de basura. El generador
	 *  las fija según el camino; un diseñador puede marcarlas a mano en un actor colocado. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Module|Walls",
		meta = (Bitmask, BitmaskEnum = "/Script/Tortunabo.ETNTerrainModuleSide"))
	uint8 BlockedExits = 0;

	/** Módulo reflejado (Y local -> -Y): una curva a la izquierda se juega como una a la
	 *  derecha. Duplica la variedad de la librería sin más assets. Replicado una vez. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Module")
	bool bMirrored = false;

	/** Lados (espacio local del módulo colocado) que dan fuera del mapa: su terreno se
	 *  hunde en una costa irregular y una caja invisible impide salir. Replicado una vez. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Module",
		meta = (Bitmask, BitmaskEnum = "/Script/Tortunabo.ETNTerrainModuleSide"))
	uint8 OuterSides = 0;

	/** Semilla del montón de basura de cada muro. Replicada: mismo montón en todas las máquinas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Module|Walls")
	int32 WallSeed = 0;

	/** Vecinos (hasta 8) con los que se funden los bordes: sin ellos el módulo se construye
	 *  tal cual. Replicado una vez. */
	UPROPERTY(VisibleAnywhere, Replicated, Category = "Module|Seams")
	TArray<FTNSeamNeighbor> SeamNeighbors;

	/** Media anchura de la banda de fusión a cada lado de un borde compartido, en uu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module|Seams", meta = (ClampMin = "0.0"))
	float SeamBand = 4000.f;

	/** Material de la basura. Debe leer el color de PerInstanceCustomData (3 floats). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module|Walls")
	TObjectPtr<UMaterialInterface> JunkMaterial;

	/** Piezas de los muros, indexadas por TNGridJunk::EJunkShape. Con colisión. */
	UPROPERTY(VisibleAnywhere, Category = "Module|Walls")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> WallJunk;

	/** Caja de colisión invisible por lado (Norte, Este, Sur, Oeste): garantiza que no hay paso. */
	UPROPERTY(VisibleAnywhere, Category = "Module|Walls")
	TArray<TObjectPtr<UBoxComponent>> WallBlockers;

	/** Caja invisible a lo largo de cada lado exterior (Norte, Este, Sur, Oeste). */
	UPROPERTY(VisibleAnywhere, Category = "Module|Walls")
	TArray<TObjectPtr<UBoxComponent>> OuterBlockers;

	/** Bosque de algas, indexado por TNTerrainBiome::EFoliageShape. Sin colisión: se
	 *  atraviesa, solo tapa la vista. Lo siembra la máscara del asset. */
	UPROPERTY(VisibleAnywhere, Category = "Module|Biome")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Foliage;

private:
	/** Arcos, túneles y monolitos, una sección cada uno a partir de la 1. */
	void BuildRocks(const TNTerrainModule::FModuleColors& Colors, const TNTerrainModule::FModuleField& Field);
	void BuildWalls();
	void BuildFoliage(const TNTerrainModule::FModuleField& Field);
	/** Malla del terreno fundida con SeamNeighbors (TNTerrainSeam::BuildFusedMesh). */
	TNGridTerrain::FTileMesh BuildSeamedMesh(const TNTerrainModule::FModuleField& Field,
		const TNTerrainModule::FModuleColors& Colors, const TNTerrainModule::FModuleColors& BlendColors) const;

	/** Alturas con la costa exterior aplicada (las usan malla, colisión y algas). */
	TArray<uint16> PlacedHeights;

	/** Asset con el que se construyó la malla actual, para no reconstruir en balde. */
	TWeakObjectPtr<const UTN_TerrainModuleAsset> BuiltFromAsset;
	bool bBuiltMirrored = false;
	uint8 BuiltOuterSides = 0;
	TArray<FTNSeamNeighbor> BuiltSeamNeighbors;
};
