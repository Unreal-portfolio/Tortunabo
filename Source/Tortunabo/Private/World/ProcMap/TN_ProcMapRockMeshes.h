#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "TN_ProcMapMeshKit.h"

/**
 * Rocas del camino con estilos por bioma (TN_ProcMapGenerator_Build.cpp): peñascos redondos, losas
 * inclinadas, partidos, apilados, de estratos, columnas de basalto, con musgo, con cristales o de
 * coral; agujas esbeltas, inclinadas, gemelas, chimeneas de hadas, pilares kársticos con vegetación y
 * órganos de basalto; mogotes, tors de bloques, mesas de estratos y domos de lava. Local: base en el
 * origen (suelo), Z arriba, cm. El estilo lo decide la semilla entre los propios del bioma.
 */
namespace TNRockMesh
{
	using namespace TNProcMesh;
	using TNProcMap::LerpD;

	enum class EBoulderStyle : uint8 { Round, Slab, Split, Stacked, Strata, Basalt, Mossy, Crystal, Coral, Count };
	enum class ESpireStyle : uint8 { Spire, Leaning, Twin, Hoodoo, Karst, Organ, Mogote, Tor, StrataMesa, LavaDome, Count };

	/** Colores de las rocas de un bioma. */
	struct FTNRockColors
	{
		FLinearColor Rock;
		FLinearColor Band;      ///< Segundo tono (estratos, vetas).
		FLinearColor Moss;
		FLinearColor Crystal;
		FLinearColor Glow;      ///< Grietas de lava.
	};

	inline double TNRockRand(uint32 Seed, int32 K, double Lo, double Hi)
	{
		return LerpD(Lo, Hi, 0.5 + 0.5 * TNProcHashNoise(K, 131, Seed));
	}

	/** Estilo de peñasco según el bioma y la semilla. */
	inline EBoulderStyle TNBoulderStyleFor(ETNProcBiome Biome, uint32 Seed)
	{
		using S = EBoulderStyle;
		static const S Jungle[] = { S::Round, S::Mossy, S::Mossy, S::Stacked, S::Split };
		static const S Beach[] = { S::Round, S::Coral, S::Coral, S::Stacked, S::Strata };
		static const S Desert[] = { S::Strata, S::Strata, S::Slab, S::Stacked, S::Split };
		static const S Volcanic[] = { S::Basalt, S::Basalt, S::Split, S::Round };
		static const S Rocky[] = { S::Round, S::Slab, S::Split, S::Crystal, S::Stacked };
		static const S Other[] = { S::Round, S::Slab, S::Mossy };
		const uint32 H = (Seed * 2654435761u) >> 7;
		switch (Biome)
		{
			case ETNProcBiome::Jungle:   return Jungle[H % UE_ARRAY_COUNT(Jungle)];
			case ETNProcBiome::Beach:    return Beach[H % UE_ARRAY_COUNT(Beach)];
			case ETNProcBiome::Desert:   return Desert[H % UE_ARRAY_COUNT(Desert)];
			case ETNProcBiome::Volcanic: return Volcanic[H % UE_ARRAY_COUNT(Volcanic)];
			case ETNProcBiome::Rocky:    return Rocky[H % UE_ARRAY_COUNT(Rocky)];
			default:                     return Other[H % UE_ARRAY_COUNT(Other)];
		}
	}

