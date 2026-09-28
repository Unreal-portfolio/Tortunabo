// Lógica pura de la playa del modo carrera (TNBeachLayout): terreno fijo (relieve, corredores, crestas, pozas y
// trincheras), salida, sprint, meta, zambullida y reparto por ronda. Sin mundo ni actores: se prueba el mismo código que
// usa ATN_BeachRaceGenerator. Correr desde Session Frontend (categoría "Tortunabo.Beach") o sin ventana:
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
			&& FMath::IsNearlyEqual(A.Radius, B.Radius, 1e-6) && FMath::IsNearlyEqual(A.Core, B.Core, 1e-6) && FMath::IsNearlyEqual(A.HalfLength, B.HalfLength, 1e-6)
			&& A.Spec.Seed == B.Spec.Seed && A.Spec.SizeScale == B.Spec.SizeScale && A.Spec.Extent == B.Spec.Extent && A.Role == B.Role;
	}

	bool SameLayout(const TNBeachLayout::FRoundLayout& A, const TNBeachLayout::FRoundLayout& B)
	{
		if (A.Items.Num() != B.Items.Num() || A.Stamps.Num() != B.Stamps.Num() || A.Interest.Num() != B.Interest.Num()) { return false; }
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
		for (int32 i = 0; i < A.Interest.Num(); ++i)
		{
			if (A.Interest[i].Kind != B.Interest[i].Kind || !A.Interest[i].Pos.Equals(B.Interest[i].Pos, 0.001) || !A.Interest[i].To.Equals(B.Interest[i].To, 0.001))
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

	/**
	 * Tramos rectos libres hacia el mar (como PlugStraightLines): cada 4 m a lo ancho, el tramo más largo sin nada de lo que
	 * ocupa cada elemento (ni pasos de quads), pozas, trincheras ni crestas. Devuelve cuántas filas pasan de MaxRun.
	 */
	int32 CountLongStraightRuns(const TNBeachLayout::FRoundLayout& L, double MaxRun, double& OutLongest)
	{
		using namespace TNBeachLayout;
		constexpr double Cell = 200.0;
		const double Usable = SideReach - 100.0;
		const int32 NXc = FMath::CeilToInt32((ItemsEndX - ItemsStartX) / Cell);
		const int32 NYc = FMath::CeilToInt32(2.0 * Usable / Cell);
		TArray<uint8> Occ;
		Occ.Init(0, NXc * NYc);
		auto Mark = [&Occ, NXc, NYc, Usable](const FVector2D& A, const FVector2D& B, double R)
		{
			const int32 IX0 = FMath::Max(0, FMath::FloorToInt32((FMath::Min(A.X, B.X) - R - ItemsStartX) / Cell));
			const int32 IX1 = FMath::Min(NXc - 1, FMath::CeilToInt32((FMath::Max(A.X, B.X) + R - ItemsStartX) / Cell));
			const int32 IY0 = FMath::Max(0, FMath::FloorToInt32((FMath::Min(A.Y, B.Y) - R + Usable) / Cell));
			const int32 IY1 = FMath::Min(NYc - 1, FMath::CeilToInt32((FMath::Max(A.Y, B.Y) + R + Usable) / Cell));
			for (int32 IY = IY0; IY <= IY1; ++IY)
			{
				for (int32 IX = IX0; IX <= IX1; ++IX)
				{
					double T = 0.0;
					if (TNProcMap::DistPointSegment(FVector2D(ItemsStartX + (IX + 0.5) * Cell, -Usable + (IY + 0.5) * Cell), A, B, T) <= R) { Occ[IY * NXc + IX] = 1; }
				}
			}
		};
		for (const FItem& Item : L.Items)
		{
			if (!Item.bOverlay) { Mark(Item.EndA(), Item.EndB(), Item.Core); }
		}
		MarkTerrainObstacles(Mark);
		int32 Long = 0;
		OutLongest = 0.0;
		for (int32 IY = 1; IY < NYc; IY += 2)
		{
			int32 Run = 0;
			int32 Best = 0;
			for (int32 IX = 0; IX < NXc; ++IX)
			{
				Run = Occ[IY * NXc + IX] ? 0 : Run + 1;
				Best = FMath::Max(Best, Run);
			}
			OutLongest = FMath::Max(OutLongest, Best * Cell);
			if (Best * Cell > MaxRun) { ++Long; }
		}
		return Long;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Terreno fijo: perfil, salida, meta, zambullida y asientos
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

	// Salida: cuatro huevos distintos, en llano, detrás de todo el reparto; desde cada uno se ve el mar por encima del borde
	// y las banderas de meta que flotan.
	for (int32 i = 0; i < TNBeachLayout::NumStartSpots; ++i)
	{
		const FVector Spot = TNBeachLayout::StartSpot(i);
		TestTrue(FString::Printf(TEXT("salida %d por detrás del reparto (60 m)"), i), Spot.X <= TNBeachLayout::ItemsStartX - 6000.0);
		TestTrue(FString::Printf(TEXT("salida %d delante del muro de detrás"), i), Spot.X > TNBeachLayout::BackWallX + 500.0);
		TestTrue(FString::Printf(TEXT("salida %d casi en llano (la cuesta suave de la playa)"), i), TNBeachLayoutTest::SlopeDegAt(Spot.X, Spot.Y) < 4.0);
		TestTrue(FString::Printf(TEXT("salida %d, en la zona que lanzan los huevos"), i), Spot.X > TNBeachLayout::BackWallX && Spot.X < TNBeachLayout::EggLaunchReachX);
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

	// Sprint de desempate: a mitad del recorrido, todos los sitios en arena seca y casi llana.
	const double SprintX = TNBeachLayout::SprintLineX();
	TestTrue(FString::Printf(TEXT("sprint a mitad del recorrido (%.0f m)"), SprintX / 100.0), FMath::Abs(SprintX - 0.5 * TNBeachLayout::Length) <= 3000.0);
	for (int32 i = 0; i < TNBeachLayout::NumStartSpots * 3; ++i)
	{
		const FVector Spot = TNBeachLayout::SprintSpot(i);
		TestTrue(FString::Printf(TEXT("sitio %d del sprint en arena seca y llana"), i), TNBeachLayout::IsDryFlatSpot(FVector2D(Spot.X, Spot.Y)));
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

	// Asientos: el suelo bajo la huella queda liso y a nivel a la cota natural del centro, fuera del borde vuelve a la arena
	// natural y el borde se sube andando; el paso de quads no cambia la altura (solo oscurece).
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
		Item.Core = Item.Radius;
		Item.HalfLength = Case.HalfLength;
		TArray<TNBeachLayout::FStamp> Stamps;
		Stamps.Add(TNBeachLayout::MakeStamp(Item));
		const TNBeachLayout::FStamp& Stamp = Stamps[0];
		auto Seated = [&Stamps](const FVector2D& P) { return TNBeachLayout::StampedZ(Stamps, P.X, P.Y, TNBeachLayout::GroundZ(P.X, P.Y)); };
		const FString Name = UEnum::GetValueAsString(Case.Element);
		TestTrue(Name + TEXT(": el centro, a la cota de su asiento (donde va su origen)"),
			FMath::IsNearlyEqual(Seated(Case.Pos), TNBeachLayout::PlacementZ(Item), 0.01));
		if (Stamp.bTintOnly)
		{
			bool bUnchanged = true;
			float Tint = 0.f;
			for (double Y = -12000.0; Y <= 12000.0; Y += 1500.0)
			{
				const FVector2D P(Case.Pos.X + 300.0, Y);
				bUnchanged &= FMath::IsNearlyEqual(Seated(P), TNBeachLayout::GroundZ(P.X, P.Y), 0.01);
			}
			TNBeachLayout::StampedZ(Stamps, Case.Pos.X, 0.0, 0.0, &Tint);
			TestTrue(Name + TEXT(": no cambia el suelo"), bUnchanged);
			TestTrue(Name + TEXT(": oscurece sus rodadas"), Tint > 0.f);
			continue;
		}
		double MaxInside = 0.0;
		double MaxBorder = 0.0;
		for (int32 a = 0; a < 16; ++a)
		{
			const double Ang = TNProcMap::TwoPi * a / 16.0;
			const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
			const FVector2D Base = Item.EndA() + (Item.EndB() - Item.EndA()) * (a % 3 == 0 ? 0.0 : (a % 3 == 1 ? 0.5 : 1.0));
			// Dentro: a nivel.
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
		TestTrue(FString::Printf(TEXT("%s: liso por dentro (pendiente %.3f)"), *Name, MaxInside), MaxInside < 0.001);
		// Solo los asientos que el reparto admitiría (sin paredes): sobre una cresta, el reparto no lo pondría ahí.
		if (TNBeachLayout::SeatIsGentle(Item))
		{
			TestTrue(FString::Printf(TEXT("%s: el borde se sube andando (%.1f°)"), *Name, MaxBorder), MaxBorder < 44.0);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("%s en (%.0f, %.0f) m: el reparto no lo asentaría ahí (borde de %.1f°)."), *Name, Case.Pos.X / 100.0, Case.Pos.Y / 100.0, MaxBorder));
		}
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Terreno fijo: relieve, corredores, crestas, pozas y trincheras
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachReliefTest,
	"Tortunabo.Beach.Relief",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachReliefTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachLayout;

	// Irregular pero andable: nada por encima de ~42° (la tortuga sube hasta ~44°), bastante relieve y cuestas variadas.
	double MaxSlope = 0.0;
	double Sum = 0.0;
	double SumSq = 0.0;
	int32 Samples = 0;
	int32 Steep = 0;
	for (double X = 3000.0; X < Length - RockStart; X += 700.0)
	{
		for (double Y = -HalfWidth + 500.0; Y <= HalfWidth - 500.0; Y += 700.0)
		{
			const double Slope = TNBeachLayoutTest::SlopeDegAt(X, Y);
			MaxSlope = FMath::Max(MaxSlope, Slope);
			Steep += Slope > 10.0 ? 1 : 0;
			const double Relief = SandZ(X, Y) - BaseZ(X, Y);
			Sum += Relief;
			SumSq += Relief * Relief;
			++Samples;
		}
	}
	const double Mean = Sum / Samples;
	const double Deviation = FMath::Sqrt(FMath::Max(0.0, SumSq / Samples - Mean * Mean));
	TestTrue(FString::Printf(TEXT("se sube andando (pendiente máxima %.1f° < 42°)"), MaxSlope), MaxSlope < 42.0);
	TestTrue(FString::Printf(TEXT("relieve irregular (desviación %.0f cm > 90)"), Deviation), Deviation > 90.0);
	TestTrue(FString::Printf(TEXT("cuestas variadas (%.0f %% por encima de 10°)"), 100.0 * Steep / Samples), Steep > Samples / 10);

	// Corredores: más bajos que las dunas de al lado donde van separados.
	double Lower = 0.0;
	int32 CorridorSamples = 0;
	for (double X = 12000.0; X < Length - 12000.0; X += 2000.0)
	{
		for (int32 K = 0; K < NumCorridors; ++K)
		{
			const FCorridorSample C = CorridorAt(K, X);
			if (C.Weight < 0.9 || (K < 2 && CorridorSplit(X) < 0.8)) { continue; }
			const double Floor = ReliefZ(X, C.Y);
			const double Sides = 0.5 * (ReliefZ(X, C.Y - 2.2 * C.HalfW) + ReliefZ(X, C.Y + 2.2 * C.HalfW));
			Lower += Sides - Floor;
			++CorridorSamples;
		}
	}
	TestTrue(FString::Printf(TEXT("hay corredores separados (%d muestras)"), CorridorSamples), CorridorSamples > 40);
	TestTrue(FString::Printf(TEXT("los corredores van más bajos que las dunas de al lado (%.0f cm de media)"), Lower / FMath::Max(1, CorridorSamples)),
		CorridorSamples > 0 && Lower / CorridorSamples > 60.0);

	// Crestas: altas, con su cara empinada hacia la salida en las que cruzan, dentro de la playa; alguna con cornisa.
	int32 Lipped = 0;
	for (int32 r = 0; r < Ridges().Num(); ++r)
	{
		const FRidge& Ridge = Ridges()[r];
		const FString Ctx = FString::Printf(TEXT("cresta %d"), r);
		TestTrue(Ctx + TEXT(": de 1,5 m o más"), Ridge.Height >= 150.0);
		TestTrue(Ctx + TEXT(": dentro de la playa"), FMath::Abs(Ridge.Center.Y) + Ridge.HalfLength * FMath::Abs(Ridge.Along.Y) <= HalfWidth);
		TestTrue(Ctx + TEXT(": después de la salida y antes de la roca"), Ridge.Center.X > 12000.0 && Ridge.Center.X < Length - 9000.0);
		if (!Ridge.bDivider) { TestTrue(Ctx + TEXT(": la cara empinada mira a la salida"), Ridge.Windward.X > 0.8); }
		Lipped += Ridge.bLip ? 1 : 0;
		// La cara de sotavento, a ~30° (la sola duna, sin el resto del relieve).
		const double H = RidgeCrestHeight(Ridge, 0.0);
		const FVector2D Crest = RidgeCrestPoint(Ridge, 0.0);
		const FVector2D SlipMid = Crest - Ridge.Windward * (0.5 * RidgeSlipWidth(H));
		const double Rise = RidgeZ(Ridge, SlipMid + Ridge.Windward * 25.0) - RidgeZ(Ridge, SlipMid - Ridge.Windward * 25.0);
		TestTrue(FString::Printf(TEXT("%s: sotavento a menos de 35° (%.1f°)"), *Ctx, FMath::RadiansToDegrees(FMath::Atan(Rise / 50.0))), Rise / 50.0 < FMath::Tan(FMath::DegreesToRadians(35.0)));
	}
	TestTrue(TEXT("hay crestas con cornisa"), Lipped >= 3 && LipSamples().Num() > 20);

	// Pozas: agua por debajo de toda su orilla, honda en medio (se nada), orillas que se suben andando y agua nadable.
	for (int32 p = 0; p < Pools().Num(); ++p)
	{
		const FPool& Pool = Pools()[p];
		const FString Ctx = FString::Printf(TEXT("poza %d"), p);
		double LowestRim = TNumericLimits<double>::Max();
		for (int32 k = 0; k < 32; ++k)
		{
			const FVector2D Q = PoolPoint(Pool, TNProcMap::TwoPi * k / 32.0, 1.05);
			LowestRim = FMath::Min(LowestRim, SandZ(Q.X, Q.Y));
		}
		TestTrue(FString::Printf(TEXT("%s: el agua no rebosa (orilla %.0f cm por encima)"), *Ctx, LowestRim - Pool.Water), LowestRim >= Pool.Water);
		TestTrue(FString::Printf(TEXT("%s: honda en medio (%.0f cm)"), *Ctx, Pool.Water - SandZ(Pool.Center.X, Pool.Center.Y)), Pool.Water - SandZ(Pool.Center.X, Pool.Center.Y) >= 130.0);
		TestTrue(Ctx + TEXT(": entre la salida y la roca"), Pool.Center.X - Pool.OuterR() > ItemsStartX && Pool.Center.X + Pool.OuterR() < Length - RockStart);
		double MaxBank = 0.0;
		for (int32 k = 0; k < 12; ++k)
		{
			const double Theta = TNProcMap::TwoPi * k / 12.0;
			for (double U = 0.3; U < 1.6; U += 0.05)
			{
				const FVector2D A = PoolPoint(Pool, Theta, U);
				const FVector2D B = PoolPoint(Pool, Theta, U + 0.05);
				const double Run = FMath::Max(1.0, FVector2D::Distance(A, B));
				MaxBank = FMath::Max(MaxBank, FMath::RadiansToDegrees(FMath::Atan(FMath::Abs(SandZ(B.X, B.Y) - SandZ(A.X, A.Y)) / Run)));
			}
		}
		TestTrue(FString::Printf(TEXT("%s: se sale andando (orilla a %.1f°)"), *Ctx, MaxBank), MaxBank < 40.0);
		TArray<FBox> Boxes;
		PoolSwimBoxes(Pool, Boxes);
		bool bBoxesOk = Boxes.Num() > 0;
		for (const FBox& Box : Boxes) { bBoxesOk &= FMath::IsNearlyEqual(Box.Max.Z, Pool.Water, 0.01) && Box.Min.Z < Pool.Water - 300.0; }
		TestTrue(Ctx + TEXT(": agua nadable hasta la superficie"), bBoxesOk);
		TestTrue(Ctx + TEXT(": es agua (PoolAt)"), PoolAt(Pool.Center, 1.0) == p);
	}

	// Trincheras: el canal cavado en su eje, entero junto a él y la arena natural lejos de todos los canales. Se mira una
	// rejilla alrededor de cada línea y no un punto fijo por delante: las dos líneas están a ~30 m y ese punto de la de
	// detrás caía a 5 m de la de delante.
	for (const FTrench& Trench : Trenches())
	{
		TestTrue(TEXT("trinchera de un lado a otro de buena parte de la playa"), Trench.Max.Y - Trench.Min.Y > 15000.0 && Trench.Points.Num() > 8);
		const FVector2D Mid = 0.5 * (Trench.Points[4] + Trench.Points[5]);
		TestTrue(TEXT("canal cavado en el eje"), FMath::IsNearlyEqual(NaturalZ(Mid.X, Mid.Y) - SandZ(Mid.X, Mid.Y), TrenchDig, 1.0));
		int32 FarSamples = 0;
		bool bFarNatural = true;
		bool bNearDug = true;
		for (double X = Trench.Min.X - 4000.0; X <= Trench.Max.X + 4000.0; X += 250.0)
		{
			for (double Y = Trench.Min.Y; Y <= Trench.Max.Y; Y += 1000.0)
			{
				const double Dist = TrenchDistance(FVector2D(X, Y));
				const double Carve = TrenchCarve(X, Y);
				if (Dist >= TrenchDigReach)
				{
					++FarSamples;
					bFarNatural &= Carve == 0.0;
				}
				else if (Dist <= TrenchDigFlat)
				{
					bNearDug &= FMath::IsNearlyEqual(Carve, TrenchDig, 0.01);
				}
			}
		}
		TestTrue(FString::Printf(TEXT("lejos de los canales, la arena natural (%d puntos)"), FarSamples), FarSamples > 100 && bFarNatural);
		TestTrue(TEXT("junto al eje, el canal entero"), bNearDug);
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
		TestTrue(FString::Printf(TEXT("semilla %d: hay reparto (%d elementos)"), Seed, A.Items.Num()), A.Items.Num() > 500);
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
	using TNBeachLayout::EInterestKind;

	// Todo lo del contrato que no tiene pasada propia puede salir en las bandas (lo nuevo entra solo).
	for (int32 i = 0; i < static_cast<int32>(ETNBeachElement::Count); ++i)
	{
		const ETNBeachElement E = static_cast<ETNBeachElement>(i);
		const TNBeachLayout::FElementRule Rule = TNBeachLayout::RuleOf(E);
		TestTrue(FString::Printf(TEXT("%s se reparte"), *UEnum::GetValueAsString(E)), Rule.bSpecial || (Rule.Weight > 0.0 && Rule.MinT < Rule.MaxT && Rule.MaxPerRound > 0));
	}

	constexpr int32 NumSeeds = 24;
	double AreaFirst = 0.0;
	double AreaLast = 0.0;
	double SeawardSum = 0.0;
	int32 SeawardNum = 0;
	int32 WithTwoDungeons = 0;
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
		int32 Seated = 0;
		int32 Catapults = 0;
		int32 Trampolines = 0;
		int32 Movers = 0;
		bool bBounds = true;
		bool bNoOverlap = true;
		bool bFarFromStart = true;
		bool bSpecOk = true;
		bool bLanesOk = true;
		bool bTerrainOk = true;
		bool bArcsFree = true;
		TArray<const FItem*> Gulls;
		TNBeachLayout::FPassGrid Grid;
		Grid.Init();
		for (int32 i = 0; i < L.Items.Num(); ++i)
		{
			const FItem& It = L.Items[i];
			++Seen[static_cast<int32>(It.Element)];
			Grid.Stamp(It, 1);
			bSpecOk &= It.Spec.Element == It.Element && It.Spec.SizeScale >= 0.59f && It.Spec.SizeScale <= 1.41f && It.Core <= It.Radius + 0.01;
			bSpecOk &= FMath::IsNearlyEqual(static_cast<double>(It.Spec.Extent), It.HalfLength * 2.0, 1.0);
			bTerrainOk &= TNBeachLayout::TerrainAllows(It) && TNBeachLayout::SeatIsGentle(It);
			if (!It.bOverlay) { bBounds &= TNBeachLayout::InBounds(It); }
			if (TNBeachLayout::HasSeat(It)) { ++Seated; }
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
					bFarFromStart &= TNProcMap::DistPointSegment(Spot, It.EndA(), It.EndB(), T) - It.Radius >= 6000.0;
				}
			}
			// El arco de salto de las catapultas y trampolines del relleno, libre de todo lo demás.
			if (TNBeachLayout::RuleOf(It.Element).bLauncher && It.Role != EItemRole::Launcher)
			{
				const FItem Arc = TNBeachLayout::FBuilder::JumpArcZone(It);
				for (int32 j = 0; j < L.Items.Num(); ++j)
				{
					if (j != i && !L.Items[j].bOverlay && TNBeachLayout::Clearance(Arc, L.Items[j]) < -1.0) { bArcsFree = false; }
				}
			}
			switch (It.Element)
			{
				case ETNBeachElement::SandDungeon: ++Dungeons; break;
				case ETNBeachElement::GullZone: Gulls.Add(&It); break;
				case ETNBeachElement::Catapult: ++Catapults; break;
				case ETNBeachElement::Trampoline: ++Trampolines; break;
				case ETNBeachElement::MovingPlatform: ++Movers; break;
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
				if (T < 1.0 / 3.0) { AreaFirst += It.CoreArea(); }
				if (T > 2.0 / 3.0) { AreaLast += It.CoreArea(); }
			}
		}
		// Gaviotas: repartidas y cada zona con su círculo.
		bool bGullsApart = true;
		bool bGullsDistinct = true;
		for (int32 a = 0; a < Gulls.Num(); ++a)
		{
			for (int32 b = 0; b < a; ++b)
			{
				bGullsApart &= FVector2D::Distance(Gulls[a]->Pos, Gulls[b]->Pos) >= 15000.0;
				bGullsDistinct &= FMath::Abs(Gulls[a]->Spec.SizeScale - Gulls[b]->Spec.SizeScale) > 0.01f;
			}
		}
		// Puntos interesantes.
		int32 Kinds[6] = { 0, 0, 0, 0, 0, 0 };
		for (const TNBeachLayout::FInterestPoint& Point : L.Interest) { ++Kinds[static_cast<int32>(Point.Kind)]; }
		double Longest = 0.0;
		const int32 LongRuns = TNBeachLayoutTest::CountLongStraightRuns(L, 1.6 * TNBeachLayout::MaxStraightRun, Longest);

		TestTrue(Ctx + TEXT(": todo dentro de la playa repartible (60 m de la salida, 30 m del borde)"), bBounds);
		TestTrue(Ctx + TEXT(": sin solapes"), bNoOverlap);
		TestTrue(Ctx + TEXT(": nada a menos de 60 m de la salida"), bFarFromStart);
		TestTrue(Ctx + TEXT(": especificaciones coherentes"), bSpecOk);
		TestTrue(Ctx + TEXT(": nada pisa pozas, trincheras ni cornisas, y sin asientos con paredes"), bTerrainOk);
		TestTrue(Ctx + TEXT(": arcos de salto libres"), bArcsFree);
		TestTrue(FString::Printf(TEXT("%s: 1-3 castillos con salas (%d)"), *Ctx, Dungeons), Dungeons >= 1 && Dungeons <= 3);
		WithTwoDungeons += Dungeons >= 2 ? 1 : 0;
		TestTrue(Ctx + TEXT(": 2-3 pasos de quads a lo ancho"), Lanes >= 2 && Lanes <= 3 && bLanesOk);
		TestTrue(FString::Printf(TEXT("%s: 4-6 zonas de gaviotas separadas y distintas (%d)"), *Ctx, Gulls.Num()), Gulls.Num() >= 4 && Gulls.Num() <= 6 && bGullsApart && bGullsDistinct);
		TestEqual(Ctx + TEXT(": un asiento por elemento que lo lleva"), L.Stamps.Num(), Seated);
		TestTrue(Ctx + TEXT(": el paso sigue libre al rehacer la rejilla"), Grid.IsConnected());
		TestTrue(Ctx + TEXT(": castillo principal hacia la mitad"), L.DungeonPos.X > TNBeachLayout::XOfProgress(0.35) && L.DungeonPos.X < TNBeachLayout::XOfProgress(0.65));
		TestTrue(FString::Printf(TEXT("%s: muchos enemigos (%d, %d cangrejos)"), *Ctx, L.NumEnemies, L.NumCrabs), L.NumEnemies >= 40 && L.NumCrabs >= 10);
		TestTrue(FString::Printf(TEXT("%s: lanzadores y plataformas (%d catapultas, %d trampolines, %d plataformas móviles)"), *Ctx, Catapults, Trampolines, Movers),
			Catapults >= 4 && Trampolines >= 10 && Movers >= 4);
		TestTrue(FString::Printf(TEXT("%s: filas que obligan a zigzaguear (%d)"), *Ctx, L.NumRows), L.NumRows >= 3);
		TestTrue(FString::Printf(TEXT("%s: piezas militares (%d)"), *Ctx, L.NumMilitary), L.NumMilitary >= 6);
		TestTrue(FString::Printf(TEXT("%s: ocupación (media %.0f %%, primer tercio %.0f %%)"), *Ctx, L.CoverMean * 100.0, L.CoverFirstThird * 100.0),
			L.CoverMean >= 0.24 && L.CoverFirstThird >= 0.18 && L.BandCover.Num() == TNBeachLayout::NumBands());
		TestTrue(FString::Printf(TEXT("%s: puntos interesantes (%d arcos, %d cimas, %d atajos, %d trincheras, %d caminos)"), *Ctx,
			Kinds[static_cast<int32>(EInterestKind::JumpArc)], Kinds[static_cast<int32>(EInterestKind::Summit)], Kinds[static_cast<int32>(EInterestKind::Shortcut)],
			Kinds[static_cast<int32>(EInterestKind::Trench)], Kinds[static_cast<int32>(EInterestKind::Detour)]),
			Kinds[static_cast<int32>(EInterestKind::JumpArc)] >= 10 && Kinds[static_cast<int32>(EInterestKind::Summit)] >= 10
			&& Kinds[static_cast<int32>(EInterestKind::Shortcut)] >= 3 && Kinds[static_cast<int32>(EInterestKind::Trench)] >= 10
			&& Kinds[static_cast<int32>(EInterestKind::Detour)] >= 10);
		TestTrue(FString::Printf(TEXT("%s: sin líneas rectas libres hacia el mar (%d filas de más de %.0f m; la más larga, %.0f m)"), *Ctx, LongRuns,
			1.6 * TNBeachLayout::MaxStraightRun / 100.0, Longest / 100.0), LongRuns <= 8);
	}

	TestTrue(FString::Printf(TEXT("casi siempre dos castillos con salas o más (%d de %d)"), WithTwoDungeons, NumSeeds), WithTwoDungeons * 10 >= NumSeeds * 7);
	TestTrue(FString::Printf(TEXT("más denso hacia el mar (%.0f m² en el último tercio frente a %.0f m² en el primero)"), AreaLast / 1e4, AreaFirst / 1e4),
		AreaLast > 1.05 * AreaFirst);
	TestTrue(TEXT("cangrejos y erizos, más cerca del mar"), SeawardNum > 0 && SeawardSum / SeawardNum > 0.5);

	FString Counts;
	for (int32 i = 0; i < Seen.Num(); ++i)
	{
		Counts += FString::Printf(TEXT("%s%s %d"), Counts.IsEmpty() ? TEXT("") : TEXT(", "), *UEnum::GetValueAsString(static_cast<ETNBeachElement>(i)), Seen[i]);
	}
	AddInfo(FString::Printf(TEXT("Elementos en %d rondas: %s"), NumSeeds, *Counts));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
