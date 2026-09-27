// Lógica pura de la playa del modo carrera (TNBeachLayout): terreno fijo, salida, meta, zambullida y reparto por ronda.
// Sin mundo ni actores: se prueba el mismo código que usa ATN_BeachRaceGenerator. Correr desde Session Frontend
// (categoría "Tortunabo.Beach") o sin ventana:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "World/Beach/TN_BeachLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNBeachLayoutTest
{
	bool SameItem(const TNBeachLayout::FItem& A, const TNBeachLayout::FItem& B)
	{
		return A.Element == B.Element && A.Pos.Equals(B.Pos, 0.001) && FMath::IsNearlyEqual(A.Yaw, B.Yaw, 1e-6)
			&& FMath::IsNearlyEqual(A.Radius, B.Radius, 1e-6) && FMath::IsNearlyEqual(A.HalfLength, B.HalfLength, 1e-6)
			&& A.Spec.Seed == B.Spec.Seed && A.Spec.SizeScale == B.Spec.SizeScale && A.Spec.Extent == B.Spec.Extent;
	}

	bool SameLayout(const TNBeachLayout::FRoundLayout& A, const TNBeachLayout::FRoundLayout& B)
	{
		if (A.Items.Num() != B.Items.Num() || A.Stamps.Num() != B.Stamps.Num()) { return false; }
		for (int32 i = 0; i < A.Items.Num(); ++i)
		{
			if (!SameItem(A.Items[i], B.Items[i])) { return false; }
		}
		for (int32 i = 0; i < A.Stamps.Num(); ++i)
		{
			const TNBeachLayout::FStamp& SA = A.Stamps[i];
			const TNBeachLayout::FStamp& SB = B.Stamps[i];
			if (!SA.A.Equals(SB.A, 0.001) || !SA.B.Equals(SB.B, 0.001) || !FMath::IsNearlyEqual(SA.LevelZ, SB.LevelZ, 1e-6) || !FMath::IsNearlyEqual(SA.Radius, SB.Radius, 1e-6))
			{
				return false;
			}
		}
		return true;
	}

	/** La línea de P0 a P1 pasa por encima del suelo (con Margin) en todo el tramo de playa que cruza. */
	bool LineClearsGround(const FVector& P0, const FVector& P1, double Margin)
	{
		const FVector D = P1 - P0;
		const int32 Steps = FMath::CeilToInt32(FMath::Abs(D.X) / 250.0);
		for (int32 s = 1; s < Steps; ++s)
		{
			const FVector P = P0 + D * (static_cast<double>(s) / Steps);
			if (P.X < P0.X + 500.0 || P.X > TNBeachLayout::EdgeX(P.Y)) { continue; }
			if (P.Z < TNBeachLayout::GroundZ(P.X, P.Y) + Margin) { return false; }
		}
		return true;
	}

	double SlopeDegAt(double X, double Y)
	{
		constexpr double H = 50.0;
		const double Dx = (TNBeachLayout::GroundZ(X + H, Y) - TNBeachLayout::GroundZ(X - H, Y)) / (2.0 * H);
		const double Dy = (TNBeachLayout::GroundZ(X, Y + H) - TNBeachLayout::GroundZ(X, Y - H)) / (2.0 * H);
		return FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(Dx * Dx + Dy * Dy)));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Terreno fijo, salida, meta y zambullida
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachTerrainTest,
	"Tortunabo.Beach.Terrain",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachTerrainTest::RunTest(const FString& Parameters)
{
	using TNBeachLayout::GroundZ;

	// Perfil: baja hacia el mar y acaba en el borde a la altura del acantilado del contrato.
	TestEqual(TEXT("el borde queda CliffHeight sobre el agua"), TNBeachLayout::CliffTopZ - TNBeachLayout::WaterZ, TNBeach::CliffHeight);
	TestTrue(TEXT("la arena baja hacia el mar"), TNBeachLayout::ProfileZ(0.0) > TNBeachLayout::ProfileZ(60000.0)
		&& TNBeachLayout::ProfileZ(60000.0) > TNBeachLayout::ProfileZ(TNBeachLayout::Length));
	TestTrue(TEXT("la salida, 36 m por encima del borde"), FMath::IsNearlyEqual(TNBeachLayout::ProfileZ(0.0) - TNBeachLayout::CliffTopZ, TNBeachLayout::BeachDrop, 1.0));
	for (double Y = -12000.0; Y <= 12000.0; Y += 3000.0)
	{
		const double Edge = TNBeachLayout::EdgeX(Y);
		TestTrue(FString::Printf(TEXT("borde a 1200 m ± 2,5 m (Y %.0f)"), Y), FMath::Abs(Edge - TNBeachLayout::Length) <= TNBeachLayout::EdgeWobble + 1.0);
		const double Lip = GroundZ(Edge - 10.0, Y);
		TestTrue(FString::Printf(TEXT("la repisa de roca, a ~15,5 m del agua (Y %.0f)"), Y),
			Lip > TNBeachLayout::CliffTopZ && Lip < TNBeachLayout::CliffTopZ + TNBeachLayout::RockRise + 60.0);
		TestTrue(FString::Printf(TEXT("11 m de agua al pie (Y %.0f)"), Y), TNBeachLayout::SeabedZ(Edge + 300.0, Y) < TNBeachLayout::WaterZ - 900.0);
	}

	// Dunas suaves en toda la playa jugable.
	double MaxSlope = 0.0;
	for (double X = 3000.0; X < TNBeachLayout::Length - TNBeachLayout::RockStart; X += 500.0)
	{
		for (double Y = -TNBeachLayout::HalfWidth + 500.0; Y <= TNBeachLayout::HalfWidth - 500.0; Y += 500.0)
		{
			MaxSlope = FMath::Max(MaxSlope, TNBeachLayoutTest::SlopeDegAt(X, Y));
		}
	}
	TestTrue(FString::Printf(TEXT("dunas suaves (pendiente máxima %.1f° < 30°)"), MaxSlope), MaxSlope < 30.0);

	// Salida: cuatro sitios distintos, en llano, detrás de todo el reparto; desde cada uno se ve el mar por encima del
	// borde y las banderas de meta que flotan.
	for (int32 i = 0; i < TNBeachLayout::NumStartSpots; ++i)
	{
		const FVector Spot = TNBeachLayout::StartSpot(i);
		TestTrue(FString::Printf(TEXT("salida %d por detrás del reparto (40 m)"), i), Spot.X <= TNBeachLayout::ItemsStartX - 4000.0);
		TestTrue(FString::Printf(TEXT("salida %d delante del muro de detrás"), i), Spot.X > TNBeachLayout::BackWallX + 500.0);
		TestTrue(FString::Printf(TEXT("salida %d casi en llano (la cuesta suave de la playa)"), i), TNBeachLayoutTest::SlopeDegAt(Spot.X, Spot.Y) < 4.0);
		for (int32 j = 0; j < i; ++j)
		{
			TestTrue(FString::Printf(TEXT("salidas %d y %d separadas"), i, j), FVector::Dist2D(Spot, TNBeachLayout::StartSpot(j)) >= 900.0);
		}
		const FVector Eye = Spot + FVector(0.0, 0.0, 400.0);
		TestTrue(FString::Printf(TEXT("desde la salida %d se ve el mar por encima del borde"), i),
			TNBeachLayoutTest::LineClearsGround(Eye, FVector(TNBeachLayout::Length + 200000.0, Spot.Y, TNBeachLayout::WaterZ), 50.0));
		TestTrue(FString::Printf(TEXT("desde la salida %d se ven las banderas de meta"), i),
			TNBeachLayoutTest::LineClearsGround(Eye, FVector(TNBeachLayout::Length + 2600.0, Spot.Y, TNBeachLayout::WaterZ + 2800.0), 50.0));
	}

	// Meta: el agua más allá del filo; la repisa no.
	const double Edge0 = TNBeachLayout::EdgeX(0.0);
	TestTrue(TEXT("agua de meta al pie del acantilado"), TNBeachLayout::IsFinishWaterLocal(FVector(Edge0 + 800.0, 0.0, -20.0)));
	TestFalse(TEXT("la repisa no es meta"), TNBeachLayout::IsFinishWaterLocal(FVector(Edge0 - 300.0, 0.0, GroundZ(Edge0 - 300.0, 0.0))));
	TestFalse(TEXT("cayendo aún no es meta"), TNBeachLayout::IsFinishWaterLocal(FVector(Edge0 + 800.0, 0.0, 600.0)));

	// Zambullida: los últimos 7,5 m de la repisa y el vacío sobre el agua.
	TestTrue(TEXT("zambullida al borde"), TNBeachLayout::IsCliffJumpZoneLocal(FVector(Edge0 - 500.0, 0.0, GroundZ(Edge0 - 500.0, 0.0) + 70.0)));
	TestTrue(TEXT("zambullida sobre el vacío"), TNBeachLayout::IsCliffJumpZoneLocal(FVector(Edge0 + 1000.0, 0.0, 800.0)));
	TestFalse(TEXT("sin zambullida lejos del borde"), TNBeachLayout::IsCliffJumpZoneLocal(FVector(Edge0 - 2000.0, 0.0, GroundZ(Edge0 - 2000.0, 0.0) + 70.0)));
	TestFalse(TEXT("sin zambullida dentro del agua"), TNBeachLayout::IsCliffJumpZoneLocal(FVector(Edge0 + 1000.0, 0.0, -100.0)));

	// Asientos: el suelo bajo la huella queda liso a la cota natural del centro, fuera del borde vuelve a la arena natural y
	// el borde se sube andando. En sitios de mucho relieve: la cuesta del principio, junto a la selva y un alargado.
	struct FStampCase
	{
		ETNBeachElement Element;
		FVector2D Pos;
		double HalfLength;
	};
	const FStampCase Cases[] = {
		{ ETNBeachElement::SandDungeon, FVector2D(30000.0, 0.0), 0.0 },
		{ ETNBeachElement::SandDungeon, FVector2D(60000.0, 9500.0), 0.0 },
		{ ETNBeachElement::RockCluster, FVector2D(20000.0, 12000.0), 0.0 },
		{ ETNBeachElement::Coconut, FVector2D(45000.0, -13000.0), 0.0 },
		{ ETNBeachElement::Boardwalk, FVector2D(26000.0, 3000.0), 3000.0 },
		{ ETNBeachElement::QuadLane, FVector2D(40000.0, 0.0), TNBeachLayout::HalfWidth },
	};
	for (const FStampCase& Case : Cases)
	{
		TNBeachLayout::FItem Item;
		Item.Element = Case.Element;
		Item.Pos = Case.Pos;
		Item.Yaw = Case.Element == ETNBeachElement::QuadLane ? 90.0 : 0.0;
		Item.Radius = TNBeach::FootprintRadius(Case.Element);
		Item.HalfLength = Case.HalfLength;
		TArray<TNBeachLayout::FStamp> Stamps;
		Stamps.Add(TNBeachLayout::MakeStamp(Item));
		const TNBeachLayout::FStamp& Stamp = Stamps[0];
		auto Seated = [&Stamps](const FVector2D& P) { return TNBeachLayout::StampedZ(Stamps, P.X, P.Y, TNBeachLayout::GroundZ(P.X, P.Y)); };
		const FString Name = UEnum::GetValueAsString(Case.Element);
		TestTrue(Name + TEXT(": el centro, a la cota natural (donde va su origen)"),
			FMath::IsNearlyEqual(Seated(Case.Pos), TNBeachLayout::GroundZ(Case.Pos.X, Case.Pos.Y), 0.01));
		double MaxInside = 0.0;
		double MaxBorder = 0.0;
		for (int32 a = 0; a < 16; ++a)
		{
			const double Ang = TNProcMap::TwoPi * a / 16.0;
			const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
			const FVector2D Base = Item.EndA() + (Item.EndB() - Item.EndA()) * (a % 3 == 0 ? 0.0 : (a % 3 == 1 ? 0.5 : 1.0));
			// Dentro: liso (a nivel en los redondos; en los alargados, la cuesta suave de la playa).
			const double Z0 = Seated(Base);
			const double Z1 = Seated(Base + Dir * (Item.Radius * 0.9));
			MaxInside = FMath::Max(MaxInside, FMath::Abs(Z1 - Z0) / FMath::Max(1.0, Item.Radius * 0.9));
			// Borde: de la huella a la arena natural.
			for (double R = Item.Radius; R < Item.Radius + Stamp.Blend; R += 20.0)
			{
				const double Za = Seated(Base + Dir * R);
				const double Zb = Seated(Base + Dir * (R + 20.0));
				MaxBorder = FMath::Max(MaxBorder, FMath::RadiansToDegrees(FMath::Atan(FMath::Abs(Zb - Za) / 20.0)));
			}
			const FVector2D Outside = Base + Dir * (Item.Radius + Stamp.Blend + 10.0);
			double T = 0.0;
			if (TNProcMap::DistPointSegment(Outside, Item.EndA(), Item.EndB(), T) >= Item.Radius + Stamp.Blend)
			{
				TestTrue(Name + TEXT(": fuera del borde, la arena natural"), FMath::IsNearlyEqual(Seated(Outside), TNBeachLayout::GroundZ(Outside.X, Outside.Y), 0.01));
			}
		}
		TestTrue(FString::Printf(TEXT("%s: liso por dentro (pendiente %.3f)"), *Name, MaxInside), MaxInside < 0.06);
		TestTrue(FString::Printf(TEXT("%s: el borde se sube andando (%.1f°)"), *Name, MaxBorder), MaxBorder < 40.0);
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Reparto: determinismo
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachLayoutDeterminismTest,
	"Tortunabo.Beach.Layout.Determinism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachLayoutDeterminismTest::RunTest(const FString& Parameters)
{
	for (const int32 Seed : { 1, 7, 12345, -99, 2026 })
	{
		TNBeachLayout::FRoundLayout A;
		TNBeachLayout::FRoundLayout B;
		TNBeachLayout::GenerateRound(Seed, A);
		TNBeachLayout::GenerateRound(Seed, B);
		TestTrue(FString::Printf(TEXT("semilla %d: el mismo reparto dos veces"), Seed), TNBeachLayoutTest::SameLayout(A, B));
		TestTrue(FString::Printf(TEXT("semilla %d: hay reparto"), Seed), A.Items.Num() > 30);
	}
	TNBeachLayout::FRoundLayout One;
	TNBeachLayout::FRoundLayout Two;
	TNBeachLayout::GenerateRound(1, One);
	TNBeachLayout::GenerateRound(2, Two);
	TestFalse(TEXT("semillas distintas, repartos distintos"), TNBeachLayoutTest::SameLayout(One, Two));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Reparto: reglas sobre muchas semillas
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachLayoutRulesTest,
	"Tortunabo.Beach.Layout.Rules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachLayoutRulesTest::RunTest(const FString& Parameters)
{
	using TNBeachLayout::FItem;
	using TNBeachLayout::EItemRole;

	// Todo lo del contrato que no tiene pasada propia puede salir en las bandas (lo nuevo entra solo).
	for (int32 i = 0; i < static_cast<int32>(ETNBeachElement::Count); ++i)
	{
		const ETNBeachElement E = static_cast<ETNBeachElement>(i);
		const TNBeachLayout::FElementRule Rule = TNBeachLayout::RuleOf(E);
		TestTrue(FString::Printf(TEXT("%s se reparte"), *UEnum::GetValueAsString(E)), Rule.bSpecial || (Rule.Weight > 0.0 && Rule.MinT < Rule.MaxT && Rule.MaxPerRound > 0));
	}

	constexpr int32 NumSeeds = 40;
	double AreaFirst = 0.0;
	double AreaLast = 0.0;
	double SeawardSum = 0.0;
	int32 SeawardNum = 0;
	TArray<int32> Seen;
	Seen.Init(0, static_cast<int32>(ETNBeachElement::Count));

	for (int32 s = 0; s < NumSeeds; ++s)
	{
		const int32 Seed = 1000 + s * 7919;
		const FString Ctx = FString::Printf(TEXT("semilla %d"), Seed);
		TNBeachLayout::FRoundLayout L;
		TNBeachLayout::GenerateRound(Seed, L);
		TestTrue(Ctx + TEXT(": paso libre"), L.bPassageOk);

		int32 Dungeons = 0;
		int32 Lanes = 0;
		int32 Gulls = 0;
		int32 Grounded = 0;
		bool bBounds = true;
		bool bNoOverlap = true;
		bool bFarFromStart = true;
		bool bSpecOk = true;
		bool bLanesOk = true;
		TNBeachLayout::FPassGrid Grid;
		Grid.Init();
		for (int32 i = 0; i < L.Items.Num(); ++i)
		{
			const FItem& It = L.Items[i];
			++Seen[static_cast<int32>(It.Element)];
			Grid.Stamp(It, 1);
			bSpecOk &= It.Spec.Element == It.Element && It.Spec.SizeScale >= 0.59f && It.Spec.SizeScale <= 1.41f;
			bSpecOk &= FMath::IsNearlyEqual(static_cast<double>(It.Spec.Extent), It.HalfLength * 2.0, 1.0);
			if (!It.bOverlay)
			{
				bBounds &= TNBeachLayout::InBounds(It);
				++Grounded;
			}
			for (int32 j = 0; j < i; ++j)
			{
				const FItem& Other = L.Items[j];
				if (Other.bOverlay == It.bOverlay) { bNoOverlap &= TNBeachLayout::Clearance(It, Other) >= -1.0; }
			}
			if (!It.bOverlay)
			{
				for (int32 k = 0; k < TNBeachLayout::NumStartSpots; ++k)
				{
					const FVector2D Spot(TNBeachLayout::StartSpot(k));
					double T = 0.0;
					bFarFromStart &= TNProcMap::DistPointSegment(Spot, It.EndA(), It.EndB(), T) - It.Radius >= 4000.0;
				}
			}
			switch (It.Element)
			{
				case ETNBeachElement::SandDungeon: ++Dungeons; break;
				case ETNBeachElement::GullZone: ++Gulls; break;
				case ETNBeachElement::QuadLane:
					++Lanes;
					bLanesOk &= FMath::IsNearlyEqual(static_cast<double>(It.Spec.Extent), TNBeach::CourseWidth, 1.0) && FMath::IsNearlyEqual(It.Yaw, 90.0, 0.01);
					break;
				default: break;
			}
			if (It.Element == ETNBeachElement::GiantCrab || It.Element == ETNBeachElement::SeaUrchin)
			{
				SeawardSum += TNBeachLayout::ProgressOfX(It.Pos.X);
				++SeawardNum;
			}
			if (!It.bOverlay && It.Role == EItemRole::Fill)
			{
				const double T = TNBeachLayout::ProgressOfX(It.Pos.X);
				if (T < 1.0 / 3.0) { AreaFirst += It.Area(); }
				if (T > 2.0 / 3.0) { AreaLast += It.Area(); }
			}
		}
		TestTrue(Ctx + TEXT(": todo dentro de la playa repartible (40 m de la salida, 30 m del borde)"), bBounds);
		TestTrue(Ctx + TEXT(": sin solapes"), bNoOverlap);
		TestTrue(Ctx + TEXT(": nada a menos de 40 m de la salida"), bFarFromStart);
		TestTrue(Ctx + TEXT(": especificaciones coherentes"), bSpecOk);
		TestEqual(Ctx + TEXT(": un castillo con salas"), Dungeons, 1);
		TestTrue(Ctx + TEXT(": 2-3 pasos de quads a lo ancho"), Lanes >= 2 && Lanes <= 3 && bLanesOk);
		TestTrue(Ctx + TEXT(": 2-4 zonas de gaviotas"), Gulls >= 2 && Gulls <= 4);
		TestEqual(Ctx + TEXT(": un asiento por elemento en el suelo"), L.Stamps.Num(), Grounded);
		TestTrue(Ctx + TEXT(": el paso sigue libre al rehacer la rejilla"), Grid.IsConnected());
		TestTrue(Ctx + TEXT(": castillo hacia la mitad"), L.DungeonPos.X > TNBeachLayout::XOfProgress(0.35) && L.DungeonPos.X < TNBeachLayout::XOfProgress(0.65));
	}

	TestTrue(FString::Printf(TEXT("más denso hacia el mar (%.0f m² en el último tercio frente a %.0f m² en el primero)"), AreaLast / 1e4, AreaFirst / 1e4),
		AreaLast > 1.5 * AreaFirst);
	TestTrue(TEXT("cangrejos y erizos, más cerca del mar"), SeawardNum > 0 && SeawardSum / SeawardNum > 0.55);

	FString Counts;
	for (int32 i = 0; i < Seen.Num(); ++i)
	{
		Counts += FString::Printf(TEXT("%s%s %d"), Counts.IsEmpty() ? TEXT("") : TEXT(", "), *UEnum::GetValueAsString(static_cast<ETNBeachElement>(i)), Seen[i]);
	}
	AddInfo(FString::Printf(TEXT("Elementos en %d rondas: %s"), NumSeeds, *Counts));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