	/** Estilo de aguja (bSpire) o de mogote según el bioma y la semilla. */
	inline ESpireStyle TNSpireStyleFor(ETNProcBiome Biome, bool bSpire, uint32 Seed)
	{
		using S = ESpireStyle;
		const uint32 H = (Seed * 2246822519u) >> 9;
		if (bSpire)
		{
			static const S Jungle[] = { S::Karst, S::Karst, S::Spire, S::Leaning };
			static const S Desert[] = { S::Hoodoo, S::Hoodoo, S::Spire, S::Leaning };
			static const S Volcanic[] = { S::Organ, S::Organ, S::Spire };
			static const S Rocky[] = { S::Spire, S::Twin, S::Leaning, S::Twin };
			static const S Other[] = { S::Spire, S::Leaning, S::Twin };
			switch (Biome)
			{
				case ETNProcBiome::Jungle:   return Jungle[H % UE_ARRAY_COUNT(Jungle)];
				case ETNProcBiome::Desert:
				case ETNProcBiome::Beach:    return Desert[H % UE_ARRAY_COUNT(Desert)];
				case ETNProcBiome::Volcanic: return Volcanic[H % UE_ARRAY_COUNT(Volcanic)];
				case ETNProcBiome::Rocky:    return Rocky[H % UE_ARRAY_COUNT(Rocky)];
				default:                     return Other[H % UE_ARRAY_COUNT(Other)];
			}
		}
		static const S Desert[] = { S::StrataMesa, S::StrataMesa, S::Mogote };
		static const S Volcanic[] = { S::LavaDome, S::Mogote, S::Tor };
		static const S Rocky[] = { S::Tor, S::Tor, S::Mogote };
		static const S Other[] = { S::Mogote, S::Tor };
		switch (Biome)
		{
			case ETNProcBiome::Desert:
			case ETNProcBiome::Beach:    return Desert[H % UE_ARRAY_COUNT(Desert)];
			case ETNProcBiome::Volcanic: return Volcanic[H % UE_ARRAY_COUNT(Volcanic)];
			case ETNProcBiome::Rocky:    return Rocky[H % UE_ARRAY_COUNT(Rocky)];
			default:                     return Other[H % UE_ARRAY_COUNT(Other)];
		}
	}

	/** Losa: prisma de ocho esquinas con ruido, inclinada y medio enterrada. */
	inline void TNRockSlab(FTNProcMeshBuffers& M, const FVector& Base, double Rx, double Ry, double Hz, double TiltDeg, double YawDeg, uint32 Seed, const FLinearColor& Col)
	{
		const double Ya = FMath::DegreesToRadians(YawDeg);
		const double Ta = FMath::DegreesToRadians(TiltDeg);
		const FVector Ax(FMath::Cos(Ya), FMath::Sin(Ya), 0.0);
		const FVector Ay0(-FMath::Sin(Ya), FMath::Cos(Ya), 0.0);
		// Inclinada alrededor de Ax: Ay y Az giran juntos.
		const FVector Ay = Ay0 * FMath::Cos(Ta) + FVector::UpVector * FMath::Sin(Ta);
		const FVector Az = FVector::CrossProduct(Ax, Ay);
		FVector P[8];
		int32 k = 0;
		for (const double Sz : { -1.0, 1.0 })
		{
			for (const double Sy : { -1.0, 1.0 })
			{
				for (const double Sx : { -1.0, 1.0 })
				{
					const double J = 1.0 + 0.18 * TNProcHashNoise(k, 5, Seed);
					P[k++] = Base + (Ax * (Sx * Rx) + Ay * (Sy * Ry) + Az * (Sz * Hz)) * J;
				}
			}
		}
		const FVector C = (P[0] + P[7]) * 0.5;
		auto Q = [&](int32 A, int32 B, int32 Cc, int32 D) { M.AddQuad(P[A], P[B], P[Cc], P[D], (P[A] + P[B] + P[Cc] + P[D]) * 0.25 - C, Col * (0.9f + 0.1f * static_cast<float>(TNProcHashNoise(A, B, Seed)))); };
		Q(4, 5, 7, 6); Q(0, 2, 3, 1); Q(0, 1, 5, 4); Q(2, 6, 7, 3); Q(0, 4, 6, 2); Q(1, 3, 7, 5);
	}

	/** Columna de basalto: prisma hexagonal de techo algo inclinado. */
	inline void TNRockHexColumn(FTNProcMeshBuffers& M, const FVector& Base, double R, double H, uint32 Seed, const FLinearColor& Col)
	{
		TNProcAddCylinder(M, Base - FVector(0.0, 0.0, 20.0), Base + FVector(0.0, 0.0, H), R, R * 0.97, 6, Col * (0.9f + 0.15f * static_cast<float>(0.5 + 0.5 * TNProcHashNoise(3, 1, Seed))));
	}

