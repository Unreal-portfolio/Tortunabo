#pragma once

#include "CoreMinimal.h"
#include "TN_GridTerrainTypes.generated.h"

/**
 * Parámetros del terreno generado sobre el grid. Viven en los Class Defaults de
 * ATN_GridTerrainTile: servidor y clientes leen los mismos valores, no se replican.
 *
 * Invariante: CorridorHalfWidthMax + BankWidth <= CellSize / 2. Es lo que garantiza
 * pared completa entre dos pasillos paralelos de celdas vecinas.
 */
USTRUCT(BlueprintType)
struct FTNGridTerrainSettings
{
	GENERATED_BODY()

	/** Semiancho del pasillo en los estilos más cerrados (roquedal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Corridor", meta = (ClampMin = "100.0"))
	float CorridorHalfWidthMin = 550.f;

	/** Semiancho del pasillo en los estilos más abiertos (dunas). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Corridor", meta = (ClampMin = "100.0"))
	float CorridorHalfWidthMax = 750.f;

	/** Anchura horizontal del talud, de suelo a cresta. Cuanto menor, más vertical la pared. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Walls", meta = (ClampMin = "50.0"))
	float BankWidth = 250.f;

	/** Altura mínima de la cresta sobre el suelo del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Walls", meta = (ClampMin = "100.0"))
	float WallHeight = 600.f;

	/** Cuánto puede morder el contorno irregular de la pared hacia dentro del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Walls", meta = (ClampMin = "0.0"))
	float WallOutlineJitter = 80.f;

	/** Amplitud de la ondulación de arena en el suelo del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Corridor", meta = (ClampMin = "0.0"))
	float FloorRippleAmplitude = 12.f;

	/** Altura extra de las dunas sobre la cresta, fuera del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Filler", meta = (ClampMin = "0.0"))
	float DuneAmplitude = 450.f;

	/** Altura extra de las crestas de roca, fuera del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Filler", meta = (ClampMin = "0.0"))
	float RockAmplitude = 550.f;

	/** Cota del plano de agua, relativa al generador. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Filler")
	float WaterLevel = -60.f;

	/** Profundidad de las cuencas de marisma por debajo del agua. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Filler", meta = (ClampMin = "0.0"))
	float BasinDepth = 180.f;

	/** Vértices por lado de la malla de cada celda. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Mesh", meta = (ClampMin = "3", ClampMax = "129"))
	int32 VertsPerSide = 49;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors")
	FLinearColor PathColor = FLinearColor(0.86f, 0.72f, 0.46f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors")
	FLinearColor SandColor = FLinearColor(0.70f, 0.54f, 0.30f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors")
	FLinearColor RockColor = FLinearColor(0.23f, 0.22f, 0.22f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors")
	FLinearColor WetSandColor = FLinearColor(0.30f, 0.24f, 0.15f);
};

/**
 * Todo lo que un tile necesita para construir su trozo de terreno. Se replica una sola
 * vez (COND_InitialOnly). Lleva el camino entero para no depender de que el cliente
 * regenere el mismo a partir de la semilla.
 */
USTRUCT()
struct FTNGridTileInit
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Seed = 0;

	UPROPERTY()
	int32 GridSize = 0;

	UPROPERTY()
	float CellSize = 0.f;

	/** X = columna, Y = fila. */
	UPROPERTY()
	FIntPoint Coord = FIntPoint::ZeroValue;

	UPROPERTY()
	TArray<FIntPoint> Path;
};
