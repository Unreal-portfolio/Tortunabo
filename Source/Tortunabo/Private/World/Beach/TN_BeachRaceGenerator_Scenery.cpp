// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachRaceGenerator — escenografía de la playa: la salida en el linde de la selva
// (árbol colosal con raíces tabulares, hojas enormes y el cartel «¡A LA META!»), la
// meta (arco de neumático del mapa procedural a escala, banderolas y boyas con
// banderas a cuadros que flotan), la selva de los bordes (vegetación instanciada del
// mapa procedural a 200-300 m), las huellas del reparto y el chapuzón de la meta.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/Beach/TN_BeachRaceGenerator.h"
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

	UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial();
	TNBeachRaceKit::Upload(GroveSolidMesh, 0, Solid, Mat, true);
	TNBeachRaceKit::Upload(GroveDecoMesh, 0, Deco, Mat, false);
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

	TMap<int32, TArray<FTransform>> ByMesh;
	const uint32 Seed = TNBeachLayout::TerrainSeed ^ 0x7EE5u;
	auto Plant = [&ByMesh](TNProcMap::FRng& R, const FPick& P, const FVector2D& Pos, double Yaw)
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
		const int32 Key = (static_cast<int32>(P.Shape) * 16 + static_cast<int32>(P.Biome)) * 4 + Variant;
		ByMesh.FindOrAdd(Key).Add(FTransform(FRotator(0.0, Yaw, 0.0), FVector(Pos, Low - Sink), FVector(S, S, S * Stretch)));
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
				const FVector2D P(X0 + (Gx + R.Unit()) * Cell, Y0 + (Gy + R.Unit()) * Cell);
				if (!InJungle(P)) { continue; }
				const double Outside = FMath::Abs(P.Y) - TNBeachLayout::HalfWidth;
				const bool bNear = Outside < 16000.0;
				if (!R.Chance((bNear ? 0.92 : 0.6) * JungleDensity)) { continue; }
				const FPick& Choice = bNear ? Pick(R, NearBig, static_cast<int32>(UE_ARRAY_COUNT(NearBig))) : Pick(R, FarBig, static_cast<int32>(UE_ARRAY_COUNT(FarBig)));
				double Yaw = R.Range(0.0, 360.0);
				if (Choice.Shape == EShape::Palm && Outside > 0.0 && Outside < 9000.0)
				{
					// Las palmeras de la orilla se inclinan hacia la playa (la malla se inclina hacia su +X).
					Yaw = (P.Y > 0.0 ? -90.0 : 90.0) + R.Range(-35.0, 35.0);
				}
				else if (Choice.Shape == EShape::Palm && Outside <= 0.0)
				{
					Yaw = R.Range(-35.0, 35.0);
				}
				Plant(R, Choice, P, Yaw);
			}
		}
	}
	// Primera fila de palmeras a lo largo de las dos orillas, inclinadas sobre la arena (con cocos al pie en el reparto).
	{
		TNProcMap::FRng R(static_cast<uint64>(Seed) * 0x51ull + 3ull);
		for (const double Side : { -1.0, 1.0 })
		{
			for (double X = 1500.0; X < TNBeachLayout::Length - 4000.0; X += R.Range(2800.0, 4200.0))
			{
				if (!R.Chance(0.85 * JungleDensity)) { continue; }
				const FVector2D P(X, Side * (TNBeachLayout::HalfWidth + R.Range(1900.0, 2900.0)));
				Plant(R, NearBig[0], P, (Side > 0.0 ? -90.0 : 90.0) + R.Range(-25.0, 25.0));
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
				const FVector2D P(X0 + (Gx + R.Unit()) * Cell, Y0 + (Gy + R.Unit()) * Cell);
				if (!InJungle(P) || !R.Chance(0.55 * JungleDensity)) { continue; }
				Plant(R, Pick(R, Under, static_cast<int32>(UE_ARRAY_COUNT(Under))), P, R.Range(0.0, 360.0));
			}
		}
	}

	// Una malla por especie, bioma y variante (las del mapa procedural), instanciada.
	UMaterialInterface* Mat = TNBeachRaceKit::FoliageMaterial();
	for (TPair<int32, TArray<FTransform>>& Entry : ByMesh)
	{
		const int32 Variant = Entry.Key % 4;
		const ETNProcBiome Biome = static_cast<ETNProcBiome>((Entry.Key / 4) % 16);
		const EShape Shape = static_cast<EShape>(Entry.Key / 4 / 16);
		FLinearColor GroundC, PathC, RockC, BedC;
		TN_DefaultBiomeColors(Biome, GroundC, PathC, RockC, BedC);
		TNProcMesh::FTNProcMeshBuffers Buffers;
		TNFloraMesh::TNFloraBuild(Buffers, Shape, TNFloraMesh::TNFloraPaletteFor(Biome, GroundC, RockC), Variant, TNProcMap::HashCell(Seed, Entry.Key, Variant));
		const TNFloraMesh::FTNFloraWind Wind = TNFloraMesh::TNFloraWindOf(Shape);
		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(this, Buffers, Mat, false, Wind.Stiffness, Wind.Exponent);
		if (!Mesh) { continue; }
		FloraMeshes.Add(Mesh);
		// Viento solo cerca de la cámara (a esta escala casi no se ve y apagado ahorra sombras).
		if (UInstancedStaticMeshComponent* Comp = MakeFlora(Mesh, TNFloraMesh::TNFloraLookOf(Shape).bShadow, Wind.Stiffness > 0.f ? 20000 : 0))
		{
			Comp->AddInstances(Entry.Value, false, false);
		}
	}
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
		// Amarillo decorado, naranja trampas, rojo enemigos, morado quads, celeste gaviotas, marrón pasarelas guía y
		// rosa el castillo y sus alas.
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
			case TNBeachLayout::EItemRole::DungeonWing: Color = FLinearColor(1.f, 0.2f, 0.7f, 0.f); break;
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
