// ─────────────────────────────────────────────────────────────────────────────
// ATN_ProcMapGenerator — construcción de geometría: terreno, agua, límites y
// estructuras (tableros colosales, techos de cueva, isletas, labios, pasarelas).
// ─────────────────────────────────────────────────────────────────────────────

#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapTerrain.h"
#include "World/ProcMap/TN_ProcWaterActors.h"
#include "World/ProcMap/TN_ProcMapActorUtils.h"
#include "Core/TN_Log.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Async/ParallelFor.h"
#include "TN_ProcMapMeshKit.h"
#include "TN_ProcMapFormationMeshes.h"
#include "TN_ProcMapCaveMeshes.h"
#include "TN_ProcMapCaveDecor.h"
#include "TN_ProcMapFinishMeshes.h"
#include "TN_ProcMapPropMeshes.h"
#include "TN_ProcMapRockMeshes.h"
#include "TN_ProcMapAmbientFX.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"

using namespace TNProcMesh;

namespace
{
	/** Luminancia de un color lineal. */
	float TNLuminance(const FLinearColor& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; }

	/**
	 * Color del suelo del camino con contraste claro frente a sus paredes (media del suelo y la roca del
	 * bioma), sea cual sea el color de los assets: en los biomas claros (arena, roca gris, pueblos) se
	 * oscurece hasta 0,36 veces la luminancia de las paredes, como tierra pisada; en los oscuros (selva,
	 * volcán, manglar) se aclara hasta ~2,2 veces. Conserva el tono del camino del bioma, algo menos
	 * saturado para que no chille.
	 */
	FLinearColor TNContrastPath(const FLinearColor& Path, const FLinearColor& Ground, const FLinearColor& Rock)
	{
		const float Walls = 0.5f * (TNLuminance(Ground) + TNLuminance(Rock));
		const float Own = FMath::Max(0.01f, TNLuminance(Path));
		const float Target = Walls > 0.28f ? FMath::Min(Own, Walls * 0.36f) : FMath::Max(Own, Walls * 2.2f + 0.06f);
		FLinearColor Out = Path * (Target / Own);
		Out = TNProcLerpColor(Out, FLinearColor(Target, Target, Target), 0.25f);
		const float Peak = FMath::Max3(Out.R, Out.G, Out.B);
		if (Peak > 0.92f) { Out = Out * (0.92f / Peak); }
		Out.A = 1.f;
		return Out;
	}

	/** Polilínea de un tramo de camino con cota y media anchura, recorrible por distancia en planta. */
	struct FTNPlankLine
	{
		TArray<FVector> P;
		TArray<double> HalfW;
		TArray<double> Acc;

		void Add(const FVector& Pt, double Hw)
		{
			Acc.Add(P.Num() == 0 ? 0.0 : Acc.Last() + FVector::Dist2D(P.Last(), Pt));
			P.Add(Pt);
			HalfW.Add(Hw);
		}

		double Length() const { return Acc.Num() > 0 ? Acc.Last() : 0.0; }

		void At(double S, FVector& OutP, FVector& OutDir, double& OutHw) const
		{
			int32 i = 0;
			while (i + 2 < Acc.Num() && Acc[i + 1] < S) { ++i; }
			const int32 j = FMath::Min(i + 1, P.Num() - 1);
			const double Span = FMath::Max(1e-3, Acc[j] - Acc[i]);
			const double T = FMath::Clamp((S - Acc[i]) / Span, 0.0, 1.0);
			OutP = FMath::Lerp(P[i], P[j], T);
			OutDir = (P[j] - P[i]).GetSafeNormal2D();
			if (OutDir.IsNearlyZero()) { OutDir = FVector(1.0, 0.0, 0.0); }
			OutHw = FMath::Lerp(HalfW[i], HalfW[j], T);
		}
	};

	/** Tablones atravesados con junta entre S0 y S1, con dos largueros debajo. */
	void TNProcAddPlanks(FTNProcMeshBuffers& Wood, const FTNPlankLine& Line, double S0, double S1, const FLinearColor& Base, uint32 Seed)
	{
		constexpr double Pitch = 34.0;
		constexpr double Board = 29.0;
		constexpr double Thick = 6.0;
		int32 Index = 0;
		for (double S = S0 + Board * 0.5; S < S1; S += Pitch, ++Index)
		{
			FVector C, Dir;
			double Hw = 0.0;
			Line.At(S, C, Dir, Hw);
			// Cada tablón algo distinto: largo, tono y un leve giro.
			const double Jitter = (TNProcTone(Index, Seed + 7u) - 1.0f) * 0.12;
			const FVector D = (Dir + FVector(-Dir.Y, Dir.X, 0.0) * Jitter).GetSafeNormal2D();
			const double Len = Hw * (0.96 + 0.08 * (TNProcTone(Index, Seed + 3u) - 0.82f) / 0.36f);
			Wood.AddBox(C - FVector(0.0, 0.0, Thick), D, FVector(Board * 0.5, Len, Thick), Base * TNProcTone(Index, Seed));
		}
		// Largueros: vigas a lo largo, bajo los tablones.
		constexpr double Step = 300.0;
		for (double S = S0; S < S1; S += Step)
		{
			FVector A, B, DirA, DirB;
			double HwA = 0.0, HwB = 0.0;
			Line.At(S, A, DirA, HwA);
			Line.At(FMath::Min(S + Step, S1), B, DirB, HwB);
			for (const double Side : { -0.7, 0.7 })
			{
				const FVector NA = FVector(-DirA.Y, DirA.X, 0.0) * (Side * HwA);
				const FVector NB = FVector(-DirB.Y, DirB.X, 0.0) * (Side * HwB);
				Wood.AddBeam(A + NA - FVector(0.0, 0.0, Thick * 2.0 + 8.0), B + NB - FVector(0.0, 0.0, Thick * 2.0 + 8.0), 9.0, Base * 0.6f);
			}
		}
	}

	/** Estilo de los puentes colosales (el del layout). */
	using ETNBridgeStyle = TNProcMap::EBridgeStyle;

	/** Tablero de losas de piedra con pretiles de 1 m y albardilla (viaducto). */
	void TNProcAddStoneDeck(FTNProcMeshBuffers& M, const FTNPlankLine& Line, double S0, double S1, const FLinearColor& Stone, uint32 Seed)
	{
		constexpr double Step = 160.0;
		int32 k = 0;
		for (double S = S0; S < S1; S += Step, ++k)
		{
			const double Se = FMath::Min(S + Step, S1);
			FVector A, B, DA, DB;
			double HA = 0.0, HB = 0.0;
			Line.At(S, A, DA, HA);
			Line.At(Se, B, DB, HB);
			const FVector D = (B - A).GetSafeNormal2D().IsNearlyZero() ? DA : (B - A).GetSafeNormal2D();
			const FVector N(-D.Y, D.X, 0.0);
			const FVector C = (A + B) * 0.5;
			const double Len = FVector::Dist2D(A, B) * 0.5 + 1.0;
			const double Hw = 0.5 * (HA + HB);
			M.AddBox(C - FVector(0.0, 0.0, 16.0), D, FVector(Len, Hw + 32.0, 16.0), Stone * TNProcTone(k, Seed));
			for (const double Side : { -1.0, 1.0 })
			{
				const FVector P = C + N * (Side * (Hw + 16.0));
				M.AddBox(P + FVector(0.0, 0.0, 48.0), D, FVector(Len, 16.0, 48.0), Stone * 0.9f * TNProcTone(k + 50, Seed));
				M.AddBox(P + FVector(0.0, 0.0, 100.0), D, FVector(Len + 1.0, 21.0, 5.0), Stone * 1.1f);
			}
		}
	}

	/**
	 * Arco rebajado de sillería bajo el tablero entre dos apoyos: intradós curvo, rosca más oscura y
	 * tímpanos hasta el tablero por las dos caras. La flecha no baja del suelo.
	 */
	template <typename FGround>
	void TNProcAddDeckArch(FTNProcMeshBuffers& M, const FTNPlankLine& Line, double Sa, double Sb, double TopZ, const FLinearColor& Stone, FGround&& Ground)
	{
		if (Sb - Sa < 400.0) { return; }
		const int32 N = FMath::Clamp(FMath::RoundToInt32((Sb - Sa) / 250.0), 6, 48);
		// Holgura en el tramo central (junto a los apoyos el suelo sube a la torre o a la pila).
		double MinClear = 1e9;
		for (int32 i = 0; i <= N; ++i)
		{
			const double U = static_cast<double>(i) / N;
			if (U < 0.2 || U > 0.8) { continue; }
			FVector P, D;
			double Hw = 0.0;
			Line.At(FMath::Lerp(Sa, Sb, U), P, D, Hw);
			MinClear = FMath::Min(MinClear, TopZ - Ground(FVector2D(P.X, P.Y)));
		}
		const double Rise = FMath::Clamp(FMath::Min((Sb - Sa) * 0.45, MinClear * 0.8), 150.0, 3200.0);
		const double Deck = TopZ - 32.0;
		const double Ring = FMath::Min(120.0, Rise * 0.4);
		for (int32 i = 0; i < N; ++i)
		{
			const double U0 = static_cast<double>(i) / N, U1 = static_cast<double>(i + 1) / N;
			FVector P0, P1, D0, D1;
			double H0 = 0.0, H1 = 0.0;
			Line.At(FMath::Lerp(Sa, Sb, U0), P0, D0, H0);
			Line.At(FMath::Lerp(Sa, Sb, U1), P1, D1, H1);
			// Intradós: arranca en los apoyos a Rise bajo el tablero y sube hasta él en la clave.
			const double Z0 = Deck - 30.0 - Rise * FMath::Square(2.0 * U0 - 1.0);
			const double Z1 = Deck - 30.0 - Rise * FMath::Square(2.0 * U1 - 1.0);
			const FVector N0(-D0.Y, D0.X, 0.0), N1(-D1.Y, D1.X, 0.0);
			const double W0 = H0 + 30.0, W1 = H1 + 30.0;
			const FVector A0 = FVector(P0.X, P0.Y, Z0) - N0 * W0, B0 = FVector(P0.X, P0.Y, Z0) + N0 * W0;
			const FVector A1 = FVector(P1.X, P1.Y, Z1) - N1 * W1, B1 = FVector(P1.X, P1.Y, Z1) + N1 * W1;
			M.AddQuad(A0, B0, B1, A1, FVector(0.0, 0.0, -1.0), Stone * 0.8f);
			for (const double Side : { -1.0, 1.0 })
			{
				const FVector E0 = FVector(P0.X, P0.Y, 0.0) + N0 * (Side * W0);
				const FVector E1 = FVector(P1.X, P1.Y, 0.0) + N1 * (Side * W1);
				const FVector Out = N0 * Side;
				const double R0 = FMath::Min(Z0 + Ring, Deck), R1 = FMath::Min(Z1 + Ring, Deck);
				M.AddQuad(E0 + FVector(0.0, 0.0, Z0), E1 + FVector(0.0, 0.0, Z1), E1 + FVector(0.0, 0.0, R1), E0 + FVector(0.0, 0.0, R0), Out, Stone * 0.78f);
				if (R0 < Deck || R1 < Deck)
				{
					M.AddQuad(E0 + FVector(0.0, 0.0, R0), E1 + FVector(0.0, 0.0, R1), E1 + FVector(0.0, 0.0, Deck), E0 + FVector(0.0, 0.0, Deck), Out, Stone * 0.95f);
				}
			}
		}
	}

