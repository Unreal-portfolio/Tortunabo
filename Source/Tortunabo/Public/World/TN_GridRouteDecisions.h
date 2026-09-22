#pragma once

#include "CoreMinimal.h"
#include "World/TN_GridPathDecisions.h"

/**
 * Rutas alternativas a nivel de MAPA, como funciones puras (mismo contrato que
 * TN_GridPathDecisions.h). El camino principal sigue siendo único; encima se trazan
 * desvíos: rutas por celdas libres que salen de una celda del camino y vuelven a él
 * unas celdas más adelante. Así el jugador elige entre dos caminos que llevan al mismo
 * sitio, y las T y cruces de la librería de módulos conectan de verdad en lugar de
 * acabar tapadas con un muro.
 *
 * Una celda de ruta se describe por su máscara de conexiones (bit = 1 << lado, con la
 * numeración de TNGridLogic): qué lados comunican con otra celda de ruta.
 */

namespace TNGridRoutes
{
	constexpr uint8 SideMask(int32 Side) { return static_cast<uint8>(1 << Side); }

	/** Parámetros de los desvíos. Span = celdas de camino principal que salta el desvío. */
	struct FTNDetourParams
	{
		int32 MaxDetours = 2;
		int32 MinSpan = 2;
		int32 MaxSpan = 4;
		/** Celdas máximas del desvío. Más largo que el tramo que salta = ruta lenta pero
		 *  alternativa; nunca más de esto para que no se convierta en otro mapa. */
		int32 MaxLength = 5;
		/** Intentos de anclaje antes de rendirse; acota el coste con grids llenos. */
		int32 MaxAttempts = 24;
	};

	/** Un desvío: celdas propias (sin las de anclaje) y los índices del camino que une. */
	struct FTNDetour
	{
		TArray<FIntPoint> Cells;
		int32 FromIndex = INDEX_NONE;
		int32 ToIndex = INDEX_NONE;
	};

	/** Celda de ruta con sus conexiones. */
	struct FTNRouteCell
	{
		FIntPoint Coord = FIntPoint::ZeroValue;
		uint8 Connections = 0;
		bool bIsStart = false;
		bool bIsEnd = false;
		bool bIsMainPath = false;
	};

