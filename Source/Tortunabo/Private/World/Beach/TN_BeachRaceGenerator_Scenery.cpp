// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachRaceGenerator — escenografía de la playa: la salida en el linde de la selva
// (árbol colosal con raíces tabulares, hojas enormes y el cartel «¡A LA META!»), la
// meta (arco de neumático del mapa procedural a escala, banderolas y boyas con
// banderas a cuadros que flotan), la selva de los bordes (vegetación instanciada del
// mapa procedural a 200-300 m), las huellas del reparto y el chapuzón de la meta.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/Beach/TN_BeachRaceGenerator.h"
#include "Core/TN_Log.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "World/ProcMap/TN_ProcMapFlora.h"
#include "World/ProcMap/TN_ProcMapTypes.h"
#include "TN_BeachRaceKit.h"
#include "../ProcMap/TN_ProcMapAmbientFX.h"
#include "../ProcMap/TN_ProcMapFinishMeshes.h"
#include "../ProcMap/TN_ProcMapFloraMeshes.h"
#include "../ProcMap/TN_ProcMapRuntimeMesh.h"

namespace TNBeachScenery
{
	using FBuffers = TNProcMesh::FTNProcMeshBuffers;

	/** Punto de una curva de Bézier cuadrática en planta. */
	FVector2D Bezier(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, double T)
	{
		const double U = 1.0 - T;
		return P0 * (U * U) + P1 * (2.0 * U * T) + P2 * (T * T);
	}

	/**
	 * Raíz tabular del árbol colosal: una aleta que sale del tronco (P0) y baja serpenteando hasta la arena (P2), con
	 * sección de lente que adelgaza hacia la punta; BaseH es su alto junto al tronco. Enterrada 2 m (sin huecos en las
	 * dunas).
	 */
	void AddButtress(FBuffers& M, const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, double BaseH, double Thick, uint32 Seed, const FLinearColor& Color)
	{
		constexpr int32 Stations = 16;
		TArray<TArray<FVector>> Rings;
		FVector2D LastTan(1.0, 0.0);
		for (int32 s = 0; s <= Stations; ++s)
		{
			const double T = static_cast<double>(s) / Stations;
			const FVector2D Q = Bezier(P0, P1, P2, T);
			const FVector2D Tan = ((P1 - P0) * (2.0 * (1.0 - T)) + (P2 - P1) * (2.0 * T)).GetSafeNormal();
			LastTan = Tan;
			const FVector2D Sd(-Tan.Y, Tan.X);
			const double Ground = TNBeachLayout::GroundZ(Q.X, Q.Y);
			const double Top = Ground + BaseH * FMath::Pow(1.0 - T, 1.35) + 120.0 * (1.0 - 0.5 * T);
			const double Bottom = Ground - 200.0;
			const double Th = Thick * FMath::Lerp(1.0, 0.35, T) * (1.0 + 0.12 * TNProcMesh::TNProcHashNoise(s, 3, Seed));
			auto At = [&Q, &Sd](double Across, double Z) { return FVector(Q + Sd * Across, Z); };
			TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
			Ring.Add(At(0.5 * Th, Bottom));
			Ring.Add(At(0.45 * Th, FMath::Lerp(Bottom, Top, 0.6)));
			Ring.Add(At(0.2 * Th, FMath::Lerp(Bottom, Top, 0.92)));
			Ring.Add(At(0.0, Top));
			Ring.Add(At(-0.2 * Th, FMath::Lerp(Bottom, Top, 0.92)));
			Ring.Add(At(-0.45 * Th, FMath::Lerp(Bottom, Top, 0.6)));
			Ring.Add(At(-0.5 * Th, Bottom));
		}
		M.AddSweep(Rings, true, Color);
		const TArray<FVector>& Tip = Rings.Last();
		FVector Center = FVector::ZeroVector;
		for (const FVector& V : Tip) { Center += V; }
		Center /= static_cast<double>(Tip.Num());
		for (int32 k = 0; k < Tip.Num(); ++k) { M.AddTri(Center, Tip[k], Tip[(k + 1) % Tip.Num()], FVector(LastTan, 0.0), Color * 0.9f); }
	}

	/**
	 * Hoja enorme de platanera: nervio que sube un poco y cae hacia la punta, lámina con los bordes algo caídos, a veces
	 * rota en tiras (como las de verdad), con las dos caras y el nervio claro.
	 */
	void AddGiantLeaf(FBuffers& M, const FVector& Root, double YawDeg, double Len, double Width, uint32 Seed)
	{
		const double Yaw = FMath::DegreesToRadians(YawDeg);
		const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
		const FVector Side(-Dir.Y, Dir.X, 0.0);
		const FVector Up(0.0, 0.0, 1.0);
		const double Tone = 0.88 + 0.24 * TNBeachRaceKit::Hash01(1, 2, Seed);
		const FLinearColor Leaf = TNBeachRaceKit::Hex(0x3E8A2Cu) * static_cast<float>(Tone);
		const FLinearColor Under = TNBeachRaceKit::Hex(0x74A94Eu) * static_cast<float>(Tone);
		const FLinearColor Torn = TNBeachRaceKit::Hex(0x86933Cu);
		const FLinearColor Rib = TNBeachRaceKit::Hex(0xB9C265u);
		auto Mid = [&Root, &Dir, &Up, Len](double T)
		{
			return Root + Dir * (Len * T) + Up * (Len * (0.18 * FMath::Sin(PI * 0.7 * T) - 0.55 * T * T));
		};
		auto HalfW = [Width](double T) { return 0.5 * Width * FMath::Pow(FMath::Max(0.0, FMath::Sin(PI * T)), 0.75); };
		constexpr int32 Segs = 14;
		for (int32 k = 0; k < Segs; ++k)
		{
			const double T0 = static_cast<double>(k) / Segs;
			const double T1 = static_cast<double>(k + 1) / Segs;
			const FVector M0 = Mid(T0);
			const FVector M1 = Mid(T1);
			const FVector Normal = FVector::CrossProduct(M1 - M0, Side).GetSafeNormal();
			for (const double S : { -1.0, 1.0 })
			{
				const bool bTear = k > 1 && k < Segs - 2 && TNBeachRaceKit::Hash01(k, S > 0.0 ? 1 : 0, Seed) < 0.18;
				const double W0 = HalfW(T0) * (bTear ? 0.45 : 1.0);
				const double W1 = HalfW(T1) * (bTear ? 0.45 : 1.0);
				const FVector E0 = M0 + Side * (S * W0) - Up * (0.12 * W0);
				const FVector E1 = M1 + Side * (S * W1) - Up * (0.12 * W1);
				M.AddQuad(M0, M1, E1, E0, Normal, bTear ? Torn : Leaf);
				M.AddQuad(M0, M1, E1, E0, -Normal, Under);
			}
			M.AddBeam(M0 + Normal * 10.0, M1 + Normal * 10.0, 35.0 * (1.0 - 0.6 * T0), Rib);
		}
	}

	/** Planta de hojas enormes: tallo grueso (con colisión, en Solid) y sus hojas desde lo alto (sin colisión, en Deco). */
	struct FGiantPlant
	{
		FVector2D Base;
		double StemH;
		double Leaves[4];
		int32 NumLeaves;
		double LeafLen;
		double LeafWidth;
	};

	// ── Lianas y hojas de los huecos de la selva (mallas estáticas instanciadas, con viento por vértice) ──

	/** Colores para una malla estática en ejecución: MakeStaticMesh decodifica una vez más, así que van en sRGB. */
	FLinearColor Srgb(uint32 Hex, float Sway)
	{
		return FLinearColor(((Hex >> 16) & 255) / 255.f, ((Hex >> 8) & 255) / 255.f, (Hex & 255) / 255.f, Sway);
	}

