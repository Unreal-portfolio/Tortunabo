#pragma once

#include "CoreMinimal.h"
#include "TN_GridTerrainTypes.generated.h"

/**
 * Parámetros del terreno generado sobre el grid. Viven en los Class Defaults de
 * ATN_GridTerrainTile: servidor y clientes leen los mismos valores, no se replican.
 *
 * Los valores por defecto están pensados para celdas de 4000 uu.
 *
 * Invariantes (los valida TNGridTerrain::IsContextValid):
 *   - CorridorHalfWidthMax + BankWidth <= CellSize / 2: pared completa entre dos pasillos
 *     paralelos de celdas vecinas.
 *   - CorridorMeander * sqrt(2) + WallOutlineJitter < CorridorHalfWidthMin: la línea recta
 *     entre centros de celda siempre pisa suelo, por mucho que serpentee el pasillo.
 *   - InteriorLaneHalfWidth * 1.6 + WallOutlineJitter < CorridorHalfWidthMin: el carril
 *     libre de obstáculos cabe entero dentro del pasillo más estrecho.
 *   - InteriorRockHeight < WallHeight / 2.
 */
USTRUCT(BlueprintType)
struct FTNGridTerrainSettings
{
	GENERATED_BODY()

	/** Semiancho del pasillo en los estilos más cerrados (roquedal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Corridor", meta = (ClampMin = "100.0"))
	float CorridorHalfWidthMin = 1100.f;

	/** Semiancho del pasillo en los estilos más abiertos (dunas). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Corridor", meta = (ClampMin = "100.0"))
	float CorridorHalfWidthMax = 1500.f;

	/** Anchura horizontal del talud, de suelo a cresta. Cuanto menor, más vertical la pared. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Walls", meta = (ClampMin = "50.0"))
	float BankWidth = 380.f;

	/** Altura mínima de la cresta sobre el suelo del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Walls", meta = (ClampMin = "100.0"))
	float WallHeight = 800.f;

	/** Cuánto puede morder el contorno irregular de la pared hacia dentro del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Walls", meta = (ClampMin = "0.0"))
	float WallOutlineJitter = 140.f;

	/** Cuánto serpentea el pasillo respecto a la línea recta entre centros de celda.
	 *  Se desvanece cerca del borde del grid para que entrada y salida no se muevan. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Corridor", meta = (ClampMin = "0.0"))
	float CorridorMeander = 380.f;

	/** Semiancho del carril central que los afloramientos de roca nunca invaden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Interior", meta = (ClampMin = "50.0"))
	float InteriorLaneHalfWidth = 280.f;

	/** Amplitud del relieve suave de duna dentro del pasillo. Debe seguir siendo caminable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Interior", meta = (ClampMin = "0.0"))
	float InteriorReliefAmplitude = 70.f;

	/** Altura de los afloramientos de roca del interior. Por debajo de media pared: subirse
	 *  a uno no sirve para saltar a la meseta. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Interior", meta = (ClampMin = "0.0"))
	float InteriorRockHeight = 320.f;

	/** Profundidad de los charcos de marisma dentro del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Interior", meta = (ClampMin = "0.0"))
	float PuddleDepth = 45.f;

	/** Amplitud de la ondulación de arena en el suelo del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Corridor", meta = (ClampMin = "0.0"))
	float FloorRippleAmplitude = 12.f;

	/** Altura extra de las dunas sobre la cresta, fuera del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Filler", meta = (ClampMin = "0.0"))
	float DuneAmplitude = 700.f;

	/** Altura extra de las crestas de roca, fuera del pasillo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Filler", meta = (ClampMin = "0.0"))
	float RockAmplitude = 850.f;

	/** Cota del plano de agua, relativa al generador. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Filler")
	float WaterLevel = -60.f;

	/** Profundidad de las cuencas de marisma por debajo del agua. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Filler", meta = (ClampMin = "0.0"))
	float BasinDepth = 220.f;

	/** Vértices por lado de la malla de cada celda. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Mesh", meta = (ClampMin = "3", ClampMax = "129"))
	int32 VertsPerSide = 97;

	/** Separación vertical de las vetas de estrato que se pintan en las paredes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors", meta = (ClampMin = "50.0"))
	float StrataPeriod = 240.f;

	// Colores en espacio LINEAL: el color de vértice llega al material sin conversión sRGB.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors")
	FLinearColor PathColor = FLinearColor(0.62f, 0.44f, 0.21f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors")
	FLinearColor SandColor = FLinearColor(0.40f, 0.25f, 0.10f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors")
	FLinearColor RockColor = FLinearColor(0.075f, 0.065f, 0.06f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Colors")
	FLinearColor WetSandColor = FLinearColor(0.09f, 0.065f, 0.035f);
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