	/**
	 * Plaza redonda de un puente colosal (TNProcMap::EFeature::BridgePlaza): suelo de losas (o chapa), pretil
	 * de 1 m con albardilla (o barandilla de hierro) abierto donde entra y sale el tablero, ménsula bajo el
	 * borde (o puntales de hierro) hasta la pila, en el centro una fuente con la tortuga (o un farol alto),
	 * bancos mirando al paisaje, farolas, una atalaya de bloques de 3 m con dos escalones de 1 m por su cara
	 * de -Dir (arriba aparece la recompensa) y la almohadilla de la medusa al pie de su otra cara.
	 */
	void TNProcAddBridgePlaza(FTNProcMeshBuffers& Solid, FTNProcMeshBuffers& Glow, FTNProcMeshBuffers& Water, const TNProcMap::FFeature& F, double DeckHw, bool bIron,
		const FLinearColor& Stone, const FLinearColor& Iron)
	{
		using namespace TNProcMap;
		const FVector C = F.Location;
		const FVector D(F.Dir.X, F.Dir.Y, 0.0);
		const FVector N(-F.Dir.Y, F.Dir.X, 0.0);
		const double R = F.Radius;
		const double Top = C.Z;
		const FLinearColor Floor = bIron ? Iron * 1.6f : Stone * 1.06f;
		const FLinearColor Edge = bIron ? Iron : Stone * 0.9f;
		constexpr int32 Seg = 32;
		auto Dir = [&](double A) { return D * FMath::Cos(A) + N * FMath::Sin(A); };

		// Suelo: disco con un anillo más oscuro y el centro más claro (sobre el tablero, que sigue debajo).
		for (int32 Ring = 0; Ring < 3; ++Ring)
		{
			const double Ro = Ring == 0 ? R : (Ring == 1 ? R * 0.7 : R * 0.35);
			TArray<FVector2D> Poly;
			for (int32 k = 0; k < Seg; ++k) { const FVector P = C + Dir(TwoPi * k / Seg) * Ro; Poly.Add(FVector2D(P.X, P.Y)); }
			const FLinearColor Col = Ring == 1 ? Floor * 0.86f : (Ring == 2 ? Floor * 1.1f : Floor);
			Solid.AddPrism(Poly, Top + 2.0 + Ring * 0.6, Ring == 0 ? Top - 40.0 : Top + 1.5, Col, Ring == 0);
		}
		// Pretil o barandilla por el borde, abierto en las dos entradas del tablero.
		const double Open = FMath::Asin(FMath::Clamp((DeckHw + 60.0) / R, 0.0, 0.95));
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TwoPi * k / Seg, A1 = TwoPi * (k + 1) / Seg, Am = 0.5 * (A0 + A1);
			const double Off = FMath::Min(FMath::Abs(FMath::Atan2(FMath::Sin(Am), FMath::Cos(Am))), FMath::Abs(FMath::Atan2(FMath::Sin(Am - PI), FMath::Cos(Am - PI))));
			if (Off < Open) { continue; }
			const FVector P0 = C + Dir(A0) * (R - 16.0), P1 = C + Dir(A1) * (R - 16.0);
			const FVector Mid = (P0 + P1) * 0.5;
			const FVector Along = (P1 - P0).GetSafeNormal2D();
			const double Half = FVector::Dist2D(P0, P1) * 0.5 + 2.0;
			if (bIron)
			{
				Solid.AddBeam(P0 + FVector(0.0, 0.0, 0.0), P0 + FVector(0.0, 0.0, 108.0), 4.0, Iron);
				Solid.AddBeam(P0 + FVector(0.0, 0.0, 105.0), P1 + FVector(0.0, 0.0, 105.0), 3.5, Iron);
				Solid.AddBeam(P0 + FVector(0.0, 0.0, 55.0), P1 + FVector(0.0, 0.0, 55.0), 2.5, Iron);
			}
			else
			{
				Solid.AddBox(FVector(Mid.X, Mid.Y, Top + 48.0), Along, FVector(Half, 16.0, 48.0), Edge * TNProcTone(k, F.Aux2));
				Solid.AddBox(FVector(Mid.X, Mid.Y, Top + 100.0), Along, FVector(Half + 1.0, 21.0, 5.0), Stone * 1.1f);
			}
		}
		// Debajo: ménsula de piedra que se estrecha hasta la pila, o puntales de hierro.
		if (bIron)
		{
			for (int32 k = 0; k < 8; ++k)
			{
				const FVector Rim = C + Dir(TwoPi * (k + 0.5) / 8.0) * (R * 0.9) - FVector(0.0, 0.0, 20.0);
				Solid.AddBeam(Rim, C + Dir(TwoPi * (k + 0.5) / 8.0) * 250.0 - FVector(0.0, 0.0, 420.0), 9.0, Iron * 1.2f);
			}
		}
		else
		{
			TNProcAddLathe(Solid, C - FVector(0.0, 0.0, 420.0), { 0.0, 180.0, 300.0, 380.0 }, { FMath::Min(R * 0.5, 480.0), FMath::Min(R * 0.6, 560.0), R * 0.85, R + 6.0 }, 0.0, 0u, Stone * 0.82f, Seg, 0.0);
		}
		// Centro: fuente con la tortuga (piedra) o farol alto (hierro).
		if (bIron)
		{
			TNProcAddCylinder(Solid, C, C + FVector(0.0, 0.0, 40.0), 60.0, 50.0, 8, Iron);
			TNProcAddCylinder(Solid, C + FVector(0.0, 0.0, 40.0), C + FVector(0.0, 0.0, 560.0), 14.0, 9.0, 8, Iron * 1.3f);
			for (const double S : { -1.0, 1.0 })
			{
				const FVector Arm = C + N * (S * 110.0) + FVector(0.0, 0.0, 520.0);
				Solid.AddBeam(C + FVector(0.0, 0.0, 540.0), Arm, 4.0, Iron);
				Glow.AddBox(Arm - FVector(0.0, 0.0, 30.0), D, FVector(16.0, 16.0, 22.0), FLinearColor(1.f, 0.78f, 0.38f));
			}
		}
		else
		{
			constexpr double BasinR = 190.0;
			TNProcAddCylinder(Solid, C, C + FVector(0.0, 0.0, 30.0), BasinR - 20.0, BasinR - 20.0, 16, Stone * 0.7f);
			for (int32 k = 0; k < 16; ++k)
			{
				const FVector P0 = C + Dir(TwoPi * k / 16.0) * (BasinR - 10.0), P1 = C + Dir(TwoPi * (k + 1) / 16.0) * (BasinR - 10.0);
				Solid.AddBox(FVector((P0.X + P1.X) * 0.5, (P0.Y + P1.Y) * 0.5, Top + 28.0), (P1 - P0).GetSafeNormal2D(), FVector(FVector::Dist2D(P0, P1) * 0.5 + 2.0, 14.0, 28.0), Stone * 1.1f);
			}
			TArray<FVector2D> Pool;
			for (int32 k = 0; k < 16; ++k) { const FVector P = C + Dir(TwoPi * k / 16.0) * (BasinR - 22.0); Pool.Add(FVector2D(P.X, P.Y)); }
			const int32 Base = Water.Verts.Num();
			Water.AddPrism(Pool, Top + 46.0, Top + 46.0, FLinearColor(0.16f, 0.5f, 0.76f, 0.85f), false);
			for (int32 v = Base; v < Water.Verts.Num(); ++v) { Water.Colors[v] = FLinearColor(0.16f, 0.5f, 0.76f, 0.85f); }
			FTNProcMeshBuffers Statue, StatueGlow;
			TNCaveDecor::TNTurtleStatue(Statue, StatueGlow, static_cast<uint32>(F.Aux2), Stone * 1.05f, Stone * 0.8f, FLinearColor(0.35f, 0.9f, 1.f), 0, false, false);
			const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(F.Dir.Y, F.Dir.X));
			TNPropMesh::TNPropAppend(Solid, Statue, C + FVector(0.0, 0.0, 26.0), Yaw, 0.5);
			TNPropMesh::TNPropAppend(Glow, StatueGlow, C + FVector(0.0, 0.0, 26.0), Yaw, 0.5);
		}
		// Farolas en diagonal y dos bancos mirando al paisaje, en el lado contrario a la atalaya.
		const double TowerSide = (F.Aux2 & 1) ? 1.0 : -1.0;
		for (int32 k = 0; k < 4; ++k)
		{
			const double A = PI * 0.25 + HALF_PI * k;
			const FVector P = C + Dir(A) * (R * 0.8);
			FTNProcMeshBuffers Lamp;
			TNPropMesh::TNPropLampPost(Lamp, 0);
			TNPropMesh::TNPropAppend(Solid, Lamp, P, 0.0);
			Glow.AddBox(P + FVector(0.0, 0.0, 348.0), FVector(1.0, 0.0, 0.0), FVector(14.0, 14.0, 19.0), FLinearColor(1.f, 0.78f, 0.38f));
		}
		for (const double A : { -TowerSide * HALF_PI * 0.72, -TowerSide * HALF_PI * 1.28 })
		{
			const FVector P = C + Dir(A) * (R * 0.66);
			FTNProcMeshBuffers Bench;
			TNPropMesh::TNPropBench(Bench, 0);
			// El banco mira hacia fuera (su respaldo, +Y local, hacia el centro).
			const FVector Out = Dir(A);
			TNPropMesh::TNPropAppend(Solid, Bench, P, FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X)) + 90.0);
		}
		// Atalaya: tres bloques apilados de 1 m y dos escalones de 1 m por la cara de -Dir.
		const FVector2D Tw2 = PlazaDims::TowerAt(F);
		const FVector Tw(Tw2, Top);
		constexpr double Th = PlazaDims::TowerHalf;
		const FLinearColor Block = bIron ? FLinearColor(0.46f, 0.3f, 0.16f) : Stone * 0.95f;
		const FLinearColor Trim = bIron ? Iron : Stone * 0.78f;
		for (int32 b = 0; b < 3; ++b)
		{
			Solid.AddBox(Tw + FVector(0.0, 0.0, 50.0 + 100.0 * b), D, FVector(Th - 2.0 * b, Th - 2.0 * b, 50.0), Block * TNProcTone(b, F.Aux2));
			Solid.AddBox(Tw + FVector(0.0, 0.0, 100.0 * b + 97.0), D, FVector(Th + 4.0 - 2.0 * b, Th + 4.0 - 2.0 * b, 4.0), Trim);
		}
		for (int32 s = 0; s < 2; ++s)
		{
			const double H = 100.0 * (s + 1);
			const FVector P = Tw - D * (Th + PlazaDims::StepDepth * (1.5 - s));
			Solid.AddBox(P + FVector(0.0, 0.0, H * 0.5), D, FVector(PlazaDims::StepDepth * 0.5, Th, H * 0.5), Block * 0.92f);
		}
		// Banderín en lo alto.
		Solid.AddBeam(Tw + FVector(Th - 20.0, Th - 20.0, PlazaDims::TowerH), Tw + FVector(Th - 20.0, Th - 20.0, PlazaDims::TowerH + 220.0), 3.0, Iron);
		Solid.AddBox(Tw + FVector(Th - 20.0, Th - 20.0, PlazaDims::TowerH + 190.0) + D * 30.0, D, FVector(30.0, 1.5, 18.0), FLinearColor(0.85f, 0.12f, 0.1f));
		// Almohadilla de la medusa.
		const FVector2D Jp = PlazaDims::BouncerAt(F);
		TArray<FVector2D> Pad;
		for (int32 k = 0; k < 12; ++k) { Pad.Add(Jp + FVector2D(FMath::Cos(TwoPi * k / 12.0), FMath::Sin(TwoPi * k / 12.0)) * 110.0); }
		Solid.AddPrism(Pad, Top + 4.0, Top + 2.0, FLinearColor(0.62f, 0.3f, 0.66f), false);
	}

	/** Caballete de madera bajo el tablero: dos pies en talud, dos rectos, riostras y cruces hasta el suelo. */
	void TNProcAddTrestleBent(FTNProcMeshBuffers& M, const FVector& Deck, const FVector& Dir, double Hw, double GroundZ, const FLinearColor& Timber)
	{
		const FVector N(-Dir.Y, Dir.X, 0.0);
		const double Top = Deck.Z - 26.0;
		const double H = Top - GroundZ;
		if (H < 200.0) { return; }
		auto Leg = [&](double Off0, double Off1)
		{
			const FVector T = FVector(Deck.X, Deck.Y, Top) + N * Off0;
			const FVector B = FVector(Deck.X, Deck.Y, GroundZ - 40.0) + N * Off1;
			M.AddBeam(T, B, 11.0, Timber);
		};
		for (const double Side : { -1.0, 1.0 })
		{
			Leg(Side * Hw * 0.95, Side * (Hw * 0.95 + H * 0.14));
			Leg(Side * Hw * 0.35, Side * Hw * 0.35);
		}
		M.AddBeam(FVector(Deck.X, Deck.Y, Top) - N * (Hw + 20.0), FVector(Deck.X, Deck.Y, Top) + N * (Hw + 20.0), 13.0, Timber * 0.8f);
		double PrevZ = Top;
		for (double Zc = Top - 320.0; Zc > GroundZ + 60.0; Zc -= 320.0)
		{
			const double T = (Top - Zc) / FMath::Max(1.0, H);
			const double Half = Hw * 0.95 + H * 0.14 * T;
			const double PrevT = (Top - PrevZ) / FMath::Max(1.0, H);
			const double PrevHalf = Hw * 0.95 + H * 0.14 * PrevT;
			const FVector C(Deck.X, Deck.Y, Zc), Cp(Deck.X, Deck.Y, PrevZ);
			M.AddBeam(C - N * Half, C + N * Half, 7.0, Timber * 0.9f);
			M.AddBeam(Cp - N * PrevHalf, C + N * Half, 5.0, Timber * 0.85f);
			M.AddBeam(Cp + N * PrevHalf, C - N * Half, 5.0, Timber * 0.85f);
			PrevZ = Zc;
		}
	}

	/** Barandilla rígida: postes cada PostEvery y dos pasamanos (madera o hierro). */
	void TNProcAddRigidRails(FTNProcMeshBuffers& M, const FTNPlankLine& Line, double S0, double S1, double PostEvery, double RailH, double Half, const FLinearColor& Color)
	{
		const int32 NumPosts = FMath::Max(1, FMath::RoundToInt32((S1 - S0) / PostEvery));
		for (const double Side : { -1.0, 1.0 })
		{
			FVector PrevTop = FVector::ZeroVector, PrevMid = FVector::ZeroVector;
			for (int32 k = 0; k <= NumPosts; ++k)
			{
				const double S = S0 + (S1 - S0) * k / NumPosts;
				FVector C, Dir;
				double Hw = 0.0;
				Line.At(S, C, Dir, Hw);
				const FVector Base = C + FVector(-Dir.Y, Dir.X, 0.0) * (Side * (Hw - 10.0));
				M.AddBox(Base + FVector(0.0, 0.0, RailH * 0.5), Dir, FVector(Half, Half, RailH * 0.5 + 6.0), Color);
				const FVector Top = Base + FVector(0.0, 0.0, RailH), Mid = Base + FVector(0.0, 0.0, RailH * 0.5);
				if (k > 0)
				{
					M.AddBeam(PrevTop, Top, Half * 0.7, Color);
					M.AddBeam(PrevMid, Mid, Half * 0.5, Color);
				}
				PrevTop = Top;
				PrevMid = Mid;
			}
		}
	}

	/** Cadena de eslabones alternos entre A y B (cables del puente de hierro). */
	void TNProcAddChain(FTNProcMeshBuffers& M, const FVector& A, const FVector& B, const FLinearColor& Color)
	{
		const FVector D = B - A;
		const double Len = D.Size();
		if (Len < 1.0) { return; }
		const FVector X = D / Len;
		const FVector Y0 = FVector::CrossProduct(FVector::UpVector, X).GetSafeNormal();
		const FVector Y = Y0.IsNearlyZero() ? FVector(0.0, 1.0, 0.0) : Y0;
		const FVector Z = FVector::CrossProduct(X, Y);
		const int32 Links = FMath::Max(1, FMath::RoundToInt32(Len / 34.0));
		for (int32 k = 0; k < Links; ++k)
		{
			const FVector C = A + D * ((k + 0.5) / Links);
			const FVector Wd = (k % 2) ? Y : Z;
			for (const double Sg : { -1.0, 1.0 })
			{
				M.AddBeam(C - X * 17.0 + Wd * (Sg * 7.0), C + X * 17.0 + Wd * (Sg * 7.0), 2.2, Color);
			}
		}
	}

	/** Postes a ambos bordes cada PostEvery y cuerda de barandilla con comba entre ellos. */
	void TNProcAddRopeRails(FTNProcMeshBuffers& Wood, const FTNPlankLine& Line, double S0, double S1, double PostEvery, double PostDown,
		double RailH, const FLinearColor& PostColor, const FLinearColor& RopeColor)
	{
		const int32 NumPosts = FMath::Max(1, FMath::RoundToInt((S1 - S0) / PostEvery));
		for (const double Side : { -1.0, 1.0 })
		{
			FVector PrevTop = FVector::ZeroVector;
			for (int32 k = 0; k <= NumPosts; ++k)
			{
				const double S = S0 + (S1 - S0) * k / NumPosts;
				FVector C, Dir;
				double Hw = 0.0;
				Line.At(S, C, Dir, Hw);
				const FVector N(-Dir.Y, Dir.X, 0.0);
				const FVector Base = C + N * (Side * (Hw - 12.0));
				Wood.AddBox(Base + FVector(0.0, 0.0, (RailH + 10.0 - PostDown) * 0.5), Dir, FVector(7.0, 7.0, (RailH + 10.0 + PostDown) * 0.5), PostColor);
				const FVector Top = Base + FVector(0.0, 0.0, RailH);
				if (k > 0)
				{
					// Cuerda en tres tramos con comba.
					FVector Last = PrevTop;
					for (int32 t = 1; t <= 3; ++t)
					{
						const double U = t / 3.0;
						const FVector Pt = FMath::Lerp(PrevTop, Top, U) - FVector(0.0, 0.0, 22.0 * 4.0 * U * (1.0 - U));
						Wood.AddBeam(Last, Pt, 2.5, RopeColor);
						Last = Pt;
					}
				}
				PrevTop = Top;
			}
		}
	}

	/** Eje de una muralla: su adarve (el tramo alto del cruce), recorrible por distancia en planta. */
	struct FTNWallAxis
	{
		TArray<FVector2D> P;
		TArray<double> S;
		TArray<double> Hw;

		void Build(const TArray<TNProcMap::FPathSample>& M, int32 From, int32 To)
		{
			for (int32 i = From; i <= To; ++i)
			{
				S.Add(P.Num() == 0 ? 0.0 : S.Last() + FVector2D::Distance(P.Last(), M[i].P));
				P.Add(M[i].P);
				Hw.Add(M[i].Width * 0.5);
			}
		}

		double Length() const { return S.Num() > 0 ? S.Last() : 0.0; }

		/** Distancia a lo largo del eje del punto del eje más cercano a Q. */
		double Project(const FVector2D& Q) const
		{
			double Best = 1e300, BestS = 0.0;
			for (int32 i = 0; i + 1 < P.Num(); ++i)
			{
				double T = 0.0;
				const double D = TNProcMap::DistPointSegment(Q, P[i], P[i + 1], T);
				if (D < Best) { Best = D; BestS = FMath::Lerp(S[i], S[i + 1], T); }
			}
			return BestS;
		}

		/** Punto, tangente y normal izquierda (suavizadas) y semiancho del adarve a la distancia Sq. */
		void At(double Sq, FVector2D& OutP, FVector2D& OutT, FVector2D& OutN, double& OutHw) const
		{
			int32 Lo = 0, Hi = S.Num() - 1;
			while (Hi - Lo > 1)
			{
				const int32 Mid = (Lo + Hi) / 2;
				if (S[Mid] <= Sq) { Lo = Mid; } else { Hi = Mid; }
			}
			const double T = FMath::Clamp((Sq - S[Lo]) / FMath::Max(1.0, S[Hi] - S[Lo]), 0.0, 1.0);
			OutP = P[Lo] + (P[Hi] - P[Lo]) * T;
			OutT = (P[FMath::Min(Hi + 1, P.Num() - 1)] - P[FMath::Max(Lo - 1, 0)]).GetSafeNormal();
			OutN = FVector2D(-OutT.Y, OutT.X);
			OutHw = FMath::Lerp(Hw[Lo], Hw[Hi], T);
		}
	};

	/**
	 * Muralla de un cruce entre S0 y S1 de su eje: caras en talud desde 3 m bajo el suelo hasta el
	 * pretil, en hiladas de sillares de tonos alternos; el adarve (S0W-S1W) entre dos parapetos con
	 * almenas; y, si la cruza el tramo bajo, su puerta: jambas a plomo, arco de medio punto con las
	 * dovelas marcadas y bóveda de cañón bajo el adarve. GroundAt(FVector2D) da la cota del terreno.
	 */
	template <typename FGround>
	void TNProcAddWall(FTNProcMeshBuffers& Out, const FTNWallAxis& Axis, double S0, double S1, double S0W, double S1W, double TopZ,
		const TNProcMap::FFeature* Gate, const FGround& GroundAt, const FLinearColor& Stone, uint32 Seed)
	{
		using namespace TNProcMap;
		constexpr double Course = 180.0;
		const double ParTop = TopZ + WallDims::ParapetH;

		struct FCol
		{
			double S = 0.0;
			FVector2D P = FVector2D::ZeroVector, T = FVector2D::ZeroVector, N = FVector2D::ZeroVector;
			double Hw = 0.0;
			/** Pie de cada cara (0 izquierda, 1 derecha): enterrado 3 m, o el intradós bajo el arco. */
			double Bottom[2] = { 0.0, 0.0 };
		};
		double Sg = 0.0, R = 0.0, Zs = 0.0, Floor = 0.0;
		bool bGate = Gate != nullptr;
		if (bGate)
		{
			Sg = Axis.Project(FVector2D(Gate->Location.X, Gate->Location.Y));
			R = Gate->Radius;
			Zs = TopZ - WallDims::Crown - R;
			Floor = Gate->Location.Z;
			bGate = Sg - R > S0 + 100.0 && Sg + R < S1 - 100.0 && Zs > Floor + 300.0;
		}
		auto Intrados = [&](double Sq) { const double U = Sq - Sg; return Zs + FMath::Sqrt(FMath::Max(0.0, R * R - U * U)); };
		auto MakeCol = [&](double Sq, bool bArch)
		{
			FCol C;
			C.S = Sq;
			Axis.At(Sq, C.P, C.T, C.N, C.Hw);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const double Sig = Side == 0 ? 1.0 : -1.0;
				if (bArch) { C.Bottom[Side] = Intrados(Sq); continue; }
				// Pie de la cara donde el talud corta el suelo (dos pasadas), enterrado 3 m.
				double G = GroundAt(C.P + C.N * (Sig * (C.Hw + WallDims::Parapet + 300.0)));
				G = GroundAt(C.P + C.N * (Sig * WallDims::HalfAt(C.Hw, TopZ - G)));
				C.Bottom[Side] = FMath::Min(G, TopZ - 200.0) - 300.0;
			}
			return C;
		};
		auto FacePt = [&](const FCol& C, double Sig, double Z)
		{
			const double Lat = Z >= TopZ ? C.Hw + WallDims::Parapet : WallDims::HalfAt(C.Hw, TopZ - Z);
			const FVector2D Q = C.P + C.N * (Sig * Lat);
			return FVector(Q.X, Q.Y, Z);
		};
		auto Range = [&](double A, double B, double Step, bool bArch)
		{
			TArray<FCol> Cols;
			const int32 Num = FMath::Max(1, FMath::CeilToInt((B - A) / Step));
			for (int32 k = 0; k <= Num; ++k) { Cols.Add(MakeCol(A + (B - A) * k / Num, bArch)); }
			return Cols;
		};
		TArray<TArray<FCol>> Parts;
		TArray<bool> IsArch;
		if (bGate)
		{
			Parts.Add(Range(S0, Sg - R, 250.0, false)); IsArch.Add(false);
			Parts.Add(Range(Sg - R, Sg + R, 50.0, true)); IsArch.Add(true);
			Parts.Add(Range(Sg + R, S1, 250.0, false)); IsArch.Add(false);
		}
		else
		{
			Parts.Add(Range(S0, S1, 250.0, false)); IsArch.Add(false);
		}

		// Caras en hiladas; bajo el arco, la primera hilada es la de las dovelas (más clara).
		for (int32 PartIdx = 0; PartIdx < Parts.Num(); ++PartIdx)
		{
			const TArray<FCol>& Cols = Parts[PartIdx];
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const double Sig = Side == 0 ? 1.0 : -1.0;
				for (int32 k = 0; k + 1 < Cols.Num(); ++k)
				{
					const FCol& A = Cols[k];
					const FCol& B = Cols[k + 1];
					const double Low = FMath::Max(A.Bottom[Side], B.Bottom[Side]);
					TArray<double> Lines;
					for (int32 n = FMath::FloorToInt((TopZ - Low) / Course); n >= 0; --n)
					{
						const double Zl = TopZ - n * Course;
						if (Zl > Low + 20.0) { Lines.Add(Zl); }
					}
					Lines.Add(ParTop);
					const FVector2D Nm = (A.N + B.N).GetSafeNormal() * Sig;
					const FVector Hint(Nm.X, Nm.Y, WallDims::Batter);
					double ZA = A.Bottom[Side], ZB = B.Bottom[Side];
					for (int32 r = 0; r < Lines.Num(); ++r)
					{
						const double Zt = Lines[r];
						const int32 CourseIdx = FMath::RoundToInt((TopZ - Zt) / Course);
						FLinearColor Col = Stone * TNProcTone(CourseIdx * 977 + (PartIdx * 1000 + k) / 2 * 131 + Side * 7, Seed);
						if (IsArch[PartIdx] && r == 0) { Col = Stone * ((k % 2) == 0 ? 1.18f : 1.08f); }
						Out.AddQuad(FacePt(A, Sig, ZA), FacePt(B, Sig, ZB), FacePt(B, Sig, Zt), FacePt(A, Sig, Zt), Hint, Col);
						ZA = ZB = Zt;
					}
				}
			}
		}

		// Puerta: jambas a plomo desde el suelo del paso (enterradas) hasta el arranque del arco, y
		// bóveda de cañón por el intradós.
		if (bGate)
		{
			for (int32 J = 0; J < 2; ++J)
			{
				const FCol& C = J == 0 ? Parts[0].Last() : Parts[2][0];
				const FVector Hint(C.T.X * (J == 0 ? 1.0 : -1.0), C.T.Y * (J == 0 ? 1.0 : -1.0), 0.0);
				double Z0 = Floor - 300.0;
				while (Z0 < Zs - 1.0)
				{
					const double Z1 = FMath::Min(Zs, (FMath::FloorToDouble((Z0 - TopZ) / Course) + 1.0) * Course + TopZ);
					const double Zt = Z1 <= Z0 + 1.0 ? Zs : Z1;
					Out.AddQuad(FacePt(C, 1.0, Z0), FacePt(C, -1.0, Z0), FacePt(C, -1.0, Zt), FacePt(C, 1.0, Zt), Hint,
						Stone * TNProcTone(FMath::RoundToInt(Z0 / Course) * 31 + J, Seed ^ 0x5A5Au) * 0.95f);
					Z0 = Zt;
				}
			}
			const TArray<FCol>& Arch = Parts[1];
			for (int32 k = 0; k + 1 < Arch.Num(); ++k)
			{
				const FCol& A = Arch[k];
				const FCol& B = Arch[k + 1];
				const double Sm = 0.5 * (A.S + B.S);
				const FVector2D Tm = (A.T + B.T).GetSafeNormal();
				const FVector Hint(Tm.X * (Sg - Sm), Tm.Y * (Sg - Sm), Zs - Intrados(Sm));
				Out.AddQuad(FacePt(A, 1.0, A.Bottom[0]), FacePt(A, -1.0, A.Bottom[1]), FacePt(B, -1.0, B.Bottom[1]), FacePt(B, 1.0, B.Bottom[0]), Hint,
					Stone * ((k % 2) == 0 ? 0.92f : 0.86f));
			}
		}

		// Remates de los extremos (quedan dentro de las torres).
		for (int32 E = 0; E < 2; ++E)
		{
			const FCol& C = E == 0 ? Parts[0][0] : Parts.Last().Last();
			const FVector Hint(C.T.X * (E == 0 ? -1.0 : 1.0), C.T.Y * (E == 0 ? -1.0 : 1.0), 0.0);
			const double Zb = FMath::Min(C.Bottom[0], C.Bottom[1]);
			Out.AddQuad(FacePt(C, 1.0, Zb), FacePt(C, -1.0, Zb), FacePt(C, -1.0, ParTop), FacePt(C, 1.0, ParTop), Hint, Stone * 0.9f);
		}

		// Adarve enlosado, caras interiores y cima de los parapetos, y almenas cada 2,6 m.
		const TArray<FCol> Top = Range(S0W, S1W, 250.0, false);
		for (int32 k = 0; k + 1 < Top.Num(); ++k)
		{
			const FCol& A = Top[k];
			const FCol& B = Top[k + 1];
			auto W = [&](const FCol& C, double Sig, double Lat, double Z) { const FVector2D Q = C.P + C.N * (Sig * Lat); return FVector(Q.X, Q.Y, Z); };
			Out.AddQuad(W(A, 1.0, A.Hw, TopZ), W(A, -1.0, A.Hw, TopZ), W(B, -1.0, B.Hw, TopZ), W(B, 1.0, B.Hw, TopZ), FVector::UpVector,
				Stone * 1.1f * TNProcTone(k, Seed ^ 0xADA7u));
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const double Sig = Side == 0 ? 1.0 : -1.0;
				const FVector2D Nm = (A.N + B.N).GetSafeNormal() * -Sig;
				Out.AddQuad(W(A, Sig, A.Hw, TopZ), W(B, Sig, B.Hw, TopZ), W(B, Sig, B.Hw, ParTop), W(A, Sig, A.Hw, ParTop), FVector(Nm.X, Nm.Y, 0.0), Stone * 0.97f);
				Out.AddQuad(W(A, Sig, A.Hw, ParTop), W(B, Sig, B.Hw, ParTop), W(B, Sig, B.Hw + WallDims::Parapet, ParTop), W(A, Sig, A.Hw + WallDims::Parapet, ParTop),
					FVector::UpVector, Stone * 1.05f);
			}
		}
		for (double Sm = S0W + 130.0; Sm < S1W - 100.0; Sm += 260.0)
		{
			const FCol C = MakeCol(Sm, true);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const double Sig = Side == 0 ? 1.0 : -1.0;
				const FVector2D Q = C.P + C.N * (Sig * (C.Hw + WallDims::Parapet * 0.5));
				Out.AddBox(FVector(Q.X, Q.Y, ParTop + WallDims::MerlonH * 0.5), FVector(C.T.X, C.T.Y, 0.0),
					FVector(65.0, WallDims::Parapet * 0.5, WallDims::MerlonH * 0.5), Stone * TNProcTone(FMath::RoundToInt(Sm) + Side, Seed ^ 0x3E7u));
			}
		}
	}

	/**
	 * Torre de muralla: forro de sillería en talud (32 lados) sobre el pilar del terreno, pretil de
	 * 1,9 m con almenas que cubre el de roca y se abre al adarve y al tobogán (donde el forro queda a
	 * ras del pilar), y un estandarte en lo alto.
	 */
	template <typename FGround>
	void TNProcAddWallTower(FTNProcMeshBuffers& Out, FTNProcMeshBuffers& Cloth, const TNProcMap::FLayout& Layout, const TNProcMap::FFeature& F,
		const FGround& GroundAt, const FLinearColor& Stone, const FLinearColor& Banner, uint32 Seed)
	{
		using namespace TNProcMap;
		constexpr int32 Sides = 32;
		constexpr double Course = 180.0;
		const FVector2D C(F.Location.X, F.Location.Y);
		const double TopZ = F.Height;
		const double R = F.Radius;
		const double Ri = R - 320.0;
		const double Ro = R + 220.0;
		const double ParTop = TopZ + 190.0;
		auto Dir = [](double A) { return FVector2D(FMath::Cos(A), FMath::Sin(A)); };
		TArray<bool> Open;
		for (int32 k = 0; k < Sides; ++k)
		{
			const double A = TwoPi * (k + 0.5) / Sides;
			Open.Add(TowerOpeningAt(Layout, F, C + Dir(A) * (R - 150.0)));
		}
		double Ground = TopZ;
		for (int32 k = 0; k < Sides; ++k) { Ground = FMath::Min(Ground, GroundAt(C + Dir(TwoPi * k / Sides) * (Ro + 400.0))); }
		const double Zb = Ground - 300.0;
		auto OuterAt = [&](double A, double Z, bool bFlush)
		{
			const double Rad = bFlush ? R : Ro + WallDims::Batter * FMath::Max(0.0, TopZ - Z);
			const FVector2D Q = C + Dir(A) * Rad;
			return FVector(Q.X, Q.Y, Z);
		};
		auto RingAt = [&](double A, double Rad, double Z) { const FVector2D Q = C + Dir(A) * Rad; return FVector(Q.X, Q.Y, Z); };
		for (int32 k = 0; k < Sides; ++k)
		{
			const double A0 = TwoPi * k / Sides;
			const double A1 = TwoPi * (k + 1) / Sides;
			const double Am = 0.5 * (A0 + A1);
			const FVector Hint(FMath::Cos(Am), FMath::Sin(Am), WallDims::Batter);
			const bool bOpen = Open[k];
			const double Zt = bOpen ? TopZ : ParTop;
			double Z0 = Zb;
			while (Z0 < Zt - 1.0)
			{
				const double Z1 = FMath::Min(Zt, (FMath::FloorToDouble((Z0 - TopZ) / Course) + 1.0) * Course + TopZ);
				const double Z2 = Z1 <= Z0 + 1.0 ? Zt : Z1;
				Out.AddQuad(OuterAt(A0, Z0, bOpen), OuterAt(A1, Z0, bOpen), OuterAt(A1, Z2, bOpen), OuterAt(A0, Z2, bOpen), Hint,
					Stone * TNProcTone(FMath::RoundToInt((TopZ - Z2) / Course) * 97 + k / 2, Seed));
				Z0 = Z2;
			}
			if (bOpen) { continue; }
			// Pretil: cara interior, cima y almena en medio del lado; y cierre junto a las aberturas.
			Out.AddQuad(RingAt(A1, Ri, TopZ), RingAt(A0, Ri, TopZ), RingAt(A0, Ri, ParTop), RingAt(A1, Ri, ParTop), FVector(-Hint.X, -Hint.Y, 0.0), Stone * 0.95f);
			Out.AddQuad(RingAt(A0, Ri, ParTop), RingAt(A1, Ri, ParTop), OuterAt(A1, ParTop, false), OuterAt(A0, ParTop, false), FVector::UpVector, Stone * 1.05f);
			const FVector2D Tg(-FMath::Sin(Am), FMath::Cos(Am));
			if ((k % 2) == 0)
			{
				// Almenas en el borde exterior, una sí y otra no.
				const FVector2D Mc = C + Dir(Am) * (Ro - 60.0);
				Out.AddBox(FVector(Mc.X, Mc.Y, ParTop + WallDims::MerlonH * 0.5), FVector(Tg.X, Tg.Y, 0.0),
					FVector(0.5 * Ro * (A1 - A0), 60.0, WallDims::MerlonH * 0.5), Stone * TNProcTone(k, Seed ^ 0x77u));
			}
			for (int32 E = 0; E < 2; ++E)
			{
				const int32 Nb = (k + (E == 0 ? Sides - 1 : 1)) % Sides;
				if (!Open[Nb]) { continue; }
				const double Ae = E == 0 ? A0 : A1;
				const FVector2D Te = Tg * (E == 0 ? -1.0 : 1.0);
				// Junto a una abertura: cierre del pretil y, debajo, del forro hasta el pilar.
				Out.AddQuad(RingAt(Ae, Ri, TopZ), OuterAt(Ae, TopZ, false), OuterAt(Ae, ParTop, false), RingAt(Ae, Ri, ParTop), FVector(Te.X, Te.Y, 0.0), Stone * 0.9f);
				Out.AddQuad(RingAt(Ae, R, Zb), OuterAt(Ae, Zb, false), OuterAt(Ae, TopZ, false), RingAt(Ae, R, TopZ), FVector(Te.X, Te.Y, 0.0), Stone * 0.9f);
			}
		}
		// Estandarte en el lado cerrado más alejado de las aberturas.
		int32 Best = INDEX_NONE;
		int32 BestRun = -1;
		for (int32 k = 0; k < Sides; ++k)
		{
			if (Open[k]) { continue; }
			int32 Run = 0;
			while (Run < Sides && !Open[(k + Run) % Sides] && !Open[(k - Run + Sides) % Sides]) { ++Run; }
			if (Run > BestRun) { BestRun = Run; Best = k; }
		}
		if (Best != INDEX_NONE)
		{
			const double Am = TwoPi * (Best + 0.5) / Sides;
			const FVector2D Q = C + Dir(Am) * (0.5 * (Ri + Ro));
			const FVector Base(Q.X, Q.Y, ParTop);
			const FVector TopP = Base + FVector(0.0, 0.0, 750.0);
			Cloth.AddBeam(Base, TopP, 7.0, FLinearColor(0.35f, 0.24f, 0.14f));
			const FVector2D Fd(-FMath::Sin(Am), FMath::Cos(Am));
			const FVector Fx(Fd.X * 260.0, Fd.Y * 260.0, 0.0);
			const FVector Fl0 = TopP - FVector(0.0, 0.0, 20.0);
			const FVector Fl1 = TopP - FVector(0.0, 0.0, 170.0);
			const FVector Wave(0.0, 0.0, -30.0);
			const FVector Nf(Fd.Y, -Fd.X, 0.0);
			Cloth.AddQuad(Fl0, Fl0 + Fx + Wave, Fl1 + Fx + Wave, Fl1, Nf, Banner);
			Cloth.AddQuad(Fl0, Fl1, Fl1 + Fx + Wave, Fl0 + Fx + Wave, -Nf, Banner * 0.8f);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Materiales y colores
// ─────────────────────────────────────────────────────────────────────────────

UMaterialInterface* ATN_ProcMapGenerator::ResolveMaterial(UMaterialInterface* Preferred, const TCHAR* FallbackPath) const
{
	if (Preferred)
	{
		return Preferred;
	}
	return FallbackPath ? LoadObject<UMaterialInterface>(nullptr, FallbackPath) : nullptr;
}

void ATN_ProcMapGenerator::ResolveBiomeColors(ETNProcBiome Biome, FLinearColor& Ground, FLinearColor& Path, FLinearColor& Rock, FLinearColor& Bed) const
{
	if (const UTN_ProcBiomeDataAsset* Asset = Settings ? Settings->FindBiome(Biome) : nullptr)
	{
		Ground = Asset->GroundColor;
		Path = Asset->PathColor;
		Rock = Asset->RockColor;
		Bed = Asset->BedColor;
		return;
	}
	TN_DefaultBiomeColors(Biome, Ground, Path, Rock, Bed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Terreno
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::BuildTerrain()
{
	using namespace TNProcMap;
	const double Spacing = Settings ? Settings->VertexSpacing : 150.0;
	const int32 TileQuads = Settings ? Settings->TileQuads : 48;
	const double Margin = Settings ? Settings->OuterMargin : 25000.0;
	const double Sea = Settings ? Settings->SeaExtent : 30000.0;

	LatticeSpacing = Spacing;
	LatticeOrigin = FVector2D(-Margin, -Margin);
	const int32 QuadsX = FMath::CeilToInt((Layout.WorldSize + 2.0 * Margin) / Spacing / TileQuads) * TileQuads;
	const int32 QuadsY = FMath::CeilToInt((Layout.WorldSize + Margin + Sea) / Spacing / TileQuads) * TileQuads;
	LatticeNX = QuadsX + 1;
	LatticeNY = QuadsY + 1;
	const int32 NumVerts = LatticeNX * LatticeNY;

	// ── Alturas en paralelo (lógica pura, filas disjuntas) ──────────────────
	TNProcMap::FTerrainBuilder Builder;
	Builder.Build(Layout, LatticeOrigin, Spacing, LatticeNX, LatticeNY);
	Heights.SetNumUninitialized(NumVerts);
	PathMask.SetNumUninitialized(NumVerts);
	const int32 RowsPerChunk = 16;
	const int32 NumChunks = (LatticeNY + RowsPerChunk - 1) / RowsPerChunk;
	ParallelFor(NumChunks, [&](int32 Chunk)
	{
		Builder.ComputeRows(Chunk * RowsPerChunk, FMath::Min(LatticeNY, (Chunk + 1) * RowsPerChunk), Heights, PathMask);
	});
	Builder.ExportPathEdgeDistance(PathDist);

	// ── Colores por bioma (el camino, con contraste fuerte frente a sus paredes) ──
	FLinearColor Ground[NumBiomes], PathC[NumBiomes], PathRaw[NumBiomes], Rock[NumBiomes], Bed[NumBiomes];
	for (int32 b = 0; b < NumBiomes; ++b)
	{
		ResolveBiomeColors(BiomeFromIndex(b), Ground[b], PathRaw[b], Rock[b], Bed[b]);
		PathC[b] = TNContrastPath(PathRaw[b], Ground[b], Rock[b]);
	}
	// La playa de la meta conserva su arena: desde 15 m antes de su primera muestra de orilla, el camino
	// vuelve al color del bioma.
	double ShoreStartY = 1e18;
	for (const FPathSample& S : Layout.Main)
	{
		if ((S.Flags & PathFlags::Shore) != 0) { ShoreStartY = S.P.Y; break; }
	}
	const double FinishX = Layout.EndPoint.X;

	auto HeightAt = [&](int32 X, int32 Y)
	{
		return static_cast<double>(Heights[FMath::Clamp(Y, 0, LatticeNY - 1) * LatticeNX + FMath::Clamp(X, 0, LatticeNX - 1)]);
	};

	UMaterialInterface* TerrainMat = ResolveMaterial(Settings ? Settings->TerrainMaterial.Get() : nullptr,
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	const uint32 ColorSeed = Layout.Params.Seed ^ 0xC0105u;

	const int32 TilesX = QuadsX / TileQuads;
	const int32 TilesY = QuadsY / TileQuads;
	const int32 Side = TileQuads + 1;
	const TArray<FProcMeshTangent> NoTangents;

	// Datos de cada tesela en paralelo (con 1,5 m de resolución son millones de vértices); la
	// creación de los componentes va después, en el hilo de juego.
	struct FTileData
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
	};
	TArray<FTileData> TileData;
	TileData.SetNum(TilesX * TilesY);
	ParallelFor(TileData.Num(), [&](int32 TileIndex)
	{
		const int32 Tx = TileIndex % TilesX;
		const int32 Ty = TileIndex / TilesX;
		FTileData& T = TileData[TileIndex];
		// A=(x,y), B=(x+1,y), C=(x,y+1), D=(x+1,y+1). La cara frontal de UE es
		// (C-A)x(B-A): (A,C,B)+(B,C,D) o (A,C,D)+(A,D,B) miran hacia +Z. La
		// diagonal sigue la curva de nivel (TNProcMap::SplitAlongAD).
		T.Tris.Reserve(TileQuads * TileQuads * 6);
		for (int32 y = 0; y < TileQuads; ++y)
		{
			for (int32 x = 0; x < TileQuads; ++x)
			{
				const int32 GX = Tx * TileQuads + x;
				const int32 GY = Ty * TileQuads + y;
				const int32 A = y * Side + x;
				const int32 B = A + 1;
				const int32 C = A + Side;
				const int32 D = C + 1;
				if (TNProcMap::SplitAlongAD(HeightAt(GX, GY), HeightAt(GX + 1, GY), HeightAt(GX, GY + 1), HeightAt(GX + 1, GY + 1)))
				{
					T.Tris.Add(A); T.Tris.Add(C); T.Tris.Add(D);
					T.Tris.Add(A); T.Tris.Add(D); T.Tris.Add(B);
				}
				else
				{
					T.Tris.Add(A); T.Tris.Add(C); T.Tris.Add(B);
					T.Tris.Add(B); T.Tris.Add(C); T.Tris.Add(D);
				}
			}
		}
		T.Verts.Reserve(Side * Side); T.Normals.Reserve(Side * Side); T.UVs.Reserve(Side * Side); T.Colors.Reserve(Side * Side);
		for (int32 y = 0; y <= TileQuads; ++y)
		{
			for (int32 x = 0; x <= TileQuads; ++x)
			{
				const int32 GX = Tx * TileQuads + x;
				const int32 GY = Ty * TileQuads + y;
				const double H = HeightAt(GX, GY);
				const FVector2D P = LatticeOrigin + FVector2D(GX * Spacing, GY * Spacing);
				T.Verts.Add(FVector(P.X, P.Y, H));
				const double Dx = (HeightAt(GX + 1, GY) - HeightAt(GX - 1, GY)) / (2.0 * Spacing);
				const double Dy = (HeightAt(GX, GY + 1) - HeightAt(GX, GY - 1)) / (2.0 * Spacing);
				const FVector N = FVector(-Dx, -Dy, 1.0).GetSafeNormal();
				T.Normals.Add(N);
				T.UVs.Add(P / 500.0);
				double W[NumBiomes];
				Layout.BiomeWeightsAt(P, W);
				FLinearColor G(0.f, 0.f, 0.f, 0.f), Pc(0.f, 0.f, 0.f, 0.f), Praw(0.f, 0.f, 0.f, 0.f), R(0.f, 0.f, 0.f, 0.f), Bd(0.f, 0.f, 0.f, 0.f);
				for (int32 b = 0; b < NumBiomes; ++b)
				{
					const float Wb = static_cast<float>(W[b]);
					G += Ground[b] * Wb; Pc += PathC[b] * Wb; Praw += PathRaw[b] * Wb; R += Rock[b] * Wb; Bd += Bed[b] * Wb;
				}
				const double Beach = TNProcMap::SmoothStep(ShoreStartY - 1500.0, ShoreStartY, P.Y) * TNProcMap::SmoothStep(16000.0, 9000.0, FMath::Abs(P.X - FinishX));
				Pc = TNProcLerpColor(Pc, Praw, static_cast<float>(Beach));
				const float Mask = PathMask[GY * LatticeNX + GX] / 255.f;
				// Paredes en degradado por pendiente: suelo en lo llano, roca en los taludes (28-57°) y
				// roca cada vez más oscura en los tajos (57-81°), con estratos suaves por altura en lo
				// empinado para que se lea su forma.
				const float Steep = static_cast<float>(TNProcMap::SmoothStep(0.88, 0.55, N.Z));
				const float Cliff = static_cast<float>(TNProcMap::SmoothStep(0.55, 0.15, N.Z));
				FLinearColor Col = TNProcLerpColor(G, R, Steep);
				Col = Col * FMath::Lerp(1.f, 0.62f, Cliff);
				const double Strata = FMath::Sin(H / 170.0 + 1.3 * TNProcMap::Noise2(ColorSeed + 7u, P.X / 3000.0, P.Y / 3000.0));
				Col = Col * (1.f + 0.07f * Steep * (Strata > 0.0 ? 1.f : -1.f));
				// Suelo del camino, con una línea oscura en el pie del talud que marca su borde.
				Col = TNProcLerpColor(Col, Pc, Mask);
				Col = Col * (1.f - 1.2f * Mask * (1.f - Mask));
				Col = TNProcLerpColor(Col, Bd, static_cast<float>(TNProcMap::SmoothStep(30.0, -120.0, H)));
				const float Var = 0.9f + 0.2f * static_cast<float>(0.5 + 0.5 * TNProcMap::Noise2(ColorSeed, P.X / 700.0, P.Y / 700.0));
				Col = Col * Var;
				Col.A = Mask;
				T.Colors.Add(Col);
			}
		}
	});

	for (FTileData& T : TileData)
	{
		UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
		Tile->SetupAttachment(RootComponent);
		Tile->bUseAsyncCooking = true;
		Tile->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Tile->RegisterComponent();
		Tile->CreateMeshSection_LinearColor(0, T.Verts, T.Tris, T.Normals, T.UVs, T.Colors, NoTangents, true);
		if (TerrainMat) { Tile->SetMaterial(0, TerrainMat); }
		TerrainTiles.Add(Tile);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Agua
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::BuildWater()
{
	// Plano de agua a nivel del mar sobre todo el mallado: el terreno lo tapa donde está por encima.
	if (BasicPlane)
	{
		WaterPlane = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		WaterPlane->SetupAttachment(RootComponent);
		WaterPlane->SetStaticMesh(BasicPlane);
		WaterPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WaterPlane->SetCastShadow(false);
		WaterPlane->RegisterComponent();
		const double SizeX = (LatticeNX - 1) * LatticeSpacing;
		const double SizeY = (LatticeNY - 1) * LatticeSpacing;
		WaterPlane->SetRelativeLocation(FVector(LatticeOrigin.X + SizeX * 0.5, LatticeOrigin.Y + SizeY * 0.5, TNProcMap::SeaLevel + 2.0));
		WaterPlane->SetRelativeScale3D(FVector(SizeX / 100.0, SizeY / 100.0, 1.0));
		if (UMaterialInterface* WaterMat = Settings ? Settings->WaterMaterial.Get() : nullptr)
		{
			WaterPlane->SetMaterial(0, WaterMat);
		}
		else
		{
			TNProcActors::Tint(WaterPlane, FLinearColor(0.05f, 0.3f, 0.5f));
		}
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Volumen nadable: rectángulos codiciosos sobre celdas de 4x4 quads con agua profunda.
	const int32 Step = 4;
	const int32 CW = (LatticeNX - 1) / Step;
	const int32 CH = (LatticeNY - 1) / Step;
	TArray<float> CellMin;
	CellMin.SetNumUninitialized(CW * CH);
	for (int32 cy = 0; cy < CH; ++cy)
	{
		for (int32 cx = 0; cx < CW; ++cx)
		{
			float MinH = TNumericLimits<float>::Max();
			for (int32 y = cy * Step; y <= (cy + 1) * Step; ++y)
			{
				for (int32 x = cx * Step; x <= (cx + 1) * Step; ++x)
				{
					MinH = FMath::Min(MinH, Heights[y * LatticeNX + x]);
				}
			}
			CellMin[cy * CW + cx] = MinH;
		}
	}
	const float DeepEnough = -90.f;
	TArray<uint8> Covered;
	Covered.Init(0, CW * CH);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	ATN_ProcWaterVolume* Water = World->SpawnActor<ATN_ProcWaterVolume>(ATN_ProcWaterVolume::StaticClass(), GetActorTransform(), Params);
	if (!Water)
	{
		return;
	}
	SpawnedActors.Add(Water);

	const double CellSize = Step * LatticeSpacing;
	for (int32 cy = 0; cy < CH; ++cy)
	{
		for (int32 cx = 0; cx < CW; ++cx)
		{
			const int32 Idx = cy * CW + cx;
			if (Covered[Idx] || CellMin[Idx] > DeepEnough) { continue; }
			int32 W = 1;
			float MinH = CellMin[Idx];
			while (cx + W < CW && !Covered[cy * CW + cx + W] && CellMin[cy * CW + cx + W] <= DeepEnough)
			{
				MinH = FMath::Min(MinH, CellMin[cy * CW + cx + W]);
				++W;
			}
			int32 H = 1;
			bool bGrow = true;
			while (bGrow && cy + H < CH)
			{
				for (int32 k = 0; k < W; ++k)
				{
					const int32 J = (cy + H) * CW + cx + k;
					if (Covered[J] || CellMin[J] > DeepEnough) { bGrow = false; break; }
				}
				if (bGrow)
				{
					for (int32 k = 0; k < W; ++k) { MinH = FMath::Min(MinH, CellMin[(cy + H) * CW + cx + k]); }
					++H;
				}
			}
			for (int32 yy = 0; yy < H; ++yy)
			{
				for (int32 xx = 0; xx < W; ++xx) { Covered[(cy + yy) * CW + cx + xx] = 1; }
			}
			const double Bottom = static_cast<double>(MinH) - 150.0;
			// El volumen se mide por el centro de la cápsula: techo a nivel del mar.
			const double Top = TNProcMap::SeaLevel;
			const FVector2D Min2 = LatticeOrigin + FVector2D(cx * CellSize, cy * CellSize);
			const FVector2D Size2(W * CellSize, H * CellSize);
			const FVector CenterMap(Min2.X + Size2.X * 0.5, Min2.Y + Size2.Y * 0.5, (Bottom + Top) * 0.5);
			Water->AddWaterBox(MapToWorld(CenterMap), FVector(Size2.X * 0.5, Size2.Y * 0.5, (Top - Bottom) * 0.5));
		}
	}
	UE_LOG(LogTortunabo, Log, TEXT("[ProcMap] Agua nadable: %d cajas."), Water->NumBoxes());
}

// ─────────────────────────────────────────────────────────────────────────────
// Estructuras
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::BuildStructures()
{
	using namespace TNProcMap;
	// Painted: formaciones del camino (con colisión); PaintedFar: hitos lejanos (sin ella). Color de vértice.
	FTNProcMeshBuffers Rock, Wood, Lava, SlideWater, Foliage, Painted, PaintedFar;
	// Decoración de las cuevas: lo que brilla (color de vértice emisivo) y los haces de luz (translúcido).
	FTNProcMeshBuffers Glow, Beam;
	TArray<FVector> CaveFlames, CaveMotes;
	const FLinearColor RockColor(0.32f, 0.29f, 0.26f);
	const FLinearColor WoodColor(0.45f, 0.3f, 0.16f);
	const TArray<FPathSample>& M = Layout.Main;

	// ── Puentes colosales: estilo según el bioma del cruce ─────
	// Colgante: tablones sobre dos largueros, barandilla de cuerda y, en cada apoyo (borde de torre o
	// pilar de roca), mástiles de los que cuelgan los cables principales con sus péndolas. Viaducto:
	// losas y pretiles de piedra sobre arcos rebajados entre los apoyos. Caballete: tablones y barandilla
	// de madera sobre caballetes de vigas hasta el suelo. Hierro: planchas, barandilla y pórticos de
	// hierro con cadenas en lugar de cables.
	const FLinearColor RopeColor(0.52f, 0.42f, 0.27f);
	const FLinearColor IronColor(0.16f, 0.16f, 0.18f);
	for (int32 c = 0; c < Layout.Crossings.Num(); ++c)
	{
		const FCrossing& C = Layout.Crossings[c];
		if (C.Type != ETNProcCrossingType::Bridge) { continue; }
		const FRouteStep& High = Layout.Route[C.HighStep];
		FTNPlankLine Line;
		for (int32 i = High.FirstSample; i <= High.LastSample; ++i)
		{
			Line.Add(FVector(M[i].P, C.TopZ), M[i].Width * 0.5);
		}
		const double TowerR = Layout.Params.TowerRadius;
		const double S0 = TowerR - 150.0;
		const double S1 = Line.Length() - TowerR + 150.0;
		if (S1 <= S0) { continue; }
		const uint32 Seed = Layout.Params.Seed ^ (0xB21D6u + static_cast<uint32>(c));
		const ETNProcBiome CrossBiome = Layout.Modules[C.Module].Biome;
		const ETNBridgeStyle Style = TNProcMap::BridgeStyleOf(Layout, c);
		FLinearColor Gc, Pcc, RockCc, Bdc;
		ResolveBiomeColors(CrossBiome, Gc, Pcc, RockCc, Bdc);
		const bool bSandy = CrossBiome == ETNProcBiome::Desert || CrossBiome == ETNProcBiome::Beach;
		const FLinearColor StoneC = bSandy ? TNProcLerpColor(RockCc, Gc, 0.6f) * 1.12f : TNProcLerpColor(RockCc, Gc, 0.15f) * 1.3f;
		// Plaza del puente (piedra o hierro): el tablero y sus pretiles se cortan donde la entra el borde.
		const FFeature* Plaza = nullptr;
		for (const FFeature& Fp : Layout.Features) { if (Fp.Type == EFeature::BridgePlaza && Fp.Aux == c) { Plaza = &Fp; } }
		double CutA = S1, CutB = S1;
		if (Plaza)
		{
			const double Sc = Plaza->Length - M[High.FirstSample].S;
			const double Hw = M[Plaza->PathIndex].Width * 0.5;
			const double Rc = FMath::Sqrt(FMath::Max(0.0, FMath::Square(Plaza->Radius - 20.0) - FMath::Square(Hw + 48.0)));
			CutA = FMath::Clamp(Sc - Rc, S0, S1);
			CutB = FMath::Clamp(Sc + Rc, S0, S1);
		}
		switch (Style)
		{
			case ETNBridgeStyle::Stone:
				TNProcAddStoneDeck(Painted, Line, S0, CutA, StoneC, Seed);
				if (CutB < S1) { TNProcAddStoneDeck(Painted, Line, CutB, S1, StoneC, Seed + 1u); }
				break;
			case ETNBridgeStyle::Trestle:
				TNProcAddPlanks(Wood, Line, S0, S1, WoodColor * 1.1f, Seed);
				TNProcAddRigidRails(Wood, Line, S0, S1, 250.0, 100.0, 6.0, WoodColor * 0.7f);
				break;
			case ETNBridgeStyle::Iron:
				TNProcAddPlanks(Painted, Line, S0, S1, IronColor * 1.6f, Seed);
				TNProcAddRigidRails(Painted, Line, S0, CutA, 200.0, 105.0, 4.0, IronColor);
				if (CutB < S1) { TNProcAddRigidRails(Painted, Line, CutB, S1, 200.0, 105.0, 4.0, IronColor); }
				break;
			case ETNBridgeStyle::Rope:
			default:
				TNProcAddPlanks(Wood, Line, S0, S1, WoodColor, Seed);
				TNProcAddRopeRails(Wood, Line, S0, S1, 300.0, 0.0, 105.0, WoodColor * 0.65f, RopeColor);
				break;
		}

		if (Plaza)
		{
			TNProcAddBridgePlaza(Painted, Glow, SlideWater, *Plaza, M[Plaza->PathIndex].Width * 0.5, Style == ETNBridgeStyle::Iron, StoneC, IronColor);
		}

		// Apoyos: bordes de las torres y pilares de roca de este cruce.
		TArray<double> Supports = { S0 };
		for (const FFeature& F : Layout.Features)
		{
			if (F.Type == EFeature::DeckPillar && F.Aux == c)
			{
				Supports.Add(M[F.PathIndex].S - M[High.FirstSample].S);
			}
		}
		Supports.Add(S1);
		Supports.Sort();
		if (Style == ETNBridgeStyle::Stone || Style == ETNBridgeStyle::Trestle)
		{
			// Arcos entre apoyos (desde el borde de cada pilar) o caballetes cada ~9 m, bajo el tablero.
			// Una pila o un caballete que caería sobre otro camino (el de abajo del cruce, una rama, una
			// cueva) no se pone: el arco se une al siguiente y salva el camino.
			const double PillarR = 450.0;
			auto GroundAt = [this](const FVector2D& Q) { return TerrainHeightMap(Q); };
			auto OverPath = [&](const FVector2D& Q, double R)
			{
				auto Near = [&](const TArray<FPathSample>& Arr, int32 SkipFrom, int32 SkipTo)
				{
					for (int32 i = 0; i < Arr.Num(); ++i)
					{
						if (i >= SkipFrom && i <= SkipTo) { continue; }
						const double Reach = R + Arr[i].Width * 0.5 + 300.0;
						if (FVector2D::DistSquared(Arr[i].P, Q) < Reach * Reach) { return true; }
					}
					return false;
				};
				if (Near(M, High.FirstSample - 2, High.LastSample + 2)) { return true; }
				for (const FBranch& Br : Layout.Branches) { if (Near(Br.Samples, INDEX_NONE, INDEX_NONE)) { return true; } }
				return false;
			};
			for (int32 k = 1; k < Supports.Num(); ++k)
			{
				const double A = Supports[k - 1] + (k - 1 > 0 ? PillarR : 0.0);
				const double B = Supports[k] - (k < Supports.Num() - 1 ? PillarR : 0.0);
				if (Style == ETNBridgeStyle::Stone)
				{
					// Acueducto: arcos de hasta ~36 m separados por pilas de sillería que bajan hasta el suelo.
					const int32 NArch = FMath::Max(1, FMath::CeilToInt32((B - A) / 3600.0));
					constexpr double PierHalf = 180.0;
					TArray<double> Piers;
					for (int32 a = 1; a < NArch; ++a)
					{
						const double Sp = FMath::Lerp(A, B, static_cast<double>(a) / NArch);
						FVector Pp, Dp;
						double Hwp = 0.0;
						Line.At(Sp, Pp, Dp, Hwp);
						if (!OverPath(FVector2D(Pp.X, Pp.Y), Hwp + 85.0)) { Piers.Add(Sp); }
					}
					double Prev = A;
					for (int32 a = 0; a <= Piers.Num(); ++a)
					{
						const double Next = a < Piers.Num() ? Piers[a] : B;
						TNProcAddDeckArch(PaintedFar, Line, Prev + (a > 0 ? PierHalf : 0.0), Next - (a < Piers.Num() ? PierHalf : 0.0), C.TopZ, StoneC, GroundAt);
						Prev = Next;
					}
					for (const double Sp : Piers)
					{
						FVector Pp, Dp;
						double Hwp = 0.0;
						Line.At(Sp, Pp, Dp, Hwp);
						const double G0 = TerrainHeightMap(FVector2D(Pp.X, Pp.Y)) - 80.0;
						const double Top = C.TopZ - 32.0;
						if (Top - G0 < 200.0) { continue; }
						PaintedFar.AddBox(FVector(Pp.X, Pp.Y, 0.5 * (G0 + Top)), Dp, FVector(PierHalf, Hwp + 45.0, 0.5 * (Top - G0)), StoneC * 0.9f);
						PaintedFar.AddBox(FVector(Pp.X, Pp.Y, G0 + 150.0), Dp, FVector(PierHalf + 40.0, Hwp + 85.0, 150.0), StoneC * 0.82f);
					}
					continue;
				}
				const int32 Bents = FMath::Max(1, FMath::FloorToInt32((B - A) / 900.0));
				for (int32 b = 1; b <= Bents; ++b)
				{
					const double S = FMath::Lerp(A, B, static_cast<double>(b) / (Bents + 1));
					FVector P, Dir;
					double Hw = 0.0;
					Line.At(S, P, Dir, Hw);
					const double GroundZ = TerrainHeightMap(FVector2D(P.X, P.Y));
					if (OverPath(FVector2D(P.X, P.Y), Hw + (C.TopZ - GroundZ) * 0.14 + 30.0)) { continue; }
					TNProcAddTrestleBent(PaintedFar, P, Dir, Hw, GroundZ, WoodColor * 0.85f);
				}
			}
			continue;
		}
		const bool bIron = Style == ETNBridgeStyle::Iron;
		constexpr double MastH = 750.0;
		constexpr double Low = 130.0;
		for (int32 k = 0; k < Supports.Num(); ++k)
		{
			FVector P0, Dir0;
			double Hw0 = 0.0;
			Line.At(Supports[k], P0, Dir0, Hw0);
			for (const double Side : { -1.0, 1.0 })
			{
				const FVector Base = P0 + FVector(-Dir0.Y, Dir0.X, 0.0) * (Side * (Hw0 + 25.0));
				(bIron ? Painted : Wood).AddBox(Base + FVector(0.0, 0.0, MastH * 0.5 - 60.0), Dir0, FVector(16.0, 16.0, MastH * 0.5 + 60.0), bIron ? IronColor : WoodColor * 0.55f);
			}
			if (bIron)
			{
				// Pórtico: dintel de hierro entre los dos mástiles.
				const FVector Nn(-Dir0.Y, Dir0.X, 0.0);
				Painted.AddBeam(P0 - Nn * (Hw0 + 25.0) + FVector(0.0, 0.0, MastH - 40.0), P0 + Nn * (Hw0 + 25.0) + FVector(0.0, 0.0, MastH - 40.0), 14.0, IronColor);
			}
			if (k == 0) { continue; }
			// Vano entre el apoyo anterior y este: cable parabólico y péndolas cada 3 m.
			const double A = Supports[k - 1];
			const double B = Supports[k];
			const int32 NumSeg = FMath::Max(2, FMath::RoundToInt((B - A) / 300.0));
			for (const double Side : { -1.0, 1.0 })
			{
				FVector Prev = FVector::ZeroVector;
				for (int32 t = 0; t <= NumSeg; ++t)
				{
					const double U = static_cast<double>(t) / NumSeg;
					FVector P, Dir;
					double Hw = 0.0;
					Line.At(FMath::Lerp(A, B, U), P, Dir, Hw);
					const FVector Edge = P + FVector(-Dir.Y, Dir.X, 0.0) * (Side * (Hw + 25.0));
					const FVector Cable = Edge + FVector(0.0, 0.0, Low + (MastH - Low) * FMath::Square(2.0 * U - 1.0));
					if (bIron)
					{
						if (t > 0) { TNProcAddChain(PaintedFar, Prev, Cable, IronColor * 1.3f); }
						if (t > 0 && t < NumSeg) { PaintedFar.AddBeam(Cable, Edge + FVector(0.0, 0.0, 10.0), 1.8, IronColor); }
					}
					else
					{
						if (t > 0) { Wood.AddBeam(Prev, Cable, 4.5, RopeColor * 0.8f); }
						if (t > 0 && t < NumSeg) { Wood.AddBeam(Cable, Edge + FVector(0.0, 0.0, 10.0), 2.0, RopeColor); }
					}
					Prev = Cable;
				}
			}
		}
	}

	// ── Murallas: muro almenado bajo el adarve, puerta altísima y torres de sillería ──
	// De arenisca en los biomas de arena y de la piedra del bioma en el resto.
	{
		static const FLinearColor Banners[3] = { FLinearColor(0.62f, 0.08f, 0.07f), FLinearColor(0.1f, 0.18f, 0.55f), FLinearColor(0.75f, 0.55f, 0.08f) };
		auto Ground = [this](const FVector2D& Q) { return TerrainHeightMap(Q); };
		for (int32 c = 0; c < Layout.Crossings.Num(); ++c)
		{
			const FCrossing& C = Layout.Crossings[c];
			if (C.Type != ETNProcCrossingType::Wall) { continue; }
			const FRouteStep& High = Layout.Route[C.HighStep];
			FTNWallAxis Axis;
			Axis.Build(M, High.FirstSample, High.LastSample);
			const FFeature* Gate = nullptr;
			for (const FFeature& F : Layout.Features) { if (F.Type == EFeature::Gate && F.Aux == c) { Gate = &F; } }
			const ETNProcBiome Biome = Layout.Modules[C.Module].Biome;
			FLinearColor G, Pc, RockC, Bd;
			ResolveBiomeColors(Biome, G, Pc, RockC, Bd);
			const bool bSand = Biome == ETNProcBiome::Desert || Biome == ETNProcBiome::Beach;
			const FLinearColor Stone = bSand ? TNProcLerpColor(RockC, G, 0.6f) * 1.12f : TNProcLerpColor(RockC, G, 0.15f) * 1.3f;
			const double TowerR = Layout.Params.TowerRadius;
			const double Len = Axis.Length();
			const uint32 Seed = Layout.Params.Seed ^ (0x3A11u + static_cast<uint32>(c) * 7919u);
			TNProcAddWall(Rock, Axis, TowerR - 400.0, Len - TowerR + 400.0, TowerR, Len - TowerR, C.TopZ, Gate, Ground, Stone, Seed);
			for (const FFeature& F : Layout.Features)
			{
				if (F.Type == EFeature::Tower && F.Aux == c)
				{
					TNProcAddWallTower(Rock, Foliage, Layout, F, Ground, Stone, Banners[c % 3], Seed ^ static_cast<uint32>(F.PathIndex * 2654435761u));
				}
			}
		}
	}

	for (const FFeature& F : Layout.Features)
	{
		switch (F.Type)
		{
			case EFeature::Islet:
			{
				// Hasta el lecho de la laguna (≈ -10 m): vistas desde el agua no quedan flotando.
				Rock.AddPrism(F.Polygon, F.Location.Z, -1300.0, FLinearColor(0.55f, 0.5f, 0.38f));
				break;
			}
			case EFeature::Gap:
			{
				// Labios que reducen la zanja al hueco exacto: de madera, o de roca en el río de lava de
				// una cueva (con la lava 1,5 m por debajo, de pared a pared).
				const bool bLava = IsLavaGap(F);
				const FVector2D D = F.Dir;
				const double Outer = F.Height * 0.5 + 150.0;
				const double Inner = F.Length * 0.5;
				const double HalfLen = (Outer - Inner) * 0.5;
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					const FVector2D Center2 = FVector2D(F.Location.X, F.Location.Y) + D * (Side * (Inner + HalfLen));
					(bLava ? Rock : Wood).AddBox(FVector(Center2.X, Center2.Y, F.Location.Z - (bLava ? 90.0 : 60.0)), FVector(D.X, D.Y, 0.0),
						FVector(HalfLen, F.Width * 0.5 + (bLava ? GapTrenchSideOf(F) : 0.0), bLava ? 90.0 : 60.0), bLava ? RockColor : WoodColor);
				}
				if (bLava)
				{
					const FVector2D C(F.Location.X, F.Location.Y);
					const FVector2D N = LeftNormal(D);
					const double Hl = F.Height * 0.5 + 100.0;
					const double Hs = F.Width * 0.5 + GapTrenchSideOf(F) + 100.0;
					TArray<FVector2D> Poly = { C - D * Hl - N * Hs, C + D * Hl - N * Hs, C + D * Hl + N * Hs, C - D * Hl + N * Hs };
					Lava.AddPrism(Poly, F.Location.Z - 150.0, F.Location.Z - 160.0, FLinearColor(1.f, 0.35f, 0.05f), false);
				}
				break;
			}
			case EFeature::Boardwalk:
			{
				// Pasarela de tablones sobre postes hundidos en el agua, con cuerda a los lados.
				const int32 From = FMath::Clamp(F.PathIndex, 0, M.Num() - 1);
				const int32 To = FMath::Clamp(F.Aux2, 0, M.Num() - 1);
				if (To <= From) { break; }
				FTNPlankLine Line;
				for (int32 i = From; i <= To; ++i) { Line.Add(FVector(M[i].P, M[i].Z), M[i].Width * 0.5); }
				const uint32 Seed = Layout.Params.Seed ^ (0xB0A2Du + static_cast<uint32>(From));
				TNProcAddPlanks(Wood, Line, 0.0, Line.Length(), WoodColor * 0.9f, Seed);
				TNProcAddRopeRails(Wood, Line, 0.0, Line.Length(), 260.0, 320.0, 85.0, WoodColor * 0.6f, RopeColor);
				break;
			}
			case EFeature::RiverBridge:
			{
				const FVector2D D = F.Dir;
				const FVector2D N = LeftNormal(D);
				Wood.AddBox(F.Location - FVector(0.0, 0.0, 45.0), FVector(D.X, D.Y, 0.0), FVector(F.Length * 0.5, F.Width * 0.5, 45.0), WoodColor);
				for (int32 Sd = -1; Sd <= 1; Sd += 2)
				{
					const FVector2D Rail = FVector2D(F.Location.X, F.Location.Y) + N * (Sd * (F.Width * 0.5 - 10.0));
					Wood.AddBox(FVector(Rail.X, Rail.Y, F.Location.Z + 45.0), FVector(D.X, D.Y, 0.0), FVector(F.Length * 0.5, 10.0, 45.0), WoodColor * 0.8f);
				}
				break;
			}
			case EFeature::LavaPool:
			{
				TArray<FVector2D> Disc;
				for (int32 k = 0; k < 24; ++k)
				{
					const double A = TwoPi * k / 24.0;
					Disc.Add(FVector2D(F.Location.X, F.Location.Y) + DirFromAngle(A) * (F.Radius * (1.0 + 0.06 * FMath::Sin(A * 3.0))));
				}
				Lava.AddPrism(Disc, F.Location.Z, F.Location.Z - 10.0, FLinearColor(1.f, 0.35f, 0.05f), false);
				break;
			}
			case EFeature::PathProp:
			{
				// Obstáculo de objetos del bioma (con colisión): en su marco local, sobre el suelo real.
				const FVector2D C(F.Location.X, F.Location.Y);
				const FVector2D Dx = F.Dir.GetSafeNormal().IsNearlyZero() ? FVector2D(1.0, 0.0) : F.Dir.GetSafeNormal();
				const FVector2D Dy(-Dx.Y, Dx.X);
				const double OriginZ = TerrainHeightMap(C);
				TNPropMesh::FTNPathPropParams Params;
				Params.Radius = F.Radius;
				Params.Height = F.Height;
				Params.Length = F.Length;
				Params.Seed = static_cast<uint32>(F.Aux2);
				Params.Crystal = TNPropMesh::TNPropCrystalColor(F.Biome);
				Params.bCharred = F.Biome == ETNProcBiome::Volcanic;
				auto Ground = [&](double X, double Y) { return TerrainHeightMap(C + Dx * X + Dy * Y) - OriginZ; };
				FTNProcMeshBuffers Local;
				TNPropMesh::TNPathPropBuild(Local, static_cast<EPathProp>(F.Aux), Params, Ground);
				TNFormMesh::TNFormAppend(Painted, Local, FVector(C, OriginZ), Dx);
				break;
			}
			case EFeature::Boulder:
			{
				// Peñasco con el estilo de su bioma (redondo, losa, partido, apilado, estratos, basalto, musgo,
				// cristales o coral), con color de vértice.
				const FVector2D C(F.Location.X, F.Location.Y);
				FLinearColor G, Pc, RockC, Bd;
				ResolveBiomeColors(F.Biome, G, Pc, RockC, Bd);
				const uint32 Seed = static_cast<uint32>(F.Aux);
				FTNProcMeshBuffers Local;
				TNRockMesh::TNRockBuildBoulder(Local, TNRockMesh::TNBoulderStyleFor(F.Biome, Seed), F.Radius, F.Height, Seed, TNRockMesh::TNRockColorsFor(F.Biome, RockC, G));
				TNPropMesh::TNPropAppend(Painted, Local, FVector(C, TerrainHeightMap(C)), TNRockMesh::TNRockRand(Seed, 9, 0.0, 360.0));
				break;
			}
			case EFeature::RockSpire:
			{
				// Aguja (esbelta) o mogote (bajo y ancho) con el estilo de su bioma: aguja con sombrero, inclinada,
				// gemela, chimenea de hadas, pilar kárstico con vegetación u órgano de basalto; mogote, tor, mesa
				// de estratos o domo de lava.
				const FVector2D C(F.Location.X, F.Location.Y);
				FLinearColor G, Pc, RockC, Bd;
				ResolveBiomeColors(F.Biome, G, Pc, RockC, Bd);
				const uint32 Seed = static_cast<uint32>(F.Aux);
				const bool bSpire = F.Height > F.Radius * 2.0;
				FTNProcMeshBuffers Local;
				TNRockMesh::TNRockBuildSpire(Local, TNRockMesh::TNSpireStyleFor(F.Biome, bSpire, Seed), F.Radius, F.Height, Seed,
					TNRockMesh::TNRockColorsFor(F.Biome, RockC, G));
				TNPropMesh::TNPropAppend(Painted, Local, FVector(C, TerrainHeightMap(C)), TNRockMesh::TNRockRand(Seed, 9, 0.0, 360.0));
				break;
			}
			case EFeature::Log:
			{
				const FVector2D C(F.Location.X, F.Location.Y);
				const FVector2D Half = F.Dir * (F.Length * 0.5);
				const FVector A(C - Half, TerrainHeightMap(C - Half) + F.Radius * 0.85);
				const FVector B(C + Half, TerrainHeightMap(C + Half) + F.Radius * 0.85);
				const bool bCharred = F.Biome == ETNProcBiome::Volcanic;
				const FLinearColor Bark = bCharred ? FLinearColor(0.07f, 0.06f, 0.05f) : FLinearColor(0.3f, 0.2f, 0.11f);
				const FLinearColor Cut = bCharred ? FLinearColor(0.35f, 0.12f, 0.05f) : FLinearColor(0.62f, 0.48f, 0.3f);
				TNProcAddLog(Wood, A, B, F.Radius, static_cast<uint32>(F.Aux), Bark, Cut);
				break;
			}
			case EFeature::Formation:
			{
				// Formación temática: se construye en su marco local (origen en el suelo del camino o, en los
				// hitos lejanos, en el terreno) y se apoya en el terreno real.
				const EFormation Kind = static_cast<EFormation>(F.Aux);
				FLinearColor G, Pc, RockC, Bd;
				ResolveBiomeColors(F.Biome, G, Pc, RockC, Bd);
				const FVector2D C(F.Location.X, F.Location.Y);
				const bool bFar = IsLandmarkFormation(Kind);
				const double OriginZ = bFar ? TerrainHeightMap(C) : F.Location.Z;
				const FVector2D Dx = F.Dir.GetSafeNormal().IsNearlyZero() ? FVector2D(1.0, 0.0) : F.Dir.GetSafeNormal();
				const FVector2D Dy(-Dx.Y, Dx.X);
				auto Ground = [&](double X, double Y) { return TerrainHeightMap(C + Dx * X + Dy * Y) - OriginZ; };
				TNFormMesh::FTNFormParams Params;
				Params.Width = F.Width;
				Params.Height = F.Height;
				Params.Length = F.Length;
				Params.Radius = F.Radius;
				Params.Seed = static_cast<uint32>(F.Aux2);
				Params.WaterZ = TNProcMap::SeaLevel - OriginZ;
				FTNProcMeshBuffers Local;
				TNFormMesh::TNFormBuild(Local, Kind, Params, TNFormMesh::TNFormColorsFor(F.Biome, RockC), Ground);
				TNFormMesh::TNFormAppend(bFar ? PaintedFar : Painted, Local, FVector(C, OriginZ), Dx);
				break;
			}
			case EFeature::Finish:
			{
				// Meta: neumático en arco sobre la línea (ya en el agua), boyas en toda la boca, banderines y
				// banderolas en la playa. El neumático y los mástiles llevan colisión; rótulos y telas, no.
				const FVector2D C(F.Location.X, F.Location.Y);
				const FVector2D Dx = F.Dir.GetSafeNormal().IsNearlyZero() ? FVector2D(0.0, 1.0) : F.Dir.GetSafeNormal();
				const FVector2D Dy(-Dx.Y, Dx.X);
				TNFinishMesh::FTNFinishParams Params;
				Params.Radius = F.Radius;
				Params.MouthHalf = F.Width * 0.5;
				Params.FloorZ = F.Location.Z;
				Params.Seed = static_cast<uint32>(F.Aux);
				auto Ground = [&](double X, double Y) { return TerrainHeightMap(C + Dx * X + Dy * Y) - TNProcMap::SeaLevel; };
				auto HalfWidthAt = [&](double X) { return 0.5 * FinishBeachWidthAt(Layout, F.Location.Y + X * Dx.Y); };
				FTNProcMeshBuffers Solid, Deco;
				TNFinishMesh::TNFinishBuild(Solid, Deco, Params, Ground, HalfWidthAt);
				TNFormMesh::TNFormAppend(Painted, Solid, FVector(C, TNProcMap::SeaLevel), Dx);
				TNFormMesh::TNFormAppend(PaintedFar, Deco, FVector(C, TNProcMap::SeaLevel), Dx);
				break;
			}
			case EFeature::GiantTree:
			{
				const FVector2D C(F.Location.X, F.Location.Y);
				// Secuoya: tronco rojizo con base ensanchada, raíces zancudas en arco y copa cónica por capas.
				const uint32 Seed = static_cast<uint32>(F.Aux);
				const double Ground = TerrainHeightMap(C);
				const FVector Base(C, Ground - 60.0);
				const FLinearColor Bark(0.36f, 0.17f, 0.09f);
				TArray<double> Z, R;
				for (int32 r = 0; r <= 9; ++r)
				{
					const double T = r / 9.0;
					Z.Add(F.Height * 0.9 * T);
					R.Add(F.Radius * (T < 0.08 ? FMath::Lerp(1.9, 1.0, T / 0.08) : FMath::Lerp(1.0, 0.35, (T - 0.08) / 0.92)));
				}
				TNProcAddLathe(Wood, Base, Z, R, 0.05, Seed, Bark, 12);
				const int32 NumRoots = 8 + static_cast<int32>(4.0 * (0.5 + 0.5 * TNProcHashNoise(0, 0, Seed)));
				for (int32 k = 0; k < NumRoots; ++k)
				{
					const double Ang = TwoPi * (k + 0.4 * TNProcHashNoise(k, 1, Seed)) / NumRoots;
					const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
					const double Reach = F.Radius * (2.6 + 1.2 * (0.5 + 0.5 * TNProcHashNoise(k, 2, Seed)));
					const double Top = 250.0 + 250.0 * (0.5 + 0.5 * TNProcHashNoise(k, 3, Seed));
					FVector Prev = FVector(C + Dir * (F.Radius * 0.8), Ground + Top);
					for (int32 t = 1; t <= 4; ++t)
					{
						const double U = t / 4.0;
						const FVector2D Q = C + Dir * FMath::Lerp(F.Radius * 0.8, Reach, U);
						const FVector Pt(Q, FMath::Lerp(Ground + Top, TerrainHeightMap(Q) - 40.0, U * U) + 120.0 * FMath::Sin(U * PI));
						Wood.AddBeam(Prev, Pt, F.Radius * FMath::Lerp(0.22, 0.12, U), Bark * 0.9f);
						Prev = Pt;
					}
				}
				const int32 Clumps = 6;
				for (int32 k = 0; k < Clumps; ++k)
				{
					const double T = FMath::Lerp(0.45, 0.97, static_cast<double>(k) / (Clumps - 1));
					const double CR = F.Radius * FMath::Lerp(4.2, 1.4, T) * (0.85 + 0.3 * (0.5 + 0.5 * TNProcHashNoise(k, 5, Seed)));
					const FVector Off(TNProcHashNoise(k, 6, Seed) * F.Radius * 0.8, TNProcHashNoise(k, 7, Seed) * F.Radius * 0.8, 0.0);
					TNProcAddBoulder(Foliage, Base + Off + FVector(0.0, 0.0, F.Height * T), CR, CR * 0.9, Seed + 100u + k,
						FLinearColor(0.05f, 0.2f + 0.08f * static_cast<float>(k % 2), 0.06f));
				}
				break;
			}
			case EFeature::SlideZone:
			{
				// Lámina de agua sobre la bajada (el tobogán en sí es terreno empinado): rejilla de 30 cm a lo
				// largo y ~60 cm a lo ancho. Cada vértice va 25 cm sobre el punto más alto del terreno en el
				// rectángulo de sus cuatro cuadros vecinos: así cualquier punto de la lámina (mezcla de vértices
				// que están todos por encima del terreno en ese punto) queda sobre la ladera. UV de flujo para
				// que las ondas del material corran ladera abajo, espuma blanca en los bordes y al pie y el
				// borde más transparente.
				const TArray<FPathSample>& S = F.BranchIndex == INDEX_NONE ? M : Layout.Branches[F.BranchIndex].Samples;
				const int32 From = FMath::Clamp(F.PathIndex, 0, S.Num() - 1);
				const int32 To = FMath::Clamp(F.Aux, 0, S.Num() - 1);
				constexpr double RowStep = 30.0;
				double MaxW = 0.0;
				for (int32 i = From; i <= To; ++i) { MaxW = FMath::Max(MaxW, S[i].Width); }
				const int32 Cols = FMath::Clamp(FMath::CeilToInt32(MaxW * 0.78 / 60.0), 8, 40);
				auto Lifted = [this](const FVector2D& Q, const FVector2D& Along, const FVector2D& Across, double HalfAlong, double HalfAcross)
				{
					double H = -1e18;
					for (int32 a = -2; a <= 2; ++a)
					{
						for (int32 b = -2; b <= 2; ++b)
						{
							H = FMath::Max(H, TerrainHeightMap(Q + Along * (HalfAlong * a * 0.5) + Across * (HalfAcross * b * 0.5)));
						}
					}
					return H + 25.0;
				};
				auto AddFlowQuad = [&SlideWater](const FVector (&P)[4], const FVector2D (&UV)[4], const FLinearColor (&Col)[4])
				{
					for (int32 t = 0; t < 2; ++t)
					{
						const int32 I0 = 0, I1 = t == 0 ? 1 : 2, I2 = t == 0 ? 2 : 3;
						const int32 Base = SlideWater.Verts.Num();
						SlideWater.AddTri(P[I0], P[I1], P[I2], FVector::UpVector, Col[I0]);
						// AddTri puede cambiar el orden para orientar la cara: UV y color por posición.
						for (int32 v = Base; v < SlideWater.Verts.Num(); ++v)
						{
							int32 Best = 0;
							for (int32 q = 1; q < 4; ++q) { if (FVector::DistSquared(SlideWater.Verts[v], P[q]) < FVector::DistSquared(SlideWater.Verts[v], P[Best])) { Best = q; } }
							SlideWater.UVs[v] = UV[Best];
							SlideWater.Colors[v] = Col[Best];
						}
					}
				};
				TArray<FVector> Prev;
				TArray<FVector2D> PrevUV;
				TArray<FLinearColor> PrevCol;
				double Travel = 0.0;
				FVector2D LastC = S[From].P;
				FVector2D FootC = S[To].P, FootDir = S[To].Dir;
				double FootW = S[To].Width;
				for (int32 i = From; i <= To; ++i)
				{
					const int32 Sub = i < To ? FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(S[i].P, S[i + 1].P) / RowStep)) : 1;
					for (int32 k = 0; k < Sub; ++k)
					{
						if (i == To && k > 0) { break; }
						const double U = static_cast<double>(k) / Sub;
						const FPathSample& A = S[i];
						const FPathSample& B = S[FMath::Min(i + 1, To)];
						const FVector2D C = FMath::Lerp(A.P, B.P, U);
						const FVector2D Dir = FMath::Lerp(A.Dir, B.Dir, U).GetSafeNormal();
						const FVector2D N = LeftNormal(Dir);
						const double Hw = FMath::Lerp(A.Width, B.Width, U) * 0.39;
						Travel += FVector2D::Distance(C, LastC);
						LastC = C;
						const float Foot = static_cast<float>(i - From) / FMath::Max(1, To - From);
						TArray<FVector> Row;
						TArray<FVector2D> RowUV;
						TArray<FLinearColor> RowCol;
						for (int32 c = 0; c <= Cols; ++c)
						{
							const double X = 2.0 * c / Cols - 1.0;
							const FVector2D Q = C + N * (Hw * X);
							Row.Add(FVector(Q, Lifted(Q, Dir, N, RowStep * 1.3, 2.0 * Hw / Cols * 1.15)));
							RowUV.Add(FVector2D(static_cast<double>(c) / Cols, Travel / 300.0));
							const float Edge = static_cast<float>(FMath::Pow(FMath::Abs(X), 3.0));
							FLinearColor Col = TNProcLerpColor(FLinearColor(0.18f, 0.52f, 0.8f), FLinearColor(0.96f, 0.99f, 1.f), FMath::Min(1.f, 0.75f * Edge + 0.7f * Foot * Foot * Foot));
							Col.A = FMath::Lerp(0.9f, 0.45f, Edge);
							RowCol.Add(Col);
						}
						if (Prev.Num() == Row.Num())
						{
							for (int32 c = 0; c < Cols; ++c)
							{
								const FVector P4[4] = { Prev[c], Prev[c + 1], Row[c + 1], Row[c] };
								const FVector2D UV4[4] = { PrevUV[c], PrevUV[c + 1], RowUV[c + 1], RowUV[c] };
								const FLinearColor Col4[4] = { PrevCol[c], PrevCol[c + 1], RowCol[c + 1], RowCol[c] };
								AddFlowQuad(P4, UV4, Col4);
							}
						}
						Prev = MoveTemp(Row);
						PrevUV = MoveTemp(RowUV);
						PrevCol = MoveTemp(RowCol);
						FootC = C;
						FootDir = Dir;
						FootW = FMath::Lerp(A.Width, B.Width, U);
					}
				}
				// Pocita al pie: disco de agua un poco adelantado con el borde de espuma, que se apoya en el
				// suelo por fuera (sin quedar colgado) y queda plano en el centro.
				{
					const FVector2D PC = FootC + FootDir * (FootW * 0.35);
					const double R = FMath::Clamp(FootW * 0.65, 250.0, 900.0);
					// Plana sobre el punto más alto de su disco de dentro: el suelo nunca asoma en ella.
					double PoolZ = TerrainHeightMap(PC);
					for (int32 k = 0; k < 12; ++k)
					{
						const double A = TNProcMap::TwoPi * k / 12.0;
						for (const double Rr : { 0.36, 0.72 }) { PoolZ = FMath::Max(PoolZ, TerrainHeightMap(PC + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (R * Rr))); }
					}
					PoolZ += 10.0;
					constexpr int32 Seg = 24;
					const FVector Center(PC, PoolZ);
					for (int32 k = 0; k < Seg; ++k)
					{
						const double A0 = TNProcMap::TwoPi * k / Seg, A1 = TNProcMap::TwoPi * (k + 1) / Seg;
						FVector Ring[2][2];
						FVector2D RingUV[2][2];
						for (int32 r = 0; r < 2; ++r)
						{
							const double Rr = r == 0 ? R * 0.72 : R;
							for (int32 e = 0; e < 2; ++e)
							{
								const double Aa = e == 0 ? A0 : A1;
								const FVector2D Q = PC + FVector2D(FMath::Cos(Aa), FMath::Sin(Aa)) * Rr;
								const double Zr = r == 0 ? PoolZ : FMath::Max(TerrainHeightMap(Q) + 6.0, PoolZ - 25.0);
								Ring[r][e] = FVector(Q, Zr);
								RingUV[r][e] = FVector2D((Q.X - PC.X) / 300.0, (Q.Y - PC.Y) / 300.0 + Travel / 300.0);
							}
						}
						const FLinearColor Water(0.16f, 0.5f, 0.76f, 0.88f), Foam(0.96f, 0.99f, 1.f, 0.8f);
						const FVector2D CenterUV(0.0, Travel / 300.0);
						const int32 Base = SlideWater.Verts.Num();
						SlideWater.AddTri(Center, Ring[0][0], Ring[0][1], FVector::UpVector, Water);
						for (int32 v = Base; v < SlideWater.Verts.Num(); ++v)
						{
							const FVector& Vv = SlideWater.Verts[v];
							SlideWater.UVs[v] = Vv.Equals(Center, 0.5) ? CenterUV : (Vv.Equals(Ring[0][0], 0.5) ? RingUV[0][0] : RingUV[0][1]);
						}
						const FVector Po[4] = { Ring[0][0], Ring[0][1], Ring[1][1], Ring[1][0] };
						const FVector2D UVo[4] = { RingUV[0][0], RingUV[0][1], RingUV[1][1], RingUV[1][0] };
						const FLinearColor Co[4] = { Water, Water, Foam, Foam };
						AddFlowQuad(Po, UVo, Co);
					}
				}
				break;
			}
			default:
				break;
		}
	}

	// ── Algas y nenúfares: manchas verdes flotando en las pozas junto al camino ─
	{
		FRng AlgaeRng(static_cast<uint64>(Layout.Params.Seed) * 0xA16Eull + 3ull);
		for (int32 i = 0; i < M.Num(); i += 3)
		{
			const FPathSample& Sm = M[i];
			if (Sm.Biome != ETNProcBiome::Mangrove && Sm.Biome != ETNProcBiome::Water) { continue; }
			for (int32 n = 0; n < 2; ++n)
			{
				const double Side = AlgaeRng.Chance(0.5) ? 1.0 : -1.0;
				const FVector2D Q = Sm.P + LeftNormal(Sm.Dir) * (Side * (Sm.Width * 0.5 + AlgaeRng.Range(150.0, 1800.0))) + Sm.Dir * AlgaeRng.Range(-300.0, 300.0);
				if (TerrainHeightMap(Q) > -40.0) { continue; }
				const double Rad = AlgaeRng.Range(60.0, 260.0);
				TArray<FVector2D> Poly;
				for (int32 k = 0; k < 9; ++k)
				{
					const double A = TwoPi * k / 9.0;
					Poly.Add(Q + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Rad * AlgaeRng.Range(0.6, 1.1)));
				}
				const bool bLily = Sm.Biome == ETNProcBiome::Water && AlgaeRng.Chance(0.4);
				const FLinearColor Col = bLily ? FLinearColor(0.12f, 0.42f, 0.1f) : FLinearColor(0.16f, 0.3f, 0.06f) * static_cast<float>(AlgaeRng.Range(0.8, 1.2));
				Foliage.AddPrism(Poly, TNProcMap::SeaLevel + 4.0, TNProcMap::SeaLevel - 2.0, Col, false);
			}
		}
	}

	// ── Cuevas: techo de roca sobre el túnel, interior decorado según su estilo y sus luces ──────────
	for (const FFeature& F : Layout.Features)
	{
		if (F.Type != EFeature::Cave || F.PathIndex < 0 || F.Aux >= M.Num()) { continue; }
		TArray<TNCaveMesh::FTNCaveStation> Stations;
		TArray<uint8> NoFloor;
		for (int32 i = F.PathIndex; i <= F.Aux; ++i)
		{
			TNCaveMesh::FTNCaveStation S;
			S.Floor = FVector(M[i].P, M[i].Z);
			S.Dir = M[i].Dir;
			S.HalfWidth = M[i].Width * 0.5;
			Stations.Add(S);
			// El río de lava de la cámara: sin suelo, no se decora.
			NoFloor.Add((M[i].Flags & PathFlags::Gap) != 0 ? 1 : 0);
		}
		FLinearColor G, Pc, RockC, Bd;
		ResolveBiomeColors(F.Biome, G, Pc, RockC, Bd);
		const uint32 CaveSeed = static_cast<uint32>(F.Aux2);
		const TNCaveDecor::ECaveStyle Style = TNCaveDecor::TNCaveStyleFor(F.Biome, CaveSeed);
		const bool bVolcanic = F.Biome == ETNProcBiome::Volcanic;
		TNCaveMesh::FTNCaveLook Look;
		Look.Inner = RockC * 0.55f;
		Look.Outer = RockC;
		Look.Moss = bVolcanic ? RockC * 0.8f : TNProcLerpColor(RockC, G, 0.6f);
		Look.Crystal = FLinearColor(0.45f, 0.8f, 0.95f);
		Look.bCrystals = Style == TNCaveDecor::ECaveStyle::Limestone || Style == TNCaveDecor::ECaveStyle::Crystal;
		TArray<TArray<FVector>> Inner;
		TNCaveMesh::TNCaveBuildRoof(Painted, Stations, F.Height, F.Radius, CaveSeed, Look, &Inner);

		TNCaveDecor::FTNCaveDecorOut Decor;
		TNCaveDecor::TNCaveBuildDecor(Decor, Stations, Inner, NoFloor, F.Height, CaveSeed, Style, Look);
		TNFormMesh::TNFormAppend(Painted, Decor.Solid, FVector::ZeroVector, FVector2D(1.0, 0.0));
		TNFormMesh::TNFormAppend(PaintedFar, Decor.Detail, FVector::ZeroVector, FVector2D(1.0, 0.0));
		TNFormMesh::TNFormAppend(Glow, Decor.Glow, FVector::ZeroVector, FVector2D(1.0, 0.0));
		TNFormMesh::TNFormAppend(Lava, Decor.Ember, FVector::ZeroVector, FVector2D(1.0, 0.0));
		TNFormMesh::TNFormAppend(Beam, Decor.Beam, FVector::ZeroVector, FVector2D(1.0, 0.0));
		TNFormMesh::TNFormAppend(SlideWater, Decor.Water, FVector::ZeroVector, FVector2D(1.0, 0.0));
		CaveFlames.Append(Decor.Flames);
		CaveMotes.Append(Decor.Motes);

		// Luces sin sombras: las del túnel, la estatua, cristales, setas, antorchas y el foco del lucernario.
		for (const TNCaveDecor::FTNCaveLight& Def : Decor.Lights)
		{
			UPointLightComponent* Light = nullptr;
			if (Def.bSpot)
			{
				USpotLightComponent* Spot = NewObject<USpotLightComponent>(this, NAME_None, RF_Transient);
				Spot->SetInnerConeAngle(14.f);
				Spot->SetOuterConeAngle(30.f);
				Spot->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
				Light = Spot;
			}
			else
			{
				Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
			}
			Light->SetupAttachment(RootComponent);
			Light->SetRelativeLocation(Def.P);
			Light->SetIntensityUnits(ELightUnits::Lumens);
			Light->SetIntensity(Def.Lumens);
			Light->SetAttenuationRadius(Def.Radius);
			Light->SetLightColor(Def.Color);
			Light->SetCastShadows(false);
			Light->RegisterComponent();
			CaveLights.Add(Light);
		}
	}

	// ── Componentes ─────────────────────────────────────────────────────────
	const TArray<FProcMeshTangent> NoTangents;
	UMaterialInterface* BasicMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInterface* VertexMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));

	StructureMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	StructureMesh->SetupAttachment(RootComponent);
	StructureMesh->bUseAsyncCooking = true;
	StructureMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	StructureMesh->RegisterComponent();
	if (!Rock.IsEmpty())
	{
		StructureMesh->CreateMeshSection_LinearColor(0, Rock.Verts, Rock.Tris, Rock.Normals, Rock.UVs, Rock.Colors, NoTangents, true);
		StructureMesh->SetMaterial(0, (Settings && Settings->RockMaterial) ? Settings->RockMaterial.Get() : (VertexMat ? VertexMat : BasicMat));
	}
	if (!Wood.IsEmpty())
	{
		StructureMesh->CreateMeshSection_LinearColor(1, Wood.Verts, Wood.Tris, Wood.Normals, Wood.UVs, Wood.Colors, NoTangents, true);
		StructureMesh->SetMaterial(1, (Settings && Settings->WoodMaterial) ? Settings->WoodMaterial.Get() : (VertexMat ? VertexMat : BasicMat));
	}
	// Formaciones temáticas con su color de vértice: las del camino con colisión, los hitos lejanos sin ella.
	UMaterialInterface* PaintMat = ResolveMaterial(Settings ? Settings->TerrainMaterial.Get() : nullptr,
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (!Painted.IsEmpty())
	{
		StructureMesh->CreateMeshSection_LinearColor(2, Painted.Verts, Painted.Tris, Painted.Normals, Painted.UVs, Painted.Colors, NoTangents, true);
		StructureMesh->SetMaterial(2, PaintMat);
	}
	if (!PaintedFar.IsEmpty())
	{
		StructureMesh->CreateMeshSection_LinearColor(3, PaintedFar.Verts, PaintedFar.Tris, PaintedFar.Normals, PaintedFar.UVs, PaintedFar.Colors, NoTangents, false);
		StructureMesh->SetMaterial(3, PaintMat);
	}

	DecorMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	DecorMesh->SetupAttachment(RootComponent);
	DecorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecorMesh->SetCastShadow(false);
	DecorMesh->RegisterComponent();
	if (!Lava.IsEmpty())
	{
		DecorMesh->CreateMeshSection_LinearColor(0, Lava.Verts, Lava.Tris, Lava.Normals, Lava.UVs, Lava.Colors, NoTangents, false);
		DecorMesh->SetMaterial(0, (Settings && Settings->LavaMaterial) ? Settings->LavaMaterial.Get() : (VertexMat ? VertexMat : BasicMat));
	}
	if (!SlideWater.IsEmpty())
	{
		DecorMesh->CreateMeshSection_LinearColor(1, SlideWater.Verts, SlideWater.Tris, SlideWater.Normals, SlideWater.UVs, SlideWater.Colors, NoTangents, false);
		// Agua de cascada con ondas que corren ladera abajo (UV de flujo de la lámina); si no existe el
		// material, el de los ajustes.
		UMaterialInterface* SlideMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcCascade.M_ProcCascade"));
		if (!SlideMat)
		{
			SlideMat = Settings && Settings->SlideWaterMaterial ? Settings->SlideWaterMaterial.Get()
				: (Settings && Settings->WaterMaterial ? Settings->WaterMaterial.Get() : (VertexMat ? VertexMat : BasicMat));
		}
		DecorMesh->SetMaterial(1, SlideMat);
	}
	if (!Foliage.IsEmpty())
	{
		DecorMesh->CreateMeshSection_LinearColor(2, Foliage.Verts, Foliage.Tris, Foliage.Normals, Foliage.UVs, Foliage.Colors, NoTangents, false);
		DecorMesh->SetMaterial(2, VertexMat ? VertexMat : BasicMat);
	}
	// Lo que brilla en las cuevas (setas, cristales, llamas, ojos de la estatua, cielo del lucernario):
	// emisivo del color del vértice; sin el material, el de depuración (también sin iluminar).
	if (!Glow.IsEmpty())
	{
		DecorMesh->CreateMeshSection_LinearColor(3, Glow.Verts, Glow.Tris, Glow.Normals, Glow.UVs, Glow.Colors, NoTangents, false);
		UMaterialInterface* GlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcGlow.M_ProcGlow"));
		DecorMesh->SetMaterial(3, GlowMat ? GlowMat : (VertexMat ? VertexMat : BasicMat));
	}
	// Haces de luz de los lucernarios: translúcido con la opacidad en el alfa del vértice.
	if (!Beam.IsEmpty())
	{
		UMaterialInterface* BeamMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFXSoft.M_ProcFXSoft"));
		if (BeamMat)
		{
			DecorMesh->CreateMeshSection_LinearColor(4, Beam.Verts, Beam.Tris, Beam.Normals, Beam.UVs, Beam.Colors, NoTangents, false);
			DecorMesh->SetMaterial(4, BeamMat);
		}
	}

	// ── Límites invisibles del mapa (la costa norte queda abierta hasta el mar) ─
	const double World = Layout.WorldSize;
	const double Tall = 40000.0;
	double CoastMax = 0.0;
	for (int32 k = 0; k <= 40; ++k) { CoastMax = FMath::Max(CoastMax, Layout.CoastY(World * k / 40.0)); }
	struct FWallDef { FVector Center; FVector Extent; };
	const FWallDef Walls[4] = {
		{ FVector(-300.0, World * 0.5, 0.0), FVector(300.0, World, Tall) },
		{ FVector(World + 300.0, World * 0.5, 0.0), FVector(300.0, World, Tall) },
		{ FVector(World * 0.5, -300.0, 0.0), FVector(World, 300.0, Tall) },
		{ FVector(World * 0.5, CoastMax + 16000.0, 0.0), FVector(World, 300.0, Tall) } };
	for (const FWallDef& Def : Walls)
	{
		UBoxComponent* Wall = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
		Wall->SetupAttachment(RootComponent);
		Wall->SetBoxExtent(Def.Extent);
		Wall->SetCollisionProfileName(TEXT("InvisibleWall"));
		Wall->SetHiddenInGame(true);
		Wall->RegisterComponent();
		Wall->SetRelativeLocation(Def.Center);
		BoundaryWalls.Add(Wall);
	}

	// ── Efectos ambientales (solo visuales, locales): brasas sobre la lava y bandadas de pájaros ──
	TNAmbientFX::RemoveOwner(this);
	for (const FFeature& F : Layout.Features)
	{
		const bool bPool = F.Type == EFeature::LavaPool;
		if (!bPool && !IsLavaGap(F)) { continue; }
		TNAmbientFX::FEmitterDesc Embers;
		Embers.Shape = TNAmbientFX::EShape::Ember;
		Embers.bSoft = true;
		Embers.Color = FLinearColor(1.f, 0.45f, 0.08f);
		Embers.Alpha = 0.95f;
		Embers.MaxParticles = 30;
		Embers.Rate = bPool ? 10.f : 6.f;
		Embers.SpawnRadius = static_cast<float>(bPool ? F.Radius * 0.6 : F.Width * 0.35);
		Embers.Speed = 150.f;
		Embers.Spread = 0.5f;
		Embers.Gravity = 0.f;
		Embers.Buoyancy = 45.f;
		Embers.Drag = 0.3f;
		Embers.LifeMin = 2.f;
		Embers.LifeMax = 4.f;
		Embers.SizeStart = 7.f;
		Embers.SizeEnd = 3.f;
		const double LavaZ = bPool ? F.Location.Z : F.Location.Z - 450.0;
		TNAmbientFX::AddEmitter(this, Embers, MapToWorld(FVector(F.Location.X, F.Location.Y, LavaZ + 20.0)));
	}
	// Antorchas de las cuevas: brasas que suben de la llama; en los lucernarios, polvo que flota en el haz.
	for (const FVector& Tip : CaveFlames)
	{
		TNAmbientFX::FEmitterDesc Sparks;
		Sparks.Shape = TNAmbientFX::EShape::Ember;
		Sparks.bSoft = true;
		Sparks.Color = FLinearColor(1.f, 0.55f, 0.12f);
		Sparks.Alpha = 0.95f;
		Sparks.MaxParticles = 12;
		Sparks.Rate = 5.f;
		Sparks.SpawnRadius = 5.f;
		Sparks.Speed = 60.f;
		Sparks.Spread = 0.4f;
		Sparks.Gravity = 0.f;
		Sparks.Buoyancy = 40.f;
		Sparks.Drag = 0.4f;
		Sparks.LifeMin = 0.6f;
		Sparks.LifeMax = 1.3f;
		Sparks.SizeStart = 4.f;
		Sparks.SizeEnd = 1.5f;
		Sparks.WakeDistance = 6000.f;
		TNAmbientFX::AddEmitter(this, Sparks, MapToWorld(Tip));
	}
	for (const FVector& Mote : CaveMotes)
	{
		TNAmbientFX::FEmitterDesc Dust;
		Dust.Shape = TNAmbientFX::EShape::Ember;
		Dust.bSoft = true;
		Dust.Color = FLinearColor(1.f, 0.95f, 0.8f);
		Dust.Alpha = 0.5f;
		Dust.MaxParticles = 30;
		Dust.Rate = 4.f;
		Dust.SpawnRadius = 140.f;
		Dust.Speed = 12.f;
		Dust.Spread = 1.f;
		Dust.Gravity = 0.f;
		Dust.Buoyancy = 2.f;
		Dust.Drag = 0.6f;
		Dust.LifeMin = 5.f;
		Dust.LifeMax = 9.f;
		Dust.SizeStart = 2.5f;
		Dust.SizeEnd = 2.f;
		Dust.WakeDistance = 5000.f;
		TNAmbientFX::AddEmitter(this, Dust, MapToWorld(Mote));
	}
	// Confeti de la meta (cuatro colores): estalla cuando alguien cruza la línea (ATN_ProcMapGenerator::Tick).
	for (const FFeature& F : Layout.Features)
	{
		if (F.Type != EFeature::Finish) { continue; }
		const FLinearColor Colors[4] = { FLinearColor(0.9f, 0.1f, 0.1f), FLinearColor(1.f, 0.8f, 0.05f), FLinearColor(0.1f, 0.4f, 0.95f), FLinearColor(0.1f, 0.75f, 0.25f) };
		for (const FLinearColor& Col : Colors)
		{
			TNAmbientFX::FEmitterDesc Confetti;
			Confetti.Shape = TNAmbientFX::EShape::Flake;
			Confetti.Color = Col;
			Confetti.MaxParticles = 70;
			Confetti.Rate = 0.f;
			Confetti.SpawnRadius = static_cast<float>(F.Radius * 0.8);
			Confetti.Speed = 900.f;
			Confetti.SpeedJitter = 0.4f;
			Confetti.Spread = 0.9f;
			Confetti.Gravity = -320.f;
			Confetti.Drag = 1.6f;
			Confetti.LifeMin = 3.f;
			Confetti.LifeMax = 5.f;
			Confetti.SizeStart = 12.f;
			Confetti.SizeEnd = 12.f;
			Confetti.WakeDistance = 30000.f;
			TNAmbientFX::AddEmitter(this, Confetti, MapToWorld(FVector(F.Location.X, F.Location.Y, 950.0)));
		}
	}
	{
		// Gaviotas sobre la playa de la meta y la costa; guacamayos en la selva; pájaros oscuros sobre los
		// bosques y la roca; buitres lentos en el desierto.
		FRng BirdRng(static_cast<uint64>(Layout.Params.Seed) * 0xB1ull + 17ull);
		for (const FFeature& F : Layout.Features)
		{
			if (F.Type != EFeature::Finish) { continue; }
			TNAmbientFX::AddFlock(this, MapToWorld(FVector(F.Location.X, F.Location.Y - 2500.0, 2800.0)), 7, 3200.f, 0.22f, 1.2f,
				FLinearColor(0.95f, 0.95f, 0.93f), FLinearColor(0.68f, 0.7f, 0.74f), 11u);
		}
		int32 Coastal = 0, Forest = 0, Desert = 0;
		for (const FModule& Mod : Layout.Modules)
		{
			const FVector2D C = Mod.Centroid;
			const double Ground = TerrainHeightMap(C);
			const uint32 Seed = static_cast<uint32>(BirdRng.RangeInt(1, 1 << 20));
			const float Dir = BirdRng.Chance(0.5) ? 1.f : -1.f;
			switch (Mod.Biome)
			{
				case ETNProcBiome::Beach:
					if (Coastal++ < 3)
					{
						TNAmbientFX::AddFlock(this, MapToWorld(FVector(C, Ground + BirdRng.Range(2500.0, 4000.0))), BirdRng.RangeInt(4, 7),
							static_cast<float>(BirdRng.Range(2500.0, 4200.0)), 0.2f * Dir, 1.15f, FLinearColor(0.95f, 0.95f, 0.93f), FLinearColor(0.68f, 0.7f, 0.74f), Seed);
					}
					break;
				case ETNProcBiome::Jungle:
				case ETNProcBiome::Mangrove:
					if (Forest++ < 6)
					{
						const bool bMacaw = Mod.Biome == ETNProcBiome::Jungle && BirdRng.Chance(0.6);
						TNAmbientFX::AddFlock(this, MapToWorld(FVector(C, Ground + BirdRng.Range(3000.0, 5000.0))), BirdRng.RangeInt(5, 9),
							static_cast<float>(BirdRng.Range(2800.0, 4800.0)), 0.3f * Dir, bMacaw ? 0.9f : 0.7f,
							bMacaw ? FLinearColor(0.85f, 0.12f, 0.08f) : FLinearColor(0.14f, 0.13f, 0.12f),
							bMacaw ? FLinearColor(0.1f, 0.45f, 0.85f) : FLinearColor(0.24f, 0.2f, 0.16f), Seed);
					}
					break;
				case ETNProcBiome::Water:
				case ETNProcBiome::Rocky:
					if (Forest++ < 6)
					{
						TNAmbientFX::AddFlock(this, MapToWorld(FVector(C, Ground + BirdRng.Range(3500.0, 6000.0))), BirdRng.RangeInt(6, 10),
							static_cast<float>(BirdRng.Range(3000.0, 5500.0)), 0.28f * Dir, 0.65f, FLinearColor(0.14f, 0.13f, 0.12f), FLinearColor(0.24f, 0.2f, 0.16f), Seed);
					}
					break;
				case ETNProcBiome::Desert:
					if (Desert++ < 2)
					{
						TNAmbientFX::AddFlock(this, MapToWorld(FVector(C, Ground + BirdRng.Range(5000.0, 7000.0))), BirdRng.RangeInt(3, 5),
							static_cast<float>(BirdRng.Range(3500.0, 5000.0)), 0.09f * Dir, 1.7f, FLinearColor(0.2f, 0.15f, 0.1f), FLinearColor(0.3f, 0.22f, 0.14f), Seed);
					}
					break;
				default:
					break;
			}
		}
	}
}