	/** Tubo de 5 lados a lo largo de Points (radio que adelgaza hacia el final) con el viento de cada punto en el alfa. */
	void AddVineTube(FBuffers& M, const TArray<FVector>& Points, const TArray<float>& Sway, double Radius, uint32 Hex)
	{
		const int32 N = Points.Num();
		if (N < 2) { return; }
		TArray<TArray<FVector>> Rings;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Tan = (Points[FMath::Min(i + 1, N - 1)] - Points[FMath::Max(i - 1, 0)]).GetSafeNormal();
			FVector U = FVector::CrossProduct(Tan, FMath::Abs(Tan.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
			const FVector V = FVector::CrossProduct(Tan, U);
			const double R = Radius * FMath::Lerp(1.0, 0.55, static_cast<double>(i) / (N - 1));
			TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
			for (int32 k = 0; k < 5; ++k)
			{
				const double A = TNProcMap::TwoPi * k / 5.0;
				Ring.Add(Points[i] + (U * FMath::Cos(A) + V * FMath::Sin(A)) * R);
			}
		}
		const int32 First = M.Verts.Num();
		M.AddSweep(Rings, true, Srgb(Hex, 0.f));
		// El viento de cada vértice: el de su punto más cercano del eje.
		for (int32 v = First; v < M.Verts.Num(); ++v)
		{
			int32 Best = 0;
			double BestD = TNumericLimits<double>::Max();
			for (int32 i = 0; i < N; ++i)
			{
				const double D = FVector::DistSquared(M.Verts[v], Points[i]);
				if (D < BestD)
				{
					BestD = D;
					Best = i;
				}
			}
			M.Colors[v].A = Sway[Best];
		}
	}

	/** Hoja de liana (acorazonada, dos caras) colgando de Base hacia Dir. */
	void AddVineLeaf(FBuffers& M, const FVector& Base, const FVector& Dir, double Len, uint32 Hex, float Sway)
	{
		const FVector D = Dir.GetSafeNormal();
		FVector Side = FVector::CrossProduct(D, FVector::UpVector).GetSafeNormal();
		if (Side.IsNearlyZero()) { Side = FVector(0.0, 1.0, 0.0); }
		const FVector Normal = FVector::CrossProduct(Side, D).GetSafeNormal();
		const FVector Tip = Base + D * Len;
		const FVector L = Base + D * (Len * 0.4) + Side * (Len * 0.32);
		const FVector Rr = Base + D * (Len * 0.4) - Side * (Len * 0.32);
		const FLinearColor C = Srgb(Hex, Sway);
		M.AddQuad(Base, L, Tip, Rr, Normal, C);
		M.AddQuad(Base, L, Tip, Rr, -Normal, C * 0.85f);
	}

	/** Liana colgada entre dos troncos: de (0, 0, 0) a (Span, 0, 0) con una comba de Sag, dos hebras, hojas y colgajos. */
	void BuildLianaDrape(FBuffers& M, uint32 Seed)
	{
		constexpr double Span = 10000.0;
		constexpr double Sag = 2200.0;
		constexpr int32 Segs = 22;
		for (int32 s = 0; s < 2; ++s)
		{
			TArray<FVector> Points;
			TArray<float> Sway;
			const double Phase = TNProcMap::TwoPi * TNBeachRaceKit::Hash01(s, 1, Seed);
			for (int32 i = 0; i <= Segs; ++i)
			{
				const double T = static_cast<double>(i) / Segs;
				const double Twist = (s == 0 ? 1.0 : -1.0) * 40.0 * FMath::Sin(T * PI * 6.0 + Phase);
				Points.Add(FVector(T * Span, Twist, -Sag * 4.0 * T * (1.0 - T) - (s == 0 ? 0.0 : 60.0) + 25.0 * FMath::Cos(T * PI * 4.0 + Phase)));
				Sway.Add(static_cast<float>(0.45 * FMath::Sin(PI * T)));
			}
			AddVineTube(M, Points, Sway, s == 0 ? 34.0 : 24.0, s == 0 ? 0x5C4B2Bu : 0x4E5A26u);
			if (s == 0)
			{
				for (int32 i = 1; i < Segs; ++i)
				{
					const double Down = 0.6 + 0.4 * TNBeachRaceKit::Hash01(i, 3, Seed);
					const FVector Dir(0.25 * TNProcMesh::TNProcHashNoise(i, 4, Seed), (i % 2 ? 1.0 : -1.0) * 0.6, -Down);
					AddVineLeaf(M, Points[i], Dir, 240.0 + 80.0 * TNBeachRaceKit::Hash01(i, 5, Seed), i % 3 ? 0x2C6E1Cu : 0x3F8A26u, Sway[i] + 0.1f);
				}
			}
		}
		// Colgajos: hebras finas que caen de la comba, con alguna hoja.
		for (int32 c = 0; c < 4; ++c)
		{
			const double T = 0.2 + 0.2 * c + 0.06 * TNProcMesh::TNProcHashNoise(c, 6, Seed);
			const FVector Top(T * Span, 0.0, -Sag * 4.0 * T * (1.0 - T));
			const double Drop = 600.0 + 1200.0 * TNBeachRaceKit::Hash01(c, 7, Seed);
			TArray<FVector> Points;
			TArray<float> Sway;
			for (int32 i = 0; i <= 5; ++i)
			{
				const double F = i / 5.0;
				Points.Add(Top + FVector(60.0 * FMath::Sin(F * 3.0 + c), 50.0 * FMath::Cos(F * 2.0 + c), -Drop * F));
				Sway.Add(static_cast<float>(0.45 * FMath::Sin(PI * T) + 0.4 * F));
			}
			AddVineTube(M, Points, Sway, 12.0, 0x4E5A26u);
			AddVineLeaf(M, Points[3], FVector(0.3, 0.8, -0.6), 200.0, 0x3F8A26u, Sway[3]);
			AddVineLeaf(M, Points[5], FVector(-0.4, -0.6, -0.8), 180.0, 0x2C6E1Cu, Sway[5]);
		}
	}

	/** Cortina de lianas que cuelga de (0, 0, 0) hasta ~120 m más abajo: tres hebras que se mecen, con hojas. */
	void BuildLianaCurtain(FBuffers& M, uint32 Seed)
	{
		constexpr double Drop = 12000.0;
		constexpr int32 Segs = 16;
		for (int32 s = 0; s < 3; ++s)
		{
			const FVector2D Base(120.0 * FMath::Cos(2.1 * s + Seed % 7), 120.0 * FMath::Sin(2.1 * s + Seed % 7));
			const double Len = Drop * (0.7 + 0.3 * TNBeachRaceKit::Hash01(s, 1, Seed));
			const double Phase = TNProcMap::TwoPi * TNBeachRaceKit::Hash01(s, 2, Seed);
			TArray<FVector> Points;
			TArray<float> Sway;
			for (int32 i = 0; i <= Segs; ++i)
			{
				const double T = static_cast<double>(i) / Segs;
				const double Swing = 150.0 * T * FMath::Sin(T * PI * 2.5 + Phase);
				Points.Add(FVector(Base.X + Swing, Base.Y + 0.6 * Swing, -Len * T));
				Sway.Add(static_cast<float>(0.7 * FMath::Pow(T, 1.2)));
			}
			AddVineTube(M, Points, Sway, 26.0 - 5.0 * s, s == 1 ? 0x5C4B2Bu : 0x4E5A26u);
			for (int32 i = 2; i <= Segs; i += 2)
			{
				const double A = TNProcMap::TwoPi * TNBeachRaceKit::Hash01(i, s + 3, Seed);
				AddVineLeaf(M, Points[i], FVector(FMath::Cos(A), FMath::Sin(A), -0.7), 230.0 + 90.0 * TNBeachRaceKit::Hash01(i, s + 9, Seed),
					(i + s) % 3 ? 0x2C6E1Cu : 0x4A9A2Eu, Sway[i]);
			}
		}
	}

	/** Mata de hojas enormes (como las de la salida, más pequeña): tallo de 10 m y 5-7 hojas que caen y se mecen por la punta. */
	void BuildLeafClump(FBuffers& M, uint32 Seed)
	{
		TNProcMesh::TNProcAddCylinder(M, FVector(0.0, 0.0, -60.0), FVector(0.0, 0.0, 1000.0), 85.0, 50.0, 7, Srgb(0x6F8A3Eu, 0.f));
		const int32 First = M.Verts.Num();
		const int32 Leaves = 5 + static_cast<int32>(Seed % 3u);
		for (int32 l = 0; l < Leaves; ++l)
		{
			// Hojas de 12-18 m: la punta cae ~0,4 veces el largo y se queda por encima del suelo.
			const double Yaw = 360.0 * (l + 0.35 * TNProcMesh::TNProcHashNoise(l, 1, Seed)) / Leaves;
			AddGiantLeaf(M, FVector(0.0, 0.0, 990.0), Yaw, 1200.0 + 600.0 * TNBeachRaceKit::Hash01(l, 2, Seed), 480.0 + 200.0 * TNBeachRaceKit::Hash01(l, 3, Seed),
				Seed * 13u + static_cast<uint32>(l));
		}
		// Las hojas se hacen con colores lineales (las de la salida son malla procedural): a sRGB, con el viento hacia la punta.
		for (int32 v = First; v < M.Verts.Num(); ++v)
		{
			FLinearColor& C = M.Colors[v];
			C = FLinearColor(FMath::Pow(C.R, 1.f / 2.2f), FMath::Pow(C.G, 1.f / 2.2f), FMath::Pow(C.B, 1.f / 2.2f), 0.f);
			const double Reach = FVector2D(M.Verts[v].X, M.Verts[v].Y).Size();
			C.A = static_cast<float>(0.55 * FMath::Clamp(Reach / 2000.0, 0.0, 1.0));
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Salida
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildStartGrove()
{
	TNBeachScenery::FBuffers Solid;
	TNBeachScenery::FBuffers Deco;
	const FLinearColor Bark = TNBeachRaceKit::Hex(0x6E5A45u);
	const FLinearColor BarkDark = TNBeachRaceKit::Hex(0x4F3F31u);
	const FLinearColor Moss = TNBeachRaceKit::Hex(0x5D6B34u);
	const FVector2D Trunk(TNBeachLayout::TrunkX, 0.0);
	const double TrunkGround = TNBeachLayout::GroundZ(Trunk.X, Trunk.Y);

	// Tronco colosal (una ceiba de ~300 m), ensanchado en la base; se pierde en su copa por encima de la selva.
	TNProcMesh::TNProcAddLathe(Solid, FVector(Trunk, TrunkGround - 300.0), { 0.0, 800.0, 2200.0, 4800.0, 9000.0, 15000.0, 22000.0, 28000.0 },
		{ 2600.0, 1950.0, 1600.0, 1400.0, 1200.0, 1000.0, 800.0, 650.0 }, 0.07, 0x7E0Cu, Bark, 16, 0.25);
	const double CrownZ = TrunkGround + 27500.0;
	for (int32 b = 0; b < 9; ++b)
	{
		const double A = TNProcMap::TwoPi * (b + 0.3 * TNProcMesh::TNProcHashNoise(b, 1, 0xC0Au)) / 9.0;
		const double R = 5200.0 + 2000.0 * TNBeachRaceKit::Hash01(b, 2, 0xC0Au);
		const FVector Blob(Trunk.X + FMath::Cos(A) * R, Trunk.Y + FMath::Sin(A) * R, CrownZ + 1500.0 + 2500.0 * TNBeachRaceKit::Hash01(b, 3, 0xC0Au));
		Deco.AddBeam(FVector(Trunk, CrownZ - 2000.0), Blob - FVector(0.0, 0.0, 1500.0), 320.0, BarkDark);
		TNFloraMesh::TNFloraBlob(Deco, Blob, 5000.0 + 1800.0 * TNBeachRaceKit::Hash01(b, 4, 0xC0Au), 2600.0 + 900.0 * TNBeachRaceKit::Hash01(b, 5, 0xC0Au),
			0xC0A0u + static_cast<uint32>(b), (b % 2) ? TNBeachRaceKit::Hex(0x2F6E22u) : TNBeachRaceKit::Hex(0x3D7F2Au), 9);
	}
	TNFloraMesh::TNFloraBlob(Deco, FVector(Trunk, CrownZ + 4500.0), 6500.0, 3200.0, 0xC0AFu, TNBeachRaceKit::Hex(0x357525u), 10);

	// Raíces tabulares: las dos que enmarcan la salida (bajan hasta la línea de salida a ±34 m) y las de detrás.
	for (const double Side : { -1.0, 1.0 })
	{
		const double A0 = FMath::DegreesToRadians(48.0);
		const FVector2D P0(Trunk.X + 1800.0 * FMath::Cos(A0), Side * 1800.0 * FMath::Sin(A0));
		TNBeachScenery::AddButtress(Solid, P0, FVector2D(-2300.0, Side * 2800.0), FVector2D(400.0, Side * 3400.0), 3000.0, 700.0,
			Side > 0.0 ? 0xB0A1u : 0xB0A2u, Side > 0.0 ? Bark : Bark * 0.94f);
	}
	static const double BackAngles[] = { 100.0, -100.0, 145.0, -145.0, 185.0 };
	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(BackAngles)); ++i)
	{
		const double A = FMath::DegreesToRadians(BackAngles[i]);
		const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
		const FVector2D Perp(-Dir.Y, Dir.X);
		const double Reach = 6500.0 + 2500.0 * TNBeachRaceKit::Hash01(i, 7, 0xB0Bu);
		const FVector2D P0 = Trunk + Dir * 1800.0;
		const FVector2D P2 = Trunk + Dir * Reach + Perp * (1200.0 * TNProcMesh::TNProcHashNoise(i, 8, 0xB0Bu));
		TNBeachScenery::AddButtress(Solid, P0, (P0 + P2) * 0.5 + Perp * 900.0, P2, 2600.0, 650.0, 0xB0C0u + static_cast<uint32>(i), i % 2 ? Bark : Moss);
	}

	// Plantas de hojas enormes a los lados y detrás de la salida: sus hojas cubren la salida como un techo.
	static const TNBeachScenery::FGiantPlant Plants[] = {
		{ FVector2D(-2700.0, 4100.0), 2400.0, { -48.0, -10.0, 60.0, 150.0 }, 4, 5200.0, 1500.0 },
		{ FVector2D(-2900.0, -4300.0), 2800.0, { 50.0, 10.0, -60.0, -150.0 }, 4, 5600.0, 1600.0 },
		{ FVector2D(-4300.0, 2900.0), 3600.0, { 0.0, -35.0, 70.0, 160.0 }, 4, 6000.0, 1750.0 },
		{ FVector2D(-4100.0, -2700.0), 3200.0, { 15.0, 45.0, -70.0, -170.0 }, 4, 5400.0, 1600.0 },
		{ FVector2D(-1600.0, 5600.0), 1600.0, { -30.0, 30.0, 120.0, 0.0 }, 3, 4200.0, 1300.0 },
		{ FVector2D(-1800.0, -5800.0), 1700.0, { 25.0, -25.0, -120.0, 0.0 }, 3, 4400.0, 1300.0 },
	};
	const FLinearColor Stem = TNBeachRaceKit::Hex(0x6F8A3Eu);
	for (int32 p = 0; p < static_cast<int32>(UE_ARRAY_COUNT(Plants)); ++p)
	{
		const TNBeachScenery::FGiantPlant& Plant = Plants[p];
		const double Ground = TNBeachLayout::GroundZ(Plant.Base.X, Plant.Base.Y);
		const FVector StemTop(Plant.Base, Ground + Plant.StemH);
		TNProcMesh::TNProcAddCylinder(Solid, FVector(Plant.Base, Ground - 200.0), StemTop, 260.0, 170.0, 8, Stem);
		for (int32 l = 0; l < Plant.NumLeaves; ++l)
		{
			const double Len = Plant.LeafLen * (0.85 + 0.3 * TNBeachRaceKit::Hash01(p, l, 0x1EAFu));
			TNBeachScenery::AddGiantLeaf(Deco, StemTop, Plant.Leaves[l], Len, Plant.LeafWidth, 0x1EA0u + static_cast<uint32>(p * 8 + l));
		}
	}

	// Cartel de salida entre dos palos de madera a la deriva, por encima de las tortugas: «¡A LA META!» hacia ellas y el
	// nombre del juego por detrás.
	const double GateX = 300.0;
	const double PostY = 2700.0;
	const double GateGround = TNBeachLayout::GroundZ(GateX, 0.0);
	const double BarZ = GateGround + 1500.0;
	const FLinearColor Drift = TNBeachRaceKit::Hex(0xB8AA95u);
	for (const double Side : { -1.0, 1.0 })
	{
		const double Ground = TNBeachLayout::GroundZ(GateX, Side * PostY);
		TNProcMesh::TNProcAddCylinder(Solid, FVector(GateX, Side * PostY, Ground - 200.0), FVector(GateX + 40.0, Side * (PostY + 60.0), BarZ + 250.0), 130.0, 95.0, 7, Drift);
	}
	Deco.AddBeam(FVector(GateX + 20.0, -PostY - 250.0, BarZ), FVector(GateX + 20.0, PostY + 250.0, BarZ), 70.0, Drift * 0.9f);
	{
		const FLinearColor Canvas = TNBeachRaceKit::Hex(0xF1E6CCu);
		const FLinearColor Stripe = TNBeachRaceKit::Hex(0xC7392Fu);
		const double Y0 = -2450.0;
		const double Y1 = 2450.0;
		const double ZTop = BarZ - 60.0;
		const double ZBot = BarZ - 660.0;
		constexpr int32 Strips = 12;
		for (int32 k = 0; k < Strips; ++k)
		{
			const double Ya = FMath::Lerp(Y0, Y1, static_cast<double>(k) / Strips);
			const double Yb = FMath::Lerp(Y0, Y1, static_cast<double>(k + 1) / Strips);
			// Comba de la tela entre los palos.
			const double Sa = 60.0 * FMath::Sin(PI * static_cast<double>(k) / Strips);
			const double Sb = 60.0 * FMath::Sin(PI * static_cast<double>(k + 1) / Strips);
			TNFinishMesh::TNFinishCloth(Deco, FVector(GateX, Ya, ZTop - Sa), FVector(GateX, Yb, ZTop - Sb), FVector(GateX, Yb, ZBot - Sb), FVector(GateX, Ya, ZBot - Sa),
				FVector(-1.0, 0.0, 0.0), Canvas);
			for (const double Zs : { ZTop - 40.0, ZBot + 40.0 })
			{
				for (const double Face : { -1.0, 1.0 })
				{
					Deco.AddQuad(FVector(GateX + Face * 3.0, Ya, Zs - Sa - 25.0), FVector(GateX + Face * 3.0, Yb, Zs - Sb - 25.0), FVector(GateX + Face * 3.0, Yb, Zs - Sb + 25.0),
						FVector(GateX + Face * 3.0, Ya, Zs - Sa + 25.0), FVector(Face, 0.0, 0.0), Stripe);
				}
			}
		}
		const FLinearColor Fill = TNBeachRaceKit::Hex(0xFFD23Fu);
		const FLinearColor Ink = TNBeachRaceKit::Hex(0x2A2118u);
		const double ZMid = 0.5 * (ZTop + ZBot) - 60.0;
		TNFinishMesh::TNFinishText(Deco, "^A LA META!", FVector(GateX - 4.0, 0.0, ZMid), FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(-1.0, 0.0, 0.0),
			55.0, 6.0, 4.0, Fill, Ink);
		TNFinishMesh::TNFinishText(Deco, "TORTUNAVY", FVector(GateX + 4.0, 0.0, ZMid), FVector(0.0, -1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(1.0, 0.0, 0.0),
			55.0, 6.0, 4.0, Fill, Ink);
	}

	// Nido de la salida: los cuatro huevos en fila (las bases de los del lobby, medio enterradas) dentro de un anillo de
	// arena removida que se pisa; las tapas son componentes aparte (TN_BeachRaceGenerator_Start.cpp).
	TNBeachScenery::FBuffers Eggs;
	const FLinearColor Mound = TNBeachRaceKit::Hex(0xE2C58Eu);
	for (int32 i = 0; i < TNBeachLayout::NumStartSpots; ++i)
	{
		const FVector Spot = TNBeachLayout::StartSpot(i);
		TNCastleKit::BuildEggCup(Eggs, Spot - FVector(0.0, 0.0, 8.0), TNCastleKit::Col(0xFFF3DC), TNCastleKit::Col(TNCastleKit::EggAccent(i)));
		auto RingAt = [&Spot](double A, double R, double Z) { return FVector(Spot.X + FMath::Cos(A) * R, Spot.Y + FMath::Sin(A) * R, Spot.Z + Z); };
		constexpr int32 Seg = 20;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg;
			const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			const double Bump0 = 6.0 * TNProcMesh::TNProcHashNoise(k, i, 0xE66u);
			const double Bump1 = 6.0 * TNProcMesh::TNProcHashNoise((k + 1) % Seg, i, 0xE66u);
			const FVector Inward(-FMath::Cos(0.5 * (A0 + A1)), -FMath::Sin(0.5 * (A0 + A1)), 0.0);
			// Lomo del anillo (de 1,3 m y 30 cm de alto a 2,8 m, enterrado) y su cara de dentro, hacia el huevo.
			Solid.AddQuad(RingAt(A0, 130.0, 30.0 + Bump0), RingAt(A1, 130.0, 30.0 + Bump1), RingAt(A1, 280.0, -18.0), RingAt(A0, 280.0, -18.0), FVector::UpVector, Mound);
			Solid.AddQuad(RingAt(A0, 130.0, -20.0), RingAt(A1, 130.0, -20.0), RingAt(A1, 130.0, 30.0 + Bump1), RingAt(A0, 130.0, 30.0 + Bump0), Inward, Mound * 0.9f);
		}
	}

	UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial();
	TNBeachRaceKit::Upload(GroveSolidMesh, 0, Solid, Mat, true);
	TNBeachRaceKit::Upload(GroveDecoMesh, 0, Deco, Mat, false);
	// Las bases de los huevos, con el material de los del lobby (el de color de vértice de los cosméticos).
	TNBeachRaceKit::Upload(GroveDecoMesh, 1, Eggs, TNCastleKit::VertexColorMaterial(), false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Meta
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildFinishDecor()
{
	// Arco de neumático de la meta del mapa procedural, cinco veces más grande (125 m de luz y 63 m sobre el agua), en el
	// agua 40 m más allá del filo: TORTUNAVY hacia la playa y TORTUNABO hacia el mar, boyas bajo el arco, banderines hasta
	// dos mástiles en los cabos y banderolas a lo largo de la selva en los últimos 250 m.
	static constexpr double ArchScale = 5.0;
	const double ArchX = TNBeachLayout::Length + 4000.0;
	TNFinishMesh::FTNFinishParams Params;
	Params.Radius = 1250.0;
	Params.MouthHalf = 1400.0;
	Params.FloorZ = (TNBeachLayout::SeabedZ(ArchX, 0.0) - TNBeachLayout::WaterZ) / ArchScale;
	Params.Seed = 0x7A1Du;
	auto Ground = [ArchX](double Lx, double Ly)
	{
		return (TNBeachLayout::SurfaceZ(ArchX + Lx * ArchScale, Ly * ArchScale) - TNBeachLayout::WaterZ) / ArchScale;
	};
	// Los mástiles y banderolas quedan en la ladera de la selva, fuera de los muros (15,5 m fuera de la playa).
	auto HalfWidthAt = [](double) { return (TNBeachLayout::HalfWidth + 1500.0) / ArchScale + 320.0; };
	TNBeachScenery::FBuffers LocalSolid;
	TNBeachScenery::FBuffers LocalDeco;
	TNFinishMesh::TNFinishBuild(LocalSolid, LocalDeco, Params, Ground, HalfWidthAt);
	TNBeachScenery::FBuffers Solid;
	TNBeachScenery::FBuffers Deco;
	const FVector ArchOrigin(ArchX, 0.0, TNBeachLayout::WaterZ);
	TNBeachRaceKit::AppendScaled(Solid, LocalSolid, ArchOrigin, ArchScale);
	TNBeachRaceKit::AppendScaled(Deco, LocalDeco, ArchOrigin, ArchScale);

	// Boyas con banderas a cuadros de 12 m sobre mástiles de 28 m, a 26 m del filo, unidas por un cabo con boyas pequeñas:
	// se ven desde la salida por encima del borde.
	TNBeachScenery::FBuffers Floats;
	const double FlagX = TNBeachLayout::Length + 2600.0;
	FVector PrevBuoy = FVector::ZeroVector;
	for (int32 k = -3; k <= 3; ++k)
	{
		const FVector Buoy(FlagX + 150.0 * TNProcMesh::TNProcHashNoise(k, 1, 0xF1A6u), k * 4000.0, TNBeachLayout::WaterZ + 40.0);
		TNFinishMesh::TNFinishBuoy(Floats, Buoy, 260.0, (k % 2 != 0) ? TNFinishMesh::FinishColors::White : TNFinishMesh::FinishColors::Red);
		TNProcMesh::TNProcAddCylinder(Floats, Buoy, Buoy + FVector(0.0, 0.0, 2800.0), 45.0, 32.0, 6, TNFinishMesh::FinishColors::Pole);
		TNFinishMesh::TNFinishCheckerFlag(Floats, Buoy + FVector(0.0, 0.0, 2780.0), FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, -1.0), 5, 4, 240.0, 120.0,
			0xF1A0u + static_cast<uint32>(k + 3));
		if (k > -3)
		{
			Floats.AddBeam(PrevBuoy + FVector(0.0, 0.0, -10.0), Buoy + FVector(0.0, 0.0, -10.0), 28.0, TNFinishMesh::FinishColors::Rope);
			for (int32 m = 1; m <= 3; ++m)
			{
				TNFinishMesh::TNFinishBuoy(Floats, FMath::Lerp(PrevBuoy, Buoy, m / 4.0) - FVector(0.0, 0.0, 20.0), 90.0,
					m % 2 ? TNFinishMesh::FinishColors::Red : TNFinishMesh::FinishColors::White);
			}
		}
		PrevBuoy = Buoy;
	}

	UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial();
	TNBeachRaceKit::Upload(FinishSolidMesh, 0, Solid, Mat, true);
	TNBeachRaceKit::Upload(FinishDecoMesh, 0, Deco, Mat, false);
	TNBeachRaceKit::Upload(FloatMesh, 0, Floats, Mat, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Selva de los bordes
// ─────────────────────────────────────────────────────────────────────────────

UInstancedStaticMeshComponent* ATN_BeachRaceGenerator::MakeFlora(UStaticMesh* Mesh, bool bShadow, int32 WpoDistance)
{
	if (!Mesh || !BeachRoot) { return nullptr; }
	// Transitorio y fuera de toda duplicación: la copia para jugar se construye sola (PostRegisterAllComponents).
	UHierarchicalInstancedStaticMeshComponent* Comp = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
	Comp->ComponentTags.Add(TNBeachRaceKit::GeneratedTag());
	Comp->SetStaticMesh(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCastShadow(bShadow);
	Comp->bAffectDistanceFieldLighting = false;
	if (WpoDistance > 0)
	{
		Comp->WorldPositionOffsetDisableDistance = WpoDistance;
	}
	else
	{
		Comp->bEvaluateWorldPositionOffset = false;
	}
	Comp->SetupAttachment(BeachRoot);
	Comp->RegisterComponent();
	FloraComps.Add(Comp);
	return Comp;
}

void ATN_BeachRaceGenerator::BuildJungle()
{
	if (JungleDensity <= 0.f) { return; }
	using EShape = TNProcMap::EFloraShape;
	struct FPick
	{
		EShape Shape;
		ETNProcBiome Biome;
		double Weight;
		double ScaleMin;
		double ScaleMax;
		/** Lo que ocupa a ras de suelo (tronco, raíces, aletas; cm a escala 1): ha de quedar fuera de los muros. */
		double Reach;
	};
	// Todo a TNBeach::Scale: las palmeras del mapa procedural (~9 m) salen de 230-300 m; las ceibas, de más de 250 m.
	static const FPick NearBig[] = {
		{ EShape::Palm, ETNProcBiome::Beach, 0.55, 26.0, 34.0, 25.0 },
		{ EShape::Casuarina, ETNProcBiome::Beach, 0.1, 20.0, 26.0, 25.0 },
		{ EShape::BroadTree, ETNProcBiome::Jungle, 0.15, 22.0, 30.0, 45.0 },
		{ EShape::Ceiba, ETNProcBiome::Jungle, 0.05, 11.0, 13.5, 330.0 },
		{ EShape::Pandanus, ETNProcBiome::Beach, 0.1, 22.0, 30.0, 115.0 },
		{ EShape::Rock, ETNProcBiome::Jungle, 0.05, 14.0, 26.0, 145.0 },
	};
	static const FPick FarBig[] = {
		{ EShape::BroadTree, ETNProcBiome::Jungle, 0.42, 22.0, 30.0, 45.0 },
		{ EShape::Palm, ETNProcBiome::Beach, 0.25, 26.0, 34.0, 25.0 },
		{ EShape::Ceiba, ETNProcBiome::Jungle, 0.16, 11.0, 14.0, 330.0 },
		{ EShape::Casuarina, ETNProcBiome::Beach, 0.1, 20.0, 26.0, 25.0 },
		{ EShape::Rock, ETNProcBiome::Jungle, 0.07, 16.0, 30.0, 145.0 },
	};
	static const FPick Under[] = {
		{ EShape::BananaPlant, ETNProcBiome::Jungle, 0.3, 28.0, 40.0, 20.0 },
		{ EShape::FanPalm, ETNProcBiome::Beach, 0.2, 30.0, 45.0, 25.0 },
		{ EShape::TreeFern, ETNProcBiome::Jungle, 0.15, 20.0, 30.0, 25.0 },
		{ EShape::SeaGrape, ETNProcBiome::Beach, 0.15, 22.0, 30.0, 60.0 },
		{ EShape::Bush, ETNProcBiome::Jungle, 0.15, 20.0, 30.0, 90.0 },
		{ EShape::Fern, ETNProcBiome::Jungle, 0.05, 30.0, 40.0, 60.0 },
	};
	// Lo de los huecos entre copas: enredaderas por el suelo y helechos.
	static const FPick GapCreeper = { EShape::Creeper, ETNProcBiome::Jungle, 1.0, 26.0, 34.0, 20.0 };
	static const FPick GapFern = { EShape::Fern, ETNProcBiome::Jungle, 1.0, 30.0, 42.0, 60.0 };
	static const FPick GapTreeFern = { EShape::TreeFern, ETNProcBiome::Jungle, 1.0, 20.0, 30.0, 25.0 };
	auto Pick = [](TNProcMap::FRng& R, const FPick* Table, int32 Num) -> const FPick&
	{
		double Total = 0.0;
		for (int32 i = 0; i < Num; ++i) { Total += Table[i].Weight; }
		double U = R.Unit() * Total;
		for (int32 i = 0; i < Num; ++i)
		{
			U -= Table[i].Weight;
			if (U <= 0.0) { return Table[i]; }
		}
		return Table[Num - 1];
	};
	// Dónde crece: los bancos (desde 18 m fuera de la playa: ningún tronco, que no tiene colisión, asoma dentro de los muros)
	// y la ladera de detrás de la salida (lejos del árbol colosal), sin llegar al filo de los cabos.
	auto InJungle = [](const FVector2D& P)
	{
		if (P.X < -23500.0 || P.X > TNBeachLayout::EdgeX(P.Y) - 2600.0) { return false; }
		if (FMath::Abs(P.Y) - TNBeachLayout::HalfWidth >= 1800.0) { return true; }
		return P.X <= -3800.0 && FVector2D::Distance(P, FVector2D(TNBeachLayout::TrunkX, 0.0)) > 3600.0;
	};
	/**
	 * La selva no proyecta sombra dinámica: las copas de la primera franja se inclinan sobre la arena y, aun con el sol
	 * casi cenital, dejaban casi toda la playa a la sombra (no se veía dónde iba a caer la gaviota ni dónde subirse).
	 * Además, la selva gigante sin Nanite cubre muchísimo mapa de sombras virtuales y desborda su cola de marcado. La
	 * sombra de la playa la dan sus elementos, el relieve y las rocas.
	 */
	auto ShadowZone = [](const FVector2D& /*P*/)
	{
		return false;
	};
	// Troncos (con su copa) y matas del sotobosque, para buscar los huecos.
	struct FTrunk
	{
		FVector2D P;
		double Ground;
		double Height;
		double Crown;
	};
	TArray<FTrunk> Trunks;
	TArray<FVector2D> Bushes;

	TMap<int32, TArray<FTransform>> ByMesh;
	const uint32 Seed = TNBeachLayout::TerrainSeed ^ 0x7EE5u;
	auto Plant = [&ByMesh, &Trunks, &Bushes, &ShadowZone](TNProcMap::FRng& R, const FPick& P, const FVector2D& Pos, double Yaw)
	{
		const double S = FMath::Lerp(P.ScaleMin, P.ScaleMax, R.Unit());
		const int32 Variant = R.RangeInt(0, TNProcMap::FloraVariants - 1);
		// Nada sin colisión (troncos, raíces, peñascos) dentro de los muros: ni a los lados ni por detrás de la salida.
		const double Foot0 = P.Reach * S;
		const double Outside = FMath::Abs(Pos.Y) - TNBeachLayout::HalfWidth;
		if (Outside > -Foot0 && Outside < TNBeachLayout::SideWallY - TNBeachLayout::HalfWidth + 150.0 + Foot0 && Pos.X > TNBeachLayout::BackWallX - 300.0 - Foot0)
		{
			return;
		}
		if (Outside <= -Foot0 && Pos.X > TNBeachLayout::BackWallX - 300.0 - Foot0) { return; }
		// Base en lo más bajo de su pie (no flota por el lado de abajo de la ladera) y un poco hundida.
		const double Foot = (P.Shape == EShape::Rock ? 80.0 : 40.0) * S;
		double Low = TNBeachLayout::GroundZ(Pos.X, Pos.Y);
		for (const FVector2D& Off : { FVector2D(1.0, 0.0), FVector2D(-1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(0.0, -1.0) })
		{
			Low = FMath::Min(Low, TNBeachLayout::GroundZ(Pos.X + Off.X * Foot, Pos.Y + Off.Y * Foot));
		}
		const double Sink = P.Shape == EShape::Rock ? 25.0 * S : 8.0 * S;
		const double Stretch = P.Shape == EShape::Rock ? 1.0 : R.Range(0.9, 1.1);
		const bool bShadow = TNFloraMesh::TNFloraLookOf(P.Shape).bShadow && ShadowZone(Pos);
		const int32 Key = ((static_cast<int32>(P.Shape) * 16 + static_cast<int32>(P.Biome)) * 4 + Variant) * 2 + (bShadow ? 1 : 0);
		ByMesh.FindOrAdd(Key).Add(FTransform(FRotator(0.0, Yaw, 0.0), FVector(Pos, Low - Sink), FVector(S, S, S * Stretch)));
		// Alto y copa aproximados de las mallas del mapa procedural a escala 1 (cm).
		double Height = 0.0;
		double Crown = 0.0;
		switch (P.Shape)
		{
			case EShape::Palm: Height = 900.0; Crown = 380.0; break;
			case EShape::BroadTree: Height = 950.0; Crown = 300.0; break;
			case EShape::Ceiba: Height = 2300.0; Crown = 480.0; break;
			case EShape::Casuarina: Height = 850.0; Crown = 250.0; break;
			case EShape::Pandanus: Height = 450.0; Crown = 250.0; break;
			default: break;
		}
		if (Height > 0.0)
		{
			Trunks.Add({ Pos, Low, Height * S * Stretch, Crown * S });
		}
		else if (P.Shape != EShape::Rock)
		{
			Bushes.Add(Pos);
		}
	};

	// Árboles grandes: rejilla de 48 m con desorden; espesa junto a la playa y más clara lejos.
	{
		constexpr double Cell = 4800.0;
		const double X0 = -24000.0;
		const double Y0 = -(TNBeachLayout::HalfWidth + 46000.0);
		const int32 NXc = FMath::CeilToInt32((TNBeachLayout::Length - X0) / Cell);
		const int32 NYc = FMath::CeilToInt32(-2.0 * Y0 / Cell);
		for (int32 Gy = 0; Gy < NYc; ++Gy)
		{
			for (int32 Gx = 0; Gx < NXc; ++Gx)
			{
				TNProcMap::FRng R(static_cast<uint64>(TNProcMap::HashCell(Seed, Gx, Gy)) * 0x9E3779B1ull + 17ull);
				const double Py = Y0 + (Gy + R.Unit()) * Cell;
				const FVector2D P(X0 + (Gx + R.Unit()) * Cell, Py);
				if (!InJungle(P)) { continue; }
				const double Outside = FMath::Abs(P.Y) - TNBeachLayout::HalfWidth;
				const bool bNear = Outside < 16000.0;
				if (!R.Chance((bNear ? 0.92 : 0.6) * JungleDensity)) { continue; }
				const FPick& Choice = bNear ? Pick(R, NearBig, static_cast<int32>(UE_ARRAY_COUNT(NearBig))) : Pick(R, FarBig, static_cast<int32>(UE_ARRAY_COUNT(FarBig)));
				double Yaw = R.Range(0.0, 360.0);
				if (Choice.Shape == EShape::Palm && Outside > 0.0 && Outside < 9000.0)
				{
					// Las palmeras de la orilla se inclinan hacia la playa y hacia el mar, en diagonal (la malla se inclina hacia
					// su +X): de frente, sus copas cubrían casi toda la arena vista desde arriba.
					const double Turn = R.Range(30.0, 60.0);
					Yaw = P.Y > 0.0 ? -90.0 + Turn : 90.0 - Turn;
				}
				else if (Choice.Shape == EShape::Palm && Outside <= 0.0)
				{
					Yaw = R.Range(-35.0, 35.0);
				}
				Plant(R, Choice, P, Yaw);
			}
		}
	}
	// Primera fila de palmeras a lo largo de las dos orillas, inclinadas sobre la arena hacia el mar (con cocos al pie en el
	// reparto). Algo más bajas que las de detrás y en diagonal: las copas asoman sobre los lados de la playa y dejan el
	// centro a cielo abierto (se ve la gaviota, su sombra y dónde subirse).
	{
		static const FPick FrontPalm = { EShape::Palm, ETNProcBiome::Beach, 1.0, 22.0, 29.0, 25.0 };
		TNProcMap::FRng R(static_cast<uint64>(Seed) * 0x51ull + 3ull);
		for (const double Side : { -1.0, 1.0 })
		{
			for (double X = 1500.0; X < TNBeachLayout::Length - 4000.0; X += R.Range(2800.0, 4200.0))
			{
				if (!R.Chance(0.85 * JungleDensity)) { continue; }
				const FVector2D P(X, Side * (TNBeachLayout::HalfWidth + R.Range(1900.0, 2900.0)));
				const double Turn = R.Range(30.0, 60.0);
				Plant(R, FrontPalm, P, Side > 0.0 ? -90.0 + Turn : 90.0 - Turn);
			}
		}
	}
	// Sotobosque de hojas grandes, solo cerca (lo que se ve desde la playa).
	{
		constexpr double Cell = 2200.0;
		const double X0 = -14000.0;
		const double Y0 = -(TNBeachLayout::HalfWidth + 10000.0);
		const int32 NXc = FMath::CeilToInt32((TNBeachLayout::Length - X0) / Cell);
		const int32 NYc = FMath::CeilToInt32(-2.0 * Y0 / Cell);
		for (int32 Gy = 0; Gy < NYc; ++Gy)
		{
			for (int32 Gx = 0; Gx < NXc; ++Gx)
			{
				TNProcMap::FRng R(static_cast<uint64>(TNProcMap::HashCell(Seed ^ 0x50B0u, Gx, Gy)) * 0x9E3779B1ull + 29ull);
				const double Py = Y0 + (Gy + R.Unit()) * Cell;
				const FVector2D P(X0 + (Gx + R.Unit()) * Cell, Py);
				if (!InJungle(P) || !R.Chance(0.55 * JungleDensity)) { continue; }
				const double Yaw = R.Range(0.0, 360.0);
				const FPick& Choice = Pick(R, Under, static_cast<int32>(UE_ARRAY_COUNT(Under)));
				Plant(R, Choice, P, Yaw);
			}
		}
	}

	// Huecos entre copas junto a la playa y detrás de la salida (lejos de todo tronco y de toda mata): lianas colgadas de
	// un tronco a otro y cortinas que cuelgan de ellas, enredaderas y helechos por el suelo y matas de hojas enormes que
	// tapan. Sin sombra; con viento cerca de la cámara.
	TArray<FTransform> Drapes[2];
	TArray<FTransform> Curtains[2];
	TArray<FTransform> Clumps[2];
	int32 NumGaps = 0;
	{
		constexpr double Cell = 1500.0;
		const double X0 = -14000.0;
		const double Y0 = -(TNBeachLayout::HalfWidth + 12000.0);
		const int32 NXc = FMath::CeilToInt32((TNBeachLayout::Length - 4000.0 - X0) / Cell);
		const int32 NYc = FMath::CeilToInt32(-2.0 * Y0 / Cell);
		for (int32 Gy = 0; Gy < NYc; ++Gy)
		{
			for (int32 Gx = 0; Gx < NXc; ++Gx)
			{
				TNProcMap::FRng R(static_cast<uint64>(TNProcMap::HashCell(Seed ^ 0x6A95u, Gx, Gy)) * 0x9E3779B1ull + 41ull);
				const double Py = Y0 + (Gy + R.Unit()) * Cell;
				const FVector2D P(X0 + (Gx + R.Unit()) * Cell, Py);
				if (!InJungle(P) || !R.Chance(0.85 * JungleDensity)) { continue; }
				// Los dos troncos más cercanos y la mata más cercana.
				int32 First = INDEX_NONE;
				int32 Second = INDEX_NONE;
				double D1 = TNumericLimits<double>::Max();
				double D2 = TNumericLimits<double>::Max();
				for (int32 t = 0; t < Trunks.Num(); ++t)
				{
					const double D = FVector2D::DistSquared(P, Trunks[t].P);
					if (D < D1)
					{
						D2 = D1;
						Second = First;
						D1 = D;
						First = t;
					}
					else if (D < D2)
					{
						D2 = D;
						Second = t;
					}
				}
				double DBush = TNumericLimits<double>::Max();
				for (const FVector2D& B : Bushes) { DBush = FMath::Min(DBush, FVector2D::DistSquared(P, B)); }
				if (First == INDEX_NONE || D1 < FMath::Square(2200.0) || DBush < FMath::Square(1500.0)) { continue; }
				++NumGaps;
				const double Ground = TNBeachLayout::GroundZ(P.X, P.Y);
				// Suelo: enredadera, helecho y, a menudo, una mata de hojas enormes.
				if (R.Chance(0.6))
				{
					const double Cx = R.Range(-300.0, 300.0);
					const double Cy = R.Range(-300.0, 300.0);
					const double CreeperYaw = R.Range(0.0, 360.0);
					Plant(R, GapCreeper, P + FVector2D(Cx, Cy), CreeperYaw);
				}
				if (R.Chance(0.5))
				{
					const FPick& Fern = R.Chance(0.5) ? GapFern : GapTreeFern;
					const double Ang = R.Range(0.0, TNProcMap::TwoPi);
					Plant(R, Fern, P + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * 700.0, R.Range(0.0, 360.0));
				}
				if (R.Chance(0.65))
				{
					const double Ang = R.Range(0.0, TNProcMap::TwoPi);
					const FVector2D Q = P + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * R.Range(200.0, 800.0);
					const double S = R.Range(0.9, 1.5);
					const double ClumpYaw = R.Range(0.0, 360.0);
					const int32 ClumpVariant = R.RangeInt(0, 1);
					Clumps[ClumpVariant].Add(FTransform(FRotator(0.0, ClumpYaw, 0.0), FVector(Q, TNBeachLayout::GroundZ(Q.X, Q.Y) - 30.0), FVector(S)));
				}
				// Lianas: de un tronco al otro, por encima del hueco, y cortinas colgando de ellas.
				const FTrunk& A = Trunks[First];
				const bool bPair = Second != INDEX_NONE && D2 < FMath::Square(9000.0);
				const double Span = bPair ? FVector2D::Distance(A.P, Trunks[Second].P) : 0.0;
				if (bPair && Span > 3500.0 && Span < 11000.0)
				{
					const FTrunk& B = Trunks[Second];
					const double HA = A.Ground + 0.62 * A.Height;
					const double HB = B.Ground + 0.62 * B.Height;
					const double Scale = Span / 10000.0;
					const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.P.Y - A.P.Y, B.P.X - A.P.X));
					const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(HB - HA, Span));
					Drapes[R.RangeInt(0, 1)].Add(FTransform(FRotator(Pitch, Yaw, 0.0), FVector(A.P, HA), FVector(Scale)));
					const int32 Hanging = R.RangeInt(1, 3);
					for (int32 h = 0; h < Hanging; ++h)
					{
						const double T = R.Range(0.25, 0.75);
						const FVector2D Q = A.P + (B.P - A.P) * T;
						const double Top = FMath::Lerp(HA, HB, T) - 2200.0 * Scale * 4.0 * T * (1.0 - T) - 50.0;
						const double Len = Top - TNBeachLayout::GroundZ(Q.X, Q.Y) - R.Range(1200.0, 3500.0);
						if (Len < 3000.0) { continue; }
						const double CurtainScale = FMath::Clamp(Len / 12000.0, 0.4, 1.8);
						const double CurtainYaw = R.Range(0.0, 360.0);
						const int32 CurtainVariant = R.RangeInt(0, 1);
						Curtains[CurtainVariant].Add(FTransform(FRotator(0.0, CurtainYaw, 0.0), FVector(Q, Top), FVector(CurtainScale)));
					}
				}
				else
				{
					// Sin pareja: cuelgan del borde de la copa más cercana, del lado del hueco.
					const FVector2D Dir = (P - A.P).GetSafeNormal();
					const FVector2D Q = A.P + Dir * (0.8 * A.Crown);
					const double Top = A.Ground + 0.75 * A.Height;
					const double Len = Top - Ground - R.Range(1500.0, 4000.0);
					if (Len > 3000.0)
					{
						const double CurtainYaw = R.Range(0.0, 360.0);
						const int32 CurtainVariant = R.RangeInt(0, 1);
						Curtains[CurtainVariant].Add(FTransform(FRotator(0.0, CurtainYaw, 0.0), FVector(Q, Top), FVector(FMath::Clamp(Len / 12000.0, 0.4, 1.8))));
					}
				}
			}
		}
	}

	// Una malla por especie, bioma y variante (las del mapa procedural), instanciada; con sombra o sin ella, en dos componentes.
	UMaterialInterface* Mat = TNBeachRaceKit::FoliageMaterial();
	TMap<int32, UStaticMesh*> MeshBySpecies;
	int32 Shadowed = 0;
	int32 Unshadowed = 0;
	for (TPair<int32, TArray<FTransform>>& Entry : ByMesh)
	{
		const bool bShadow = (Entry.Key & 1) != 0;
		const int32 Species = Entry.Key >> 1;
		const int32 Variant = Species % 4;
		const ETNProcBiome Biome = static_cast<ETNProcBiome>((Species / 4) % 16);
		const EShape Shape = static_cast<EShape>(Species / 4 / 16);
		const TNFloraMesh::FTNFloraWind Wind = TNFloraMesh::TNFloraWindOf(Shape);
		UStaticMesh* Mesh = MeshBySpecies.FindRef(Species);
		if (!Mesh)
		{
			FLinearColor GroundC, PathC, RockC, BedC;
			TN_DefaultBiomeColors(Biome, GroundC, PathC, RockC, BedC);
			TNProcMesh::FTNProcMeshBuffers Buffers;
			TNFloraMesh::TNFloraBuild(Buffers, Shape, TNFloraMesh::TNFloraPaletteFor(Biome, GroundC, RockC), Variant, TNProcMap::HashCell(Seed, Species, Variant));
			Mesh = TNProcRuntimeMesh::MakeStaticMesh(this, Buffers, Mat, false, Wind.Stiffness, Wind.Exponent);
			if (!Mesh) { continue; }
			FloraMeshes.Add(Mesh);
			MeshBySpecies.Add(Species, Mesh);
		}
		// Viento solo cerca de la cámara (a esta escala casi no se ve y apagado ahorra sombras).
		if (UInstancedStaticMeshComponent* Comp = MakeFlora(Mesh, bShadow, Wind.Stiffness > 0.f ? 20000 : 0))
		{
			Comp->AddInstances(Entry.Value, false, false);
			(bShadow ? Shadowed : Unshadowed) += Entry.Value.Num();
		}
	}

	// Lianas y matas de los huecos: dos variantes de cada una, sin sombra, con viento cerca y hasta 350 m de la cámara.
	int32 GapInstances = 0;
	auto AddGapMeshes = [this, Mat, &GapInstances](TArray<FTransform>* Instances, void (*Build)(TNBeachScenery::FBuffers&, uint32), uint32 BaseSeed)
	{
		for (int32 v = 0; v < 2; ++v)
		{
			if (Instances[v].Num() == 0) { continue; }
			TNBeachScenery::FBuffers Buffers;
			Build(Buffers, BaseSeed + static_cast<uint32>(v) * 101u);
			UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(this, Buffers, Mat, false, 0.f, 1.5f, -2.f);
			if (!Mesh) { continue; }
			FloraMeshes.Add(Mesh);
			if (UInstancedStaticMeshComponent* Comp = MakeFlora(Mesh, false, 20000))
			{
				Comp->SetCullDistances(30000, 35000);
				Comp->AddInstances(Instances[v], false, false);
				GapInstances += Instances[v].Num();
			}
		}
	};
	AddGapMeshes(Drapes, &TNBeachScenery::BuildLianaDrape, 0x11A0u);
	AddGapMeshes(Curtains, &TNBeachScenery::BuildLianaCurtain, 0x22B0u);
	AddGapMeshes(Clumps, &TNBeachScenery::BuildLeafClump, 0x33C0u);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] selva: %d árboles y plantas con sombra y %d sin ella · %d huecos entre copas con %d lianas y matas."),
		Shadowed, Unshadowed, NumGaps, GapInstances);
}

// ─────────────────────────────────────────────────────────────────────────────
// Huellas del reparto (para revisarlo)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildFootprints()
{
	if (!FootprintMesh) { return; }
	FootprintMesh->ClearAllMeshSections();
	// En juego solo se dibujan si se piden (TN.Beach.ShowFootprints 1; el Tick las dibuja si se piden después).
	if (Layout.Items.Num() == 0 || !ShouldShowFootprints()) { return; }
	TNBeachScenery::FBuffers M;
	for (const TNBeachLayout::FItem& Item : Layout.Items)
	{
		// Amarillo decorado, naranja trampas, rojo enemigos, morado quads, celeste gaviotas, marrón pasarelas guía, rosa
		// los castillos (con salas y sus alas, y los enormes), verde azulado las filas, oliva lo militar, blanco azulado los
		// lanzadores de su pasada, marrón oscuro los rincones y gris los tapones de las líneas rectas.
		FLinearColor Color(1.f, 0.8f, 0.1f, 0.f);
		switch (TNBeach::CategoryOf(Item.Element))
		{
			case ETNBeachCategory::Trap: Color = FLinearColor(1.f, 0.4f, 0.05f, 0.f); break;
			case ETNBeachCategory::Enemy: Color = FLinearColor(0.9f, 0.08f, 0.08f, 0.f); break;
			default: break;
		}
		switch (Item.Role)
		{
			case TNBeachLayout::EItemRole::QuadLane: Color = FLinearColor(0.55f, 0.15f, 0.9f, 0.f); break;
			case TNBeachLayout::EItemRole::GullZone: Color = FLinearColor(0.1f, 0.75f, 1.f, 0.f); break;
			case TNBeachLayout::EItemRole::GuidePath: Color = FLinearColor(0.55f, 0.35f, 0.15f, 0.f); break;
			case TNBeachLayout::EItemRole::Dungeon:
			case TNBeachLayout::EItemRole::DungeonWing:
			case TNBeachLayout::EItemRole::Castle: Color = FLinearColor(1.f, 0.2f, 0.7f, 0.f); break;
			case TNBeachLayout::EItemRole::Row: Color = FLinearColor(0.1f, 0.8f, 0.6f, 0.f); break;
			case TNBeachLayout::EItemRole::Military: Color = FLinearColor(0.45f, 0.55f, 0.15f, 0.f); break;
			case TNBeachLayout::EItemRole::Launcher: Color = FLinearColor(0.7f, 1.f, 1.f, 0.f); break;
			case TNBeachLayout::EItemRole::Nook: Color = FLinearColor(0.35f, 0.2f, 0.1f, 0.f); break;
			case TNBeachLayout::EItemRole::Plug: Color = FLinearColor(0.75f, 0.75f, 0.75f, 0.f); break;
			// Ronda 3: fortalezas en rosa oscuro, cofres en dorado y los enemigos de sitio fijo (pulpos, ermitaños, pulgas,
			// tanques y guardias) en granate.
			case TNBeachLayout::EItemRole::Fortress: Color = FLinearColor(0.7f, 0.05f, 0.45f, 0.f); break;
			case TNBeachLayout::EItemRole::Chest: Color = FLinearColor(1.f, 0.85f, 0.f, 0.f); break;
			case TNBeachLayout::EItemRole::Lair: Color = FLinearColor(0.5f, 0.f, 0.1f, 0.f); break;
			default: break;
		}
		// Contorno de la cápsula: dos medias vueltas unidas por sus lados rectos.
		const FVector2D Ax = Item.Axis();
		const FVector2D Sd(-Ax.Y, Ax.X);
		TArray<FVector2D> Ring;
		constexpr int32 Half = 18;
		for (int32 e = 0; e < 2; ++e)
		{
			const FVector2D C = e == 0 ? Item.EndB() : Item.EndA();
			const double Base = e == 0 ? -HALF_PI : HALF_PI;
			for (int32 k = 0; k <= Half; ++k)
			{
				const double A = Base + PI * k / Half;
				Ring.Add(C + (Ax * FMath::Cos(A) + Sd * FMath::Sin(A)) * Item.Radius);
			}
		}
		const int32 N = Ring.Num();
		// Sobre los asientos de la ronda (solo los de alrededor).
		TArray<TNBeachLayout::FStamp> Near;
		const double Reach = Item.HalfLength + Item.Radius + 100.0;
		for (const TNBeachLayout::FStamp& Stamp : Layout.Stamps)
		{
			if (FVector2D::Distance((Stamp.A + Stamp.B) * 0.5, Item.Pos) < Reach + FVector2D::Distance(Stamp.A, Stamp.B) * 0.5 + Stamp.Radius + Stamp.Blend)
			{
				Near.Add(Stamp);
			}
		}
		auto Lift = [&Near](const FVector2D& P)
		{
			return FVector(P, TNBeachLayout::StampedZ(Near, P.X, P.Y, TNBeachLayout::GroundZ(P.X, P.Y)) + 40.0);
		};
		for (int32 k = 0; k < N; ++k)
		{
			const FVector2D& P0 = Ring[k];
			const FVector2D& P1 = Ring[(k + 1) % N];
			const FVector2D Out0 = (P0 - Item.Pos).GetSafeNormal() * 40.0;
			const FVector2D Out1 = (P1 - Item.Pos).GetSafeNormal() * 40.0;
			M.AddQuad(Lift(P0 - Out0), Lift(P1 - Out1), Lift(P1 + Out1), Lift(P0 + Out0), FVector::UpVector, Color);
		}
		// Flecha hacia su eje X local (por dónde se entra y se sale, o el largo de los alargados).
		const double Arrow = FMath::Clamp(Item.Radius * 0.6, 150.0, 900.0);
		M.AddTri(Lift(Item.Pos + Ax * Arrow), Lift(Item.Pos - Ax * (Arrow * 0.3) + Sd * (Arrow * 0.4)), Lift(Item.Pos - Ax * (Arrow * 0.3) - Sd * (Arrow * 0.4)),
			FVector::UpVector, Color);
	}
	// Puntos interesantes (para el botín): arcos de salto en blanco y atajos en fucsia (tiras de Pos a To); rincones,
	// cimas, trincheras y caminos alternativos, rombos (marrón, amarillo, oliva y azul).
	auto LiftAll = [this](const FVector2D& P)
	{
		return FVector(P, TNBeachLayout::StampedZ(Layout.Stamps, P.X, P.Y, TNBeachLayout::GroundZ(P.X, P.Y)) + 60.0);
	};
	for (const TNBeachLayout::FInterestPoint& Point : Layout.Interest)
	{
		FLinearColor Color(1.f, 1.f, 1.f, 0.f);
		switch (Point.Kind)
		{
			case TNBeachLayout::EInterestKind::Shortcut: Color = FLinearColor(1.f, 0.1f, 0.9f, 0.f); break;
			case TNBeachLayout::EInterestKind::Nook: Color = FLinearColor(0.35f, 0.2f, 0.1f, 0.f); break;
			case TNBeachLayout::EInterestKind::Summit: Color = FLinearColor(1.f, 0.95f, 0.2f, 0.f); break;
			case TNBeachLayout::EInterestKind::Trench: Color = FLinearColor(0.45f, 0.55f, 0.15f, 0.f); break;
			case TNBeachLayout::EInterestKind::Detour: Color = FLinearColor(0.2f, 0.45f, 1.f, 0.f); break;
			default: break;
		}
		const FVector2D Span = Point.To - Point.Pos;
		if (Span.SizeSquared() > 1.0)
		{
			const FVector2D Side = FVector2D(-Span.Y, Span.X).GetSafeNormal() * 60.0;
			constexpr int32 Pieces = 6;
			for (int32 k = 0; k < Pieces; ++k)
			{
				const FVector2D P0 = Point.Pos + Span * (static_cast<double>(k) / Pieces);
				const FVector2D P1 = Point.Pos + Span * (static_cast<double>(k + 1) / Pieces);
				M.AddQuad(LiftAll(P0 - Side), LiftAll(P1 - Side), LiftAll(P1 + Side), LiftAll(P0 + Side), FVector::UpVector, Color);
			}
		}
		else
		{
			const double R = 350.0;
			M.AddQuad(LiftAll(Point.Pos + FVector2D(R, 0.0)), LiftAll(Point.Pos + FVector2D(0.0, R)), LiftAll(Point.Pos - FVector2D(R, 0.0)),
				LiftAll(Point.Pos - FVector2D(0.0, R)), FVector::UpVector, Color);
		}
	}
	TNBeachRaceKit::Upload(FootprintMesh, 0, M, TNCastleKit::VertexColorMaterial(), false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Chapuzón de la meta (local, en cada máquina)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::StartLiving()
{
	UWorld* World = GetWorld();
	if (bLiving || !World || !World->IsGameWorld() || GetNetMode() == NM_DedicatedServer) { return; }
	bLiving = true;
	const FVector Origin = GetActorTransform().TransformPosition(FVector(TNBeachLayout::Length + 1000.0, 0.0, TNBeachLayout::WaterZ));
	{
		// Gotas que saltan en corona.
		TNAmbientFX::FEmitterDesc Drops;
		Drops.Shape = TNAmbientFX::EShape::Drop;
		Drops.Color = FLinearColor(0.85f, 0.95f, 1.f);
		Drops.MaxParticles = 60;
		Drops.Rate = 0.f;
		Drops.SpawnRadius = 80.f;
		Drops.Speed = 850.f;
		Drops.SpeedJitter = 0.4f;
		Drops.Spread = 0.75f;
		Drops.Gravity = -980.f;
		Drops.Drag = 0.1f;
		Drops.LifeMin = 0.7f;
		Drops.LifeMax = 1.3f;
		Drops.SizeStart = 45.f;
		Drops.SizeEnd = 25.f;
		Drops.WakeDistance = 60000.f;
		SplashDropsFX = TNAmbientFX::AddEmitter(this, Drops, Origin);
	}
	{
		// Espuma blanca que se abre sobre el agua.
		TNAmbientFX::FEmitterDesc Foam;
		Foam.Shape = TNAmbientFX::EShape::Puff;
		Foam.bSoft = true;
		Foam.bCloud = true;
		Foam.Color = FLinearColor(0.95f, 0.98f, 1.f);
		Foam.Alpha = 0.7f;
		Foam.MaxParticles = 24;
		Foam.Rate = 0.f;
		Foam.SpawnRadius = 120.f;
		Foam.Speed = 260.f;
		Foam.Spread = 1.f;
		Foam.Gravity = 0.f;
		Foam.Buoyancy = 20.f;
		Foam.Drag = 1.2f;
		Foam.LifeMin = 1.f;
		Foam.LifeMax = 1.8f;
		Foam.SizeStart = 160.f;
		Foam.SizeEnd = 420.f;
		Foam.WakeDistance = 60000.f;
		SplashFoamFX = TNAmbientFX::AddEmitter(this, Foam, Origin);
	}
	{
		// Ondas que se abren en el agua.
		TNAmbientFX::FEmitterDesc Rings;
		Rings.Shape = TNAmbientFX::EShape::Ring;
		Rings.bSoft = true;
		Rings.Color = FLinearColor(0.95f, 0.98f, 1.f);
		Rings.Alpha = 0.6f;
		Rings.MaxParticles = 6;
		Rings.Rate = 0.f;
		Rings.SpawnRadius = 0.f;
		Rings.Speed = 0.f;
		Rings.SpeedJitter = 0.f;
		Rings.Spread = 0.f;
		Rings.Gravity = 0.f;
		Rings.Drag = 0.f;
		Rings.LifeMin = 1.6f;
		Rings.LifeMax = 2.2f;
		Rings.SizeStart = 150.f;
		Rings.SizeEnd = 1400.f;
		Rings.WakeDistance = 60000.f;
		SplashRingFX = TNAmbientFX::AddEmitter(this, Rings, Origin);
	}
}

void ATN_BeachRaceGenerator::StopLiving()
{
	TNAmbientFX::RemoveOwner(this);
	SplashDropsFX = INDEX_NONE;
	SplashFoamFX = INDEX_NONE;
	SplashRingFX = INDEX_NONE;
	bLiving = false;
}

void ATN_BeachRaceGenerator::Splash(const FVector& WorldLocation)
{
	if (TNAmbientFX::FEmitter* Drops = TNAmbientFX::GetEmitter(this, SplashDropsFX))
	{
		Drops->Origin = WorldLocation;
		TNAmbientFX::Burst(*Drops, 28);
	}
	if (TNAmbientFX::FEmitter* Foam = TNAmbientFX::GetEmitter(this, SplashFoamFX))
	{
		Foam->Origin = WorldLocation + FVector(0.0, 0.0, 40.0);
		TNAmbientFX::Burst(*Foam, 10);
	}
	if (TNAmbientFX::FEmitter* Rings = TNAmbientFX::GetEmitter(this, SplashRingFX))
	{
		Rings->Origin = WorldLocation + FVector(0.0, 0.0, 6.0);
		TNAmbientFX::Burst(*Rings, 2);
	}
}
