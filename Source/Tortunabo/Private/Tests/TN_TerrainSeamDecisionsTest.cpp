// Fusión de bordes entre módulos vecinos (TN_TerrainSeamDecisions.h). Sin mundo ni
// actores: se montan 3x3 celdas con alturas, giros y espejos distintos y se comprueba lo
// que ATN_TerrainModuleTile necesita en producción: que dos tiles vecinos construyan la
// misma altura y la misma normal en cada vértice compartido, que los pesos sumen 1, que
// un módulo sin vecinos no cambie y que dos lagos que se tocan queden unidos.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.TerrainSeam; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "World/TN_TerrainSeamDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 SeamTestResolution = 21;
	constexpr double SeamTestSize = 20000.0;

	/** Una celda colocada en el mundo: centro (X Norte, Y Este), giro, espejo y alturas. */
	struct FSeamTestPlacement
	{
		FVector2D Center = FVector2D::ZeroVector;
		int32 YawSteps = 0;
		bool bMirrored = false;
		TArray<uint16> Heights;
	};

	TNTerrainModule::FModuleField SeamTestField(const TArray<uint16>& Heights, bool bMirrored)
	{
		TNTerrainModule::FModuleField Field;
		Field.Resolution = SeamTestResolution;
		Field.Size = SeamTestSize;
		Field.Heights = Heights;
		Field.bMirrorY = bMirrored;
		return Field;
	}

	/** 3x3 celdas con alturas aleatorias (±40 m), giros y espejos sorteados. */
	TArray<FSeamTestPlacement> SeamTestGrid(int32 Seed)
	{
		FRandomStream Stream(Seed);
		TArray<FSeamTestPlacement> Grid;
		for (int32 Row = -1; Row <= 1; ++Row)
		{
			for (int32 Col = -1; Col <= 1; ++Col)
			{
				FSeamTestPlacement& Placement = Grid.AddDefaulted_GetRef();
				Placement.Center = FVector2D(Row, Col) * SeamTestSize;
				Placement.YawSteps = Stream.RandRange(0, 3);
				Placement.bMirrored = Stream.FRand() < 0.5f;
				Placement.Heights.SetNumUninitialized(SeamTestResolution * SeamTestResolution);
				for (uint16& Height : Placement.Heights)
				{
					Height = static_cast<uint16>(32768 + Stream.RandRange(-16000, 16000));
				}
			}
		}
		return Grid;
	}

	/** Celdas de la fusión vistas desde el tile Own, como las monta ATN_TerrainModuleTile. */
	TArray<TNTerrainSeam::FSeamCell> SeamTestCells(const TArray<FSeamTestPlacement>& Grid, int32 Own)
	{
		TArray<TNTerrainSeam::FSeamCell> Cells;
		auto Add = [&](int32 Index)
		{
			const FSeamTestPlacement& Placement = Grid[Index];
			TNTerrainSeam::FSeamCell& Cell = Cells.AddDefaulted_GetRef();
			Cell.Field = SeamTestField(Placement.Heights, Placement.bMirrored);
			Cell.Center = TNTerrainSeam::RotateSteps(Placement.Center - Grid[Own].Center, -Grid[Own].YawSteps);
			Cell.RelYawSteps = Placement.YawSteps - Grid[Own].YawSteps;
		};
		Add(Own);
		for (int32 Index = 0; Index < Grid.Num(); ++Index)
		{
			if (Index != Own) { Add(Index); }
		}
		return Cells;
	}

	/** Vértice del tile Owner que cae en el punto de mundo World, o INDEX_NONE. */
	int32 SeamTestVertexAt(const FSeamTestPlacement& Owner, const FVector2D& World)
	{
		const double Half = SeamTestSize * 0.5;
		const double Step = SeamTestSize / (SeamTestResolution - 1);
		const FVector2D Local = TNTerrainSeam::RotateSteps(World - Owner.Center, -Owner.YawSteps);
		const int32 I = FMath::RoundToInt32((Local.X + Half) / Step);
		const int32 J = FMath::RoundToInt32((Local.Y + Half) / Step);
		if (I < 0 || J < 0 || I >= SeamTestResolution || J >= SeamTestResolution) { return INDEX_NONE; }
		return I * SeamTestResolution + J;
	}

	FVector SeamTestWorldNormal(const FVector& Normal, int32 YawSteps)
	{
		const FVector2D XY = TNTerrainSeam::RotateSteps(FVector2D(Normal.X, Normal.Y), YawSteps);
		return FVector(XY.X, XY.Y, Normal.Z);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainSeamPartitionTest,
	"Tortunabo.TerrainSeam.PartitionOfUnity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainSeamPartitionTest::RunTest(const FString& Parameters)
{
	const TArray<FSeamTestPlacement> Grid = SeamTestGrid(7);
	const TArray<TNTerrainSeam::FSeamCell> Cells = SeamTestCells(Grid, 4);
	const TNTerrainSeam::FSeamSettings Settings;
	FRandomStream Stream(11);
	bool bSumsToOne = true;
	for (int32 Sample = 0; Sample < 500; ++Sample)
	{
		// Puntos del tile central y su franja: los 9 vecinos cubren toda la banda.
		const FVector2D P(Stream.FRandRange(-0.7, 0.7) * SeamTestSize, Stream.FRandRange(-0.7, 0.7) * SeamTestSize);
		double Sum = 0.0;
		for (const TNTerrainSeam::FSeamCell& Cell : Cells) { Sum += TNTerrainSeam::CellWeight(Cell, P, Settings); }
		bSumsToOne &= FMath::IsNearlyEqual(Sum, 1.0, 1e-9);
	}
	TestTrue(TEXT("los pesos de las 3x3 celdas suman 1"), bSumsToOne);
	TestTrue(TEXT("en el centro solo pesa la propia celda"),
		FMath::IsNearlyEqual(TNTerrainSeam::CellWeight(Cells[0], FVector2D::ZeroVector, Settings), 1.0));
	TestTrue(TEXT("sobre el borde, cada lado pesa la mitad"),
		FMath::IsNearlyEqual(TNTerrainSeam::AxisWeight(0.0, Settings.Band), 0.5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainSeamContinuityTest,
	"Tortunabo.TerrainSeam.Continuity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainSeamContinuityTest::RunTest(const FString& Parameters)
{
	const double Half = SeamTestSize * 0.5;
	for (int32 Seed = 0; Seed < 12; ++Seed)
	{
		const TArray<FSeamTestPlacement> Grid = SeamTestGrid(100 + Seed);
		// Tile central (4) frente a su vecino Este (5), Norte (7) y en diagonal (8).
		const TNGridTerrain::FTileMesh Center = TNTerrainSeam::BuildFusedMesh(SeamTestCells(Grid, 4));
		for (const int32 Other : { 5, 7, 8 })
		{
			const TNGridTerrain::FTileMesh Mesh = TNTerrainSeam::BuildFusedMesh(SeamTestCells(Grid, Other));
			int32 Shared = 0;
			bool bSameHeight = true;
			bool bSameNormal = true;
			for (int32 V = 0; V < Center.Vertices.Num(); ++V)
			{
				const FVector2D Local(Center.Vertices[V].X, Center.Vertices[V].Y);
				const FVector2D World = Grid[4].Center + TNTerrainSeam::RotateSteps(Local, Grid[4].YawSteps);
				const FVector2D InOther = TNTerrainSeam::RotateSteps(World - Grid[Other].Center, -Grid[Other].YawSteps);
				if (FMath::Abs(InOther.X) > Half + 1.0 || FMath::Abs(InOther.Y) > Half + 1.0) { continue; }
				const int32 W = SeamTestVertexAt(Grid[Other], World);
				if (W == INDEX_NONE) { continue; }
				++Shared;
				bSameHeight &= FMath::IsNearlyEqual(Center.Vertices[V].Z, Mesh.Vertices[W].Z, 0.01);
				bSameNormal &= SeamTestWorldNormal(Center.Normals[V], Grid[4].YawSteps)
					.Equals(SeamTestWorldNormal(Mesh.Normals[W], Grid[Other].YawSteps), 1e-4);
			}
			const int32 Expected = Other == 8 ? 1 : SeamTestResolution;
			TestEqual(FString::Printf(TEXT("semilla %d, vecino %d: vértices compartidos"), Seed, Other), Shared, Expected);
			TestTrue(FString::Printf(TEXT("semilla %d, vecino %d: misma altura en el borde"), Seed, Other), bSameHeight);
			TestTrue(FString::Printf(TEXT("semilla %d, vecino %d: misma normal en el borde"), Seed, Other), bSameNormal);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainSeamAloneTest,
	"Tortunabo.TerrainSeam.Alone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainSeamAloneTest::RunTest(const FString& Parameters)
{
	const TArray<FSeamTestPlacement> Grid = SeamTestGrid(3);
	TArray<TNTerrainSeam::FSeamCell> Cells = SeamTestCells(Grid, 4);
	Cells.SetNum(1);
	const TNGridTerrain::FTileMesh Mesh = TNTerrainSeam::BuildFusedMesh(Cells);
	TestEqual(TEXT("un vértice por muestra"), Mesh.Vertices.Num(), SeamTestResolution * SeamTestResolution);
	bool bUnchanged = true;
	for (int32 I = 0; I < SeamTestResolution; ++I)
	{
		for (int32 J = 0; J < SeamTestResolution; ++J)
		{
			bUnchanged &= FMath::IsNearlyEqual(Mesh.Vertices[I * SeamTestResolution + J].Z, Cells[0].Field.HeightAt(I, J), 1e-6);
		}
	}
	TestTrue(TEXT("sin vecinos el terreno no cambia"), bUnchanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainSeamLakeTest,
	"Tortunabo.TerrainSeam.LakesJoin",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainSeamLakeTest::RunTest(const FString& Parameters)
{
	// Cuatro celdas alrededor de una esquina: tierra a +10 m salvo un lago (-6 m) centrado
	// en la esquina común, repartido entre las cuatro. Si cada módulo lleva su parte del
	// agua hasta el borde, la fusión la conserva: un solo lago, sin tierra en la esquina.
	const double Half = SeamTestSize * 0.5;
	const double Step = SeamTestSize / (SeamTestResolution - 1);
	TArray<FSeamTestPlacement> Grid;
	for (int32 Row = 0; Row <= 1; ++Row)
	{
		for (int32 Col = 0; Col <= 1; ++Col)
		{
			FSeamTestPlacement& Placement = Grid.AddDefaulted_GetRef();
			Placement.Center = FVector2D(Row, Col) * SeamTestSize;
			const FVector2D Corner(Half, Half);   // esquina común (0,5 ; 0,5) en celdas
			Placement.Heights.SetNumUninitialized(SeamTestResolution * SeamTestResolution);
			for (int32 I = 0; I < SeamTestResolution; ++I)
			{
				for (int32 J = 0; J < SeamTestResolution; ++J)
				{
					const FVector2D World = Placement.Center + FVector2D(I * Step - Half, J * Step - Half);
					const bool bLake = FVector2D::Distance(World, Corner) < 0.45 * SeamTestSize;
					Placement.Heights[I * SeamTestResolution + J] = static_cast<uint16>(32768 + (bLake ? -2400 : 4000));
				}
			}
		}
	}
	const TNGridTerrain::FTileMesh Mesh = TNTerrainSeam::BuildFusedMesh(SeamTestCells(Grid, 0));
	const double CornerZ = Mesh.Vertices[(SeamTestResolution - 1) * SeamTestResolution + SeamTestResolution - 1].Z;
	TestTrue(TEXT("la esquina común queda bajo el agua"), CornerZ < -400.0);
	const double EdgeZ = Mesh.Vertices[(SeamTestResolution - 3) * SeamTestResolution + SeamTestResolution - 1].Z;
	TestTrue(TEXT("el borde junto a la esquina queda bajo el agua"), EdgeZ < -400.0);
	return true;
}

#endif
