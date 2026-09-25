#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapTypes.h"
#include "TN_ProcMapGenerator.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UPrimitiveComponent;
class UMaterialInterface;
class UPCGComponent;
class APlayerStart;
class ATN_ProcEggNest;
class ATN_ProcWaterVolume;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProcMapGenerated, int32, Generation);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnProcMapGeneratedNative, int32 /*Generation*/);

/** Lo único que se replica del mapa: con esto cada máquina genera el mismo. */
USTRUCT(BlueprintType)
struct TORTUNABO_API FTNProcMapNetConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ProcMap")
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ProcMap")
	ETNProcGameMode Mode = ETNProcGameMode::Coop;

	UPROPERTY(BlueprintReadOnly, Category = "ProcMap")
	ETNProcDifficulty Difficulty = ETNProcDifficulty::Normal;

	/** Se incrementa en cada (re)generación, p. ej. entre rondas. 0 = sin mapa. */
	UPROPERTY(BlueprintReadOnly, Category = "ProcMap")
	int32 Generation = 0;
};

/**
 * ATN_ProcMapGenerator
 *
 * Materializa el mapa procedural por módulos irregulares. La decisión de qué va
 * dónde vive en la lógica pura TNProcMap (TN_ProcMapGenerate.h, testeada); este
 * actor la traduce a:
 *   - Terreno: tiles de UProceduralMeshComponent con colisión y color por vértice
 *     (biomas mezclados, camino, roca en pendientes). Nanite no aplica a mallas
 *     generadas en runtime; la vegetación sí puede usar meshes con Nanite.
 *   - Agua: plano a nivel del mar + volumen nadable con cajas sobre el agua profunda.
 *   - Estructuras: tableros de puentes colosales, techos de cueva, isletas, labios
 *     de huecos, pasarelas, puentes del río, lava.
 *   - Vegetación y props: HISM por capa de bioma (y grafo PCG opcional por bioma).
 *   - Actores: géiseres, toboganes, zonas de muerte, corrientes, remolinos,
 *     depredadores, criaturas rebotadoras, pilas de huevos, muros y compuertas 2vs2,
 *     meta y PlayerStarts.
 *
 * Red: solo se replica FTNProcMapNetConfig. El servidor la fija (ServerGenerate) y
 * cada cliente genera lo mismo en OnRep. Los actores que deben replicar (enemigos,
 * huevos, puzles) los crea solo el servidor; los de movimiento (géiser, tobogán,
 * agua) se crean en todas las máquinas para que la predicción del cliente cuadre.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ProcMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	ATN_ProcMapGenerator();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor: genera (o regenera) el mapa con esta semilla y lo replica a todos. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ProcMap")
	void ServerGenerate(int32 InSeed, ETNProcGameMode InMode, ETNProcDifficulty InDifficulty);

	/** Genera con los parámetros de edición. Botón en el panel Details. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "ProcMap")
	void GenerateInEditor();

	/** Borra todo lo generado. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "ProcMap")
	void Clear();

	UFUNCTION(BlueprintPure, Category = "ProcMap")
	bool IsMapReady() const { return bMapReady; }

	/** Generación que ya está construida en ESTA máquina. */
	UFUNCTION(BlueprintPure, Category = "ProcMap")
	int32 GetBuiltGeneration() const { return BuiltGeneration; }

	/** Generación pedida por el servidor (replicada). */
	UFUNCTION(BlueprintPure, Category = "ProcMap")
	int32 GetRequestedGeneration() const { return NetConfig.Generation; }

	const FTNProcMapNetConfig& GetNetConfig() const { return NetConfig; }
	const TNProcMap::FLayout& GetLayout() const { return Layout; }
	const FTNProcMapProfile& GetActiveProfile() const { return ActiveProfile; }

	/** Transform de salida para el jugador N (alrededor del claro inicial). */
	UFUNCTION(BlueprintPure, Category = "ProcMap")
	FTransform GetStartTransform(int32 PlayerIndex) const;

	/** Progreso (cm a lo largo del camino principal) de una posición del mundo. */
	UFUNCTION(BlueprintPure, Category = "ProcMap")
	float GetPathProgress(const FVector& WorldLocation) const;

	/** Punto del camino principal (mundo) a un progreso dado, con su dirección. */
	UFUNCTION(BlueprintCallable, Category = "ProcMap")
	FVector GetPathLocationAtProgress(float Progress, FVector& OutDirection) const;

	UFUNCTION(BlueprintPure, Category = "ProcMap")
	float GetMainPathLength() const;

	/** Minutos estimados de recorrido del camino principal a una velocidad media. */
	UFUNCTION(BlueprintPure, Category = "ProcMap")
	float EstimateTraversalMinutes(float AverageSpeedCmPerSec = 550.f) const;

	/** Pilas de huevos (solo en servidor; los clientes las ven como actores replicados). */
	const TArray<TWeakObjectPtr<ATN_ProcEggNest>>& GetEggNests() const { return EggNests; }

	/** Altura del terreno generado en un punto del mundo (sin trazas: vale antes de cocinar colisión). */
	float GetTerrainHeightAt(const FVector& WorldLocation) const;

	UTN_ProcMapSettings* GetSettings() const { return Settings; }

	/** Ajustes a usar si el generador del nivel no tiene (lo llama el GameMode antes de generar). */
	void SetSettingsIfMissing(UTN_ProcMapSettings* InSettings) { if (!Settings) { Settings = InSettings; } }

	FOnProcMapGeneratedNative OnMapGeneratedNative;

	UPROPERTY(BlueprintAssignable, Category = "ProcMap")
	FOnProcMapGenerated OnMapGenerated;

