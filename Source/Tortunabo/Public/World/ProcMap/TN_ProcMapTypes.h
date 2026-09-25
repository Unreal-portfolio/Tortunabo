#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "TN_ProcMapTypes.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UPCGGraphInterface;

/**
 * Configuración editable del mapa procedural: perfil de generación por modo y
 * dificultad, y un DataAsset por bioma con colores, vegetación y peligros.
 * Todo tiene valores por defecto razonables: sin assets asignados el mapa sale
 * en greybox con formas básicas del motor.
 */

/** Cómo se coloca un peligro/enemigo respecto al camino. */
UENUM(BlueprintType)
enum class ETNProcHazardPlacement : uint8
{
	OnPath      UMETA(DisplayName = "Sobre el camino"),
	PathEdge    UMETA(DisplayName = "Borde del camino"),
	NearPath    UMETA(DisplayName = "Junto al camino"),
	InWater     UMETA(DisplayName = "En el agua"),
	AbovePath   UMETA(DisplayName = "Sobre el camino (aire)"),
	OffPathFar  UMETA(DisplayName = "Lejos del camino")
};

/** Dónde puede ir una capa de vegetación/props. */
UENUM(BlueprintType)
enum class ETNProcScatterZone : uint8
{
	/** Terreno fuera del camino (lo normal para árboles, rocas...). */
	OffPath     UMETA(DisplayName = "Fuera del camino"),
	/** Arcén: borde del camino (basura, conchas, hierba). */
	PathEdge    UMETA(DisplayName = "Borde del camino"),
	/** Muros y zonas altas del perímetro (bosque denso, rocas grandes). */
	Walls       UMETA(DisplayName = "Muros del borde"),
	/** Bajo el nivel del agua poco profunda (raíces de manglar, juncos). */
	Shallows    UMETA(DisplayName = "Aguas someras")
};

/** Parámetros de generación para un modo y una dificultad. */
USTRUCT(BlueprintType)
struct TORTUNABO_API FTNProcMapProfile
{
	GENERATED_BODY()

	/** Módulos por lado (3x3, 6x6, 8x8...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "1", ClampMax = "10"))
	int32 GridSize = 6;

	/** Lado nominal de un módulo (cm). 40000 = 400 m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "8000.0"))
	float ModuleSize = 40000.f;

	/** Fracción de módulos por los que pasa el camino principal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float Coverage = 0.78f;

	/** Longitud del camino dentro de un módulo respecto a la línea recta entre portales. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float Sinuosity = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "300.0"))
	float PathWidthMin = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "300.0"))
	float PathWidthMax = 3500.f;

	/** Probabilidad de pasos estrechos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float NarrowChance = 0.22f;

	/** Cruces colosales (puente o cueva) sobre módulos ya recorridos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "0", ClampMax = "8"))
	int32 NumCrossings = 2;

	/** Bifurcaciones que vuelven a unirse (exploración / alternativas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "0", ClampMax = "16"))
	int32 NumBranches = 7;

	/** Carriles paralelos con puzle de lanzamiento y sabotaje (2vs2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "0", ClampMax = "8"))
	int32 NumLanes = 0;

	/** Módulos máximos que abarca una rama. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino", meta = (ClampMin = "1", ClampMax = "3"))
	int32 BranchMaxModules = 3;

	/** Huecos de salto por km de camino. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltos", meta = (ClampMin = "0.0"))
	float GapsPerKm = 3.f;

	/** Hueco mínimo/máximo (cm). La tortuga salta 2 m corriendo y 4 m con dive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltos", meta = (ClampMin = "50.0"))
	float GapMin = 130.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saltos", meta = (ClampMin = "50.0"))
	float GapMax = 390.f;

	/** Altura del tablero/mesa de los cruces colosales sobre el suelo (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colosal", meta = (ClampMin = "1500.0"))
	float ColossalHeightMin = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colosal", meta = (ClampMin = "1500.0"))
	float ColossalHeightMax = 5500.f;

	/** Qué son los módulos por los que no pasa el camino. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapa")
	ETNProcEmptyModuleMode EmptyModuleMode = ETNProcEmptyModuleMode::Mixed;

	/** Regiones de bioma (0 = automático según el tamaño). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapa", meta = (ClampMin = "0", ClampMax = "8"))
	int32 NumBiomeRegions = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapa")
	bool bRiver = false;

	/** Una pila de huevos de respawn cada N cruces de módulo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (ClampMin = "1", ClampMax = "10"))
	int32 EggNestEveryNPortals = 2;

	/** Multiplicador de densidad de peligros y enemigos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligros", meta = (ClampMin = "0.0"))
	float HazardDensity = 1.f;

	/** 0 = fácil, 1 = difícil: huecos, isletas, peligros mínimos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligros", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Difficulty01 = 0.5f;

	/** Tormenta que persigue al grupo por el camino (solo Coop). Velocidad en cm/s; 0 = sin tormenta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tormenta", meta = (ClampMin = "0.0"))
	float StormSpeed = 380.f;

	/** Segundos de gracia antes de que la tormenta empiece a avanzar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tormenta", meta = (ClampMin = "0.0"))
	float StormGraceSeconds = 60.f;

	/** Convierte a los parámetros de la lógica pura. */
	TNProcMap::FGenParams ToGenParams(uint32 Seed) const;
};

