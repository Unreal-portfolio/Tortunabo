#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_TerrainModuleTile.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class UTN_TerrainModuleAsset;
enum class ETNTerrainModuleTopology : uint8;

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

	/** Construye (o reconstruye) la malla a partir de ModuleAsset. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Module")
	void BuildModule();

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

private:
	/** Asset con el que se construyó la malla actual, para no reconstruir en balde. */
	TWeakObjectPtr<const UTN_TerrainModuleAsset> BuiltFromAsset;
};
