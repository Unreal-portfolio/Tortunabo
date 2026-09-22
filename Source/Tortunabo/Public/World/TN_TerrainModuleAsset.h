#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TN_TerrainModuleAsset.generated.h"

/**
 * Topología de un módulo de terreno: por qué lados entra o sale el camino cuando el
 * módulo está sin rotar (YawSteps 0). Lados según TN_GridPathDecisions.h: Norte = +X
 * (avance), Este = +Y. El camino siempre entra por el Sur.
 */
UENUM(BlueprintType)
enum class ETNTerrainModuleTopology : uint8
{
	/** Sur y Norte. */
	Straight,
	/** Sur y Oeste. */
	CurveLeft,
	/** Sur y Este. */
	CurveRight,
	/** Sur, Norte y Oeste. */
	TLeft,
	/** Sur, Norte y Este. */
	TRight,
	/** Los cuatro lados. */
	Cross
};

/**
 * UTN_TerrainModuleAsset
 *
 * Heightfield de un módulo de terreno: una celda del grid, de ModuleSize uu de lado
 * (400 m por defecto). Los genera Scripts/gen_terrain_modules.py y los importa
 * Scripts/import_terrain_modules.py; los diseñadores parten de ellos como plantilla.
 *
 * Contrato del borde (lo comprueba TNTerrainModule::HasCanonicalBorder): los cuatro
 * lados de TODOS los módulos tienen el mismo perfil de alturas, simétrico respecto a
 * su punto medio. Así cualquier módulo casa con cualquier otro, con cualquier rotación,
 * sin costura. Un módulo que no usa una salida la cierra con una rampa por dentro del
 * borde, nunca modificando el borde.
 *
 * Alturas: Z_uu = (Heights[i] - HeightZero) * HeightScale, con i = fila * Resolution +
 * columna; la fila recorre el eje X local (Sur a Norte) y la columna el eje Y (Oeste a
 * Este). El primer y último vértice de cada eje caen exactamente sobre el borde.
 */
UCLASS(BlueprintType)
class TORTUNABO_API UTN_TerrainModuleAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	ETNTerrainModuleTopology Topology = ETNTerrainModuleTopology::Straight;

	/** Semilla con la que se generó. Solo informativa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	int32 Seed = 0;

	/** Vértices por lado del heightfield. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	int32 Resolution = 0;

	/** Unidades de Unreal por unidad de Heights. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	float HeightScale = 0.25f;

	/** Valor de Heights que corresponde a Z = 0 (cota del suelo en las salidas). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	int32 HeightZero = 32768;

	/** Resolution * Resolution alturas, fila a fila. Oculto al panel Details: son decenas
	 *  de miles de valores. Se rellena con SetHeightfield desde el script de importación. */
	UPROPERTY()
	TArray<uint16> Heights;

	/** Sustituye el heightfield y su codificación. InHeights debe tener InResolution²
	 *  valores en [0, 65535]. Única vía de escritura: Resolution, HeightScale y HeightZero
	 *  no se editan sueltos porque dejarían de casar con las alturas. */
	UFUNCTION(BlueprintCallable, Category = "Module")
	bool SetHeightfield(int32 InResolution, float InHeightScale, int32 InHeightZero, const TArray<int32>& InHeights);

	/** Hay heightfield y su tamaño casa con Resolution. */
	bool IsValidModule() const
	{
		return Resolution >= 2 && Heights.Num() == Resolution * Resolution;
	}
};
