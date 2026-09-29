// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachRaceGenerator — relieve fijo con malla propia: las dos líneas de trincheras
// (caballones de arena con tablones por dentro, sacos terreros en el lado del mar,
// tarimas en el fondo y puentes de tablones), las cornisas de arena de las crestas que
// se saltan, las rocas de las pozas de marea y la superficie del agua de las pozas.
// Dónde va cada cosa lo dice TN_BeachLayout.h (lógica pura); aquí solo las mallas.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/Beach/TN_BeachRaceGenerator.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "TN_BeachRaceKit.h"

namespace TNBeachFeatures
{
	using FBuffers = TNProcMesh::FTNProcMeshBuffers;

	/** Estación del eje de una trinchera: punto, desplazamiento lateral por cm (en inglete en las esquinas) y dirección. */
	struct FStation
	{
		FVector2D P = FVector2D::ZeroVector;
		/** Hacia la izquierda de la marcha, ya escalado por la mitra: P + Off * d cae en la paralela a distancia d. */
		FVector2D Off = FVector2D(0.0, 1.0);
		FVector2D Along = FVector2D(1.0, 0.0);
		/** Distancia recorrida desde el primer punto (cm). */
		double S = 0.0;
	};

	/** Estaciones cada ~Step a lo largo de la polilínea; las paralelas quedan rectas en cada tramo y unidas en inglete. */
	TArray<FStation> MakeStations(const TArray<FVector2D>& Points, double Step)
	{
		TArray<FStation> Result;
		const int32 N = Points.Num();
		if (N < 2) { return Result; }
		TArray<FVector2D> VertexOff;
		VertexOff.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D DPrev = (i > 0 ? Points[i] - Points[i - 1] : Points[1] - Points[0]).GetSafeNormal();
			const FVector2D DNext = (i + 1 < N ? Points[i + 1] - Points[i] : Points[i] - Points[i - 1]).GetSafeNormal();
			const FVector2D LPrev(-DPrev.Y, DPrev.X);
			const FVector2D LNext(-DNext.Y, DNext.X);
			FVector2D L = (LPrev + LNext).GetSafeNormal();
			if (L.IsNearlyZero()) { L = LNext; }
			const double CosHalf = FMath::Max(0.5, FVector2D::DotProduct(L, LNext));
			VertexOff[i] = L / CosHalf;
		}
		double Walked = 0.0;
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const FVector2D A = Points[i];
			const FVector2D B = Points[i + 1];
			const double Len = FVector2D::Distance(A, B);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Len / Step));
			const FVector2D Along = (B - A).GetSafeNormal();
			for (int32 s = 0; s < Steps; ++s)
			{
				const double T = static_cast<double>(s) / Steps;
				FStation Station;
				Station.P = A + (B - A) * T;
				Station.Off = VertexOff[i] + (VertexOff[i + 1] - VertexOff[i]) * T;
				Station.Along = Along;
				Station.S = Walked + Len * T;
				Result.Add(Station);
			}
			Walked += Len;
		}
		FStation Last;
		Last.P = Points.Last();
		Last.Off = VertexOff.Last();
		Last.Along = (Points.Last() - Points[N - 2]).GetSafeNormal();
		Last.S = Walked;
		Result.Add(Last);
		return Result;
	}

	FLinearColor SandTone(double Noise)
	{
		return FLinearColor(0.86f, 0.76f, 0.52f) * static_cast<float>(0.9 + 0.1 * Noise);
	}

	FLinearColor WoodTone(int32 Index)
	{
		static const uint32 Tones[] = { 0x8A6A45u, 0x7A5C3Bu, 0x967552u, 0x6E5236u };
		return TNBeachRaceKit::Hex(Tones[((Index % 4) + 4) % 4]);
	}

	FLinearColor BagTone(int32 Index)
	{
		static const uint32 Tones[] = { 0xA8956Au, 0x9B8A60u, 0xB3A174u, 0x8F7F58u, 0x7F7A4Eu };
		return TNBeachRaceKit::Hex(Tones[((Index % 5) + 5) % 5]);
	}

	/** Saco terrero: bloque algo redondeado (dos cajas) a lo largo de Along. */
	void AddSandbag(FBuffers& M, const FVector& Center, const FVector2D& Along, int32 Index)
	{
		const FVector Axis(Along, 0.0);
		const FLinearColor Tone = BagTone(Index);
		M.AddBox(Center, Axis, FVector(29.0, 16.0, 9.0), Tone);
		M.AddBox(Center + FVector(0.0, 0.0, 5.0), Axis, FVector(25.0, 13.0, 7.0), Tone * 1.06f);
	}

	/**
	 * Una línea de trinchera: dos caballones (cara de dentro de tablones y vertical, lomo de arena, falda hacia fuera), los
	 * sacos del lado del mar en tramos con huecos, dos puentes de tablones de lado a lado, postes y tarima (sin colisión).
	 */
	void BuildTrench(FBuffers& Solid, FBuffers& Deco, const TNBeachLayout::FTrench& Trench, uint32 Seed)
	{
		using namespace TNBeachLayout;
		const TArray<FStation> Stations = MakeStations(Trench.Points, 150.0);
		const int32 NS = Stations.Num();
		if (NS < 2) { return; }
		const double TotalLen = Stations.Last().S;
		// Puentes a un tercio y a dos tercios.
		const double Bridges[2] = { TotalLen * (0.3 + 0.08 * TNBeachRaceKit::Hash01(1, 2, Seed)), TotalLen * (0.66 + 0.08 * TNBeachRaceKit::Hash01(3, 4, Seed)) };

		for (const double Side : { 1.0, -1.0 })
		{
			// Cotas de cada estación en este lado.
			TArray<FVector> InBottom, InTop, OutTop, Foot;
			InBottom.SetNum(NS);
			InTop.SetNum(NS);
			OutTop.SetNum(NS);
			Foot.SetNum(NS);
			bool bSea = false;
			for (int32 k = 0; k < NS; ++k)
			{
				const FStation& St = Stations[k];
				const FVector2D Dir = St.Off * Side;
				bSea |= Dir.X > 0.3;
				const double Floor = SandZ(St.P.X, St.P.Y);
				const FVector2D Mid = St.P + Dir * (0.5 * (TrenchHalfChannel + TrenchBermTop));
				const double Top = NaturalZ(Mid.X, Mid.Y) + TrenchBermHeight;
				const FVector2D In = St.P + Dir * TrenchHalfChannel;
				const FVector2D Out = St.P + Dir * TrenchBermTop;
				const FVector2D FootP = St.P + Dir * TrenchBermFoot;
				InBottom[k] = FVector(In, FMath::Min(Floor, SandZ(In.X, In.Y)) - 30.0);
				InTop[k] = FVector(In, Top);
				OutTop[k] = FVector(Out, Top);
				Foot[k] = FVector(FootP, SandZ(FootP.X, FootP.Y) - 15.0);
			}
			for (int32 k = 0; k + 1 < NS; ++k)
			{
				const FVector2D Dir = (Stations[k].Off * Side).GetSafeNormal();
				const FVector Inward(-Dir, 0.0);
				const double Noise = 0.5 + 0.5 * TNProcMesh::TNProcHashNoise(k, Side > 0.0 ? 1 : 2, Seed);
				// Cara de dentro: tablones (el tono cambia de tablón en tablón).
				Solid.AddQuad(InBottom[k], InBottom[k + 1], InTop[k + 1], InTop[k], Inward, WoodTone(k + (Side > 0.0 ? 0 : 2)));
				Solid.AddQuad(InTop[k], InTop[k + 1], OutTop[k + 1], OutTop[k], FVector::UpVector, SandTone(Noise) * 1.03f);
				Solid.AddQuad(OutTop[k], OutTop[k + 1], Foot[k + 1], Foot[k], FVector(Dir, 0.6), SandTone(Noise));
			}
			// Remates de las puntas.
			for (int32 End = 0; End < 2; ++End)
			{
				const int32 k = End == 0 ? 0 : NS - 1;
				const FVector Out(End == 0 ? -Stations[k].Along : Stations[k].Along, 0.0);
				Solid.AddTri(InBottom[k], InTop[k], OutTop[k], Out, SandTone(0.5));
				Solid.AddTri(InBottom[k], OutTop[k], Foot[k], Out, SandTone(0.5));
			}
			// Postes de los tablones (sin colisión) cada 4,5 m.
			for (int32 k = 0; k < NS; k += 3)
			{
				const FVector2D Dir = (Stations[k].Off * Side).GetSafeNormal();
				const FVector Base = FVector(FVector2D(InTop[k]) - Dir * 7.0, InBottom[k].Z + 10.0);
				const double H = InTop[k].Z + 8.0 - Base.Z;
				Deco.AddBox(Base + FVector(0.0, 0.0, 0.5 * H), FVector(Stations[k].Along, 0.0), FVector(7.0, 7.0, 0.5 * H), TNBeachRaceKit::Hex(0x4A3826u));
			}
			// Sacos terreros en el lomo del lado del mar: tramos de 7 con un hueco de 1,6 m (por ahí se sale de un salto), dos
			// capas; nada encima de los puentes.
			if (bSea)
			{
				int32 Bag = 0;
				int32 Station = 0;
				for (double S = 60.0; S < TotalLen - 60.0; )
				{
					const bool bBridge = FMath::Abs(S - Bridges[0]) < 220.0 || FMath::Abs(S - Bridges[1]) < 220.0;
					const int32 InGroup = Bag % 8;
					if (InGroup == 7 || bBridge)
					{
						S += bBridge ? 120.0 : 160.0;
						++Bag;
						continue;
					}
					while (Station + 1 < NS - 1 && Stations[Station + 1].S < S) { ++Station; }
					const FStation& A = Stations[Station];
					const FStation& B = Stations[FMath::Min(Station + 1, NS - 1)];
					const double Span = FMath::Max(1.0, B.S - A.S);
					const double T = FMath::Clamp((S - A.S) / Span, 0.0, 1.0);
					const FVector2D P = A.P + (B.P - A.P) * T;
					const FVector2D Off = A.Off + (B.Off - A.Off) * T;
					const FVector2D Where = P + Off * Side * (0.5 * (TrenchHalfChannel + TrenchBermTop));
					const double Top = FMath::Lerp(InTop[Station].Z, InTop[FMath::Min(Station + 1, NS - 1)].Z, T);
					AddSandbag(Solid, FVector(Where, Top + 9.0), A.Along, Bag);
					if (InGroup >= 1 && InGroup <= 5)
					{
						AddSandbag(Solid, FVector(Where - A.Along * 30.0, Top + 30.0), A.Along, Bag + 3);
					}
					S += 60.0;
					++Bag;
				}
			}
		}

		// Tarima del fondo (sin colisión: apenas levanta) y puentes de tablones de lomo a lomo (con colisión).
		for (int32 k = 0; k + 1 < NS; ++k)
		{
			for (int32 Half = 0; Half < 2; ++Half)
			{
				const FVector2D P = Stations[k].P + (Stations[k + 1].P - Stations[k].P) * (0.25 + 0.5 * Half);
				const FVector2D Across = Stations[k].Off.GetSafeNormal();
				Deco.AddBox(FVector(P, SandZ(P.X, P.Y) + 4.0), FVector(Across, 0.0), FVector(135.0, 11.0, 3.0), WoodTone(k * 2 + Half) * 0.9f);
			}
		}
		for (const double At : Bridges)
		{
			int32 Station = 0;
			while (Station + 1 < NS && Stations[Station + 1].S < At) { ++Station; }
			const FStation& St = Stations[Station];
			const FVector2D Across = St.Off.GetSafeNormal();
			double Top = -TNumericLimits<double>::Max();
			for (const double Side : { 1.0, -1.0 })
			{
				const FVector2D Mid = St.P + St.Off * Side * (0.5 * (TrenchHalfChannel + TrenchBermTop));
				Top = FMath::Max(Top, NaturalZ(Mid.X, Mid.Y) + TrenchBermHeight);
			}
			for (int32 j = 0; j < 4; ++j)
			{
				const FVector2D P = St.P + St.Along * ((j - 1.5) * 30.0);
				Solid.AddBox(FVector(P, Top + 6.0), FVector(Across, 0.0), FVector(330.0, 13.0, 5.0), WoodTone(j + 1));
			}
		}
	}

	/**
	 * Cornisa de una cresta: labio de arena en lo alto, con la cara de sotavento casi vertical (algo volada) a LipHeight del
	 * suelo de esa cara tal y como lo dibuja el terreno, y la de barlovento enterrada. Se salta desde abajo.
	 */
	template <typename FGroundFn>
	void BuildLip(FBuffers& M, const TNBeachLayout::FRidge& Ridge, FGroundFn&& GroundAt, uint32 Seed)
	{
		using namespace TNBeachLayout;
		TArray<TArray<FVector>> Run;
		auto Flush = [&M, &Run]()
		{
			if (Run.Num() >= 2)
			{
				M.AddSweep(Run, false, FLinearColor(0.86f, 0.76f, 0.52f) * 0.97f);
				// Remates de las puntas del labio.
				for (int32 End = 0; End < 2; ++End)
				{
					const TArray<FVector>& Ring = End == 0 ? Run[0] : Run.Last();
					const FVector Out = End == 0 ? Run[0][0] - Run[1][0] : Run.Last()[0] - Run[Run.Num() - 2][0];
					for (int32 k = 1; k + 1 < Ring.Num(); ++k) { M.AddTri(Ring[0], Ring[k], Ring[k + 1], Out, FLinearColor(0.86f, 0.76f, 0.52f) * 0.93f); }
				}
			}
			Run.Reset();
		};
		int32 Index = 0;
		for (double U = -LipMaxU * Ridge.HalfLength; U <= LipMaxU * Ridge.HalfLength; U += 150.0, ++Index)
		{
			if (!IsLipAt(Ridge, U))
			{
				Flush();
				continue;
			}
			const FVector2D C = RidgeCrestPoint(Ridge, U);
			const FVector2D W = Ridge.Windward;
			const FVector2D SlipFoot = C - W * 80.0;
			const FVector2D WindFoot = C + W * 140.0;
			const double ZSlip = GroundAt(SlipFoot.X, SlipFoot.Y);
			const double ZWind = GroundAt(WindFoot.X, WindFoot.Y);
			const double Top = FMath::Min(ZSlip, ZWind) + LipHeight + 6.0 * TNProcMesh::TNProcHashNoise(Index, 5, Seed);
			TArray<FVector>& Ring = Run.AddDefaulted_GetRef();
			// Perfil de sotavento a barlovento (abierto por debajo: queda enterrado).
			Ring.Add(FVector(C - W * 80.0, ZSlip - 45.0));
			Ring.Add(FVector(C - W * 95.0, Top - 10.0));
			Ring.Add(FVector(C - W * 70.0, Top + 4.0));
			Ring.Add(FVector(C - W * 10.0, Top + 12.0));
			Ring.Add(FVector(C + W * 70.0, Top + 2.0));
			Ring.Add(FVector(C + W * 160.0, FMath::Min(Top, ZWind) - 40.0));
		}
		Flush();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Trincheras, cornisas y rocas
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildFeatures()
{
	TNBeachFeatures::FBuffers Trenches;
	TNBeachFeatures::FBuffers Lips;
	TNBeachFeatures::FBuffers Rocks;
	TNBeachFeatures::FBuffers Deco;

	const TArray<TNBeachLayout::FTrench>& AllTrenches = TNBeachLayout::Trenches();
	for (int32 t = 0; t < AllTrenches.Num(); ++t)
	{
		TNBeachFeatures::BuildTrench(Trenches, Deco, AllTrenches[t], 0x7E4Cu + static_cast<uint32>(t) * 31u);
	}

	// El labio se mide contra el terreno tal y como se dibuja (su triángulo), no contra la fórmula: así se salta.
	auto GroundAt = [this](double X, double Y) { return MeshGroundZ(X, Y); };
	const TArray<TNBeachLayout::FRidge>& AllRidges = TNBeachLayout::Ridges();
	for (int32 r = 0; r < AllRidges.Num(); ++r)
	{
		if (AllRidges[r].bLip) { TNBeachFeatures::BuildLip(Lips, AllRidges[r], GroundAt, 0x11B0u + static_cast<uint32>(r)); }
	}

	// Rocas en la orilla de las pozas de marea (hacia el mar), medio metidas en el agua.
	const TArray<TNBeachLayout::FPool>& AllPools = TNBeachLayout::Pools();
	for (int32 p = 0; p < AllPools.Num(); ++p)
	{
		const TNBeachLayout::FPool& Pool = AllPools[p];
		if (!Pool.bTide) { continue; }
		TNProcMap::FRng Rng(static_cast<uint64>(TNBeachLayout::TerrainSeed) * 57ull + static_cast<uint64>(p) * 7919ull);
		const int32 Count = Rng.RangeInt(5, 8);
		for (int32 k = 0; k < Count; ++k)
		{
			const double Theta = TNProcMap::TwoPi * (k + Rng.Range(0.1, 0.9)) / Count;
			const double U = Rng.Range(0.98, 1.12);
			const FVector2D P = TNBeachLayout::PoolPoint(Pool, Theta, U);
			const double Radius = Rng.Range(150.0, 320.0);
			const double Height = Rng.Range(110.0, 260.0);
			const FLinearColor Tone = TNProcMesh::TNProcLerpColor(FLinearColor(0.42f, 0.38f, 0.33f), FLinearColor(0.22f, 0.3f, 0.16f), static_cast<float>(Rng.Range(0.0, 0.45)));
			TNProcMesh::TNProcAddBoulder(Rocks, FVector(P, TNBeachLayout::SandZ(P.X, P.Y) - 20.0), Radius, Height, static_cast<uint32>(Rng.RangeInt(1, 1 << 20)), Tone);
		}
	}

	// Caras planas con su color (como el acantilado); con colisión lo que se pisa o se salta.
	UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial();
	TNBeachRaceKit::Upload(FeatureMesh, 0, Trenches, Mat, true);
	TNBeachRaceKit::Upload(FeatureMesh, 1, Lips, Mat, true);
	TNBeachRaceKit::Upload(FeatureMesh, 2, Rocks, Mat, true);
	TNBeachRaceKit::Upload(FeatureDecoMesh, 0, Deco, Mat, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Agua de las pozas
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildPoolWater()
{
	TNBeachFeatures::FBuffers Water;
	const FLinearColor Turquoise = TNBeachRaceKit::Hex(0x39B9C4u);
	constexpr int32 Segments = 48;
	for (const TNBeachLayout::FPool& Pool : TNBeachLayout::Pools())
	{
		// Un poco más allá de la orilla: queda bajo la arena, que sube desde el agua.
		const FVector Center(Pool.Center, Pool.Water);
		for (int32 k = 0; k < Segments; ++k)
		{
			const FVector2D A = TNBeachLayout::PoolPoint(Pool, TNProcMap::TwoPi * k / Segments, 1.15);
			const FVector2D B = TNBeachLayout::PoolPoint(Pool, TNProcMap::TwoPi * (k + 1) / Segments, 1.15);
			Water.AddTri(Center, FVector(A, Pool.Water), FVector(B, Pool.Water), FVector::UpVector, Turquoise);
		}
	}
	// El mar animado del mapa procedural con la hondura y la espuma de una poza de un par de metros.
	UMaterialInterface* WaterMat = TNBeachRaceKit::SeaMaterial();
	if (WaterMat)
	{
		PoolMaterial = UMaterialInstanceDynamic::Create(WaterMat, this);
		if (PoolMaterial)
		{
			PoolMaterial->SetFlags(RF_Transient);
			PoolMaterial->SetScalarParameterValue(TEXT("DepthRange"), 300.f);
			PoolMaterial->SetScalarParameterValue(TEXT("FoamWidth"), 60.f);
			WaterMat = PoolMaterial.Get();
		}
	}
	TNBeachRaceKit::Upload(PoolMesh, 0, Water, WaterMat, false);
}
