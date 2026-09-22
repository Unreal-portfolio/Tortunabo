// Lógica pura de los desvíos del mapa en grid: rutas alternativas que salen del camino
// principal y vuelven a él. Se testean las funciones de TN_GridRouteDecisions.h que
// ATN_GridMapGenerator usa en producción (RNG inyectado). Correr desde Session Frontend
// (categoría "Tortunabo.Grid.Routes") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Grid; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "World/TN_GridPathDecisions.h"
#include "World/TN_GridRouteDecisions.h"
#include "World/TN_TerrainModuleDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 RouteTestGridSize = 6;
	constexpr int32 RouteTestSeedCount = 200;

	struct FRouteTestMap
	{
		TArray<FIntPoint> Path;
		TArray<TNGridRoutes::FTNDetour> Detours;
	};

	FRouteTestMap GenerateRouteMap(int32 Seed, const TNGridRoutes::FTNDetourParams& Params)
	{
		FRandomStream Stream(Seed);
		auto RandRange = [&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); };
		FRouteTestMap Map;
		Map.Path = TNGridLogic::GeneratePath(RouteTestGridSize, 10, 20, RandRange);
		Map.Detours = TNGridRoutes::PlanDetours(RouteTestGridSize, Map.Path, Params, RandRange);
		return Map;
	}

	bool AreNeighbours(const FIntPoint& A, const FIntPoint& B)
	{
		return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) == 1;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Invariantes de los desvíos sobre muchas semillas
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNGridDetourInvariantsTest,
	"Tortunabo.Grid.Routes.DetourInvariants",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNGridDetourInvariantsTest::RunTest(const FString& Parameters)
{
	const TNGridRoutes::FTNDetourParams Params;
	int32 SeedsWithDetour = 0;

	for (int32 Seed = 0; Seed < RouteTestSeedCount; ++Seed)
	{
		const FRouteTestMap Map = GenerateRouteMap(Seed, Params);
		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);
		if (!TestTrue(Ctx + TEXT(": hay camino"), Map.Path.Num() > 0)) { continue; }

		TestTrue(Ctx + TEXT(": no pasa de MaxDetours"), Map.Detours.Num() <= Params.MaxDetours);
		SeedsWithDetour += Map.Detours.Num() > 0 ? 1 : 0;

		TSet<FIntPoint> Taken(Map.Path);
		TSet<int32> Anchors;
		for (const TNGridRoutes::FTNDetour& Detour : Map.Detours)
		{
			const int32 Span = Detour.ToIndex - Detour.FromIndex;
			TestTrue(Ctx + TEXT(": salto dentro de [MinSpan, MaxSpan]"), Span >= Params.MinSpan && Span <= Params.MaxSpan);
			TestTrue(Ctx + TEXT(": longitud dentro de [1, MaxLength]"),
				Detour.Cells.Num() >= 1 && Detour.Cells.Num() <= Params.MaxLength);
			TestFalse(Ctx + TEXT(": anclaje de salida sin reutilizar"), Anchors.Contains(Detour.FromIndex));
			TestFalse(Ctx + TEXT(": anclaje de entrada sin reutilizar"), Anchors.Contains(Detour.ToIndex));
			Anchors.Add(Detour.FromIndex);
			Anchors.Add(Detour.ToIndex);

			if (Detour.Cells.Num() == 0) { continue; }
			TestTrue(Ctx + TEXT(": sale de su anclaje"), AreNeighbours(Map.Path[Detour.FromIndex], Detour.Cells[0]));
			TestTrue(Ctx + TEXT(": entra en su anclaje"), AreNeighbours(Detour.Cells.Last(), Map.Path[Detour.ToIndex]));

			for (int32 i = 0; i < Detour.Cells.Num(); ++i)
			{
				const FIntPoint& Cell = Detour.Cells[i];
				TestTrue(Ctx + TEXT(": dentro del grid"), TNGridLogic::IsInsideGrid(RouteTestGridSize, Cell));
				TestFalse(Ctx + TEXT(": no pisa camino ni otro desvío"), Taken.Contains(Cell));
				Taken.Add(Cell);
				if (i > 0) { TestTrue(Ctx + TEXT(": contiguo"), AreNeighbours(Detour.Cells[i - 1], Cell)); }
			}
		}
	}

	// Con grid 6x6 y camino de 10-20 celdas casi siempre queda sitio; si esto baja, el
	// planificador ha dejado de encontrar rutas y el mapa vuelve a ser un pasillo.
	TestTrue(FString::Printf(TEXT("al menos la mitad de las semillas tienen desvío (%d/%d)"), SeedsWithDetour, RouteTestSeedCount),
		SeedsWithDetour * 2 >= RouteTestSeedCount);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Conexiones de las celdas de ruta
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNGridRouteCellsTest,
	"Tortunabo.Grid.Routes.RouteCells",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNGridRouteCellsTest::RunTest(const FString& Parameters)
{
	using namespace TNGridRoutes;
	const FTNDetourParams Params;

	for (int32 Seed = 0; Seed < RouteTestSeedCount; ++Seed)
	{
		const FRouteTestMap Map = GenerateRouteMap(Seed, Params);
		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);
		if (Map.Path.Num() == 0) { continue; }

		const TArray<FTNRouteCell> Cells = BuildRouteCells(Map.Path, Map.Detours);
		TMap<FIntPoint, uint8> ConnectionsOf;
		for (const FTNRouteCell& Cell : Cells) { ConnectionsOf.Add(Cell.Coord, Cell.Connections); }

		int32 ExpectedCells = Map.Path.Num();
		for (const FTNDetour& Detour : Map.Detours) { ExpectedCells += Detour.Cells.Num(); }
		TestEqual(Ctx + TEXT(": una celda por casilla de ruta"), Cells.Num(), ExpectedCells);

		for (const FTNRouteCell& Cell : Cells)
		{
			for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
			{
				if (!(Cell.Connections & SideMask(Side))) { continue; }
				const FIntPoint Other = Cell.Coord + TNGridLogic::StepForSide(Side);
				const uint8* OtherMask = ConnectionsOf.Find(Other);
				TestTrue(Ctx + TEXT(": conexión simétrica"),
					OtherMask && (*OtherMask & SideMask(TNGridLogic::OppositeSide(Side))));
			}

			if (!Cell.bIsMainPath)
			{
				TestEqual(Ctx + TEXT(": celda de desvío con dos conexiones"), FMath::CountBits(Cell.Connections), 2);
			}
			TestTrue(Ctx + TEXT(": salidas pedidas cubribles por alguna topología"),
				FMath::CountBits(RequiredExits(Cell)) >= 2 && FMath::CountBits(RequiredExits(Cell)) <= 4);
		}

		// Todo es alcanzable desde el inicio siguiendo conexiones.
		TSet<FIntPoint> Reached;
		TArray<FIntPoint> Open = { Map.Path[0] };
		while (Open.Num() > 0)
		{
			const FIntPoint Current = Open.Pop();
			if (Reached.Contains(Current)) { continue; }
			Reached.Add(Current);
			for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
			{
				if (ConnectionsOf[Current] & SideMask(Side)) { Open.Add(Current + TNGridLogic::StepForSide(Side)); }
			}
		}
		TestEqual(Ctx + TEXT(": ruta conexa"), Reached.Num(), Cells.Num());
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sin desvíos, las salidas coinciden con las del camino clasificado de siempre
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNGridRouteCompatTest,
	"Tortunabo.Grid.Routes.MatchesClassifiedPath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNGridRouteCompatTest::RunTest(const FString& Parameters)
{
	TNGridRoutes::FTNDetourParams NoDetours;
	NoDetours.MaxDetours = 0;

	for (int32 Seed = 0; Seed < RouteTestSeedCount; ++Seed)
	{
		const FRouteTestMap Map = GenerateRouteMap(Seed, NoDetours);
		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);
		TestEqual(Ctx + TEXT(": sin desvíos"), Map.Detours.Num(), 0);

		const TArray<TNGridLogic::FTNGridCell> Classified = TNGridLogic::ClassifyPath(Map.Path);
		const TArray<TNGridRoutes::FTNRouteCell> Cells = TNGridRoutes::BuildRouteCells(Map.Path, Map.Detours);
		if (!TestEqual(Ctx + TEXT(": misma cantidad de celdas"), Cells.Num(), Classified.Num())) { continue; }

		for (int32 i = 0; i < Cells.Num(); ++i)
		{
			TestEqual(Ctx + TEXT(": salidas pedidas = RequiredExitMask"),
				TNGridRoutes::RequiredExits(Cells[i]), TNTerrainModule::RequiredExitMask(Classified[i]));
			TestEqual(Ctx + TEXT(": conexiones = ConnectedExitMask"),
				Cells[i].Connections, TNTerrainModule::ConnectedExitMask(Classified, i));
		}
	}

	TestTrue(TEXT("determinista"), [&]()
	{
		const TNGridRoutes::FTNDetourParams Params;
		const FRouteTestMap A = GenerateRouteMap(4242, Params);
		const FRouteTestMap B = GenerateRouteMap(4242, Params);
		if (A.Path != B.Path || A.Detours.Num() != B.Detours.Num()) { return false; }
		for (int32 i = 0; i < A.Detours.Num(); ++i)
		{
			if (A.Detours[i].Cells != B.Detours[i].Cells) { return false; }
		}
		return true;
	}());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
