// Lógica pura del mapa procedural (TNProcMap): módulos irregulares, ruta, camino,
// cruces colosales, huecos, isletas y terreno. Sin mundo ni actores: se testea el
// mismo código que usa ATN_ProcMapGenerator. Correr desde Session Frontend
// (categoría "Tortunabo.ProcMap") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.ProcMap; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/ProcMap/TN_ProcMapGenerate.h"
#include "World/ProcMap/TN_ProcMapTerrain.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TNProcMap::FGenParams MakeParams(uint32 Seed, int32 Grid)
	{
		TNProcMap::FGenParams P;
		P.Seed = Seed;
		P.GridSize = Grid;
		P.NumCrossings = Grid >= 6 ? 2 : (Grid >= 3 ? 1 : 0);
		P.NumBranches = Grid >= 3 ? 3 : 1;
		P.bRiver = (Seed % 3) == 0;
		P.Difficulty01 = static_cast<double>(Seed % 5) / 4.0;
		return P;
	}

	bool HasFlag(const TNProcMap::FPathSample& S, uint32 Mask) { return (S.Flags & Mask) != 0; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Invariantes del layout sobre muchas semillas y tamaños
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNProcMapLayoutInvariantsTest,
	"Tortunabo.ProcMap.LayoutInvariants",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNProcMapLayoutInvariantsTest::RunTest(const FString& Parameters)
{
	using namespace TNProcMap;
	const int32 Grids[3] = { 3, 6, 8 };
	const int32 SeedsPerGrid[3] = { 12, 12, 4 };

	for (int32 g = 0; g < 3; ++g)
	{
		for (int32 s = 0; s < SeedsPerGrid[g]; ++s)
		{
			const int32 Grid = Grids[g];
			const uint32 Seed = static_cast<uint32>(1000 * g + s + 1);
			const FString Ctx = FString::Printf(TEXT("grid %d semilla %u"), Grid, Seed);
			FLayout L;
			if (!TestTrue(Ctx + TEXT(": genera layout"), GenerateLayout(MakeParams(Seed, Grid), L) && L.bValid))
			{
				continue;
			}

			// Ruta: sur → norte, módulos vecinos consecutivos, playa al final.
			TestEqual(Ctx + TEXT(": empieza en el borde sur"), L.Modules[L.Route[0].Module].GridCoord.Y, 0);
			TestEqual(Ctx + TEXT(": acaba en el borde norte"), L.Modules[L.Route.Last().Module].GridCoord.Y, Grid - 1);
			TestTrue(Ctx + TEXT(": la meta es playa"), L.Modules[L.Route.Last().Module].Biome == ETNProcBiome::Beach);
			bool bNeighbors = true;
			for (int32 k = 1; k < L.Route.Num(); ++k)
			{
				bNeighbors &= L.Modules[L.Route[k - 1].Module].Neighbors.Contains(L.Route[k].Module);
			}
			TestTrue(Ctx + TEXT(": módulos consecutivos son vecinos"), bNeighbors);

			bool bVisits = true;
			for (const FModule& M : L.Modules) { bVisits &= M.VisitCount <= 2; }
			for (const FCrossing& C : L.Crossings) { bVisits &= L.Modules[C.Module].VisitCount == 2; }
			TestTrue(Ctx + TEXT(": como mucho dos pasadas por módulo (solo en cruces)"), bVisits);

			const int32 Target = FMath::RoundToInt(L.Params.Coverage * Grid * Grid);
			TestTrue(Ctx + TEXT(": cobertura razonable"), L.UniqueModulesOnRoute >= FMath::Min(Grid, Target - Grid * Grid / 4));

			// Camino: muestras contiguas y de ida hacia el mar.
			bool bContiguous = true;
			for (int32 i = 1; i < L.Main.Num(); ++i)
			{
				bContiguous &= FVector2D::Distance(L.Main[i - 1].P, L.Main[i].P) <= L.Params.SampleSpacing * 1.6;
			}
			TestTrue(Ctx + TEXT(": camino sin saltos de muestreo"), bContiguous);
			TestTrue(Ctx + TEXT(": el camino acaba en el mar"), L.Main.Last().Z < 0.0);

			// Pendiente de los tramos normales dentro del límite (con margen por el suavizado).
			const uint32 Steep = PathFlags::Slide | PathFlags::Elevated | PathFlags::Colossal | PathFlags::TowerTop
				| PathFlags::UnderTower | PathFlags::CliffUp | PathFlags::Shore | PathFlags::Portal | PathFlags::GeyserBase;
			int32 SteepCount = 0;
			for (int32 i = 1; i < L.Main.Num(); ++i)
			{
				if (HasFlag(L.Main[i], Steep) || HasFlag(L.Main[i - 1], Steep)) { continue; }
				const double Ds = FMath::Max(1.0, L.Main[i].S - L.Main[i - 1].S);
				if (FMath::Abs(L.Main[i].Z - L.Main[i - 1].Z) / Ds > L.Params.MaxPathSlope * 1.25) { ++SteepCount; }
			}
			TestEqual(Ctx + TEXT(": sin rampas más empinadas que MaxPathSlope"), SteepCount, 0);

			// Huecos de salto dentro de las métricas de la tortuga (1,3–3,9 m).
			bool bGaps = true;
			for (const FFeature& F : L.Features)
			{
				if (F.Type == EFeature::Gap) { bGaps &= F.Length >= L.Params.GapMin - 1.0 && F.Length <= L.Params.GapMax + 1.0; }
			}
			TestTrue(Ctx + TEXT(": huecos dentro de [GapMin, GapMax]"), bGaps);

			// Isletas: el hueco entre isletas consecutivas es saltable.
			bool bIslets = true;
			const FFeature* Prev = nullptr;
			for (const FFeature& F : L.Features)
			{
				if (F.Type != EFeature::Islet) { continue; }
				if (Prev && Prev->PathIndex <= F.PathIndex && F.PathIndex - Prev->PathIndex < 12)
				{
					const double Between = FVector2D::Distance(FVector2D(Prev->Location.X, Prev->Location.Y), FVector2D(F.Location.X, F.Location.Y))
						- (Prev->Length + F.Length) * 0.5;
					bIslets &= Between <= L.Params.IsletGapMax + 60.0;
					bIslets &= FMath::Abs(F.Location.Z - Prev->Location.Z) <= L.Params.MaxStepUp;
				}
				Prev = &F;
			}
			TestTrue(Ctx + TEXT(": isletas alcanzables de un salto"), bIslets);

			// Estructura mínima: salida, meta, huevos; dos torres por cruce.
			TestEqual(Ctx + TEXT(": una zona de salida"), L.CountFeatures(EFeature::StartArea), 1);
			TestEqual(Ctx + TEXT(": una meta"), L.CountFeatures(EFeature::Finish), 1);
			TestTrue(Ctx + TEXT(": al menos una pila de huevos"), L.CountFeatures(EFeature::EggNest) >= 1);
			TestEqual(Ctx + TEXT(": dos torres por cruce colosal"), L.CountFeatures(EFeature::Tower), L.Crossings.Num() * 2);

			// Ramas y sendas: salen de un camino (el principal u otra rama) y llegan a otro más adelante.
			bool bBranches = true;
			for (const FBranch& B : L.Branches)
			{
				bBranches &= B.RejoinSample > B.ForkSample;
				const FVector2D From = B.FromBranch == INDEX_NONE ? L.Main[B.ForkSample].P : L.Branches[B.FromBranch].Samples[B.FromSample].P;
				const FVector2D To = B.ToBranch == INDEX_NONE ? L.Main[B.RejoinSample].P : L.Branches[B.ToBranch].Samples[B.ToSample].P;
				bBranches &= FVector2D::Distance(B.Samples[0].P, From) < 1.0;
				bBranches &= FVector2D::Distance(B.Samples.Last().P, To) < 1.0;
			}
			TestTrue(Ctx + TEXT(": ramas que se reincorporan"), bBranches);
		}
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Módulos: todos conexos y sin huecos
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNProcMapModulesConnectedTest,
	"Tortunabo.ProcMap.ModulesConnected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNProcMapModulesConnectedTest::RunTest(const FString& Parameters)
{
	using namespace TNProcMap;
	for (uint32 Seed = 1; Seed <= 6; ++Seed)
	{
		FLayout L;
		L.Params = SanitizeParams(MakeParams(Seed, 6));
		BuildModules(L, FRng(Seed));
		const int32 N = L.RasterW * L.RasterH;

		bool bAllLabeled = true;
		for (int32 i = 0; i < N; ++i) { bAllLabeled &= L.ModuleOfCell[i] >= 0; }
		TestTrue(FString::Printf(TEXT("semilla %u: todas las celdas tienen módulo"), Seed), bAllLabeled);

		// BFS en 4-vecindad desde una celda de cada módulo: debe alcanzar todas las suyas.
		for (const FModule& M : L.Modules)
		{
			TArray<uint8> Seen;
			Seen.Init(0, N);
			int32 Start = INDEX_NONE;
			for (int32 i = 0; i < N && Start == INDEX_NONE; ++i) { if (L.ModuleOfCell[i] == M.Id) { Start = i; } }
			if (!TestTrue(FString::Printf(TEXT("semilla %u módulo %d: no vacío"), Seed, M.Id), Start != INDEX_NONE)) { continue; }
			TArray<int32> Queue;
			Queue.Add(Start);
			Seen[Start] = 1;
			int32 Reached = 0;
			for (int32 h = 0; h < Queue.Num(); ++h)
			{
				const int32 C = Queue[h];
				++Reached;
				const int32 X = C % L.RasterW;
				const int32 Y = C / L.RasterW;
				const int32 DX[4] = { 1, -1, 0, 0 };
				const int32 DY[4] = { 0, 0, 1, -1 };
				for (int32 k = 0; k < 4; ++k)
				{
					const int32 NX = X + DX[k];
					const int32 NY = Y + DY[k];
					if (!L.CellInside(NX, NY)) { continue; }
					const int32 NIdx = L.CellIndex(NX, NY);
					if (!Seen[NIdx] && L.ModuleOfCell[NIdx] == M.Id) { Seen[NIdx] = 1; Queue.Add(NIdx); }
				}
			}
			TestEqual(FString::Printf(TEXT("semilla %u módulo %d: conexo"), Seed, M.Id), Reached, M.CellCount);
			// Tamaño medio del orden de ModuleSize²: entre 35% y 250% del nominal.
			const double Nominal = (L.Params.ModuleSize / L.Params.CellSize) * (L.Params.ModuleSize / L.Params.CellSize);
			TestTrue(FString::Printf(TEXT("semilla %u módulo %d: tamaño razonable"), Seed, M.Id),
				M.CellCount > Nominal * 0.35 && M.CellCount < Nominal * 2.5);
		}
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Determinismo: la red solo replica la semilla
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNProcMapDeterminismTest,
	"Tortunabo.ProcMap.Determinism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNProcMapDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace TNProcMap;
	for (uint32 Seed = 7; Seed <= 9; ++Seed)
	{
		FLayout A, B;
		GenerateLayout(MakeParams(Seed, 6), A);
		GenerateLayout(MakeParams(Seed, 6), B);
		TestEqual(TEXT("mismo número de muestras"), A.Main.Num(), B.Main.Num());
		TestEqual(TEXT("mismo número de features"), A.Features.Num(), B.Features.Num());
		bool bSame = A.Main.Num() == B.Main.Num();
		for (int32 i = 0; bSame && i < A.Main.Num(); ++i)
		{
			bSame &= A.Main[i].P == B.Main[i].P && A.Main[i].Z == B.Main[i].Z && A.Main[i].Flags == B.Main[i].Flags;
		}
		TestTrue(FString::Printf(TEXT("semilla %u: camino idéntico"), Seed), bSame);

		FLayout C;
		GenerateLayout(MakeParams(Seed + 100, 6), C);
		TestTrue(FString::Printf(TEXT("semilla %u: otra semilla da otro mapa"), Seed),
			C.Main.Num() != A.Main.Num() || !(C.Main[C.Main.Num() / 2].P == A.Main[A.Main.Num() / 2].P));
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Terreno: el cauce del camino queda a la cota del camino y el mar bajo cero
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNProcMapTerrainTest,
	"Tortunabo.ProcMap.Terrain",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNProcMapTerrainTest::RunTest(const FString& Parameters)
{
	using namespace TNProcMap;
	FLayout L;
	GenerateLayout(MakeParams(21, 3), L);
	if (!TestTrue(TEXT("layout válido"), L.bValid)) { return true; }

	const double Spacing = 400.0;
	const double Margin = 8000.0;
	const int32 NX = FMath::CeilToInt((L.WorldSize + 2.0 * Margin) / Spacing) + 1;
	const int32 NY = FMath::CeilToInt((L.WorldSize + 2.0 * Margin + 10000.0) / Spacing) + 1;
	FTerrainBuilder TB;
	TB.Build(L, FVector2D(-Margin, -Margin), Spacing, NX, NY);
	TArray<float> H;
	TArray<uint8> Mask;
	H.SetNum(NX * NY);
	Mask.SetNum(NX * NY);
	TB.ComputeRows(0, NY, H, Mask);

	bool bFinite = true;
	for (const float V : H) { bFinite &= FMath::IsFinite(V) && V > -100000.0f && V < 100000.0f; }
	TestTrue(TEXT("alturas finitas y acotadas"), bFinite);

	// Muestras normales anchas del camino: el vértice más cercano está a la cota del camino.
	int32 Checked = 0, Off = 0;
	const uint32 Skip = PathFlags::NotTerrain | PathFlags::Special | PathFlags::Colossal | PathFlags::Tunnel;
	for (int32 i = 0; i < L.Main.Num(); i += 7)
	{
		const FPathSample& S = L.Main[i];
		if ((S.Flags & Skip) != 0 || S.Width < 1200.0) { continue; }
		const int32 IX = FMath::RoundToInt((S.P.X + Margin) / Spacing);
		const int32 IY = FMath::RoundToInt((S.P.Y + Margin) / Spacing);
		if (IX < 0 || IY < 0 || IX >= NX || IY >= NY) { continue; }
		++Checked;
		if (FMath::Abs(H[IY * NX + IX] - S.Z) > 120.0) { ++Off; }
	}
	TestTrue(TEXT("se comprobaron muestras"), Checked > 10);
	TestTrue(FString::Printf(TEXT("cauce a la cota del camino (%d de %d fuera)"), Off, Checked), Off <= Checked / 20);

	// Mar abierto al norte.
	const int32 SeaRow = NY - 2;
	bool bSea = true;
	for (int32 x = NX / 4; x < 3 * NX / 4; ++x) { bSea &= H[SeaRow * NX + x] < 0.0f; }
	TestTrue(TEXT("mar abierto al norte"), bSea);
	return true;
}

#endif
