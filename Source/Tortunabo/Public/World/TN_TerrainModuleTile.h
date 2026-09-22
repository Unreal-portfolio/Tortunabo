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
	void InitializeModule(uint8 InBlockedExits, int32 InWallSeed);

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

	/** Material de los tableros de puente. Si falta, el cubo del motor sale gris. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UMaterialInterface> BridgeMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** Una instancia de cubo escalado por puente del asset. Subobjeto por defecto con
	 *  nombre estable para seguir siendo direccionable por red. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UInstancedStaticMeshComponent> BridgeInstances;

	/** Bocas (en espacio local, sin rotar) tapadas con un muro de basura. El generador
	 *  las fija según el camino; un diseñador puede marcarlas a mano en un actor colocado. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Module|Walls",
		meta = (Bitmask, BitmaskEnum = "/Script/Tortunabo.ETNTerrainModuleSide"))
	uint8 BlockedExits = 0;

	/** Semilla del montón de basura de cada muro. Replicada: mismo montón en todas las máquinas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Module|Walls")
	int32 WallSeed = 0;

	/** Material de la basura. Debe leer el color de PerInstanceCustomData (3 floats). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module|Walls")
	TObjectPtr<UMaterialInterface> JunkMaterial;

	/** Piezas de los muros, indexadas por TNGridJunk::EJunkShape. Con colisión. */
	UPROPERTY(VisibleAnywhere, Category = "Module|Walls")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> WallJunk;

	/** Caja de colisión invisible por lado (Norte, Este, Sur, Oeste): garantiza que no hay paso. */
	UPROPERTY(VisibleAnywhere, Category = "Module|Walls")
	TArray<TObjectPtr<UBoxComponent>> WallBlockers;

private:
	void BuildBridges();
	void BuildWalls();

	/** Asset con el que se construyó la malla actual, para no reconstruir en balde. */
	TWeakObjectPtr<const UTN_TerrainModuleAsset> BuiltFromAsset;
};