	/**
	 * Camino más corto entre From y To por celdas libres (BFS). El orden de vecinos se
	 * baraja con RandRange para que empates distintos den formas distintas.
	 * @return Celdas de From a To inclusive, o vacío si no hay ruta dentro de MaxLength.
	 */
	inline TArray<FIntPoint> FindFreeRoute(int32 GridSize, const FIntPoint& From, const FIntPoint& To,
		const TSet<FIntPoint>& Blocked, int32 MaxLength, TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
	{
		using namespace TNGridLogic;
		if (!IsInsideGrid(GridSize, From) || !IsInsideGrid(GridSize, To)
			|| Blocked.Contains(From) || Blocked.Contains(To) || MaxLength <= 0)
		{
			return {};
		}

		TMap<FIntPoint, FIntPoint> Parent;
		TMap<FIntPoint, int32> Depth;
		TArray<FIntPoint> Frontier;
		Parent.Add(From, From);
		Depth.Add(From, 1);
		Frontier.Add(From);

		for (int32 Head = 0; Head < Frontier.Num(); ++Head)
		{
			const FIntPoint Current = Frontier[Head];
			if (Current == To) { break; }
			if (Depth[Current] >= MaxLength) { continue; }

			int32 Sides[NumSides] = { SideNorth, SideEast, SideSouth, SideWest };
			for (int32 i = NumSides - 1; i > 0; --i)
			{
				Swap(Sides[i], Sides[RandRange(0, i)]);
			}

			for (const int32 Side : Sides)
			{
				const FIntPoint Next = Current + StepForSide(Side);
				if (!IsInsideGrid(GridSize, Next) || Blocked.Contains(Next) || Parent.Contains(Next)) { continue; }
				Parent.Add(Next, Current);
				Depth.Add(Next, Depth[Current] + 1);
				Frontier.Add(Next);
			}
		}

		if (!Parent.Contains(To)) { return {}; }

		TArray<FIntPoint> Route;
		for (FIntPoint Cell = To; ; Cell = Parent[Cell])
		{
			Route.Insert(Cell, 0);
			if (Cell == From) { break; }
		}
		return Route;
	}

	/**
	 * Traza hasta Params.MaxDetours desvíos sobre el camino principal. Cada desvío sale
	 * de Path[FromIndex] por un vecino libre y entra en Path[ToIndex] por otro, con
	 * ToIndex - FromIndex en [MinSpan, MaxSpan]. Los desvíos no comparten celdas entre sí
	 * ni con el camino, y un mismo anclaje no se reutiliza (una celda no pasa de cruz).
	 */
	inline TArray<FTNDetour> PlanDetours(int32 GridSize, const TArray<FIntPoint>& Path,
		const FTNDetourParams& Params, TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
	{
		using namespace TNGridLogic;
		TArray<FTNDetour> Detours;
		const int32 MinSpan = FMath::Max(Params.MinSpan, 2);
		if (Params.MaxDetours <= 0 || Path.Num() <= MinSpan || Params.MaxSpan < MinSpan) { return Detours; }

		TSet<FIntPoint> Occupied(Path);
		TSet<int32> UsedAnchors;

		for (int32 Attempt = 0; Attempt < Params.MaxAttempts && Detours.Num() < Params.MaxDetours; ++Attempt)
		{
			const int32 FromIndex = RandRange(0, Path.Num() - 1 - MinSpan);
			const int32 ToIndex = FMath::Min(FromIndex + RandRange(MinSpan, Params.MaxSpan), Path.Num() - 1);
			if (UsedAnchors.Contains(FromIndex) || UsedAnchors.Contains(ToIndex)) { continue; }

			// Probar las parejas de vecinos libres en orden barajado; vale la primera ruta.
			TArray<FIntPoint> Exits;
			TArray<FIntPoint> Entries;
			for (int32 Side = 0; Side < NumSides; ++Side)
			{
				const FIntPoint A = Path[FromIndex] + StepForSide(Side);
				const FIntPoint B = Path[ToIndex] + StepForSide(Side);
				if (IsInsideGrid(GridSize, A) && !Occupied.Contains(A)) { Exits.Add(A); }
				if (IsInsideGrid(GridSize, B) && !Occupied.Contains(B)) { Entries.Add(B); }
			}
			if (Exits.Num() == 0 || Entries.Num() == 0) { continue; }
			const FIntPoint Exit = Exits[RandRange(0, Exits.Num() - 1)];
			const FIntPoint Entry = Entries[RandRange(0, Entries.Num() - 1)];

			TArray<FIntPoint> Route = FindFreeRoute(GridSize, Exit, Entry, Occupied, Params.MaxLength, RandRange);
			if (Route.Num() == 0) { continue; }

			FTNDetour Detour;
			Detour.Cells = MoveTemp(Route);
			Detour.FromIndex = FromIndex;
			Detour.ToIndex = ToIndex;
			for (const FIntPoint& Cell : Detour.Cells) { Occupied.Add(Cell); }
			UsedAnchors.Add(FromIndex);
			UsedAnchors.Add(ToIndex);
			Detours.Add(MoveTemp(Detour));
		}

		return Detours;
	}

	/**
	 * Celdas de ruta (camino principal primero, en orden, y luego las de cada desvío) con
	 * sus conexiones. Las bocas del grid (Sur del inicio, Norte del final) no cuentan como
	 * conexión: fuera del grid no hay nada.
	 */
	inline TArray<FTNRouteCell> BuildRouteCells(const TArray<FIntPoint>& Path, const TArray<FTNDetour>& Detours)
	{
		using namespace TNGridLogic;
		TArray<FTNRouteCell> Cells;
		TMap<FIntPoint, int32> IndexOf;

		auto AddCell = [&](const FIntPoint& Coord, bool bMain) -> FTNRouteCell&
		{
			if (const int32* Existing = IndexOf.Find(Coord)) { return Cells[*Existing]; }
			FTNRouteCell& Cell = Cells.AddDefaulted_GetRef();
			Cell.Coord = Coord;
			Cell.bIsMainPath = bMain;
			IndexOf.Add(Coord, Cells.Num() - 1);
			return Cell;
		};

		auto Link = [&](const FIntPoint& A, const FIntPoint& B)
		{
			Cells[IndexOf[A]].Connections |= SideMask(SideTowards(A, B));
			Cells[IndexOf[B]].Connections |= SideMask(SideTowards(B, A));
		};

		for (int32 i = 0; i < Path.Num(); ++i)
		{
			FTNRouteCell& Cell = AddCell(Path[i], true);
			Cell.bIsStart = i == 0;
			Cell.bIsEnd = i == Path.Num() - 1;
			if (i > 0) { Link(Path[i - 1], Path[i]); }
		}

		for (const FTNDetour& Detour : Detours)
		{
			if (Detour.Cells.Num() == 0 || !Path.IsValidIndex(Detour.FromIndex) || !Path.IsValidIndex(Detour.ToIndex)) { continue; }
			for (const FIntPoint& Coord : Detour.Cells) { AddCell(Coord, false); }
			Link(Path[Detour.FromIndex], Detour.Cells[0]);
			for (int32 i = 1; i < Detour.Cells.Num(); ++i) { Link(Detour.Cells[i - 1], Detour.Cells[i]); }
			Link(Detour.Cells.Last(), Path[Detour.ToIndex]);
		}

		return Cells;
	}

	/**
	 * Salidas que debe ofrecer el módulo de una celda: sus conexiones más la boca virtual
	 * del grid (Sur en el inicio, Norte en el final), que orienta el módulo hacia fuera
	 * aunque luego se tape.
	 */
	inline uint8 RequiredExits(const FTNRouteCell& Cell)
	{
		uint8 Mask = Cell.Connections;
		if (Cell.bIsStart) { Mask |= SideMask(TNGridLogic::SideSouth); }
		if (Cell.bIsEnd)   { Mask |= SideMask(TNGridLogic::SideNorth); }
		return Mask;
	}
}
