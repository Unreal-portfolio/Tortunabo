#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_GridMapGenerator.generated.h"

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
 * Alcance actual: demo. No replica ni se integra con ATN_ChunkManager. Misma semilla →
 * mismo mapa, de modo que en red bastará con replicar la semilla.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_GridMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	ATN_GridMapGenerator();

	virtual void BeginPlay() override;

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

	/** Lado de una celda en unidades de Unreal. Debe coincidir con el tamaño de los tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GridMap", meta = (ClampMin = "100.0"))
	float CellSize = 2000.f;

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

private:
	AActor* SpawnTile(TSubclassOf<AActor> TileClass, FIntPoint Cell, int32 YawSteps);

	void DrawPathDebug(const TArray<FIntPoint>& Path) const;

	/** Tag de los tiles generados. Clear() los localiza por tag y no por una lista propia:
	 *  en editor el generador se reconstruye al tocar propiedades y la lista se perdería. */
	static const FName GeneratedTileTag;
};