	/** Peñasco en su estilo (local, base en el suelo). */
	inline void TNRockBuildBoulder(FTNProcMeshBuffers& M, EBoulderStyle Style, double R, double H, uint32 Seed, const FTNRockColors& C)
	{
		const FVector O = FVector::ZeroVector;
		switch (Style)
		{
			case EBoulderStyle::Slab:
			{
				// Losa gruesa inclinada con otra menor apoyada en ella.
				const double Yaw = TNRockRand(Seed, 2, 0.0, 180.0);
				TNRockSlab(M, O + FVector(0.0, 0.0, H * 0.32), R, R * 0.7, H * 0.45, TNRockRand(Seed, 1, 14.0, 30.0), Yaw, Seed, C.Rock);
				const double A = FMath::DegreesToRadians(Yaw + 90.0);
				TNRockSlab(M, O + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * (R * 0.85) + FVector(0.0, 0.0, H * 0.2), R * 0.5, R * 0.4, H * 0.3,
					-TNRockRand(Seed, 6, 20.0, 40.0), Yaw + 15.0, Seed + 21u, C.Rock * 0.92f);
				break;
			}
			case EBoulderStyle::Split:
			{
				const double A = FMath::DegreesToRadians(TNRockRand(Seed, 3, 0.0, 180.0));
				const FVector D(FMath::Cos(A), FMath::Sin(A), 0.0);
				TNProcAddBoulder(M, O + D * (R * 0.55), R * 0.6, H * 0.95, Seed, C.Rock);
				TNProcAddBoulder(M, O - D * (R * 0.55), R * 0.56, H * 0.8, Seed + 11u, C.Rock * 0.94f);
				// Cuña de roca caída en la grieta.
				TNProcAddBoulder(M, O + FVector(-D.Y, D.X, 0.0) * (R * 0.35), R * 0.22, H * 0.25, Seed + 17u, C.Rock * 0.85f);
				break;
			}
			case EBoulderStyle::Stacked:
			{
				TNProcAddBoulder(M, O, R, H * 0.6, Seed, C.Rock);
				TNProcAddBoulder(M, O + FVector(R * 0.1, -R * 0.08, H * 0.5), R * 0.62, H * 0.45, Seed + 5u, C.Rock * 0.95f);
				if (H > 160.0) { TNProcAddBoulder(M, O + FVector(-R * 0.05, R * 0.05, H * 0.86), R * 0.36, H * 0.28, Seed + 9u, C.Rock * 1.05f); }
				break;
			}
			case EBoulderStyle::Strata:
			{
				const int32 N = 4;
				for (int32 b = 0; b < N; ++b)
				{
					const double Z0 = -20.0 + (H + 20.0) * b / N, Z1 = -20.0 + (H + 20.0) * (b + 1) / N;
					const double T = (b + 0.5) / N;
					const double Rr = R * FMath::Sin(LerpD(0.45, 0.88, T) * PI) * (b % 2 ? 0.93 : 1.0);
					TNProcAddLathe(M, O, { Z0, Z1 }, { Rr, Rr * (b == N - 1 ? 0.7 : 0.97) }, 0.1, Seed + static_cast<uint32>(b), b % 2 ? C.Band : C.Rock, 9, b == N - 1 ? 0.15 : 0.0);
				}
				break;
			}
			case EBoulderStyle::Basalt:
			{
				const int32 N = 3 + static_cast<int32>(Seed % 3);
				for (int32 c = 0; c < N; ++c)
				{
					const double A = TNProcMap::TwoPi * c / N;
					const double D = c == 0 ? 0.0 : R * 0.55;
					TNRockHexColumn(M, O + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.0), R * 0.42, H * TNRockRand(Seed, c, 0.55, 1.05), Seed + static_cast<uint32>(c), C.Rock);
				}
				break;
			}
			case EBoulderStyle::Mossy:
			{
				TNProcAddBoulder(M, O, R, H, Seed, C.Rock);
				TArray<double> Z, Rr;
				for (int32 r = 3; r <= 5; ++r)
				{
					const double T = r / 6.0;
					Z.Add(-25.0 + H * (0.5 - 0.5 * FMath::Cos(T * PI)) * 1.1 + 4.0);
					Rr.Add(R * FMath::Sin(FMath::Lerp(0.35, 0.92, T) * PI) * 1.04);
				}
				TNProcAddLathe(M, O, Z, Rr, 0.18, Seed + 3u, C.Moss, 9);
				break;
			}
			case EBoulderStyle::Crystal:
			{
				TNProcAddBoulder(M, O, R, H, Seed, C.Rock);
				for (int32 c = 0; c < 4; ++c)
				{
					const double A = TNProcMap::TwoPi * c / 4 + TNRockRand(Seed, c, 0.0, 1.0);
					const FVector Dir = FVector(FMath::Cos(A) * 0.9, FMath::Sin(A) * 0.9, 0.8).GetSafeNormal();
					const double Zc = H * TNRockRand(Seed, c + 30, 0.35, 0.7);
					const double Rs = R * FMath::Sin(FMath::Lerp(0.35, 0.92, Zc / (H * 1.1)) * PI) * 0.8;
					const FVector B = O + FVector(FMath::Cos(A) * Rs, FMath::Sin(A) * Rs, Zc);
					const double L = TNRockRand(Seed, c + 10, 0.35, 0.6) * R;
					TNProcAddCylinder(M, B - Dir * (L * 0.3), B + Dir * L, L * 0.22, L * 0.18, 6, C.Crystal, false);
					TNProcAddLathe(M, B + Dir * L, { 0.0, L * 0.25 }, { L * 0.14, 0.5 }, 0.0, 0u, C.Crystal * 1.25f, 6);
				}
				break;
			}
			case EBoulderStyle::Coral:
			{
				TNProcAddBoulder(M, O, R, H, Seed, C.Band);
				for (int32 h = 0; h < 8; ++h)
				{
					const double A = TNRockRand(Seed, h, 0.0, TNProcMap::TwoPi);
					const double Zt = TNRockRand(Seed, h + 20, 0.2, 0.8) * H;
					const double Rr = R * FMath::Sin(FMath::Lerp(0.35, 0.92, Zt / (H * 1.1)) * PI) * 0.97;
					M.AddBox(O + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Zt), FVector(FMath::Cos(A), FMath::Sin(A), 0.0), FVector(5.0, 7.0, 7.0), C.Rock * 0.35f);
				}
				break;
			}
			case EBoulderStyle::Round:
			default:
				TNProcAddBoulder(M, O, R, H, Seed, C.Rock);
				break;
		}
	}

	/** Aguja o mogote en su estilo (local, base en el suelo). */
	inline void TNRockBuildSpire(FTNProcMeshBuffers& M, ESpireStyle Style, double R, double H, uint32 Seed, const FTNRockColors& C)
	{
		const FVector O(0.0, 0.0, -40.0);
		auto Taper = [&](const FVector& Base, double Rb, double Hb, double Top, double Jit, uint32 S, const FLinearColor& Col, const FVector& Lean)
		{
			TArray<double> Z, Rr;
			for (int32 r = 0; r <= 8; ++r)
			{
				const double T = r / 8.0;
				Z.Add(Hb * T);
				Rr.Add(Rb * LerpD(1.25, Top, T));
			}
			// Inclinación: cada anillo se desplaza con la altura (torno propio por anillos).
			TArray<TArray<FVector>> Rings;
			for (int32 r = 0; r <= 8; ++r)
			{
				TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
				const FVector Cc = Base + FVector(0.0, 0.0, Z[r]) + Lean * (Z[r] / FMath::Max(1.0, Hb));
				for (int32 k = 0; k < 11; ++k)
				{
					const double A = TNProcMap::TwoPi * k / 11;
					const double J = 1.0 + Jit * TNProcHashNoise(r, k, S);
					Ring.Add(Cc + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * (Rr[r] * J));
				}
			}
			M.AddSweep(Rings, true, Col);
			const FVector TopC = Base + FVector(0.0, 0.0, Hb) + Lean;
			for (int32 k = 0; k < 11; ++k)
			{
				M.AddTri(TopC + FVector(0.0, 0.0, Rr[8] * 0.3), Rings[8][k], Rings[8][(k + 1) % 11], FVector::UpVector, Col * 1.05f);
			}
		};
		switch (Style)
		{
			case ESpireStyle::Leaning:
			{
				const double A = FMath::DegreesToRadians(TNRockRand(Seed, 1, 0.0, 360.0));
				Taper(O, R, H, 0.5, 0.18, Seed, C.Rock, FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * H * 0.22);
				break;
			}
			case ESpireStyle::Twin:
			{
				const double A = FMath::DegreesToRadians(TNRockRand(Seed, 1, 0.0, 360.0));
				const FVector D(FMath::Cos(A), FMath::Sin(A), 0.0);
				Taper(O + D * R * 0.55, R * 0.75, H, 0.5, 0.18, Seed, C.Rock, D * H * 0.06);
				Taper(O - D * R * 0.6, R * 0.6, H * 0.68, 0.45, 0.2, Seed + 7u, C.Rock * 0.94f, -D * H * 0.05);
				break;
			}
			case ESpireStyle::Hoodoo:
			{
				// Chimenea de hadas: tambores de estratos que se estrechan y un sombrero de roca dura.
				const int32 N = 5;
				double Z = O.Z;
				for (int32 b = 0; b < N; ++b)
				{
					const double T = static_cast<double>(b) / N;
					const double Hb = (H - R * 0.5) / N;
					const double Rb = R * LerpD(1.2, 0.55, T) * (b % 2 ? 0.86 : 1.0);
					TNProcAddLathe(M, FVector(0.0, 0.0, Z), { 0.0, Hb * 0.5, Hb }, { Rb, Rb * 0.9, Rb * 0.95 }, 0.1, Seed + static_cast<uint32>(b), b % 2 ? C.Band : C.Rock, 9, 0.0);
					Z += Hb;
				}
				TNRockSlab(M, FVector(0.0, 0.0, Z + R * 0.18), R * 0.95, R * 0.85, R * 0.2, TNRockRand(Seed, 4, 3.0, 10.0), TNRockRand(Seed, 5, 0.0, 180.0), Seed + 13u, C.Rock * 0.8f);
				break;
			}
			case ESpireStyle::Karst:
			{
				Taper(O, R * 0.9, H, 0.8, 0.14, Seed, C.Rock, FVector::ZeroVector);
				// Vegetación en la cima y cortinas de musgo por los lados.
				TNProcAddLathe(M, O + FVector(0.0, 0.0, H - 10.0), { 0.0, R * 0.35, R * 0.6 }, { R * 1.25, R * 1.05, R * 0.4 }, 0.25, Seed + 3u, C.Moss, 9);
				for (int32 k = 0; k < 4; ++k)
				{
					const double A = TNProcMap::TwoPi * k / 4 + 0.4;
					const FVector D(FMath::Cos(A), FMath::Sin(A), 0.0);
					const double Rr = R * 0.9 * 0.82;
					M.AddQuad(O + D * (Rr + 6.0) + FVector(0.0, 0.0, H - 5.0) - FVector(-D.Y, D.X, 0.0) * 25.0,
						O + D * (Rr + 6.0) + FVector(0.0, 0.0, H - 5.0) + FVector(-D.Y, D.X, 0.0) * 25.0,
						O + D * (Rr * 1.15 + 6.0) + FVector(0.0, 0.0, H * 0.45) + FVector(-D.Y, D.X, 0.0) * 12.0,
						O + D * (Rr * 1.15 + 6.0) + FVector(0.0, 0.0, H * 0.45) - FVector(-D.Y, D.X, 0.0) * 12.0, D, C.Moss * 0.9f);
				}
				break;
			}
			case ESpireStyle::Organ:
			{
				// Órgano de basalto: columnas hexagonales en escalera alrededor de la más alta.
				const int32 N = 7;
				for (int32 c = 0; c < N; ++c)
				{
					const double A = TNProcMap::TwoPi * c / (N - 1);
					const double D = c == 0 ? 0.0 : R * 0.78;
					const double Hc = c == 0 ? H : H * TNRockRand(Seed, c, 0.45, 0.85);
					TNRockHexColumn(M, O + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.0), R * 0.42, Hc, Seed + static_cast<uint32>(c), C.Rock);
				}
				break;
			}
			case ESpireStyle::Mogote:
			{
				TArray<double> Z, Rr;
				for (int32 r = 0; r <= 5; ++r) { const double T = r / 5.0; Z.Add(H * T); Rr.Add(R * FMath::Lerp(1.15, 0.8, T * T)); }
				TNProcAddLathe(M, O, Z, Rr, 0.12, Seed, C.Rock, 11);
				break;
			}
			case ESpireStyle::Tor:
			{
				// Tor: bloques redondeados apilados en dos o tres columnas.
				for (int32 c = 0; c < 3; ++c)
				{
					const double A = TNProcMap::TwoPi * c / 3 + TNRockRand(Seed, c, 0.0, 0.8);
					const FVector B = O + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * R * 0.45 + FVector(0.0, 0.0, 40.0);
					double Z = 0.0;
					const int32 Blocks = 2 + static_cast<int32>((Seed + c) % 2);
					for (int32 b = 0; b < Blocks; ++b)
					{
						const double Hb = H / 3.2 * TNRockRand(Seed, c * 5 + b, 0.8, 1.1);
						TNRockSlab(M, B + FVector(0.0, 0.0, Z + Hb * 0.5), R * 0.42, R * 0.36, Hb * 0.5, TNRockRand(Seed, b + 30, 0.0, 8.0), TNRockRand(Seed, b + 40, 0.0, 180.0), Seed + static_cast<uint32>(c * 7 + b), C.Rock * (b % 2 ? 0.94f : 1.0f));
						Z += Hb;
					}
				}
				break;
			}
			case ESpireStyle::StrataMesa:
			{
				const int32 N = 4;
				for (int32 b = 0; b < N; ++b)
				{
					const double Z0 = H * b / N, Z1 = H * (b + 1) / N;
					const double Rb = R * LerpD(1.2, 0.9, static_cast<double>(b) / N) * (b % 2 ? 0.94 : 1.0);
					TNProcAddLathe(M, O, { Z0, Z1 }, { Rb, Rb * 0.97 }, 0.08, Seed + static_cast<uint32>(b), b % 2 ? C.Band : C.Rock, 12, b == N - 1 ? 0.04 : 0.0);
				}
				break;
			}
			case ESpireStyle::LavaDome:
			{
				TNProcAddLathe(M, O, { 0.0, H * 0.5, H * 0.85, H }, { R * 1.2, R * 1.0, R * 0.62, R * 0.2 }, 0.14, Seed, C.Rock, 11);
				for (int32 k = 0; k < 9; ++k)
				{
					const double A = TNProcMap::TwoPi * k / 9 + TNRockRand(Seed, k, 0.0, 0.5);
					const FVector D(FMath::Cos(A), FMath::Sin(A), 0.0);
					const FVector Up(0.0, 0.0, 1.0);
					M.AddBeam(O + D * R * 1.19 + Up * (H * 0.05 + 40.0), O + D * R * 0.76 + Up * (H * 0.72 + 40.0), 11.0, C.Glow);
				}
				break;
			}
			case ESpireStyle::Spire:
			default:
				Taper(O, R, H, 0.55, 0.18, Seed, C.Rock, FVector::ZeroVector);
				TNProcAddBoulder(M, O + FVector(0.0, 0.0, H - 30.0), R * 1.25, R * 0.9, Seed + 77u, C.Rock * 0.85f);
				break;
		}
	}

	/** Colores de las rocas de un bioma a partir del color de su roca y su suelo. */
	inline FTNRockColors TNRockColorsFor(ETNProcBiome Biome, const FLinearColor& RockC, const FLinearColor& Ground)
	{
		FTNRockColors C;
		C.Rock = TNProcLerpColor(RockC, Ground, 0.25f);
		C.Band = TNProcLerpColor(RockC, Ground, 0.6f) * 1.08f;
		C.Moss = FLinearColor(0.16f, 0.42f, 0.12f);
		C.Crystal = FLinearColor(0.75f, 0.82f, 0.95f);
		C.Glow = FLinearColor(1.0f, 0.42f, 0.05f);
		switch (Biome)
		{
			case ETNProcBiome::Desert: C.Band = FLinearColor(0.78f, 0.45f, 0.26f); C.Rock = FLinearColor(0.85f, 0.62f, 0.4f); break;
			case ETNProcBiome::Beach:  C.Band = FLinearColor(0.82f, 0.62f, 0.62f); break;
			case ETNProcBiome::Volcanic: C.Rock = FLinearColor(0.1f, 0.09f, 0.1f); C.Band = FLinearColor(0.22f, 0.12f, 0.1f); break;
			case ETNProcBiome::Jungle: C.Moss = FLinearColor(0.12f, 0.4f, 0.09f); break;
			default: break;
		}
		return C;
	}
}
