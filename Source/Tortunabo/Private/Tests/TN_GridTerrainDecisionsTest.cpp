// Lógica pura del terreno del mapa en grid. Sin mundo, sin actores — se testean las
// funciones de TN_GridTerrainDecisions.h que ATN_GridTerrainTile usa en producción.
// Lo que se protege aquí es JUGABILIDAD: que el pasillo se pueda andar, que las paredes
// no se puedan escalar ni tengan huecos, y que las celdas casen sin costuras.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Terrain; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "World/TN_GridJunkDecisions.h"
#include "World/TN_GridPathDecisions.h"
#include "World/TN_GridTerrainDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 TerrainTestGridSize = 6;
	constexpr double TerrainTestCellSize = 4000.0;
	constexpr int32 TerrainTestSeedCount = 50;

	TArray<FIntPoint> TerrainTestPath(int32 Seed)
	{
		FRandomStream Stream(Seed);
		return TNGridLogic::GeneratePath(TerrainTestGridSize, 10, 20,
			[&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); });
	}

	TNGridTerrain::FTerrainContext TerrainTestContext(int32 Seed, const TArray<FIntPoint>& Path,
		const FTNGridTerrainSettings& Settings = FTNGridTerrainSettings())
	{
		return TNGridTerrain::BuildContext(Seed, TerrainTestGridSize, TerrainTestCellSize, Path, Settings);
	}

	double TanDegrees(double Degrees)
	{
		return FMath::Tan(FMath::DegreesToRadians(Degrees));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Se llega andando de la entrada a la salida, y solo a la salida
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainWalkableRouteTest,
	"Tortunabo.Terrain.WalkableRoute",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainWalkableRouteTest::RunTest(const FString& Parameters)
{
	using namespace TNGridTerrain;

	// Inundación sobre una rejilla de 100 uu desde la entrada, con el mismo criterio que
	// el CharacterMovementComponent: un punto es pisable si la pendiente del terreno ahí
	// está por debajo del ángulo caminable (~45°). El interior del pasillo tiene relieve
	// y rocas, así que esto es lo que de verdad garantiza que el mapa se puede jugar:
	//   - la salida es alcanzable,
	//   - andando no se pasa de media pared (se puede pisar el pie del talud o una roca
	//     baja, pero nunca llegar a la meseta),
	//   - no se sale del grid por ningún sitio que no sea la entrada o la salida.
	constexpr double GridStep = 100.0;
	constexpr int32 RouteSeedCount = 20;
	constexpr double GradientStep = 25.0;
	const double MaxWalkableSlope = TanDegrees(45.0);
	const double Low = -TerrainTestCellSize * 0.5;
	const double High = (TerrainTestGridSize - 0.5) * TerrainTestCellSize;

	for (int32 Seed = 0; Seed < RouteSeedCount; ++Seed)
	{
		const TArray<FIntPoint> Path = TerrainTestPath(Seed);
		const FTerrainContext Context = TerrainTestContext(Seed, Path);
		const FTNGridTerrainSettings& S = Context.Settings;

		const FVector2D Entry(Low, CellCenter(TerrainTestCellSize, Path[0]).Y);
		const FVector2D Exit(High, CellCenter(TerrainTestCellSize, Path.Last()).Y);
		const double OpeningRadius = S.CorridorHalfWidthMax + S.BankWidth;

		auto ToPoint = [GridStep](const FIntPoint& Node) { return FVector2D(Node.X * GridStep, Node.Y * GridStep); };
		const FIntPoint EntryNode(FMath::RoundToInt32(Entry.X / GridStep), FMath::RoundToInt32(Entry.Y / GridStep));
		const FIntPoint ExitNode(FMath::RoundToInt32(Exit.X / GridStep), FMath::RoundToInt32(Exit.Y / GridStep));

		// Los objetos de basura sólidos también cortan el paso: se marcan sus huellas.
		TSet<FIntPoint> BlockedByJunk;
		for (const FIntPoint& Cell : Path)
		{
			for (const TNGridJunk::FJunkInstance& Junk : TNGridJunk::BuildTileJunk(Context, Seed, Cell))
			{
				if (!Junk.bSolid) { continue; }
				const int32 Reach = FMath::CeilToInt32(Junk.Radius / GridStep);
				const FIntPoint JunkNode(FMath::RoundToInt32(Junk.GridPosition.X / GridStep), FMath::RoundToInt32(Junk.GridPosition.Y / GridStep));
				for (int32 DX = -Reach; DX <= Reach; ++DX)
				{
					for (int32 DY = -Reach; DY <= Reach; ++DY)
					{
						const FIntPoint Node = JunkNode + FIntPoint(DX, DY);
						if (FVector2D::Distance(ToPoint(Node), Junk.GridPosition) <= Junk.Radius) { BlockedByJunk.Add(Node); }
					}
				}
			}
		}

		TMap<FIntPoint, double> Reached;
		TArray<FIntPoint> Frontier;
		Reached.Add(EntryNode, SampleHeight(Context, ToPoint(EntryNode)));
		Frontier.Add(EntryNode);

		double HighestReached = 0.0;
		FVector2D HighestPoint = FVector2D::ZeroVector;
		int32 BorderLeaks = 0;
		while (Frontier.Num() > 0)
		{
			const FIntPoint Node = Frontier.Pop(EAllowShrinking::No);
			const double NodeHeight = Reached[Node];
			if (NodeHeight > HighestReached)
			{
				HighestReached = NodeHeight;
				HighestPoint = ToPoint(Node);
			}

			for (const FIntPoint& Offset : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
			{
				const FIntPoint Next = Node + Offset;
				if (Reached.Contains(Next) || BlockedByJunk.Contains(Next)) { continue; }

				const FVector2D P = ToPoint(Next);
				if (P.X < Low || P.X > High || P.Y < Low || P.Y > High)
				{
					const bool bThroughOpening = FVector2D::Distance(P, Entry) <= OpeningRadius || FVector2D::Distance(P, Exit) <= OpeningRadius;
					BorderLeaks += bThroughOpening ? 0 : 1;
					continue;
				}

				const double SlopeX = (SampleHeight(Context, P + FVector2D(GradientStep, 0.0)) - SampleHeight(Context, P - FVector2D(GradientStep, 0.0))) / (2.0 * GradientStep);
				const double SlopeY = (SampleHeight(Context, P + FVector2D(0.0, GradientStep)) - SampleHeight(Context, P - FVector2D(0.0, GradientStep))) / (2.0 * GradientStep);
				if (FMath::Sqrt(SlopeX * SlopeX + SlopeY * SlopeY) >= MaxWalkableSlope) { continue; }

				const double NextHeight = SampleHeight(Context, P);
				if (FMath::Abs(NextHeight - NodeHeight) > GridStep) { continue; }

				Reached.Add(Next, NextHeight);
				Frontier.Add(Next);
			}
		}

		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);
		TestTrue(Ctx + TEXT(": la salida es alcanzable andando desde la entrada"), Reached.Contains(ExitNode));
		const FTerrainSample Top = EvaluateTerrain(Context, HighestPoint);
		TestTrue(Ctx + FString::Printf(TEXT(": andando no se pasa de media pared (max %.0f uu en (%.0f, %.0f), pasillo=%.2f, roca=%.2f)"),
			HighestReached, HighestPoint.X, HighestPoint.Y, Top.CorridorMask, Top.Obstacle),
			HighestReached < S.WallHeight * 0.5);
		TestEqual(Ctx + TEXT(": sin fugas por el perímetro"), BorderLeaks, 0);
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pared completa entre pasillos vecinos no conectados
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainWallBetweenCorridorsTest,
	"Tortunabo.Terrain.WallBetweenCorridors",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainWallBetweenCorridorsTest::RunTest(const FString& Parameters)
{
	using namespace TNGridTerrain;
	int32 PairsChecked = 0;

	for (int32 Seed = 0; Seed < TerrainTestSeedCount; ++Seed)
	{
		const TArray<FIntPoint> Path = TerrainTestPath(Seed);
		const FTerrainContext Context = TerrainTestContext(Seed, Path);
		const double MinWall = Context.Settings.WallHeight * 0.9;

		for (int32 i = 0; i < Path.Num(); ++i)
		{
			for (int32 j = i + 2; j < Path.Num(); ++j)
			{
				const FIntPoint Delta = Path[j] - Path[i];
				if (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) != 1) { continue; }

				// El pasillo serpentea, así que la cresta no cae exactamente en el borde común:
				// lo que importa es que en la recta que une ambos centros haya pared completa.
				const FVector2D From = CellCenter(TerrainTestCellSize, Path[i]);
				const FVector2D To = CellCenter(TerrainTestCellSize, Path[j]);
				const FVector2D Across = (To - From).GetSafeNormal();
				const FVector2D Along(-Across.Y, Across.X);

				// Sin huecos: se cruza de un pasillo al otro cada 100 uu a lo largo del borde
				// común y en TODOS los cruces tiene que haber cresta. Se deja fuera el último
				// 15% de cada extremo: ahí puede estar la punta redondeada de la isla de un giro
				// en U, donde ambos pasillos ya se están uniendo.
				double LowestRidge = TNumericLimits<double>::Max();
				for (double Offset = -0.35 * TerrainTestCellSize; Offset <= 0.35 * TerrainTestCellSize; Offset += 100.0)
				{
					double Ridge = 0.0;
					for (int32 Step = 0; Step <= 160; ++Step)
					{
						Ridge = FMath::Max(Ridge, SampleHeight(Context, FMath::Lerp(From, To, Step / 160.0) + Along * Offset));
					}
					LowestRidge = FMath::Min(LowestRidge, Ridge);
				}
				++PairsChecked;
				TestTrue(FString::Printf(TEXT("semilla %d: pared sin huecos entre celdas %d y %d"), Seed, i, j), LowestRidge >= MinWall);
			}
		}
	}

	TestTrue(TEXT("El muestreo incluye pasillos paralelos"), PairsChecked > 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// El talud no se puede escalar
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainBankSteepTest,
	"Tortunabo.Terrain.BankSteep",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainBankSteepTest::RunTest(const FString& Parameters)
{
	using namespace TNGridTerrain;
	constexpr double RayStep = 10.0;
	constexpr int32 WindowSteps = 30; // 300 uu
	const double HalfCell = TerrainTestCellSize * 0.5;

	for (int32 Seed = 0; Seed < TerrainTestSeedCount; ++Seed)
	{
		const TArray<FIntPoint> Path = TerrainTestPath(Seed);
		const FTerrainContext Context = TerrainTestContext(Seed, Path);

		for (const TNGridLogic::FTNGridCell& Cell : TNGridLogic::ClassifyPath(Path))
		{
			if (Cell.Type != TNGridLogic::ETNGridTileType::Straight) { continue; }

			// Recta a lo largo de X (YawSteps 0) → las paredes quedan a ±Y, y viceversa.
			const FVector2D Side = (Cell.YawSteps == 0) ? FVector2D(0.0, 1.0) : FVector2D(1.0, 0.0);
			const FVector2D Center = CellCenter(TerrainTestCellSize, Cell.Coord);

			for (const double Sign : { -1.0, 1.0 })
			{
				TArray<double> Heights;
				// Hasta algo más allá del borde de la celda: el serpenteo desplaza la cresta.
				for (double Distance = 0.0; Distance <= HalfCell + 500.0; Distance += RayStep)
				{
					Heights.Add(SampleHeight(Context, Center + Side * (Sign * Distance)));
				}

				double BestGain = 0.0;
				for (int32 k = WindowSteps; k < Heights.Num(); ++k)
				{
					BestGain = FMath::Max(BestGain, Heights[k] - Heights[k - WindowSteps]);
				}

				const FString Ctx = FString::Printf(TEXT("semilla %d celda (%d,%d) lado %+.0f"),
					Seed, Cell.Coord.X, Cell.Coord.Y, Sign);
				// 0.75 * 800 en 300 uu = 63°: por encima del ángulo caminable del CMC (~45°).
				TestTrue(Ctx + TEXT(": el talud sube >= 75% de WallHeight en 300 uu"),
					BestGain >= Context.Settings.WallHeight * 0.75);
				TestTrue(Ctx + TEXT(": cresta completa a ese lado del pasillo"),
					FMath::Max(Heights) >= Context.Settings.WallHeight * 0.9);
			}
		}
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// El perímetro está cerrado salvo en la entrada y la salida
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainBorderClosedTest,
	"Tortunabo.Terrain.BorderClosed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainBorderClosedTest::RunTest(const FString& Parameters)
{
	using namespace TNGridTerrain;
	constexpr double SampleStep = 100.0;
	const double Low = -TerrainTestCellSize * 0.5;
	const double High = (TerrainTestGridSize - 0.5) * TerrainTestCellSize;

	for (int32 Seed = 0; Seed < TerrainTestSeedCount; ++Seed)
	{
		const TArray<FIntPoint> Path = TerrainTestPath(Seed);
		const FTerrainContext Context = TerrainTestContext(Seed, Path);
		const FTNGridTerrainSettings& S = Context.Settings;

		const FVector2D Entry(Low, CellCenter(TerrainTestCellSize, Path[0]).Y);
		const FVector2D Exit(High, CellCenter(TerrainTestCellSize, Path.Last()).Y);
		const double OpeningRadius = S.CorridorHalfWidthMax + S.BankWidth;

		bool bClosed = true;
		for (double T = Low; T <= High; T += SampleStep)
		{
			for (const FVector2D& P : { FVector2D(Low, T), FVector2D(High, T), FVector2D(T, Low), FVector2D(T, High) })
			{
				const bool bInOpening = FVector2D::Distance(P, Entry) <= OpeningRadius || FVector2D::Distance(P, Exit) <= OpeningRadius;
				if (!bInOpening && SampleHeight(Context, P) < S.WallHeight * 0.9)
				{
					bClosed = false;
				}
			}
		}

		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);
		TestTrue(Ctx + TEXT(": perímetro cerrado fuera de las aberturas"), bClosed);
		TestTrue(Ctx + TEXT(": la entrada queda a nivel de suelo"), SampleHeight(Context, Entry) < S.InteriorReliefAmplitude * 1.5);
		TestTrue(Ctx + TEXT(": la salida queda a nivel de suelo"), SampleHeight(Context, Exit) < S.InteriorReliefAmplitude * 1.5);
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Las celdas casan sin costuras
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainSeamlessTilesTest,
	"Tortunabo.Terrain.SeamlessTiles",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainSeamlessTilesTest::RunTest(const FString& Parameters)
{
	using namespace TNGridTerrain;

	const FTerrainContext Context = TerrainTestContext(7, TerrainTestPath(7));
	const int32 V = Context.Settings.VertsPerSide;

	const FTileMesh Tile = BuildTileMesh(Context, FIntPoint(2, 2));
	const FTileMesh NextRow = BuildTileMesh(Context, FIntPoint(2, 3));
	const FTileMesh NextCol = BuildTileMesh(Context, FIntPoint(3, 2));

	if (!TestEqual(TEXT("Vértices por celda"), Tile.Vertices.Num(), V * V))
	{
		return false;
	}
	TestEqual(TEXT("Triángulos por celda"), Tile.Triangles.Num(), (V - 1) * (V - 1) * 6);

	// Cara visible hacia arriba. Unreal usa sentido horario como cara frontal, así que
	// el producto vectorial de las aristas debe apuntar a -Z. Con el orden contrario el
	// terreno solo se ve desde abajo (regresión real: primera captura de la demo).
	bool bAllFaceUp = true;
	for (int32 t = 0; t + 2 < Tile.Triangles.Num(); t += 3)
	{
		const FVector& A = Tile.Vertices[Tile.Triangles[t]];
		const FVector& B = Tile.Vertices[Tile.Triangles[t + 1]];
		const FVector& C = Tile.Vertices[Tile.Triangles[t + 2]];
		bAllFaceUp &= FVector::CrossProduct(B - A, C - A).Z < 0.0;
	}
	TestTrue(TEXT("Todos los triángulos miran hacia +Z"), bAllFaceUp);

	double WorstHeight = 0.0;
	double WorstNormal = 0.0;
	double WorstColor = 0.0;
	auto Compare = [&](const FTileMesh& A, int32 IndexA, const FTileMesh& B, int32 IndexB)
	{
		WorstHeight = FMath::Max(WorstHeight, FMath::Abs(A.Vertices[IndexA].Z - B.Vertices[IndexB].Z));
		WorstNormal = FMath::Max(WorstNormal, (A.Normals[IndexA] - B.Normals[IndexB]).Size());
		const FLinearColor ColorDelta = A.Colors[IndexA] - B.Colors[IndexB];
		WorstColor = FMath::Max(WorstColor, static_cast<double>(FMath::Abs(ColorDelta.R) + FMath::Abs(ColorDelta.G) + FMath::Abs(ColorDelta.B)));
	};

	for (int32 k = 0; k < V; ++k)
	{
		Compare(Tile, (V - 1) * V + k, NextRow, k);         // última fila de vértices ↔ primera de la celda Norte
		Compare(Tile, k * V + (V - 1), NextCol, k * V);     // última columna ↔ primera de la celda Este
	}

	TestTrue(TEXT("Alturas idénticas en el borde común"), WorstHeight < 1e-6);
	TestTrue(TEXT("Normales idénticas en el borde común (sin corte de luz)"), WorstNormal < 1e-4);
	TestTrue(TEXT("Colores idénticos en el borde común"), WorstColor < 1e-4);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Variedad, determinismo y validación de parámetros
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainStylesAndDeterminismTest,
	"Tortunabo.Terrain.StylesAndDeterminism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainStylesAndDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace TNGridTerrain;

	for (int32 Seed = 0; Seed < TerrainTestSeedCount; ++Seed)
	{
		const TArray<FIntPoint> Path = TerrainTestPath(Seed);
		FRandomStream Stream(Seed);
		const TArray<int32> Styles = AssignCellStyles(TerrainTestGridSize, Path,
			[&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); });

		bool bNoRepeat = true;
		bool bAllValid = !Styles.ContainsByPredicate([](int32 Style) { return Style < 0 || Style >= NumStyles; });
		for (int32 i = 1; i < Path.Num(); ++i)
		{
			const int32 Current = Styles[Path[i].Y * TerrainTestGridSize + Path[i].X];
			const int32 Previous = Styles[Path[i - 1].Y * TerrainTestGridSize + Path[i - 1].X];
			bNoRepeat &= Current != Previous;
		}

		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);
		TestTrue(Ctx + TEXT(": ningún módulo repite el estilo del anterior"), bNoRepeat);
		TestTrue(Ctx + TEXT(": todas las celdas tienen estilo válido"), bAllValid);
	}

	const TArray<FIntPoint> Path = TerrainTestPath(3);
	const FVector2D Probe(6600.0, 9400.0);
	TestEqual(TEXT("Misma semilla → misma altura"),
		SampleHeight(TerrainTestContext(3, Path), Probe), SampleHeight(TerrainTestContext(3, Path), Probe));
	TestNotEqual(TEXT("Otra semilla de ruido → otra altura"),
		SampleHeight(TerrainTestContext(3, Path), Probe), SampleHeight(TerrainTestContext(4, Path), Probe));

	FTNGridTerrainSettings TooWide;
	TooWide.CorridorHalfWidthMax = 1800.f; // 1800 + 380 > 2000: no cabe la pared
	TestTrue(TEXT("Parámetros por defecto válidos"), IsContextValid(TerrainTestContext(3, Path)));
	TestFalse(TEXT("Pasillo + talud > media celda → contexto inválido"), IsContextValid(TerrainTestContext(3, Path, TooWide)));
	FTNGridTerrainSettings TooWinding;
	TooWinding.CorridorMeander = 800.f; // 800 * sqrt(2) + 140 > 1100: la recta entre centros pisaría pared
	TestFalse(TEXT("Serpenteo mayor que el pasillo → contexto inválido"), IsContextValid(TerrainTestContext(3, Path, TooWinding)));
	FTNGridTerrainSettings LaneTooWide;
	LaneTooWide.InteriorLaneHalfWidth = 700.f; // 700 * 1.6 + 140 > 1100: el carril no cabe en el pasillo estrecho
	TestFalse(TEXT("Carril libre más ancho que el pasillo → contexto inválido"), IsContextValid(TerrainTestContext(3, Path, LaneTooWide)));
	FTNGridTerrainSettings RocksTooTall;
	RocksTooTall.InteriorRockHeight = 500.f; // >= media pared: servirían de escalón hacia la meseta
	TestFalse(TEXT("Rocas del interior >= media pared → contexto inválido"), IsContextValid(TerrainTestContext(3, Path, RocksTooTall)));
	TestFalse(TEXT("Sin camino → contexto inválido"), IsContextValid(TerrainTestContext(3, {})));
	TestEqual(TEXT("Contexto inválido → malla vacía"), BuildTileMesh(TerrainTestContext(3, Path, TooWide), FIntPoint(0, 0)).Vertices.Num(), 0);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Basura: viste la pared sin estorbar el paso ni servir de escalera
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainJunkRulesTest,
	"Tortunabo.Terrain.JunkRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainJunkRulesTest::RunTest(const FString& Parameters)
{
	using namespace TNGridTerrain;
	using namespace TNGridJunk;

	for (int32 Seed = 0; Seed < 10; ++Seed)
	{
		const TArray<FIntPoint> Path = TerrainTestPath(Seed);
		const FTerrainContext Context = TerrainTestContext(Seed, Path);
		const FTNGridTerrainSettings& S = Context.Settings;
		const double HalfCell = TerrainTestCellSize * 0.5;

		int32 SolidCount = 0;
		int32 DecorCount = 0;
		bool bLaneFree = true;
		bool bSolidsLow = true;
		bool bInsideOwnTile = true;

		for (const FIntPoint& Cell : Path)
		{
			const FVector2D Center = CellCenter(TerrainTestCellSize, Cell);
			for (const FJunkInstance& Junk : BuildTileJunk(Context, Seed, Cell))
			{
				const FTerrainSample Sample = EvaluateTerrain(Context, Junk.GridPosition);
				const FVector2D Local = Junk.GridPosition - Center;
				bInsideOwnTile &= FMath::Abs(Local.X) <= HalfCell && FMath::Abs(Local.Y) <= HalfCell;

				if (Junk.bSolid)
				{
					++SolidCount;
					bLaneFree &= Sample.CenterlineDistance - Junk.Radius >= S.InteriorLaneHalfWidth;
					bSolidsLow &= Junk.TopHeight <= S.WallHeight * MaxSolidTopFraction;
				}
				else
				{
					++DecorCount;
				}
			}
		}

		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);
		TestTrue(Ctx + TEXT(": ningún objeto sólido invade el carril libre"), bLaneFree);
		TestTrue(Ctx + TEXT(": ningún objeto sólido pasa del 45% de la pared"), bSolidsLow);
		TestTrue(Ctx + TEXT(": cada objeto pertenece a su celda"), bInsideOwnTile);
		TestTrue(Ctx + FString::Printf(TEXT(": hay fila de objetos sólidos al pie de la pared (%d)"), SolidCount), SolidCount >= Path.Num() * 10);
		TestTrue(Ctx + FString::Printf(TEXT(": hay decorado cubriendo el talud (%d)"), DecorCount), DecorCount >= Path.Num() * 30);
	}

	// Determinismo (servidor y cliente generan lo mismo) y sin duplicados entre celdas vecinas.
	const TArray<FIntPoint> Path = TerrainTestPath(5);
	const FTerrainContext Context = TerrainTestContext(5, Path);
	const TArray<FJunkInstance> First = BuildTileJunk(Context, 5, Path[1]);
	const TArray<FJunkInstance> Again = BuildTileJunk(Context, 5, Path[1]);
	bool bSame = First.Num() == Again.Num();
	for (int32 i = 0; bSame && i < First.Num(); ++i)
	{
		bSame = First[i].GridPosition == Again[i].GridPosition && First[i].bSolid == Again[i].bSolid
			&& First[i].Transform.Equals(Again[i].Transform, 0.0) && First[i].Color == Again[i].Color;
	}
	TestTrue(TEXT("Misma semilla y celda → misma basura"), bSame && First.Num() > 0);

	TSet<FIntPoint> SeenPositions;
	bool bNoDuplicates = true;
	for (const FIntPoint& Cell : { Path[1], Path[2] })
	{
		for (const FJunkInstance& Junk : BuildTileJunk(Context, 5, Cell))
		{
			bool bAlreadySeen = false;
			SeenPositions.Add(FIntPoint(FMath::RoundToInt32(Junk.GridPosition.X), FMath::RoundToInt32(Junk.GridPosition.Y)), &bAlreadySeen);
			bNoDuplicates &= !bAlreadySeen;
		}
	}
	TestTrue(TEXT("Dos celdas vecinas no repiten ningún objeto"), bNoDuplicates);

	FTNGridTerrainSettings NoJunk;
	NoJunk.JunkSpacing = 0.f;
	TestEqual(TEXT("JunkSpacing 0 → sin basura"), BuildTileJunk(TerrainTestContext(5, Path, NoJunk), 5, Path[1]).Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
