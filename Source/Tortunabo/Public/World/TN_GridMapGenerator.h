#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_GridMapGenerator.generated.h"

class ATN_GridTerrainTile;
class ATN_TerrainModuleTile;
namespace TNTerrainBiome { struct FCellBiome; }
enum class ETNTerrainEdge : uint8;
class UMaterialInterface;
class UStaticMeshComponent;
namespace TNGridRoutes { struct FTNDetour; }

/**
 * ATN_GridMapGenerator
 *
 * Genera de una vez un mapa sobre un grid cuadrado: un único camino de rectas y giros
 * desde la fila 0 hasta la última fila, y un tile de relleno en cada celda restante.
 * La decisión de qué va en cada celda vive en TN_GridPathDecisions.h (lógica pura y
 * testeada); este actor solo la traduce a actores en el mundo.
 *
 * Disposición en el mundo (espacio local del generador):
 *   fila    → +X (avance)      columna → +Y (derecha)
 *   centro de celda = (Fila * CellSize, Columna * CellSize, 0)
 *   yaw del tile    = YawSteps * 90°
 *
 * Los tiles deben modelarse centrados en su origen, con el recto abierto a lo largo de
 * su eje X y el giro abierto por -X y +Y (ver convenciones en TN_GridPathDecisions.h).
 *
 * Tres modos, por prioridad:
 *   - Módulos (ModuleClasses no vacío): un ATN_TerrainModuleTile por celda de ruta,
 *     elegido por topología y rotado para ofrecer las salidas que pide la ruta. Además del
 *     camino principal se trazan desvíos que salen de él y vuelven más adelante (ver
 *     TN_GridRouteDecisions.h). Las celdas sin ruta solo reciben módulo con
 *     bFillEmptyCellsWithModules. Ver TN_TerrainModuleDecisions.h.
 *   - Terreno (TerrainTileClass asignado): un ATN_GridTerrainTile replicado por celda;
 *     entre todos forman un heightfield continuo (ver TN_GridTerrainDecisions.h).
 *   - Greybox (nada de lo anterior): tiles de recta, giro y relleno hechos de cajas.
 *
 * El generador solo actúa con autoridad. No se integra todavía con ATN_ChunkManager ni
 * con el flujo de partida.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_GridMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	ATN_GridMapGenerator();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Borra el mapa anterior y genera uno nuevo. Ejecutable desde el panel Details. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "GridMap")
	void Generate();

	/** Destruye todos los tiles generados por este actor. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "GridMap")
	void Clear();

	/** Centro en mundo de una celda (X = columna, Y = fila). */
	UFUNCTION(BlueprintPure, Category = "GridMap")
	FVector GetCellWorldLocation(FIntPoint Cell) const;