protected:
	/** Configuración del mapa (biomas, perfiles, materiales). Opcional: hay valores greybox. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap")
	TObjectPtr<UTN_ProcMapSettings> Settings;

	/** Si ningún GameMode lo pide en X segundos, el servidor genera con los valores de edición. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap")
	bool bAutoGenerateIfIdle = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap|Editor")
	int32 EditorSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap|Editor")
	bool bEditorRandomSeed = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap|Editor")
	ETNProcGameMode EditorMode = ETNProcGameMode::Coop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap|Editor")
	ETNProcDifficulty EditorDifficulty = ETNProcDifficulty::Normal;

	/** Salta la vegetación (iterar rápido sobre la forma del mapa). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap|Debug")
	bool bSkipScatter = false;

	/** Dibuja camino, ramas y módulos con líneas de debug. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcMap|Debug")
	bool bDebugDraw = false;

private:
	UPROPERTY(ReplicatedUsing = OnRep_NetConfig)
	FTNProcMapNetConfig NetConfig;

	UFUNCTION()
	void OnRep_NetConfig();

	/** Construye todo lo de NetConfig en esta máquina. */
	void BuildFromNetConfig();

	bool BuildLayout();
	void BuildTerrain();
	void BuildWater();
	void BuildStructures();
	void BuildScatter();
	/** Vegetación y rocas sueltas procedurales por bioma (TN_ProcMapFlora.h). */
	void BuildFlora();
	void SpawnTraversalActors();
	void SpawnServerActors();
	void SpawnHazards();
	void RunBiomePCG();
	void BuildProgressIndex();
	void DrawDebug() const;
	void ReportReadyToServer();
	void FreezeLocalPawnUntilReady();

	// ── Utilidades ──────────────────────────────────────────────────────────
	FVector MapToWorld(const FVector& MapPoint) const;
	FVector MapToWorld2D(const FVector2D& MapPoint, double Z) const { return MapToWorld(FVector(MapPoint.X, MapPoint.Y, Z)); }
	FVector WorldToMap(const FVector& WorldPoint) const;
	double TerrainHeightMap(const FVector2D& MapPoint) const;
	FVector TerrainNormalMap(const FVector2D& MapPoint) const;
	double PathDistanceMap(const FVector2D& MapPoint) const;
	UMaterialInterface* ResolveMaterial(UMaterialInterface* Preferred, const TCHAR* FallbackPath) const;
	void ResolveBiomeColors(ETNProcBiome Biome, FLinearColor& Ground, FLinearColor& Path, FLinearColor& Rock, FLinearColor& Bed) const;
	AActor* SpawnMapActor(UClass* Class, const FTransform& Transform, bool bTrackAsServer);

	TNProcMap::FLayout Layout;
	FTNProcMapProfile ActiveProfile;
	bool bMapReady = false;
	int32 BuiltGeneration = 0;
	bool bReportedReady = false;
	float IdleTimer = 0.f;

	// Malla de alturas (espacio del mapa) para colocar cosas sin depender de la colisión.
	TArray<float> Heights;
	TArray<uint8> PathMask;
	TArray<float> PathDist;
	FVector2D LatticeOrigin = FVector2D::ZeroVector;
	double LatticeSpacing = 250.0;
	int32 LatticeNX = 0;
	int32 LatticeNY = 0;

	// Índice de progreso: puntos del camino (principal y ramas) con su distancia.
	struct FProgressPoint
	{
		FVector P;
		float Progress;
	};
	TArray<FProgressPoint> ProgressPoints;
	TArray<TArray<int32>> ProgressBuckets;
	FVector2D ProgressOrigin = FVector2D::ZeroVector;
	double ProgressCell = 3000.0;
	int32 ProgressW = 0;
	int32 ProgressH = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> TerrainTiles;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> StructureMesh;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> DecorMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WaterPlane;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> ScatterComponents;

	/** Mallas de la vegetación procedural (una por bioma, especie y variante), construidas en ejecución. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> FloraMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPCGComponent>> PCGComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> BoundaryWalls;

	// Formas básicas del motor para greybox (cargadas en el constructor para que se cocinen).
	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicPlane;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicCube;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicCylinder;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicSphere;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BasicCone;

	bool bFrozeLocalPawn = false;

	TArray<TWeakObjectPtr<ATN_ProcEggNest>> EggNests;
	TArray<FTransform> StartTransforms;
};
