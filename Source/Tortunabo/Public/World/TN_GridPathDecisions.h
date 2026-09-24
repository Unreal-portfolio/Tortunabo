#pragma once

#include "CoreMinimal.h"

/**
 * Generación del mapa en grid como funciones PURAS: sin UWorld, sin actores, sin
 * estado — mismo contrato que TN_ChunkDecisions.h. ATN_GridMapGenerator delega aquí
 * para que lo que cubre el Automation framework sea EXACTAMENTE lo que corre en juego.
 *
 * Modelo: grid cuadrado, un único camino sin ramas hecho de rectas y giros. El camino
 * entra por la fila 0 y sale por la última fila.
 *
 * Convenciones:
 *  - FIntPoint: X = columna, Y = fila.
 *  - Lados de una celda como enteros 0..3 (Norte, Este, Sur, Oeste), con Norte = +fila.
 *    El valor coincide con los pasos de yaw de 90° que usa el generador al colocar tiles.
 *  - Tile recto canónico (YawSteps 0): abierto por Sur y Norte.
 *  - Tile de giro canónico (YawSteps 0): abierto por Sur y Este. Rotado 0..3 veces
 *    cubre los cuatro giros posibles.
 *
 * La aleatoriedad se INYECTA como functor (Min, Max) → int32 en [Min, Max] inclusive.
 */

namespace TNGridLogic
{
	enum class ETNGridTileType : uint8
	{
		Empty,
		Straight,
		Turn
	};

	/** Celda de camino ya clasificada: qué tile lleva y con qué rotación. */
	struct FTNGridCell
	{
		FIntPoint Coord = FIntPoint::ZeroValue;
		ETNGridTileType Type = ETNGridTileType::Empty;
		int32 YawSteps = 0;
		bool bIsStart = false;
		bool bIsEnd = false;
	};

	constexpr int32 SideNorth = 0;
	constexpr int32 SideEast  = 1;
	constexpr int32 SideSouth = 2;
	constexpr int32 SideWest  = 3;
	constexpr int32 NumSides  = 4;

	/** Tope de nodos visitados por búsqueda: garantiza que GeneratePath nunca cuelga. */
	constexpr int32 MaxSearchVisits = 200000;

	inline FIntPoint StepForSide(int32 Side)
	{
		switch (Side)
		{
			case SideNorth: return FIntPoint(0, 1);
			case SideEast:  return FIntPoint(1, 0);
			case SideSouth: return FIntPoint(0, -1);
			default:        return FIntPoint(-1, 0);
		}
	}

	inline int32 OppositeSide(int32 Side)
	{
		return (Side + 2) % NumSides;
	}

	/** Lado de `From` que mira a `To`. Precondición: celdas vecinas ortogonales. */
	inline int32 SideTowards(const FIntPoint& From, const FIntPoint& To)
	{
		const FIntPoint Delta = To - From;
		if (Delta.Y > 0) { return SideNorth; }
		if (Delta.X > 0) { return SideEast; }
		if (Delta.Y < 0) { return SideSouth; }
		return SideWest;
	}

	inline bool IsInsideGrid(int32 GridSize, const FIntPoint& Cell)
	{
		return Cell.X >= 0 && Cell.X < GridSize && Cell.Y >= 0 && Cell.Y < GridSize;
	}

	/** Índice lineal de una celda (fila * GridSize + columna). */
	inline int32 CellIndex(int32 GridSize, const FIntPoint& Cell)
	{
		return Cell.Y * GridSize + Cell.X;
	}

	/** Estado de una búsqueda en curso. Vive solo dentro de GeneratePath. */
	struct FPathSearch
	{
		int32 GridSize = 0;
		int32 AcceptFromLength = 0;
		int32 MaxLength = 0;
		int32 VisitsLeft = MaxSearchVisits;
		TArray<FIntPoint> Path;
		TArray<bool> Visited;
	};