/** Perfil asociado a un modo y una dificultad. */
USTRUCT(BlueprintType)
struct TORTUNABO_API FTNProcModeProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perfil")
	ETNProcGameMode Mode = ETNProcGameMode::Coop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perfil")
	ETNProcDifficulty Difficulty = ETNProcDifficulty::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perfil")
	FTNProcMapProfile Profile;
};

/** Una capa de vegetación / props instanciados. */
USTRUCT(BlueprintType)
struct TORTUNABO_API FTNProcScatterLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	TObjectPtr<UStaticMesh> Mesh;

	/** Material opcional (si no, el del mesh). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	ETNProcScatterZone Zone = ETNProcScatterZone::OffPath;

	/** Instancias por cada 100 m². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0"))
	float DensityPer100m2 = 1.f;

	/** Escala uniforme mínima/máxima (X = min, Y = max). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	FVector2D ScaleRange = FVector2D(0.8, 1.2);

	/** Escala no uniforme adicional (para greybox con formas básicas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	FVector ScaleAxes = FVector(1.0, 1.0, 1.0);

	/** Distancia mínima al borde del camino (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0"))
	float MinPathDistance = 300.f;

	/** Pendiente máxima (grados). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxSlopeDeg = 35.f;

	/** Hunde la instancia en el suelo (cm) para que no flote en pendiente. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	float ZOffset = -10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bAlignToNormal = false;

	/** Árboles, rocas grandes: bloquean. Hierba, basura pequeña: no. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bCollision = true;

	/** Distancia de culling (cm, 0 = sin culling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0"))
	float CullDistance = 30000.f;

	/** Greybox: tiñe el material (parámetro "Color") para distinguir capas sin arte. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bApplyTint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (EditCondition = "bApplyTint"))
	FLinearColor Tint = FLinearColor::White;
};

/** Un peligro/enemigo/spawner que el generador coloca en el bioma. */
USTRUCT(BlueprintType)
struct TORTUNABO_API FTNProcHazardEntry
{
	GENERATED_BODY()

	/** Clase a spawnear (BP de enemigo, zona de spawn, peligro...). Si replica, solo la crea el servidor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligro")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligro", meta = (ClampMin = "0.0"))
	float PerKm = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligro")
	ETNProcHazardPlacement Placement = ETNProcHazardPlacement::NearPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligro")
	ETNProcDifficulty MinDifficulty = ETNProcDifficulty::Easy;

	/** Distancia mínima entre dos del mismo tipo (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligro", meta = (ClampMin = "0.0"))
	float Clearance = 3000.f;

	/** Solo en el camino principal (no en ramas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligro")
	bool bMainPathOnly = false;

	/** Altura extra sobre el suelo (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peligro")
	float ZOffset = 0.f;
};

/** Aspecto y contenido de un bioma. */
UCLASS(BlueprintType)
class TORTUNABO_API UTN_ProcBiomeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bioma")
	ETNProcBiome Biome = ETNProcBiome::Jungle;

