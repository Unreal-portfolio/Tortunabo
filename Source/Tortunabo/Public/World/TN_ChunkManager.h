#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_ChunkManager.generated.h"

class UBoxComponent;
class USceneComponent;

/**
 * Niveles de dificultad de los chunks.
 */
UENUM(BlueprintType)
enum class ETNChunkDifficulty : uint8
{
	Easy   UMETA(DisplayName = "Easy"),
	Medium UMETA(DisplayName = "Medium"),
	Hard   UMETA(DisplayName = "Hard"),
};

/**
 * ATN_ChunkManager — Gestor de generación procedural de mapa por chunks.
 *
 * Colocar una instancia Blueprint de este actor en el mapa de carrera (LVL_Run).
 * Su posición en el mundo define dónde se spawnea el primer chunk.
 *
 * Cada chunk Blueprint debe tener tres componentes con nombres EXACTOS:
 *   - "InSocket"    → SceneComponent en el punto de entrada del chunk
 *   - "OutSocket"   → SceneComponent en el punto de salida del chunk
 *   - "EndTrigger"  → BoxComponent que detecta cuando un jugador cruza el chunk
 *
 * El sistema es completamente server-authoritative:
 *   - El ChunkManager solo existe en el servidor.
 *   - Los actores spawneados replican a todos los clientes automáticamente.
 *   - Todos los jugadores ven los mismos chunks al mismo tiempo.
 *
 * Dificultad progresiva:
 *   chunks 0 .. EasyToMediumThreshold-1         → pool EASY
 *   chunks EasyToMediumThreshold .. MediumToHardThreshold-1 → pool MEDIUM
 *   chunks MediumToHardThreshold+              → pool HARD
 *   Cuando PassedChunkCount >= TotalChunksBeforeFinal → spawn FinalChunkClass
 */
UCLASS()
class TORTUNABO_API ATN_ChunkManager : public AActor
{
	GENERATED_BODY()

public:
	ATN_ChunkManager();

	virtual void BeginPlay() override;

	// ── Pools de chunks por dificultad ───────────────────────────────────────

	/** Chunks fáciles (primeros de la carrera). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Easy")
	TArray<TSubclassOf<AActor>> EasyChunkClasses;

	/** Chunks de dificultad media. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Medium")
	TArray<TSubclassOf<AActor>> MediumChunkClasses;

	/** Chunks difíciles (final de la carrera). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Hard")
	TArray<TSubclassOf<AActor>> HardChunkClasses;

	// ── Umbrales de dificultad ───────────────────────────────────────────────

	/**
	 * Número de chunks pasados a partir del cual el pool cambia de EASY a MEDIUM.
	 * Ejemplo: 3 → los chunks 0, 1, 2 son EASY; el 3 en adelante es MEDIUM.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Difficulty",
		meta = (ClampMin = "1"))
	int32 EasyToMediumThreshold = 3;

	/**
	 * Número de chunks pasados a partir del cual el pool cambia de MEDIUM a HARD.
	 * Debe ser mayor que EasyToMediumThreshold.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Difficulty",
		meta = (ClampMin = "2"))
	int32 MediumToHardThreshold = 7;

	// ── Chunk final ──────────────────────────────────────────────────────────

	/**
	 * Chunk especial que se spawnea al final de la carrera.
	 * DEBE contener un ATN_FinishLineVolume colocado dentro del BP.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Final")
	TSubclassOf<AActor> FinalChunkClass;

	/**
	 * Número total de chunks aleatorios antes de spawnear el chunk final.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Final",
		meta = (ClampMin = "1"))
	int32 TotalChunksBeforeFinal = 10;

	// ── Buffer ───────────────────────────────────────────────────────────────

	/**
	 * Número de chunks a mantener activos por delante del jugador líder.
	 * Con KeepBehind=1, el buffer total = KeepAhead + KeepBehind.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Buffer",
		meta = (ClampMin = "1"))
	int32 KeepAhead = 5;

	/**
	 * Número de chunks a mantener activos por detrás (para evitar pop-in abrupto).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunks|Buffer",
		meta = (ClampMin = "0"))
	int32 KeepBehind = 1;

	// ── Utilidad de debug ────────────────────────────────────────────────────

	/** Si true, dibuja las posiciones de InSocket/OutSocket de cada chunk spawneado. */
	UPROPERTY(EditDefaultsOnly, Category = "Chunks|Debug")
	bool bDebugDrawSockets = false;

private:

	// Lista de chunks actualmente activos (FIFO: [0] = más viejo).
	TArray<TWeakObjectPtr<AActor>> ActiveChunks;

	// Transform donde se alineará el InSocket del siguiente chunk a spawnear.
	FTransform NextSpawnTransform;

	// Índice del último chunk seleccionado (para evitar repetición inmediata).
	int32 LastSelectedIndex = -1;

	// Cuántos chunks ha cruzado el jugador líder.
	int32 PassedChunkCount = 0;

	// Si ya se spawneó el chunk final.
	bool bFinalSpawned = false;

	// El único EndTrigger activo en cada momento.
	// Solo el trigger del chunk más adelantado está conectado → imposible doble-spawn.
	TWeakObjectPtr<UBoxComponent> ActiveEndTrigger;

	// ── Spawning ─────────────────────────────────────────────────────────────

	/** Spawnea el siguiente chunk aleatorio del pool de dificultad actual. */
	void SpawnNextChunk();

	/** Spawnea el chunk final de la carrera. */
	void SpawnFinalChunk();

	/**
	 * Spawnea un actor de chunk y lo alinea para que su InSocket quede en TargetTransform.
	 * Devuelve el actor spawneado, o nullptr si falla.
	 */
	AActor* SpawnAlignedChunk(TSubclassOf<AActor> ChunkClass, const FTransform& TargetTransform);

	/** Elimina los chunks más antiguos para mantener el buffer. */
	void CleanupChunks();

	/** Devuelve el número máximo de chunks activos permitidos. */
	int32 GetKeepAliveCount() const { return KeepAhead + KeepBehind; }

	/** Devuelve el pool de chunks correspondiente a la dificultad actual. */
	ETNChunkDifficulty GetCurrentDifficulty() const;

	/** Selecciona una clase de chunk aleatoria del pool dado, evitando LastSelectedIndex.
	 *  Devuelve nullptr si el pool está vacío. */
	TSubclassOf<AActor> SelectRandomFromPool(const TArray<TSubclassOf<AActor>>& Pool,
	                                          int32& OutSelectedIndex) const;

	// ── Componente helpers ───────────────────────────────────────────────────

	/** Busca un USceneComponent hijo por nombre exacto. */
	static USceneComponent* FindSceneComponentByName(AActor* Actor, FName Name);

	/** Busca un UBoxComponent hijo por nombre exacto. */
	static UBoxComponent* FindBoxComponentByName(AActor* Actor, FName Name);

	// ── Callbacks de overlap ─────────────────────────────────────────────────

	UFUNCTION()
	void OnChunkEndOverlap(UPrimitiveComponent* OverlappedComp,
	                       AActor* OtherActor,
	                       UPrimitiveComponent* OtherComp,
	                       int32 OtherBodyIndex,
	                       bool bFromSweep,
	                       const FHitResult& SweepResult);
};