protected:
	/** Celdas por lado del grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap", meta = (ClampMin = "1", ClampMax = "32"))
	int32 GridSize = 6;

	/** Lado de una celda en unidades de Unreal. En modo greybox debe coincidir con el
	 *  tamaño de los tiles (2000); en modo terreno, con lo que admita FTNGridTerrainSettings;
	 *  en modo módulos, con ModuleSize de los módulos (40000 = 400 m). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap", meta = (ClampMin = "100.0"))
	float CellSize = 4000.f;

	/** Longitud mínima del camino, en celdas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap", meta = (ClampMin = "1"))
	int32 MinPathLength = 10;

	/** Longitud máxima del camino, en celdas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap", meta = (ClampMin = "1"))
	int32 MaxPathLength = 20;

	/** Semilla del mapa. Misma semilla → mismo mapa. Ignorada si bRandomSeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Seed")
	int32 Seed = 1337;

	/** Si true, cada Generate() sortea una semilla nueva (queda en LastUsedSeed). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Seed")
	bool bRandomSeed = false;

	/** Semilla del último mapa generado, para poder reproducirlo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "GridMap|Seed")
	int32 LastUsedSeed = 0;

	/** Si true, genera el mapa en BeginPlay (solo con autoridad). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap")
	bool bGenerateOnBeginPlay = true;

	/** Módulos disponibles (hijos de ATN_TerrainModuleTile con asset). Si hay alguno, el
	 *  mapa se construye con módulos y se ignoran los otros dos modos. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Modules")
	TArray<TSubclassOf<ATN_TerrainModuleTile>> ModuleClasses;

	/** Si true, las celdas sin camino también reciben un módulo (rotación sorteada). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Modules")
	bool bFillEmptyCellsWithModules = false;

	/** Si true, el camino se reparte en regiones de bioma (arena, agua, algas) según la
	 *  semilla, con módulos mixtos en las fronteras (TNTerrainBiome::PlanPathBiomes). Si
	 *  ningún módulo encaja con el bioma de una celda, se usa cualquiera que ofrezca las salidas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Modules")
	bool bUseBiomeRegions = true;

	/** Si true, un módulo también se puede colocar reflejado (dobla la variedad). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Modules")
	bool bAllowMirroredModules = true;

	/** Desvíos máximos: rutas alternativas que salen del camino y vuelven a él (solo módulos). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Routes", meta = (ClampMin = "0"))
	int32 MaxDetours = 2;

	/** Celdas de camino principal que salta un desvío como mínimo (2 = rodea una sola celda). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Routes", meta = (ClampMin = "2"))
	int32 MinDetourSpan = 2;

	/** Celdas de camino principal que salta un desvío como máximo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Routes", meta = (ClampMin = "2"))
	int32 MaxDetourSpan = 4;

	/** Longitud máxima de un desvío, en celdas propias. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Routes", meta = (ClampMin = "1"))
	int32 MaxDetourLength = 5;

	/** Cota del plano de agua en modo módulos, relativa al generador. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Modules")
	float ModuleWaterLevel = -400.f;

	/** Si está asignado, el mapa se genera como terreno continuo y se ignoran los tiles greybox. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Terrain")
	TSubclassOf<ATN_GridTerrainTile> TerrainTileClass;

	/** Material del plano de agua (modos terreno y módulos). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Terrain")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** Plano de agua bajo todo el grid. Depende solo de datos del nivel, así que cada
	 *  máquina lo coloca por su cuenta: no necesita replicarse. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GridMap|Terrain")
	TObjectPtr<UStaticMeshComponent> WaterPlane;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Tiles")
	TSubclassOf<AActor> StraightTileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Tiles")
	TSubclassOf<AActor> TurnTileClass;

	/** Rellenos para las celdas sin camino; se elige uno por celda con la semilla. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Tiles")
	TArray<TSubclassOf<AActor>> FillerTileClasses;

	/** Dibuja el recorrido del camino sobre los tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Debug")
	bool bDebugDrawPath = true;

	/** Si true, tras generar se mueven los APlayerStart del nivel a la celda de inicio, en
	 *  corro alrededor de su centro y apoyados en el suelo. El nivel no sabe dónde caerá el
	 *  inicio: sin esto los jugadores aparecen donde se dejó el PlayerStart a mano. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Spawn")
	bool bPlacePlayerStartsAtStartCell = true;

	/** Radio del corro de PlayerStarts alrededor del centro de la celda de inicio, en uu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap|Spawn", meta = (ClampMin = "0.0"))
	float PlayerStartRingRadius = 400.f;

private:
	void GenerateGreybox(const TArray<FIntPoint>& Path, TFunctionRef<int32(int32 Min, int32 Max)> RandRange);
	void GenerateTerrain(const TArray<FIntPoint>& Path);
	void GenerateModules(const TArray<FIntPoint>& Path, const TArray<TNGridRoutes::FTNDetour>& Detours,
		TFunctionRef<int32(int32 Min, int32 Max)> RandRange);

	/** Módulo y rotación que ofrecen al menos las salidas pedidas (espacio de mundo), o
	 *  clase nula si ninguno las ofrece. Con Wanted, solo entran los módulos que mejor
	 *  encajan con ese bioma (TNTerrainBiome::BiomeMatchScore). */
	TSubclassOf<ATN_TerrainModuleTile> PickModuleForExits(uint8 RequiredExits,
		TFunctionRef<int32(int32 Min, int32 Max)> RandRange, int32& OutYawSteps, bool& bOutMirrored,
		const TNTerrainBiome::FCellBiome* Wanted = nullptr, const ETNTerrainEdge* WantedEdges = nullptr,
		const TSet<UClass*>* UsedClasses = nullptr) const;

	/** Spawn diferido: devuelve el tile sin terminar para poder inicializarlo; el llamante
	 *  debe cerrar con FinishTile. */
	AActor* BeginSpawnTile(TSubclassOf<AActor> TileClass, FIntPoint Cell, int32 YawSteps, FTransform& OutTransform);
	void FinishTile(AActor* Tile, const FTransform& Transform);
	AActor* SpawnTile(TSubclassOf<AActor> TileClass, FIntPoint Cell, int32 YawSteps);

	bool IsModuleMode() const { return ModuleClasses.Num() > 0; }

	void UpdateWaterPlane();

	void PlacePlayerStarts(FIntPoint StartCell) const;

	void DrawPathDebug(const TArray<FIntPoint>& Path, const TArray<TNGridRoutes::FTNDetour>& Detours) const;

	/** Tag de los tiles generados. Clear() los localiza por tag y no por una lista propia:
	 *  en editor el generador se reconstruye al tocar propiedades y la lista se perdería. */
	static const FName GeneratedTileTag;

	/** Tags adicionales del primer y último tile del camino (spawn de jugadores, meta). */
	static const FName StartTileTag;
	static const FName EndTileTag;
};
