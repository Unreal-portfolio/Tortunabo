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
 * Bioma de un módulo. Decide la paleta de color, el bosque de algas y, en la generación
 * offline, la forma (mar poco profundo, fuerte con foso, cañón...). Ver
 * Scripts/gen_terrain_modules.py y TN_TerrainBiomeDecisions.h.
 */
UENUM(BlueprintType)
enum class ETNTerrainBiome : uint8
{
	/** Arena: desierto de dunas o cañón y montaña. */
	Sand,
	/** Agua: la muralla del camino entre un mar poco profundo, o un fuerte con foso. */
	Water,
	/** Algas: bosque frondoso de algas en tierra. */
	Algae
};

/**
 * Monolito: pilar de roca que el heightfield no puede representar (paredes verticales de
 * pocos metros de radio). El tile lo construye con TNTerrainModule::BuildMonolithMesh.
 * Todo en uu y en espacio local del módulo.
 */
USTRUCT(BlueprintType)
struct FTNTerrainModuleMonolith
{
	GENERATED_BODY()

	/** Centro del pie en el plano. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monolith")
	FVector2D Center = FVector2D::ZeroVector;

	/** Cota del pie: queda enterrado bajo el punto más bajo de su huella. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monolith")
	float BaseHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monolith", meta = (ClampMin = "50.0"))
	float Radius = 400.f;

	/** Altura sobre el pie. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monolith", meta = (ClampMin = "100.0"))
	float Height = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monolith")
	float Yaw = 0.f;

	/** Inclinación del eje en grados, hacia +X local del pilar (girado por Yaw). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monolith", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float Lean = 0.f;
};

/**
 * Puente de un módulo: tablero recto que salva el hueco que un pasillo abre en una ruta
 * alta. El heightfield no puede representar un voladizo, así que el tile lo coloca como
 * instancia de malla. Todo en uu y en espacio local del módulo.
 */
USTRUCT(BlueprintType)
struct FTNTerrainModuleBridge
{
	GENERATED_BODY()

	/** Centro del tablero en el plano (X = Sur a Norte, Y = Oeste a Este). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	FVector2D Center = FVector2D::ZeroVector;

	/** Yaw del eje largo del tablero, en grados (0 = eje X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	float Yaw = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge", meta = (ClampMin = "100.0"))
	float Length = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge", meta = (ClampMin = "100.0"))
	float Width = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge", meta = (ClampMin = "10.0"))
	float Thickness = 120.f;

	/** Cota de la cara superior del tablero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	float DeckHeight = 1000.f;
};

/** Zona llana del módulo (plaza), candidata a asentar un puzzle. En uu, espacio local. */
USTRUCT(BlueprintType)
struct FTNTerrainModuleFlatArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlatArea")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlatArea", meta = (ClampMin = "100.0"))
	float Radius = 3000.f;

	/** Cota del suelo de la plaza. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlatArea")
	float Height = 0.f;
};

/**
 * UTN_TerrainModuleAsset
 *
 * Heightfield de un módulo de terreno: una celda del grid, de ModuleSize uu de lado
 * (200 m en la librería vigente). Los genera Scripts/gen_terrain_modules.py y los importa
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

	/** Puentes de las rutas altas. Editables: un diseñador puede moverlos o quitarlos. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TArray<FTNTerrainModuleBridge> Bridges;

	/** Plazas llanas (candidatas a puzzle), tal como las generó el script. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TArray<FTNTerrainModuleFlatArea> FlatAreas;

	/** Pilares de roca. Editables como los puentes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TArray<FTNTerrainModuleMonolith> Monoliths;

	/** Bioma principal: manda en la paleta donde la máscara vale 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Biome")
	ETNTerrainBiome Biome = ETNTerrainBiome::Sand;

	/** Bioma hacia el que funde un módulo mixto. Igual a Biome en un módulo puro. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Biome")
	ETNTerrainBiome SecondaryBiome = ETNTerrainBiome::Sand;

	/** Un valor por vértice, como Heights. Byte alto: peso de SecondaryBiome (0-255).
	 *  Byte bajo: densidad del bosque de algas (0-255). Vacío = bioma puro sin algas. */
	UPROPERTY()
	TArray<uint16> BiomeMask;

	/** Sustituye la máscara de bioma. Debe tener Resolution² valores en [0, 65535]; llamar
	 *  después de SetHeightfield. Un array vacío la borra. */
	UFUNCTION(BlueprintCallable, Category = "Module|Biome")
	bool SetBiomeMask(const TArray<int32>& InMask);

	bool HasBiomeMask() const { return BiomeMask.Num() == Resolution * Resolution && Resolution >= 2; }

	bool IsMixed() const { return SecondaryBiome != Biome && HasBiomeMask(); }

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
