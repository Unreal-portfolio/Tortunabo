// Lógica pura del generador de mapa en grid: camino único de rectas y giros.
// Sin mundo, sin actores — se testean las funciones de TN_GridPathDecisions.h que
// ATN_GridMapGenerator usa en producción (RNG inyectado). Correr desde Session
// Frontend (categoría "Tortunabo.Grid") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Grid; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "World/TN_GridPathDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 GridTestSize = 6;
	constexpr int32 GridTestMinLength = 10;
	constexpr int32 GridTestMaxLength = 20;
	constexpr int32 GridTestSeedCount = 200;

	TArray<FIntPoint> GeneratePathForSeed(int32 Seed, int32 GridSize = GridTestSize,
		int32 MinLength = GridTestMinLength, int32 MaxLength = GridTestMaxLength)
	{
		FRandomStream Stream(Seed);
		return TNGridLogic::GeneratePath(GridSize, MinLength, MaxLength,
			[&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); });
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Invariantes del camino sobre muchas semillas
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNGridPathInvariantsTest,
	"Tortunabo.Grid.PathInvariants",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNGridPathInvariantsTest::RunTest(const FString& Parameters)
{
	for (int32 Seed = 0; Seed < GridTestSeedCount; ++Seed)
	{
		const TArray<FIntPoint> Path = GeneratePathForSeed(Seed);
		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);

		if (!TestTrue(Ctx + TEXT(": genera camino"), Path.Num() > 0))
		{
			continue;
		}

		TestEqual(Ctx + TEXT(": empieza en la fila 0"), Path[0].Y, 0);
		TestEqual(Ctx + TEXT(": acaba en la última fila"), Path.Last().Y, GridTestSize - 1);
		TestTrue(Ctx + TEXT(": longitud dentro de [Min, Max]"),
			Path.Num() >= GridTestMinLength && Path.Num() <= GridTestMaxLength);

		TSet<FIntPoint> Visited;
		bool bAllInside = true;
		bool bContiguous = true;
		for (int32 i = 0; i < Path.Num(); ++i)
		{
			const FIntPoint& Cell = Path[i];
			bAllInside &= Cell.X >= 0 && Cell.X < GridTestSize && Cell.Y >= 0 && Cell.Y < GridTestSize;
			Visited.Add(Cell);

			if (i > 0)
			{
				const FIntPoint Delta = Cell - Path[i - 1];
				bContiguous &= (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y)) == 1;
			}
		}

		TestTrue(Ctx + TEXT(": todas las celdas dentro del grid"), bAllInside);
		TestTrue(Ctx + TEXT(": cada paso es vecino ortogonal del anterior"), bContiguous);
		TestEqual(Ctx + TEXT(": sin celdas repetidas"), Visited.Num(), Path.Num());
	}

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Determinismo y variedad
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNGridPathDeterminismTest,
	"Tortunabo.Grid.PathDeterminism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNGridPathDeterminismTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Misma semilla → mismo camino"),
		GeneratePathForSeed(1234) == GeneratePathForSeed(1234));

	int32 DistinctFromFirst = 0;
	const TArray<FIntPoint> First = GeneratePathForSeed(0);
	for (int32 Seed = 1; Seed < 20; ++Seed)
	{
		if (GeneratePathForSeed(Seed) != First)
		{
			++DistinctFromFirst;
		}
	}
	TestTrue(TEXT("Semillas distintas producen caminos distintos"), DistinctFromFirst > 0);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parámetros imposibles
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNGridPathInvalidParamsTest,
	"Tortunabo.Grid.PathInvalidParams",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNGridPathInvalidParamsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Min > GridSize² → vacío"), GeneratePathForSeed(1, 6, 37, 40).Num(), 0);
	TestEqual(TEXT("Max < GridSize (no llega al otro borde) → vacío"), GeneratePathForSeed(1, 6, 1, 5).Num(), 0);
	TestEqual(TEXT("Min > Max → vacío"), GeneratePathForSeed(1, 6, 12, 10).Num(), 0);
	TestEqual(TEXT("GridSize <= 0 → vacío"), GeneratePathForSeed(1, 0, 1, 1).Num(), 0);

	// Borde válido: el camino más corto posible es una columna recta.
	const TArray<FIntPoint> Shortest = GeneratePathForSeed(1, 6, 6, 6);
	TestEqual(TEXT("Min == Max == GridSize → columna recta de 6 celdas"), Shortest.Num(), 6);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Clasificación de tiles (recta / giro + rotación)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNGridClassifyPathTest,
	"Tortunabo.Grid.ClassifyPath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNGridClassifyPathTest::RunTest(const FString& Parameters)
{
	using namespace TNGridLogic;

	// Camino en S que ejerce las 2 rectas y los 4 giros. X = columna, Y = fila.
	//   fila 3:  (2,3) fin
	//   fila 2:  (0,2) (1,2) (2,2)
	//   fila 1:  (0,1) (1,1) (2,1)
	//   fila 0:                (2,0) inicio
	const TArray<FIntPoint> Path = {
		{2, 0}, {2, 1}, {1, 1}, {0, 1}, {0, 2}, {1, 2}, {2, 2}, {2, 3}
	};
	const TArray<FTNGridCell> Cells = ClassifyPath(Path);

	if (!TestEqual(TEXT("Una celda clasificada por celda de camino"), Cells.Num(), Path.Num()))
	{
		return false;
	}

	struct FExpected { ETNGridTileType Type; int32 YawSteps; };
	const FExpected Expected[] = {
		{ ETNGridTileType::Straight, 0 }, // entra por Sur, sale por Norte
		{ ETNGridTileType::Turn,     1 }, // Sur  → Oeste
		{ ETNGridTileType::Straight, 1 }, // Este → Oeste
		{ ETNGridTileType::Turn,     3 }, // Este → Norte
		{ ETNGridTileType::Turn,     0 }, // Sur  → Este
		{ ETNGridTileType::Straight, 1 }, // Oeste → Este
		{ ETNGridTileType::Turn,     2 }, // Oeste → Norte
		{ ETNGridTileType::Straight, 0 }, // Sur  → Norte (fin)
	};

	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		const FString Ctx = FString::Printf(TEXT("celda %d"), i);
		TestTrue(Ctx + TEXT(": coordenada preservada"), Cells[i].Coord == Path[i]);
		TestTrue(Ctx + TEXT(": tipo de tile"), Cells[i].Type == Expected[i].Type);
		TestEqual(Ctx + TEXT(": rotación"), Cells[i].YawSteps, Expected[i].YawSteps);
		TestTrue(Ctx + TEXT(": flag de inicio"), Cells[i].bIsStart == (i == 0));
		TestTrue(Ctx + TEXT(": flag de fin"), Cells[i].bIsEnd == (i == Cells.Num() - 1));
	}

	TestEqual(TEXT("Camino vacío → sin celdas"), ClassifyPath({}).Num(), 0);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Mapa de relleno
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNGridFillerMapTest,
	"Tortunabo.Grid.FillerMap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNGridFillerMapTest::RunTest(const FString& Parameters)
{
	using namespace TNGridLogic;

	constexpr int32 NumFillerTypes = 3;
	const TArray<FIntPoint> Path = GeneratePathForSeed(42);
	FRandomStream Stream(42);
	const TArray<int32> Fillers = BuildFillerMap(GridTestSize, Path, NumFillerTypes,
		[&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); });

	if (!TestEqual(TEXT("Una entrada por celda del grid"), Fillers.Num(), GridTestSize * GridTestSize))
	{
		return false;
	}

	const TSet<FIntPoint> PathCells(Path);
	bool bPathCellsEmpty = true;
	bool bEmptyCellsFilled = true;
	for (int32 Row = 0; Row < GridTestSize; ++Row)
	{
		for (int32 Col = 0; Col < GridTestSize; ++Col)
		{
			const int32 Filler = Fillers[CellIndex(GridTestSize, FIntPoint(Col, Row))];
			if (PathCells.Contains(FIntPoint(Col, Row)))
			{
				bPathCellsEmpty &= Filler == INDEX_NONE;
			}
			else
			{
				bEmptyCellsFilled &= Filler >= 0 && Filler < NumFillerTypes;
			}
		}
	}

	TestTrue(TEXT("Ninguna celda de camino recibe relleno"), bPathCellsEmpty);
	TestTrue(TEXT("Toda celda vacía recibe un relleno válido"), bEmptyCellsFilled);

	const TArray<int32> NoTypes = BuildFillerMap(GridTestSize, Path, 0,
		[](int32 Min, int32) { return Min; });
	TestFalse(TEXT("Sin tipos de relleno → todo INDEX_NONE"),
		NoTypes.ContainsByPredicate([](int32 Value) { return Value != INDEX_NONE; }));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