	/** DFS con backtracking. true = Search.Path es un camino completo y válido. */
	inline bool ExtendPath(FPathSearch& Search, TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
	{
		if (--Search.VisitsLeft < 0) { return false; }

		const FIntPoint Current = Search.Path.Last();
		const int32 LastRow = Search.GridSize - 1;

		if (Current.Y == LastRow && Search.Path.Num() >= Search.AcceptFromLength) { return true; }
		if (Search.Path.Num() >= Search.MaxLength) { return false; }

		int32 Sides[NumSides] = { SideNorth, SideEast, SideSouth, SideWest };
		for (int32 i = NumSides - 1; i > 0; --i)
		{
			Swap(Sides[i], Sides[RandRange(0, i)]);
		}

		for (const int32 Side : Sides)
		{
			const FIntPoint Next = Current + StepForSide(Side);
			if (!IsInsideGrid(Search.GridSize, Next) || Search.Visited[CellIndex(Search.GridSize, Next)])
			{
				continue;
			}

			// Poda: desde Next ya no se llega a la última fila sin pasarse de MaxLength.
			const int32 CellsLeftAfterNext = Search.MaxLength - (Search.Path.Num() + 1);
			if (LastRow - Next.Y > CellsLeftAfterNext) { continue; }

			Search.Path.Add(Next);
			Search.Visited[CellIndex(Search.GridSize, Next)] = true;

			if (ExtendPath(Search, RandRange)) { return true; }

			Search.Visited[CellIndex(Search.GridSize, Next)] = false;
			Search.Path.Pop();
		}

		return false;
	}

	/**
	 * Camino único de la fila 0 a la última fila, sin celdas repetidas, con longitud
	 * (en celdas) dentro de [MinLength, MaxLength].
	 * @return Array vacío si los parámetros son imposibles o la búsqueda agota su tope;
	 *         el llamante decide cómo reportarlo. Nunca se devuelve un camino inválido.
	 */
	inline TArray<FIntPoint> GeneratePath(int32 GridSize, int32 MinLength, int32 MaxLength,
		TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
	{
		const int32 NumCells = GridSize * GridSize;
		if (GridSize <= 0 || MinLength > MaxLength || MaxLength < GridSize || MinLength > NumCells)
		{
			return {};
		}

		FPathSearch Search;
		Search.GridSize = GridSize;
		Search.MaxLength = FMath::Min(MaxLength, NumCells);
		// Longitud objetivo sorteada dentro del rango: sin esto el DFS acepta siempre el
		// primer camino que alcanza MinLength y todos los mapas salen igual de cortos.
		Search.AcceptFromLength = RandRange(FMath::Max(MinLength, GridSize), Search.MaxLength);
		Search.Visited.Init(false, NumCells);

		const FIntPoint Start(RandRange(0, GridSize - 1), 0);
		Search.Path.Add(Start);
		Search.Visited[CellIndex(GridSize, Start)] = true;

		if (!ExtendPath(Search, RandRange))
		{
			return {};
		}
		return Search.Path;
	}

	/**
	 * Asigna tile y rotación a cada celda del camino. La primera celda entra desde fuera
	 * por su lado Sur y la última sale por su lado Norte.
	 */
	inline TArray<FTNGridCell> ClassifyPath(const TArray<FIntPoint>& Path)
	{
		TArray<FTNGridCell> Cells;
		Cells.Reserve(Path.Num());

		for (int32 i = 0; i < Path.Num(); ++i)
		{
			const bool bIsStart = i == 0;
			const bool bIsEnd = i == Path.Num() - 1;
			const int32 EntrySide = bIsStart ? SideSouth : SideTowards(Path[i], Path[i - 1]);
			const int32 ExitSide = bIsEnd ? SideNorth : SideTowards(Path[i], Path[i + 1]);

			FTNGridCell Cell;
			Cell.Coord = Path[i];
			Cell.bIsStart = bIsStart;
			Cell.bIsEnd = bIsEnd;

			if (ExitSide == OppositeSide(EntrySide))
			{
				Cell.Type = ETNGridTileType::Straight;
				Cell.YawSteps = ExitSide % 2;
			}
			else
			{
				// Los dos lados abiertos de un giro son consecutivos; el canónico abre
				// {Este, Sur}, así que la rotación es la distancia del menor a Este.
				const int32 LowSide = ((EntrySide + 1) % NumSides == ExitSide) ? EntrySide : ExitSide;
				Cell.Type = ETNGridTileType::Turn;
				Cell.YawSteps = (LowSide - SideEast + NumSides) % NumSides;
			}

			Cells.Add(Cell);
		}

		return Cells;
	}

	/**
	 * Índice de tipo de relleno por celda (indexado con CellIndex). Las celdas de camino
	 * quedan en INDEX_NONE; también todas si NumFillerTypes <= 0.
	 */
	inline TArray<int32> BuildFillerMap(int32 GridSize, const TArray<FIntPoint>& Path,
		int32 NumFillerTypes, TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
	{
		TArray<int32> Fillers;
		if (GridSize <= 0) { return Fillers; }

		Fillers.Init(INDEX_NONE, GridSize * GridSize);
		if (NumFillerTypes <= 0) { return Fillers; }

		const TSet<FIntPoint> PathCells(Path);
		for (int32 Row = 0; Row < GridSize; ++Row)
		{
			for (int32 Col = 0; Col < GridSize; ++Col)
			{
				const FIntPoint Cell(Col, Row);
				if (!PathCells.Contains(Cell))
				{
					Fillers[CellIndex(GridSize, Cell)] = RandRange(0, NumFillerTypes - 1);
				}
			}
		}

		return Fillers;
	}
}
