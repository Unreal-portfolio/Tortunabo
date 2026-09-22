// Lógica pura de los módulos de terreno. Sin mundo, sin actores — se testean las
// funciones de TN_TerrainModuleDecisions.h que ATN_TerrainModuleTile y
// ATN_GridMapGenerator usan en producción: que un módulo gire hasta ofrecer las salidas
// que pide el camino, que el borde canónico se detecte, y que la malla cubra la celda.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.TerrainModule; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "World/TN_GridPathDecisions.h"
#include "World/TN_TerrainModuleDecisions.h"
#include "World/TN_TerrainModuleWallDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 ModuleTestGridSize = 6;
	constexpr int32 ModuleTestSeedCount = 40;

	TArray<FIntPoint> ModuleTestPath(int32 Seed)
	{
		FRandomStream Stream(Seed);
		return TNGridLogic::GeneratePath(ModuleTestGridSize, 10, 20,
			[&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); });
	}

	/** Heightfield sintético con borde canónico: cresta constante en el borde, plano dentro. */
	TArray<uint16> CanonicalTestHeights(int32 Resolution, uint16 Border, uint16 Interior)
	{
		TArray<uint16> Heights;
		Heights.Init(Interior, Resolution * Resolution);
		for (int32 K = 0; K < Resolution; ++K)
		{
			Heights[K] = Border;
			Heights[(Resolution - 1) * Resolution + K] = Border;
			Heights[K * Resolution] = Border;
			Heights[K * Resolution + Resolution - 1] = Border;
		}
		return Heights;
	}

	TNTerrainModule::FModuleField TestField(int32 Resolution, const TArray<uint16>& Heights, double Size = 40000.0)
	{
		TNTerrainModule::FModuleField Field;
		Field.Resolution = Resolution;
		Field.Size = Size;
		Field.Heights = Heights;
		Field.HeightScale = 0.25;
		Field.HeightZero = 32768;
		return Field;
	}

	const ETNTerrainModuleTopology AllTopologies[TNTerrainModule::NumTopologies] = {
		ETNTerrainModuleTopology::Straight, ETNTerrainModuleTopology::CurveLeft, ETNTerrainModuleTopology::CurveRight,
		ETNTerrainModuleTopology::TLeft, ETNTerrainModuleTopology::TRight, ETNTerrainModuleTopology::Cross
	};
}