	/** Color del suelo (vertex color del terreno). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colores")
	FLinearColor GroundColor = FLinearColor(0.2f, 0.45f, 0.15f);

	/** Color del camino (tierra, arena apisonada...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colores")
	FLinearColor PathColor = FLinearColor(0.55f, 0.42f, 0.25f);

	/** Color de las pendientes fuertes y muros. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colores")
	FLinearColor RockColor = FLinearColor(0.35f, 0.33f, 0.3f);

	/** Color del lecho bajo el agua. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colores")
	FLinearColor BedColor = FLinearColor(0.6f, 0.55f, 0.4f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contenido")
	TArray<FTNProcScatterLayer> Scatter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contenido")
	TArray<FTNProcHazardEntry> Hazards;

	/** Mesh de la criatura flotante con comportamiento de medusa de este bioma (medusa, nenúfar, boya...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Agua")
	TObjectPtr<UStaticMesh> WaterBouncerMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Agua")
	FLinearColor WaterBouncerColor = FLinearColor(0.9f, 0.5f, 0.9f);

	/** Grafo PCG opcional que se ejecuta sobre el mapa generado para este bioma (extensión). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PCG")
	TObjectPtr<UPCGGraphInterface> PCGGraph;

	/**
	 * Rellena colores, vegetación y peligros con el greybox del bioma (lo mismo que
	 * usa el generador cuando no hay asset). Punto de partida para sustituir formas
	 * básicas por arte. Ojo: Scatter o Hazards vacíos significan "nada".
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Bioma")
	void ResetToGreyboxDefaults();
};

/** Configuración global del mapa procedural. */
UCLASS(BlueprintType)
class TORTUNABO_API UTN_ProcMapSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Un asset por bioma (se busca por su campo Biome). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biomas")
	TArray<TObjectPtr<UTN_ProcBiomeDataAsset>> Biomes;

	/** Perfiles por modo y dificultad. Si falta alguno se usa el de código (TN_MakeDefaultProcProfile). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perfiles")
	TArray<FTNProcModeProfile> Profiles;

	/** Rellena Profiles con los 9 perfiles por defecto (Coop/Carrera/2vs2 × Fácil/Normal/Difícil) para editarlos. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Perfiles")
	void FillDefaultProfiles();

	// ── Terreno ─────────────────────────────────────────────────────────────

	/** Separación de vértices del terreno (cm). Menos = más detalle y más coste. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terreno", meta = (ClampMin = "100.0", ClampMax = "800.0"))
	float VertexSpacing = 250.f;

	/** Cuadrados por lado de cada tile de terreno. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terreno", meta = (ClampMin = "8", ClampMax = "128"))
	int32 TileQuads = 48;

	/** Margen de terreno fuera del mapa jugable (muros, horizonte). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terreno", meta = (ClampMin = "0.0"))
	float OuterMargin = 25000.f;

	/** Mar que se genera más allá de la costa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terreno", meta = (ClampMin = "0.0"))
	float SeaExtent = 30000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materiales")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materiales")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materiales")
	TObjectPtr<UMaterialInterface> LavaMaterial;

	/** Roca de las estructuras colosales e isletas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materiales")
	TObjectPtr<UMaterialInterface> RockMaterial;

	/** Madera de pasarelas, labios de huecos y puentes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materiales")
	TObjectPtr<UMaterialInterface> WoodMaterial;

	/** Agua que baja por los toboganes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materiales")
	TObjectPtr<UMaterialInterface> SlideWaterMaterial;

	// ── Clases de los elementos del mapa (por defecto las C++) ─────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clases")
	TSubclassOf<AActor> GeyserClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clases")
	TSubclassOf<AActor> EggNestClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clases")
	TSubclassOf<AActor> ThrowWallClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clases")
	TSubclassOf<AActor> SabotageGateClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clases")
	TSubclassOf<AActor> SwitchClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clases")
	TSubclassOf<AActor> WaterBouncerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clases")
	TSubclassOf<AActor> PathStormClass;

	/** Busca el asset del bioma (nullptr si no hay). */
	const UTN_ProcBiomeDataAsset* FindBiome(ETNProcBiome Biome) const;

	/** Perfil del modo/dificultad; si no está configurado, uno por defecto coherente. */
	FTNProcMapProfile ResolveProfile(ETNProcGameMode Mode, ETNProcDifficulty Difficulty) const;
};

/** Perfiles por defecto (sin DataAsset): Coop 3/6/8, Carrera 2/3/4, 2vs2 2/3/4 con carriles. */
TORTUNABO_API FTNProcMapProfile TN_MakeDefaultProcProfile(ETNProcGameMode Mode, ETNProcDifficulty Difficulty);

/** Colores de greybox por bioma cuando no hay DataAsset. */
TORTUNABO_API void TN_DefaultBiomeColors(ETNProcBiome Biome, FLinearColor& OutGround, FLinearColor& OutPath, FLinearColor& OutRock, FLinearColor& OutBed);

/** Vegetación greybox por bioma (formas básicas del motor teñidas). */
TORTUNABO_API void TN_DefaultBiomeScatter(ETNProcBiome Biome, TArray<FTNProcScatterLayer>& Out);

/** Peligros greybox por bioma: fauna acuática en C++ y los BPs de enemigos, ítems y trampas del juego. */
TORTUNABO_API void TN_DefaultBiomeHazards(ETNProcBiome Biome, TArray<FTNProcHazardEntry>& Out);
