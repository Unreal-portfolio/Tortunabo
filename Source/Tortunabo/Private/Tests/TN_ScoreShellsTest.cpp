// Conchas de puntos: tamaños y valores, reparto de una recogida en iconos del HUD y escala del «pom»
// (TN_ScoreShells.h), y reparto de las conchas del mapa procedural (TN_ProcMapShells.h): determinista, dentro de
// los topes, sin pisar nada, alcanzables con el salto real de la tortuga y fuera de las cajas de muerte. Sin mundo
// ni actores: se testea el mismo código que usan ATN_ScorePickup, UTN_RunHUDWidget y ATN_ProcMapGenerator. Correr
// desde Session Frontend (categorías "Tortunabo.ScoreShells" y "Tortunabo.ProcMap") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.ScoreShells+Tortunabo.ProcMap.Shells; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/TN_ScoreShells.h"
#include "World/ProcMap/TN_ProcMapGenerate.h"
#include "World/ProcMap/TN_ProcMapShells.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNShellPlanTest
{
	/** Parámetros parecidos a los perfiles por defecto (ramas, cruces y huecos por km según el tamaño). */
	TNProcMap::FGenParams MakeShellParams(uint32 Seed, int32 Grid, double Difficulty)
	{
		TNProcMap::FGenParams P;
		P.Seed = Seed;
		P.GridSize = Grid;
		P.NumCrossings = Grid >= 8 ? 4 : (Grid >= 6 ? 2 : (Grid >= 3 ? 1 : 0));
		P.NumBranches = Grid >= 6 ? 12 : 4;
		P.GapsPerKm = TNProcMap::LerpD(9.0, 17.0, Difficulty);
		P.Difficulty01 = Difficulty;
		return P;
	}

	/** Huella en planta (cm) de lo que una concha del suelo no puede pisar; 0 si no cuenta. */
	double FootprintOf(const TNProcMap::FFeature& F)
	{
		using TNProcMap::EFeature;
		switch (F.Type)
		{
			case EFeature::Boulder:
			case EFeature::RockSpire:   return F.Radius;
			case EFeature::PathProp:    return FMath::Max(F.Radius, F.Length * 0.5);
			case EFeature::ClimbTower:  return F.Radius * 1.42;
			case EFeature::BonusPickup:
			case EFeature::Bouncer:     return 150.0;
			case EFeature::EggNest:
			case EFeature::Geyser:      return 500.0;
			default:                    return 0.0;
		}
	}

	bool InAnyKillBox(const TNProcMap::FLayout& L, const FVector& P)
	{
		for (const TNProcMap::FKillBox& K : L.KillBoxes)
		{
			if (K.Contains(P)) { return true; }
		}
		return false;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Tamaños, valores, iconos del HUD y «pom»
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNScoreShellsSplitTest,
	"Tortunabo.ScoreShells.Split",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNScoreShellsSplitTest::RunTest(const FString& Parameters)
{
	using namespace TNScoreShells;

	TestEqual(TEXT("pequeña de 1"), ValueOf(ETier::Small), 1);
	TestEqual(TEXT("normal de 25 (la de siempre)"), ValueOf(ETier::Normal), 25);
	TestEqual(TEXT("grande de 50"), ValueOf(ETier::Big), 50);
	TestEqual(TEXT("reina de 100"), ValueOf(ETier::Grand), 100);
	for (int32 t = 0; t < NumTiers; ++t)
	{
		const ETier Tier = TierFromIndex(t);
		TestTrue(FString::Printf(TEXT("el valor del tamaño %d da ese tamaño"), t), TierForValue(ValueOf(Tier)) == Tier);
		if (t > 0)
		{
			const ETier Smaller = TierFromIndex(t - 1);
			TestTrue(FString::Printf(TEXT("tamaño %d: más grande y con más radio de recogida que el anterior"), t),
				MeshScale(Tier) > MeshScale(Smaller) && CollectRadius(Tier) > CollectRadius(Smaller));
		}
	}
	TestTrue(TEXT("un Blueprint de 10 puntos es normal"), TierForValue(10) == ETier::Normal);
	TestTrue(TEXT("uno de 70, grande"), TierForValue(70) == ETier::Big);
	TestTrue(TEXT("uno de 300, reina"), TierForValue(300) == ETier::Grand);
	TestTrue(TEXT("índices de red fuera de rango, acotados"), TierFromIndex(-3) == ETier::Small && TierFromIndex(9) == ETier::Grand);

	TestEqual(TEXT("1 punto → 1 icono"), IconCountFor(1), 1);
	TestEqual(TEXT("25 puntos → 10 iconos"), IconCountFor(25), 10);
	TestEqual(TEXT("50 puntos → 14 iconos"), IconCountFor(50), 14);
	TestEqual(TEXT("100 puntos → 15 iconos (el tope)"), IconCountFor(100), MaxIcons);
	TestEqual(TEXT("0 puntos → ningún icono"), IconCountFor(0), 0);

	bool bSums = true, bBounds = true, bCrescendo = true;
	for (int32 Value = 1; Value <= 400; ++Value)
	{
		TArray<int32> Parts;
		SplitIntoIcons(Value, MaxIcons, Parts);
		int32 Sum = 0;
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			Sum += Parts[i];
			bBounds &= Parts[i] >= 1;
			if (i > 0) { bCrescendo &= Parts[i] >= Parts[i - 1]; }
		}
		bSums &= Sum == Value;
		bBounds &= Parts.Num() == IconCountFor(Value) && Parts.Num() >= 1 && Parts.Num() <= MaxIcons && Parts.Num() <= Value;
	}
	TestTrue(TEXT("los iconos suman exactamente el valor"), bSums);
	TestTrue(TEXT("cada icono vale al menos 1 y no hay más de 15"), bBounds);
	TestTrue(TEXT("el resto va a los últimos (el contador acelera al final)"), bCrescendo);
	{
		TArray<int32> Parts;
		SplitIntoIcons(0, MaxIcons, Parts);
		TestEqual(TEXT("nada que repartir → sin iconos"), Parts.Num(), 0);
		SplitIntoIcons(-5, MaxIcons, Parts);
		TestEqual(TEXT("valor negativo → sin iconos"), Parts.Num(), 0);
	}

	TestEqual(TEXT("el primer «pom» en la nota base"), PomSemitones(0), 0);
	bool bRises = true;
	for (int32 Step = 1; Step <= PomTopStep + 5; ++Step) { bRises &= PomSemitones(Step) >= PomSemitones(Step - 1); }
	TestTrue(TEXT("el «pom» nunca baja dentro de una tanda"), bRises);
	TestEqual(TEXT("sube dos octavas y ahí se queda"), PomSemitones(PomTopStep + 7), 24);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Reparto de conchas del mapa procedural
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNProcMapShellsTest,
	"Tortunabo.ProcMap.Shells",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNProcMapShellsTest::RunTest(const FString& Parameters)
{
	using namespace TNProcMap;
	using TNScoreShells::ETier;

	// Medidas con el salto real (TurtleJump: 1,2 m de alto, 3,96 m esprintando).
	TestTrue(TEXT("el arco sube menos que el salto"), ShellDims::ArcRise() < TurtleJump::Apex());
	TestTrue(TEXT("en el punto más alto del arco la concha queda al alcance del centro de la tortuga saltando"),
		FMath::Abs(TNScoreShells::Hover + ShellDims::ArcRise() - (70.0 + TurtleJump::Apex())) < TNScoreShells::CollectRadius(ETier::Small) + TurtleJump::CapsuleRadius);

	struct FCase { uint32 Seed; int32 Grid; double Diff; };
	const FCase Cases[] = { { 11, 3, 0.2 }, { 12, 3, 0.5 }, { 13, 6, 0.5 }, { 14, 6, 0.9 }, { 15, 6, 0.2 }, { 16, 4, 0.9 }, { 17, 6, 0.5 }, { 18, 8, 0.9 } };
	int32 Layouts = 0;
	int32 TierTotals[TNScoreShells::NumTiers] = {};
	int32 SpotTotals[static_cast<int32>(EShellSpot::Count)] = {};
	double KmTotal = 0.0;
	const double SprintReach = TurtleJump::Reach(TurtleJump::SprintSpeed);

	for (const FCase& Case : Cases)
	{
		const FString Ctx = FString::Printf(TEXT("semilla %u grid %d dificultad %.1f"), Case.Seed, Case.Grid, Case.Diff);
		FLayout L;
		if (!GenerateLayout(TNShellPlanTest::MakeShellParams(Case.Seed, Case.Grid, Case.Diff), L) || !L.bValid)
		{
			AddWarning(Ctx + TEXT(": sin layout, se salta."));
			continue;
		}
		++Layouts;
		KmTotal += L.MainLength() / 100000.0;

		TArray<FShellSpawn> Plan, Again;
		PlanShells(L, TArray<FVector>(), Plan);
		PlanShells(L, TArray<FVector>(), Again);
		bool bSame = Plan.Num() == Again.Num();
		for (int32 i = 0; bSame && i < Plan.Num(); ++i)
		{
			bSame &= Plan[i].Location.Equals(Again[i].Location, 0.01) && Plan[i].Tier == Again[i].Tier && Plan[i].Spot == Again[i].Spot;
		}
		TestTrue(Ctx + TEXT(": el reparto es determinista"), bSame);

		// Topes y tamaños.
		int32 Tiers[TNScoreShells::NumTiers] = {};
		for (const FShellSpawn& S : Plan)
		{
			++Tiers[static_cast<int32>(S.Tier)];
			++SpotTotals[static_cast<int32>(S.Spot)];
		}
		for (int32 t = 0; t < TNScoreShells::NumTiers; ++t) { TierTotals[t] += Tiers[t]; }
		TestTrue(Ctx + TEXT(": hay conchitas"), Tiers[0] > 0);
		TestTrue(Ctx + TEXT(": no pasan del tope"), Tiers[0] <= ShellDims::MaxSmall);
		TestEqual(Ctx + TEXT(": las normales de 25 no salen del plan"), Tiers[1], 0);
		TestTrue(Ctx + TEXT(": reinas dentro del presupuesto"), Tiers[3] <= ShellDims::GrandBudget(L));
		TestTrue(Ctx + TEXT(": grandes dentro del presupuesto"), Tiers[2] <= ShellDims::BigBudget(L));
		bool bTierBySpot = true;
		for (const FShellSpawn& S : Plan) { bTierBySpot &= IsSpecialShellSpot(S.Spot) == (S.Tier == ETier::Big || S.Tier == ETier::Grand); }
		TestTrue(Ctx + TEXT(": las especiales son de 50 o 100 y el resto de 1"), bTierBySpot);

		// Separaciones: nada encima de nada; las especiales, solas y lejos entre sí.
		bool bApart = true, bSpecialsApart = true, bSpecialsAlone = true;
		for (int32 i = 0; i < Plan.Num(); ++i)
		{
			for (int32 j = i + 1; j < Plan.Num(); ++j)
			{
				const FShellSpawn& A = Plan[i];
				const FShellSpawn& B = Plan[j];
				const double Flat = FVector2D::Distance(FVector2D(A.Location.X, A.Location.Y), FVector2D(B.Location.X, B.Location.Y));
				const bool bSpecialA = IsSpecialShellSpot(A.Spot);
				const bool bSpecialB = IsSpecialShellSpot(B.Spot);
				if (bSpecialA && bSpecialB) { bSpecialsApart &= Flat >= ShellDims::SpecialSpacing - 1.0; }
				else if (bSpecialA || bSpecialB) { bSpecialsAlone &= Flat >= ShellDims::SpecialClear - 1.0; }
				// Los de un mismo arco van a menos de 1,1 m en planta, pero a alturas distintas.
				else { bApart &= Flat >= ShellDims::SmallSpacing - 1.0 || (A.Spot == EShellSpot::JumpArc && B.Spot == EShellSpot::JumpArc && A.PathIndex == B.PathIndex); }
			}
		}
		TestTrue(Ctx + TEXT(": conchitas separadas (1,1 m en planta, salvo las de un mismo arco)"), bApart);
		TestTrue(Ctx + TEXT(": especiales a 60 m como mínimo entre sí"), bSpecialsApart);
		TestTrue(Ctx + TEXT(": nada a menos de 3,5 m de una especial"), bSpecialsAlone);

		// Nada pisa obstáculos, torres de escalada, recompensas, medusas, huevos ni géiseres.
		bool bClearOfFeatures = true;
		for (const FShellSpawn& S : Plan)
		{
			if (!S.bOnGround && S.Spot != EShellSpot::JumpArc) { continue; }
			const FVector2D At(S.Location.X, S.Location.Y);
			for (const FFeature& F : L.Features)
			{
				double Radius = TNShellPlanTest::FootprintOf(F);
				if (F.Type == EFeature::Log)
				{
					double T = 0.0;
					const FVector2D C(F.Location.X, F.Location.Y);
					const FVector2D Half = F.Dir.GetSafeNormal() * (F.Length * 0.5);
					if (DistPointSegment(At, C - Half, C + Half, T) < F.Radius + 30.0) { bClearOfFeatures = false; }
					continue;
				}
				if (Radius > 0.0 && FVector2D::Distance(At, FVector2D(F.Location.X, F.Location.Y)) < Radius)
				{
					AddInfo(FString::Printf(TEXT("%s: concha (%s) sobre el elemento %d."), *Ctx, ShellSpotName(S.Spot), static_cast<int32>(F.Type)));
					bClearOfFeatures = false;
				}
			}
		}
		TestTrue(Ctx + TEXT(": no pisan obstáculos, torres, recompensas, medusas, huevos ni géiseres"), bClearOfFeatures);

		// Las del suelo, dentro del cauce y en muestras normales (sin huecos, estructuras, uniones ni agua).
		bool bInCorridor = true, bOnPlainSamples = true, bNoKill = true;
		for (const FShellSpawn& S : Plan)
		{
			bNoKill &= !TNShellPlanTest::InAnyKillBox(L, S.Location) && !(S.bOnGround && TNShellPlanTest::InAnyKillBox(L, S.Location + FVector(0.0, 0.0, 5.0)));
			if (!S.bOnGround) { continue; }
			const TArray<FPathSample>& Samples = L.Branches.IsValidIndex(S.BranchIndex) ? L.Branches[S.BranchIndex].Samples : L.Main;
			if (!Samples.IsValidIndex(S.PathIndex)) { bInCorridor = false; continue; }
			const int32 Next = FMath::Min(S.PathIndex + 1, Samples.Num() - 1);
			double T = 0.0;
			const double Off = DistPointSegment(FVector2D(S.Location.X, S.Location.Y), Samples[S.PathIndex].P, Samples[Next].P, T);
			bInCorridor &= Off <= FMath::Max(Samples[S.PathIndex].Width, Samples[Next].Width) * 0.5 - 100.0;
			bOnPlainSamples &= (Samples[S.PathIndex].Flags & (PathFlags::Special | PathFlags::Lane)) == 0 && !IsWetBiome(Samples[S.PathIndex].Biome);
		}
		TestTrue(Ctx + TEXT(": las del suelo van dentro del cauce"), bInCorridor);
		TestTrue(Ctx + TEXT(": las del suelo, en tramos normales del camino"), bOnPlainSamples);
		TestTrue(Ctx + TEXT(": ninguna dentro de una caja de muerte"), bNoKill);

		// Alcanzables: arcos sobre huecos que se saltan y a la altura del salto; las de las murallas, sobre una brecha que
		// se salta o por la cornisa.
		bool bArcs = true, bWalls = true;
		for (const FShellSpawn& S : Plan)
		{
			const FVector2D At(S.Location.X, S.Location.Y);
			if (S.Spot == EShellSpot::JumpArc)
			{
				bool bFound = false;
				for (const FFeature& F : L.Features)
				{
					if (F.Type != EFeature::Gap || F.PathIndex != S.PathIndex || F.BranchIndex != S.BranchIndex) { continue; }
					bFound = true;
					const double Along = FMath::Abs(FVector2D::DotProduct(At - FVector2D(F.Location.X, F.Location.Y), F.Dir.GetSafeNormal()));
					const double Rise = S.Location.Z - F.Location.Z - TNScoreShells::Hover;
					bArcs &= F.Length <= SprintReach + 100.0 && Along <= F.Length * 0.5 + ShellDims::ArcOverLip + 1.0 && Rise >= -1.0 && Rise <= TurtleJump::Apex();
				}
				bArcs &= bFound;
			}
			else if (S.Spot == EShellSpot::WallBreach || S.Spot == EShellSpot::WallLedge)
			{
				bool bFound = false;
				for (const FFeature& F : L.Features)
				{
					if (F.Type != EFeature::WallBreach || F.PathIndex != S.PathIndex) { continue; }
					if (FVector2D::Distance(At, FVector2D(F.Location.X, F.Location.Y)) > F.Length * 0.5 + F.Width * 0.5 + 5.0) { continue; }
					const bool bGap = WallBreachDims::KindOf(F) == EWallBreach::Gap;
					if (bGap != (S.Spot == EShellSpot::WallBreach)) { continue; }
					bFound = true;
					if (bGap)
					{
						// En el aire sobre la brecha (que se salta) y a la altura de un salto desde el adarve.
						bWalls &= F.Length <= 0.75 * SprintReach && S.Location.Z - F.Location.Z <= TNScoreShells::Hover + TurtleJump::Apex()
							&& TNShellPlanTest::InAnyKillBox(L, FVector(At, F.Location.Z - 80.0));
					}
					else
					{
						// Sobre la cornisa, que no es zona de muerte.
						bWalls &= FMath::IsNearlyEqual(S.Location.Z, F.Location.Z + TNScoreShells::Hover, 1.0)
							&& !TNShellPlanTest::InAnyKillBox(L, FVector(At, F.Location.Z + 5.0));
					}
				}
				bWalls &= bFound;
			}
		}
		TestTrue(Ctx + TEXT(": los arcos van sobre huecos que se saltan, sin subir más que el salto"), bArcs);
		TestTrue(Ctx + TEXT(": las de las murallas, sobre una brecha que se salta o por una cornisa pisable"), bWalls);

		// Lo que ya ha puesto el servidor (Occupied) se respeta.
		{
			TArray<FVector> Occupied;
			for (const FShellSpawn& S : Plan)
			{
				if (S.Spot == EShellSpot::Trail && Occupied.Num() < 20) { Occupied.Add(FVector(S.Location.X, S.Location.Y, 300.0)); }
			}
			TArray<FShellSpawn> Around;
			PlanShells(L, Occupied, Around);
			bool bRespects = true;
			for (const FShellSpawn& S : Around)
			{
				if (!S.bOnGround) { continue; }
				for (const FVector& O : Occupied)
				{
					bRespects &= FVector2D::Distance(FVector2D(S.Location.X, S.Location.Y), FVector2D(O.X, O.Y)) >= O.Z - 1.0;
				}
			}
			TestTrue(Ctx + TEXT(": no pisa lo que ya ha puesto el servidor"), bRespects);
		}

		AddInfo(FString::Printf(TEXT("%s: camino %.1f km, %d pequeñas, %d grandes y %d reinas."), *Ctx, L.MainLength() / 100000.0, Tiers[0], Tiers[2], Tiers[3]));
	}

	TestTrue(TEXT("se generaron mapas"), Layouts > 0);
	AddInfo(FString::Printf(TEXT("En %d mapas (%.0f km de camino principal): %d pequeñas (%.1f por km), %d grandes y %d reinas."),
		Layouts, KmTotal, TierTotals[0], KmTotal > 0.0 ? TierTotals[0] / KmTotal : 0.0, TierTotals[2], TierTotals[3]));
	FString BySpot;
	for (int32 s = 0; s < static_cast<int32>(EShellSpot::Count); ++s)
	{
		if (SpotTotals[s] > 0) { BySpot += FString::Printf(TEXT("%s%s %d"), BySpot.IsEmpty() ? TEXT("") : TEXT(", "), ShellSpotName(static_cast<EShellSpot>(s)), SpotTotals[s]); }
	}
	AddInfo(TEXT("Por sitio: ") + BySpot);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