// ─────────────────────────────────────────────────────────────────────────────
// Salidas y rotación
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainModuleExitRotationTest,
	"Tortunabo.TerrainModule.ExitRotation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainModuleExitRotationTest::RunTest(const FString& Parameters)
{
	using namespace TNTerrainModule;

	// Yaw +90° lleva Norte a Este: una recta S-N pasa a ser E-W.
	TestEqual(TEXT("recta girada 1"), RotateExitMask(ExitMask(ETNTerrainModuleTopology::Straight), 1), uint8(MaskEast | MaskWest));
	TestEqual(TEXT("recta girada 2"), RotateExitMask(ExitMask(ETNTerrainModuleTopology::Straight), 2), ExitMask(ETNTerrainModuleTopology::Straight));
	TestEqual(TEXT("giro negativo"), RotateExitMask(ExitMask(ETNTerrainModuleTopology::CurveRight), -1),
		RotateExitMask(ExitMask(ETNTerrainModuleTopology::CurveRight), 3));

	// Una curva a la izquierda girada tres cuartos es una curva a la derecha: el generador
	// puede usar cualquiera de las dos familias para un giro del camino.
	TestEqual(TEXT("curva izquierda x3 = curva derecha"),
		RotateExitMask(ExitMask(ETNTerrainModuleTopology::CurveLeft), 3), ExitMask(ETNTerrainModuleTopology::CurveRight));
	TestEqual(TEXT("T izquierda x3 = T derecha x1"),
		RotateExitMask(ExitMask(ETNTerrainModuleTopology::TLeft), 3), RotateExitMask(ExitMask(ETNTerrainModuleTopology::TRight), 1));

	for (int32 Steps = 0; Steps < 4; ++Steps)
	{
		TestEqual(TEXT("la cruz es invariante"), RotateExitMask(ExitMask(ETNTerrainModuleTopology::Cross), Steps), ExitMask(ETNTerrainModuleTopology::Cross));
	}

	for (const ETNTerrainModuleTopology Topology : AllTopologies)
	{
		TestTrue(TEXT("toda topología entra por el Sur"), (ExitMask(Topology) & MaskSouth) != 0);
		TestEqual(TEXT("sin rotar se encuentra a sí misma con yaw 0"), YawStepsForExits(Topology, ExitMask(Topology)), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainModulePathExitsTest,
	"Tortunabo.TerrainModule.PathExits",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainModulePathExitsTest::RunTest(const FString& Parameters)
{
	using namespace TNTerrainModule;

	for (int32 Seed = 1; Seed <= ModuleTestSeedCount; ++Seed)
	{
		const TArray<FIntPoint> Path = ModuleTestPath(Seed);
		if (Path.Num() == 0) { continue; }
		const TArray<TNGridLogic::FTNGridCell> Cells = TNGridLogic::ClassifyPath(Path);

		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			const TNGridLogic::FTNGridCell& Cell = Cells[Index];
			const uint8 Required = RequiredExitMask(Cell);

			// Las salidas requeridas son exactamente los lados hacia la celda anterior y la
			// siguiente (entrada desde fuera por el Sur, salida al final por el Norte).
			const int32 SideBack = (Index > 0) ? TNGridLogic::SideTowards(Cell.Coord, Cells[Index - 1].Coord) : TNGridLogic::SideSouth;
			const int32 SideForward = (Index + 1 < Cells.Num()) ? TNGridLogic::SideTowards(Cell.Coord, Cells[Index + 1].Coord) : TNGridLogic::SideNorth;
			const uint8 Expected = SideBit(SideBack) | SideBit(SideForward);
			if (Required != Expected)
			{
				AddError(FString::Printf(TEXT("Semilla %d, celda %d (%d, %d): salidas %d, esperadas %d."),
					Seed, Index, Cell.Coord.X, Cell.Coord.Y, Required, Expected));
				return false;
			}

			// Recta y las dos curvas cubren cualquier celda de camino; las T y la cruz nunca
			// encajan exactamente en un camino sin ramas.
			const bool bIsTurn = Cell.Type == TNGridLogic::ETNGridTileType::Turn;
			TestEqual(TEXT("recta encaja solo en rectas"), YawStepsForExits(ETNTerrainModuleTopology::Straight, Required) != INDEX_NONE, !bIsTurn);
			TestEqual(TEXT("curva derecha encaja solo en giros"), YawStepsForExits(ETNTerrainModuleTopology::CurveRight, Required) != INDEX_NONE, bIsTurn);
			TestEqual(TEXT("curva izquierda encaja solo en giros"), YawStepsForExits(ETNTerrainModuleTopology::CurveLeft, Required) != INDEX_NONE, bIsTurn);
			TestEqual(TEXT("T no encaja"), YawStepsForExits(ETNTerrainModuleTopology::TLeft, Required), (int32)INDEX_NONE);
			TestEqual(TEXT("cruz no encaja"), YawStepsForExits(ETNTerrainModuleTopology::Cross, Required), (int32)INDEX_NONE);

			// Y la rotación devuelta ofrece de verdad esas salidas.
			const ETNTerrainModuleTopology Topology = bIsTurn ? ETNTerrainModuleTopology::CurveLeft : ETNTerrainModuleTopology::Straight;
			const int32 Yaw = YawStepsForExits(Topology, Required);
			TestEqual(TEXT("rotación coherente"), RotateExitMask(ExitMask(Topology), Yaw), Required);
		}
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Heightfield
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainModuleCanonicalBorderTest,
	"Tortunabo.TerrainModule.CanonicalBorder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainModuleCanonicalBorderTest::RunTest(const FString& Parameters)
{
	using namespace TNTerrainModule;

	constexpr int32 Resolution = 9;
	TArray<uint16> Heights = CanonicalTestHeights(Resolution, 40000, 32768);
	// Abertura simétrica en el centro de cada lado: sigue siendo canónico.
	for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
	{
		const int32 Mid = Resolution / 2;
		Heights[0 * Resolution + Mid] = 32768;
		Heights[(Resolution - 1) * Resolution + Mid] = 32768;
		Heights[Mid * Resolution + 0] = 32768;
		Heights[Mid * Resolution + Resolution - 1] = 32768;
	}
	const FModuleField Canonical = TestField(Resolution, Heights);
	TestTrue(TEXT("borde canónico"), HasCanonicalBorder(Canonical));

	TArray<uint16> Other = CanonicalTestHeights(Resolution, 40000, 35000);
	for (int32 K : { 0, Resolution - 1 })
	{
		Other[K * Resolution + Resolution / 2] = 32768;
		Other[(Resolution / 2) * Resolution + K] = 32768;
	}
	TestTrue(TEXT("dos módulos con el mismo borde casan"), BordersMatch(Canonical, TestField(Resolution, Other)));

	// Un solo valor distinto en un lado rompe el contrato.
	TArray<uint16> Broken = Heights;
	Broken[(Resolution - 1) * Resolution + 1] = 39000;
	TestFalse(TEXT("un lado distinto no es canónico"), HasCanonicalBorder(TestField(Resolution, Broken)));

	// Asimetría en un lado: los cuatro iguales pero no palíndromos.
	TArray<uint16> Asymmetric = CanonicalTestHeights(Resolution, 40000, 32768);
	Asymmetric[1] = 41000;
	Asymmetric[(Resolution - 1) * Resolution + 1] = 41000;
	Asymmetric[1 * Resolution + 0] = 41000;
	Asymmetric[1 * Resolution + Resolution - 1] = 41000;
	TestFalse(TEXT("un lado asimétrico no es canónico"), HasCanonicalBorder(TestField(Resolution, Asymmetric)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainModuleSampleAndMeshTest,
	"Tortunabo.TerrainModule.SampleAndMesh",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainModuleSampleAndMeshTest::RunTest(const FString& Parameters)
{
	using namespace TNTerrainModule;

	// Plano inclinado Z = 100 * i (uu): cada fila sube 100 uu; con HeightScale 0.25 son 400 unidades.
	constexpr int32 Resolution = 5;
	constexpr double Size = 4000.0;
	TArray<uint16> Heights;
	for (int32 I = 0; I < Resolution; ++I)
	{
		for (int32 J = 0; J < Resolution; ++J)
		{
			Heights.Add(static_cast<uint16>(32768 + I * 400));
		}
	}
	const FModuleField Field = TestField(Resolution, Heights, Size);

	// Paso de 1000 uu: en X = -2000 (fila 0) vale 0, en X = +2000 (fila 4) vale 400.
	TestEqual(TEXT("esquina Sur"), SampleHeight(Field, FVector2D(-2000.0, 0.0)), 0.0, 1e-6);
	TestEqual(TEXT("esquina Norte"), SampleHeight(Field, FVector2D(2000.0, -2000.0)), 400.0, 1e-6);
	TestEqual(TEXT("centro"), SampleHeight(Field, FVector2D(0.0, 0.0)), 200.0, 1e-6);
	TestEqual(TEXT("bilineal entre filas"), SampleHeight(Field, FVector2D(-1500.0, 700.0)), 50.0, 1e-6);
	TestEqual(TEXT("fuera del módulo se clampa"), SampleHeight(Field, FVector2D(9000.0, 0.0)), 400.0, 1e-6);

	const TNGridTerrain::FTileMesh Mesh = BuildModuleMesh(Field, FModuleColors());
	TestEqual(TEXT("vértices"), Mesh.Vertices.Num(), Resolution * Resolution);
	TestEqual(TEXT("índices"), Mesh.Triangles.Num(), (Resolution - 1) * (Resolution - 1) * 6);
	TestEqual(TEXT("normales"), Mesh.Normals.Num(), Mesh.Vertices.Num());
	TestEqual(TEXT("colores"), Mesh.Colors.Num(), Mesh.Vertices.Num());

	// La malla cubre la celda entera, centrada en el origen.
	TestEqual(TEXT("primer vértice"), Mesh.Vertices[0], FVector(-2000.0, -2000.0, 0.0));
	TestEqual(TEXT("último vértice"), Mesh.Vertices.Last(), FVector(2000.0, 2000.0, 400.0));

	// Plano inclinado: todas las normales iguales y con la pendiente correcta (100 uu por 1000 uu).
	const FVector Expected = FVector(-0.1, 0.0, 1.0).GetSafeNormal();
	for (const FVector& Normal : Mesh.Normals)
	{
		if (!Normal.Equals(Expected, 1e-4))
		{
			AddError(FString::Printf(TEXT("Normal %s, esperada %s."), *Normal.ToString(), *Expected.ToString()));
			return false;
		}
	}

	// La cara mira hacia +Z: el primer triángulo tiene sentido horario visto desde arriba.
	const FVector& A = Mesh.Vertices[Mesh.Triangles[0]];
	const FVector& B = Mesh.Vertices[Mesh.Triangles[1]];
	const FVector& C = Mesh.Vertices[Mesh.Triangles[2]];
	TestTrue(TEXT("cara hacia +Z"), FVector::CrossProduct(B - A, C - A).Z < 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainModuleCoveringAndWallsTest,
	"Tortunabo.TerrainModule.CoveringAndWalls",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainModuleCoveringAndWallsTest::RunTest(const FString& Parameters)
{
	using namespace TNTerrainModule;

	// Una cruz cubre una recta en las cuatro rotaciones; una T S-N-E cubre S-N en 0 y 2.
	TestEqual(TEXT("cruz cubre recta x4"), YawStepsCoveringExits(ETNTerrainModuleTopology::Cross, MaskSouth | MaskNorth).Num(), 4);
	TestTrue(TEXT("T derecha cubre recta x2"), YawStepsCoveringExits(ETNTerrainModuleTopology::TRight, MaskSouth | MaskNorth) == TArray<int32>({ 0, 2 }));
	TestEqual(TEXT("curva no cubre recta"), YawStepsCoveringExits(ETNTerrainModuleTopology::CurveLeft, MaskSouth | MaskNorth).Num(), 0);

	// T derecha (S-N-E) sin rotar en una recta S-N: sobra el Este, en local y en mundo.
	TestEqual(TEXT("bloqueo T en recta"), BlockedExitsLocal(ETNTerrainModuleTopology::TRight, 0, MaskSouth | MaskNorth), MaskEast);
	// Girada 2 (S-N-W en mundo): sobra el Oeste en mundo, que sigue siendo el Este local.
	TestEqual(TEXT("bloqueo T girada"), BlockedExitsLocal(ETNTerrainModuleTopology::TRight, 2, MaskSouth | MaskNorth), MaskEast);
	// Cruz girada 1 en una curva S-E: sobran N y W en mundo → N-1 = W, W-1 = S en local.
	TestEqual(TEXT("bloqueo cruz en curva"), BlockedExitsLocal(ETNTerrainModuleTopology::Cross, 1, MaskSouth | MaskEast), uint8(MaskWest | MaskSouth));
	// Nada sobrante cuando encaja exacto.
	TestEqual(TEXT("recta exacta sin bloqueo"), BlockedExitsLocal(ETNTerrainModuleTopology::Straight, 0, MaskSouth | MaskNorth), uint8(0));

	// Conexiones de un camino: el inicio y el final no conectan con el exterior del grid.
	for (int32 Seed = 1; Seed <= 10; ++Seed)
	{
		const TArray<FIntPoint> Path = ModuleTestPath(Seed);
		if (Path.Num() < 2) { continue; }
		const TArray<TNGridLogic::FTNGridCell> Cells = TNGridLogic::ClassifyPath(Path);
		TestEqual(TEXT("inicio: una conexión"), FMath::CountBits(ConnectedExitMask(Cells, 0)), 1);
		TestEqual(TEXT("final: una conexión"), FMath::CountBits(ConnectedExitMask(Cells, Cells.Num() - 1)), 1);
		for (int32 Index = 1; Index + 1 < Cells.Num(); ++Index)
		{
			TestEqual(TEXT("interior: dos conexiones"), FMath::CountBits(ConnectedExitMask(Cells, Index)), 2);
			TestTrue(TEXT("interior: las conexiones son las salidas requeridas"),
				ConnectedExitMask(Cells, Index) == RequiredExitMask(Cells[Index]));
		}
	}

	// Muro: determinista, dentro de la boca del lado y por debajo de la cresta.
	constexpr double Size = 40000.0;
	const TArray<TNTerrainModuleWall::FWallPiece> PiecesA = TNTerrainModuleWall::BuildWallPieces(TNGridLogic::SideEast, Size, 42);
	const TArray<TNTerrainModuleWall::FWallPiece> PiecesB = TNTerrainModuleWall::BuildWallPieces(TNGridLogic::SideEast, Size, 42);
	TestEqual(TEXT("piezas"), PiecesA.Num(), TNTerrainModuleWall::PiecesPerWall);
	for (int32 Index = 0; Index < PiecesA.Num(); ++Index)
	{
		const FVector P = PiecesA[Index].Transform.GetLocation();
		if (!P.Equals(PiecesB[Index].Transform.GetLocation()) || !FMath::IsWithinInclusive(P.Y, Size * 0.5 - TNTerrainModuleWall::HeapFar, Size * 0.5 - TNTerrainModuleWall::HeapNear)
			|| FMath::Abs(P.X) > TNTerrainModuleWall::MouthHalfWidth || P.Z < 0.0 || P.Z > TNTerrainModuleWall::CrestHeight * 1.05)
		{
			AddError(FString::Printf(TEXT("Pieza %d fuera de la boca Este o no determinista: %s"), Index, *P.ToString()));
			return false;
		}
	}
	const TNTerrainModuleWall::FWallBlocker Blocker = TNTerrainModuleWall::BuildWallBlocker(TNGridLogic::SideNorth, Size);
	TestTrue(TEXT("caja del Norte por dentro del borde"), Blocker.Center.X < Size * 0.5 && Blocker.Center.X + Blocker.Extent.X <= Size * 0.5);
	TestTrue(TEXT("caja del Norte cubre la boca"), Blocker.Extent.Y >= TNTerrainModuleWall::MouthHalfWidth);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainModuleBridgeTransformTest,
	"Tortunabo.TerrainModule.BridgeTransform",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainModuleBridgeTransformTest::RunTest(const FString& Parameters)
{
	FTNTerrainModuleBridge Bridge;
	Bridge.Center = FVector2D(1500.0, -800.0);
	Bridge.Yaw = 90.f;
	Bridge.Length = 6000.f;
	Bridge.Width = 1200.f;
	Bridge.Thickness = 120.f;
	Bridge.DeckHeight = 1000.f;

	const FTransform Transform = TNTerrainModule::BridgeInstanceTransform(Bridge, 100.0);

	// Escala: cubo de 100 uu → 6000 × 1200 × 120.
	TestEqual(TEXT("escala"), Transform.GetScale3D(), FVector(60.0, 12.0, 1.2));
	// El centro del cubo queda medio grosor por debajo del tablero.
	TestEqual(TEXT("posición"), Transform.GetLocation(), FVector(1500.0, -800.0, 940.0));
	// Girado 90°: el eje largo del tablero apunta a +Y.
	const FVector LongAxis = Transform.TransformVectorNoScale(FVector::ForwardVector);
	TestTrue(TEXT("eje largo"), LongAxis.Equals(FVector(0.0, 1.0, 0.0), 1e-4));
	// Los extremos del tablero, en mundo local, salen del centro a lo largo de ese eje.
	const FVector EndA = Transform.TransformPosition(FVector(50.0, 0.0, 50.0));
	TestEqual(TEXT("extremo A"), EndA, FVector(1500.0, 2200.0, 1000.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
