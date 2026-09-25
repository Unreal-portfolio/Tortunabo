#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapFlora.h"
#include "TN_ProcMapMeshKit.h"

/**
 * Mallas de la vegetación procedural (low-poly de caras planas con color de vértice): paleta de
 * cada bioma, piezas (masas de follaje cerradas, frondas, hojas de hierba, aletas, ramas) y una
 * receta por especie. Local: base en el origen, Z arriba, cm a escala 1. Sin dependencias del
 * motor más allá de CoreMinimal, para poder previsualizarlas fuera de él.
 */
namespace TNFloraMesh
{
	using namespace TNProcMesh;
	using TNProcMap::EFloraShape;

	/** Colores de la vegetación de un bioma. */
	struct FTNFloraPalette
	{
		FLinearColor Leaf;      ///< Follaje principal.
		FLinearColor LeafAlt;   ///< Segunda masa (más clara o amarillenta).
		FLinearColor Bark;
		FLinearColor Grass;
		FLinearColor Dry;       ///< Ramitas, hojas secas, madera muerta.
		FLinearColor Rock;
		FLinearColor Accent;    ///< Flores, brasas, bayas.
		FLinearColor Moss;      ///< Enredaderas, musgo o liquen de las paredes.
	};

	inline FTNFloraPalette TNFloraPaletteFor(ETNProcBiome Biome, const FLinearColor& Ground, const FLinearColor& RockColor)
	{
		FTNFloraPalette P;
		P.Rock = TNProcLerpColor(RockColor, FLinearColor(0.45f, 0.43f, 0.4f), 0.2f);
		P.Bark = FLinearColor(0.3f, 0.2f, 0.12f);
		P.Dry = FLinearColor(0.4f, 0.32f, 0.21f);
		P.Accent = FLinearColor(0.85f, 0.2f, 0.25f);
		switch (Biome)
		{
			case ETNProcBiome::Jungle:
				P.Leaf = FLinearColor(0.08f, 0.33f, 0.07f); P.LeafAlt = FLinearColor(0.15f, 0.44f, 0.09f); P.Grass = FLinearColor(0.18f, 0.48f, 0.11f);
				break;
			case ETNProcBiome::Beach:
				P.Leaf = FLinearColor(0.2f, 0.46f, 0.13f); P.LeafAlt = FLinearColor(0.3f, 0.52f, 0.16f); P.Grass = FLinearColor(0.6f, 0.55f, 0.3f);
				P.Bark = FLinearColor(0.48f, 0.37f, 0.23f);
				break;
			case ETNProcBiome::Desert:
				P.Leaf = FLinearColor(0.33f, 0.42f, 0.17f); P.LeafAlt = FLinearColor(0.22f, 0.41f, 0.17f); P.Grass = FLinearColor(0.62f, 0.52f, 0.3f);
				P.Dry = FLinearColor(0.46f, 0.36f, 0.24f); P.Accent = FLinearColor(0.95f, 0.45f, 0.6f);
				break;
			case ETNProcBiome::Volcanic:
				P.Leaf = FLinearColor(0.16f, 0.22f, 0.11f); P.LeafAlt = FLinearColor(0.24f, 0.27f, 0.16f); P.Grass = FLinearColor(0.27f, 0.29f, 0.19f);
				P.Bark = FLinearColor(0.06f, 0.05f, 0.05f); P.Dry = FLinearColor(0.2f, 0.18f, 0.16f); P.Accent = FLinearColor(1.f, 0.35f, 0.05f);
				break;
			case ETNProcBiome::Water:
				P.Leaf = FLinearColor(0.16f, 0.44f, 0.13f); P.LeafAlt = FLinearColor(0.38f, 0.56f, 0.19f); P.Grass = FLinearColor(0.26f, 0.53f, 0.17f);
				P.Accent = FLinearColor(0.95f, 0.85f, 0.3f);
				break;
			case ETNProcBiome::Rocky:
				P.Leaf = FLinearColor(0.07f, 0.24f, 0.12f); P.LeafAlt = FLinearColor(0.1f, 0.3f, 0.14f); P.Grass = FLinearColor(0.34f, 0.46f, 0.19f);
				P.Accent = FLinearColor(0.7f, 0.5f, 0.9f);
				break;
			case ETNProcBiome::Mangrove:
				P.Leaf = FLinearColor(0.07f, 0.28f, 0.09f); P.LeafAlt = FLinearColor(0.13f, 0.36f, 0.11f); P.Grass = FLinearColor(0.23f, 0.44f, 0.15f);
				P.Bark = FLinearColor(0.34f, 0.2f, 0.12f);
				break;
			case ETNProcBiome::Human:
			default:
				P.Leaf = FLinearColor(0.16f, 0.47f, 0.13f); P.LeafAlt = FLinearColor(0.22f, 0.55f, 0.16f); P.Grass = FLinearColor(0.28f, 0.57f, 0.19f);
				P.Accent = FLinearColor(0.9f, 0.25f, 0.2f);
				break;
		}
		// Enredadera en los biomas verdes; liquen gris verdoso en la roca y musgo oscuro en el volcán.
		P.Moss = Biome == ETNProcBiome::Rocky ? FLinearColor(0.3f, 0.36f, 0.2f)
			: Biome == ETNProcBiome::Volcanic ? FLinearColor(0.12f, 0.18f, 0.09f) : TNProcLerpColor(P.Leaf, P.LeafAlt, 0.4f);
		// La hierba tira un poco al color del suelo del bioma: la pradera casa con el terreno.
		P.Grass = TNProcLerpColor(P.Grass, Ground, 0.25f);
		return P;
	}

	/** Tono de una variante: una algo más oscura, otra más clara y amarillenta. */
	inline FLinearColor TNFloraVary(const FLinearColor& C, int32 Variant)
	{
		switch (Variant % 3)
		{
			case 0: return C * 0.9f;
			case 2: return FLinearColor(C.R * 1.12f + 0.02f, C.G * 1.08f, C.B * 0.95f);
			default: return C;
		}
	}

	/** Valor estable en [Lo, Hi] para la pieza K de la malla. */
	inline double TNFloraRand(uint32 Seed, int32 K, double Lo, double Hi)
	{
		return FMath::Lerp(Lo, Hi, 0.5 + 0.5 * TNProcHashNoise(K, 77, Seed));
	}

	/**
	 * Torno de caras planas con degradado vertical (más oscuro abajo) y costillas opcionales
	 * (Rib: radio de los vértices impares, para cactus). Tapa arriba siempre; abajo si bCapBottom.
	 */
	inline void TNFloraLathe(FTNProcMeshBuffers& M, const FVector& Base, const TArray<double>& Z, const TArray<double>& R, double Jitter, uint32 Seed,
		const FLinearColor& Color, int32 Seg, bool bCapBottom, double Rib = 1.0)
	{
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r < Z.Num(); ++r)
		{
			TArray<FVector> Ring;
			for (int32 k = 0; k < Seg; ++k)
			{
				const double A = TNProcMap::TwoPi * (k + 0.3 * TNProcHashNoise(r, k, Seed + 5u)) / Seg;
				const double Rad = FMath::Max(1.0, R[r] * (1.0 + Jitter * TNProcHashNoise(r, k, Seed)) * ((k % 2) ? Rib : 1.0));
				Ring.Add(Base + FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, Z[r]));
			}
			Rings.Add(Ring);
		}
		for (int32 r = 0; r + 1 < Rings.Num(); ++r)
		{
			const float Shade = 0.8f + 0.28f * static_cast<float>(r) / FMath::Max(1, Rings.Num() - 2);
			TArray<TArray<FVector>> Band = { Rings[r], Rings[r + 1] };
			M.AddSweep(Band, true, Color * Shade);
		}
		const TArray<FVector>& Top = Rings.Last();
		const FVector TopC = Base + FVector(0.0, 0.0, Z.Last() + R.Last() * 0.35);
		for (int32 k = 0; k < Seg; ++k) { M.AddTri(TopC, Top[k], Top[(k + 1) % Seg], FVector::UpVector, Color * 1.08f); }
		if (bCapBottom)
		{
			const TArray<FVector>& Bot = Rings[0];
			const FVector BotC = Base + FVector(0.0, 0.0, Z[0] - R[0] * 0.08);
			for (int32 k = 0; k < Seg; ++k) { M.AddTri(BotC, Bot[k], Bot[(k + 1) % Seg], -FVector::UpVector, Color * 0.7f); }
		}
	}

	/** Masa cerrada de follaje (elipsoide irregular) con sombra abajo y luz arriba. */
	inline void TNFloraBlob(FTNProcMeshBuffers& M, const FVector& Center, double Rxy, double Rz, uint32 Seed, const FLinearColor& Color, int32 Seg = 7)
	{
		constexpr int32 NumRings = 4;
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r < NumRings; ++r)
		{
			const double Phi = PI * (r + 1.0) / (NumRings + 1.0);
			const double Zr = -FMath::Cos(Phi) * Rz;
			const double Rad = FMath::Sin(Phi) * Rxy;
			TArray<FVector> Ring;
			for (int32 k = 0; k < Seg; ++k)
			{
				const double A = TNProcMap::TwoPi * (k + 0.5 * (r % 2)) / Seg;
				const double J = 1.0 + 0.2 * TNProcHashNoise(r, k, Seed);
				Ring.Add(Center + FVector(FMath::Cos(A) * Rad * J, FMath::Sin(A) * Rad * J, Zr + 0.12 * Rz * TNProcHashNoise(k, r, Seed + 3u)));
			}
			Rings.Add(Ring);
		}
		for (int32 r = 0; r + 1 < NumRings; ++r)
		{
			TArray<TArray<FVector>> Band = { Rings[r], Rings[r + 1] };
			M.AddSweep(Band, true, Color * (0.78f + 0.14f * r));
		}
		const FVector Bottom = Center - FVector(0.0, 0.0, Rz);
		const FVector Top = Center + FVector(0.0, 0.0, Rz);
		for (int32 k = 0; k < Seg; ++k)
		{
			M.AddTri(Bottom, Rings[0][k], Rings[0][(k + 1) % Seg], -FVector::UpVector, Color * 0.7f);
			M.AddTri(Top, Rings[NumRings - 1][k], Rings[NumRings - 1][(k + 1) % Seg], FVector::UpVector, Color * 1.15f);
		}
	}

	/** Cuenta: octaedro irregular (8 caras) para detalles pequeños (flores, bayas, brasas, cocos). */
	inline void TNFloraBead(FTNProcMeshBuffers& M, const FVector& Center, double Radius, uint32 Seed, const FLinearColor& Color)
	{
		FVector P[6];
		const FVector Axes[6] = { FVector(1.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, -1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(0.0, 0.0, -1.0) };
		for (int32 k = 0; k < 6; ++k) { P[k] = Center + Axes[k] * (Radius * (1.0 + 0.25 * TNProcHashNoise(k, 1, Seed))); }
		const int32 Ring[4] = { 0, 2, 1, 3 };
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector& A = P[Ring[k]];
			const FVector& B = P[Ring[(k + 1) % 4]];
			// Cada cara mira hacia fuera: de la cuenta a su centroide.
			M.AddTri(P[4], A, B, A + B + P[4] - Center * 3.0, Color * 1.1f);
			M.AddTri(P[5], A, B, A + B + P[5] - Center * 3.0, Color * 0.8f);
		}
	}

	/** Fronda u hoja larga de dos caras: sube, se arquea y cae hacia la punta. */
	inline void TNFloraFrond(FTNProcMeshBuffers& M, const FVector& Base, const FVector2D& Dir, double Length, double Width, double Rise, double Droop,
		const FLinearColor& Color, int32 Segs = 4)
	{
		const FVector D(Dir.X, Dir.Y, 0.0);
		const FVector Side(-Dir.Y, Dir.X, 0.0);
		FVector PrevL = Base, PrevR = Base;
		for (int32 s = 0; s <= Segs; ++s)
		{
			const double T = static_cast<double>(s) / Segs;
			const FVector C = Base + D * (Length * T) + FVector(0.0, 0.0, Rise * FMath::Sin(PI * T * 0.75) - Droop * T * T);
			const double W = Width * FMath::Sin(PI * FMath::Lerp(0.12, 1.0, T)) * (1.0 - 0.25 * T);
			const FVector Lp = C + Side * (W * 0.5);
			const FVector Rp = C - Side * (W * 0.5);
			if (s > 0)
			{
				M.AddQuad(PrevL, Lp, Rp, PrevR, FVector::UpVector, Color * (0.85f + 0.2f * static_cast<float>(T)));
				M.AddQuad(PrevL, PrevR, Rp, Lp, -FVector::UpVector, Color * 0.7f);
			}
			PrevL = Lp;
			PrevR = Rp;
		}
	}

	/** Hoja de hierba de dos caras: base más oscura, punta clara e inclinada hacia Dir. */
	inline void TNFloraBlade(FTNProcMeshBuffers& M, const FVector& Base, const FVector2D& Dir, double Height, double Width, double Bend, const FLinearColor& Color)
	{
		const FVector D(Dir.X, Dir.Y, 0.0);
		const FVector Side = FVector(-Dir.Y, Dir.X, 0.0) * (Width * 0.5);
		const FVector Mid = Base + D * (Bend * 0.35) + FVector(0.0, 0.0, Height * 0.55);
		const FVector Tip = Base + D * Bend + FVector(0.0, 0.0, Height);
		const FVector Nrm = FVector::CrossProduct(Side, Tip - Base);
		const FVector MS = Side * 0.6;
		M.AddQuad(Base - Side, Base + Side, Mid + MS, Mid - MS, Nrm, Color * 0.72f);
		M.AddQuad(Base - Side, Mid - MS, Mid + MS, Base + Side, -Nrm, Color * 0.72f);
		M.AddTri(Mid - MS, Mid + MS, Tip, Nrm, Color * 1.05f);
		M.AddTri(Mid - MS, Tip, Mid + MS, -Nrm, Color * 1.05f);
	}

	/** Aleta (raíz tabular): prisma triangular de grosor Thick entre P0 (tronco), P1 (suelo) y P2 (arriba). */
	inline void TNFloraFin(FTNProcMeshBuffers& M, const FVector& P0, const FVector& P1, const FVector& P2, double Thick, const FLinearColor& Color)
	{
		const FVector Out = (P1 - P0).GetSafeNormal2D();
		const FVector Side = FVector(-Out.Y, Out.X, 0.0) * (Thick * 0.5);
		M.AddTri(P0 + Side, P1 + Side, P2 + Side, Side, Color);
		M.AddTri(P0 - Side, P1 - Side, P2 - Side, -Side, Color);
		M.AddQuad(P1 - Side, P1 + Side, P2 + Side, P2 - Side, (P1 + P2) * 0.5 - P0, Color * 1.1f);
	}

	/** Ramas secas (una bifurcación por rama) desde el tronco. */
	inline void TNFloraTwigs(FTNProcMeshBuffers& M, int32 Count, double Z0, double Z1, double Len, double Thick, uint32 Seed, const FLinearColor& Color)
	{
		for (int32 b = 0; b < Count; ++b)
		{
			const double A = TNProcMap::TwoPi * (b + 0.35 * TNProcHashNoise(b, 11, Seed)) / Count;
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			const FVector From(0.0, 0.0, FMath::Lerp(Z0, Z1, (b + 0.5) / Count));
			const double L = Len * TNFloraRand(Seed, 20 + b, 0.7, 1.2);
			const FVector To = From + FVector(Dir.X * L, Dir.Y * L, L * TNFloraRand(Seed, 40 + b, 0.3, 0.8));
			M.AddBeam(From, To, Thick, Color);
			for (int32 f = 0; f < 2; ++f)
			{
				const double Fa = A + (f == 0 ? 0.6 : -0.6);
				const FVector Tip = To + FVector(FMath::Cos(Fa) * L * 0.45, FMath::Sin(Fa) * L * 0.45, L * 0.35);
				M.AddBeam(To, Tip, Thick * 0.55, Color * 0.95f);
			}
		}
	}

	/** Construye la malla de una especie (local: base en el origen, Z arriba, cm a escala 1). */
	inline void TNFloraBuild(FTNProcMeshBuffers& M, EFloraShape Shape, const FTNFloraPalette& Pal, int32 Variant, uint32 Seed)
	{
		const FLinearColor Leaf = TNFloraVary(Pal.Leaf, Variant);
		const FLinearColor LeafAlt = TNFloraVary(Pal.LeafAlt, Variant);
		const FLinearColor Bark = TNFloraVary(Pal.Bark, Variant + 1);
		const FLinearColor Grass = TNFloraVary(Pal.Grass, Variant);
		auto Rand = [Seed](int32 K, double Lo, double Hi) { return TNFloraRand(Seed, K, Lo, Hi); };
		auto DirAt = [Seed](int32 K, int32 Count) { const double A = TNProcMap::TwoPi * (K + 0.4 * TNProcHashNoise(K, 3, Seed)) / Count; return FVector2D(FMath::Cos(A), FMath::Sin(A)); };

		switch (Shape)
		{
			case EFloraShape::BroadTree:
			{
				const double H = Rand(1, 820.0, 1020.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 120.0, H * 0.45, H * 0.72 }, { 40.0, 30.0, 23.0, 15.0 }, 0.08, Seed, Bark, 7, false);
				for (int32 b = 0; b < 3; ++b)
				{
					const FVector2D D = DirAt(b, 3);
					M.AddBeam(FVector(0.0, 0.0, H * (0.48 + 0.08 * b)), FVector(D.X * Rand(10 + b, 120.0, 190.0), D.Y * Rand(10 + b, 120.0, 190.0), H * 0.78), 8.0, Bark * 0.9f);
				}
				const int32 NC = 6 + Variant % 2;
				for (int32 c = 0; c < NC; ++c)
				{
					const FVector2D D = DirAt(c + 5, NC);
					const double Dist = Rand(20 + c, 70.0, 180.0);
					TNFloraBlob(M, FVector(D.X * Dist, D.Y * Dist, H * Rand(30 + c, 0.72, 0.92)), Rand(40 + c, 150.0, 230.0), Rand(50 + c, 115.0, 165.0), Seed + c, (c % 2) ? Leaf : LeafAlt);
				}
				TNFloraBlob(M, FVector(0.0, 0.0, H * 0.93), 200.0, 150.0, Seed + 99u, LeafAlt * 1.05f);
				break;
			}
			case EFloraShape::Ceiba:
			{
				const double H = Rand(1, 2100.0, 2500.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 180.0, H * 0.35, H * 0.65, H * 0.86 }, { 82.0, 58.0, 50.0, 42.0, 32.0 }, 0.06, Seed, Bark, 8, false);
				for (int32 f = 0; f < 5; ++f)
				{
					const FVector2D D = DirAt(f, 5);
					const FVector Dv(D.X, D.Y, 0.0);
					TNFloraFin(M, Dv * 45.0, Dv * Rand(10 + f, 230.0, 320.0) + FVector(0.0, 0.0, -30.0), Dv * 55.0 + FVector(0.0, 0.0, Rand(20 + f, 300.0, 400.0)), 16.0, Bark * 0.92f);
				}
				for (int32 b = 0; b < 5; ++b)
				{
					const FVector2D D = DirAt(b + 2, 5);
					const double R = Rand(30 + b, 380.0, 520.0);
					M.AddBeam(FVector(0.0, 0.0, H * 0.8), FVector(D.X * R, D.Y * R, H * 0.93), 15.0, Bark * 0.9f);
				}
				for (int32 c = 0; c < 7; ++c)
				{
					const FVector2D D = DirAt(c + 7, 7);
					const double R = Rand(40 + c, 300.0, 480.0);
					TNFloraBlob(M, FVector(D.X * R, D.Y * R, H * Rand(50 + c, 0.92, 1.0)), Rand(60 + c, 260.0, 330.0), Rand(70 + c, 160.0, 210.0), Seed + c, (c % 2) ? Leaf : LeafAlt);
				}
				for (int32 c = 0; c < 4; ++c)
				{
					const FVector2D D = DirAt(c + 15, 4);
					TNFloraBlob(M, FVector(D.X * 200.0, D.Y * 200.0, H * 1.04), Rand(80 + c, 220.0, 280.0), Rand(90 + c, 130.0, 170.0), Seed + 40u + c, LeafAlt);
				}
				TNFloraBlob(M, FVector(0.0, 0.0, H * 1.0), 330.0, 170.0, Seed + 99u, Leaf);
				break;
			}
			case EFloraShape::Palm:
			{
				const double H = Rand(1, 780.0, 980.0);
				const double Lean = Rand(2, 120.0, 260.0);
				TArray<TArray<FVector>> Rings;
				constexpr int32 Stations = 7;
				for (int32 s = 0; s <= Stations; ++s)
				{
					const double T = static_cast<double>(s) / Stations;
					const FVector C(Lean * T * T, 0.0, H * T);
					const double R = FMath::Lerp(22.0, 14.0, T);
					TArray<FVector> Ring;
					for (int32 k = 0; k < 6; ++k)
					{
						const double A = TNProcMap::TwoPi * k / 6.0;
						Ring.Add(C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0));
					}
					Rings.Add(Ring);
				}
				for (int32 s = 0; s < Stations; ++s)
				{
					TArray<TArray<FVector>> Band = { Rings[s], Rings[s + 1] };
					M.AddSweep(Band, true, (s % 2) ? Bark : Bark * 0.78f);
				}
				const FVector Crown(Lean, 0.0, H);
				TNFloraBlob(M, Crown, 28.0, 24.0, Seed + 1u, Bark * 0.7f, 6);
				const int32 NF = 9;
				for (int32 f = 0; f < NF; ++f)
				{
					TNFloraFrond(M, Crown + FVector(0.0, 0.0, 12.0), DirAt(f, NF), Rand(10 + f, 340.0, 440.0), 72.0, 80.0, Rand(20 + f, 220.0, 300.0), (f % 2) ? Leaf : LeafAlt, 5);
				}
				for (int32 c = 0; c < 3; ++c)
				{
					const FVector2D D = DirAt(c + 20, 3);
					TNFloraBead(M, Crown + FVector(D.X * 24.0, D.Y * 24.0, -22.0), 15.0, Seed + 50u + c, FLinearColor(0.36f, 0.3f, 0.12f));
				}
				break;
			}
			case EFloraShape::MangroveTree:
			{
				const double H = Rand(1, 650.0, 820.0);
				const double RootTop = Rand(2, 160.0, 230.0);
				TNFloraLathe(M, FVector(0.0, 0.0, RootTop - 20.0), { 0.0, 150.0, H * 0.62 - RootTop }, { 25.0, 20.0, 14.0 }, 0.08, Seed, Bark, 7, true);
				const int32 NR = 8;
				for (int32 r = 0; r < NR; ++r)
				{
					const FVector2D D = DirAt(r, NR);
					const FVector Start(D.X * 18.0, D.Y * 18.0, RootTop + Rand(10 + r, -20.0, 50.0));
					const FVector End(D.X * Rand(20 + r, 170.0, 260.0), D.Y * Rand(20 + r, 170.0, 260.0), -40.0);
					FVector Prev = Start;
					for (int32 t = 1; t <= 3; ++t)
					{
						const double U = t / 3.0;
						const FVector Pt = FMath::Lerp(Start, End, U) + FVector(0.0, 0.0, 95.0 * FMath::Sin(PI * U));
						M.AddBeam(Prev, Pt, FMath::Lerp(7.5, 5.0, U), Bark * 0.92f);
						Prev = Pt;
					}
				}
				for (int32 b = 0; b < 3; ++b)
				{
					const FVector2D D = DirAt(b + 9, 3);
					M.AddBeam(FVector(0.0, 0.0, H * 0.55), FVector(D.X * 170.0, D.Y * 170.0, H * 0.8), 7.0, Bark);
				}
				for (int32 c = 0; c < 5; ++c)
				{
					const FVector2D D = DirAt(c + 12, 5);
					const double R = Rand(30 + c, 110.0, 220.0);
					TNFloraBlob(M, FVector(D.X * R, D.Y * R, H * Rand(40 + c, 0.82, 0.94)), Rand(50 + c, 200.0, 270.0), Rand(60 + c, 85.0, 115.0), Seed + c, (c % 2) ? Leaf : LeafAlt);
				}
				TNFloraBlob(M, FVector(0.0, 0.0, H * 0.95), 230.0, 110.0, Seed + 99u, Leaf);
				break;
			}
			case EFloraShape::YoungSequoia:
			{
				const double H = Rand(1, 1500.0, 1800.0);
				const FLinearColor Red(0.36f, 0.17f, 0.09f);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 90.0, H * 0.35, H * 0.7, H * 0.9 }, { 85.0, 52.0, 44.0, 30.0, 18.0 }, 0.05, Seed, Red, 8, false);
				for (int32 r = 0; r < 5; ++r)
				{
					const FVector2D D = DirAt(r, 5);
					M.AddBeam(FVector(D.X * 40.0, D.Y * 40.0, 110.0), FVector(D.X * 150.0, D.Y * 150.0, -25.0), 15.0, Red * 0.9f);
				}
				for (int32 c = 0; c < 8; ++c)
				{
					const double U = c / 7.0;
					const double R = FMath::Lerp(420.0, 110.0, U) * Rand(10 + c, 0.85, 1.15);
					TNFloraBlob(M, FVector(TNProcHashNoise(c, 6, Seed) * 40.0, TNProcHashNoise(c, 7, Seed) * 40.0, H * FMath::Lerp(0.32, 0.97, U)), R, R * 0.62, Seed + c, (c % 2) ? Leaf : LeafAlt, 8);
				}
				break;
			}
			case EFloraShape::Cypress:
			{
				const double H = Rand(1, 1150.0, 1400.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 60.0, H * 0.6 }, { 55.0, 30.0, 14.0 }, 0.08, Seed, Bark, 7, false);
				for (int32 k = 0; k < 4; ++k)
				{
					const FVector2D D = DirAt(k, 4);
					const double R = Rand(10 + k, 90.0, 140.0);
					TNFloraLathe(M, FVector(D.X * R, D.Y * R, -10.0), { 0.0, Rand(20 + k, 35.0, 70.0) }, { 12.0, 4.0 }, 0.1, Seed + k, Bark * 0.85f, 5, false);
				}
				TNFloraLathe(M, FVector::ZeroVector, { H * 0.2, H * 0.4, H * 0.62, H * 0.82, H }, { 120.0, 150.0, 125.0, 75.0, 8.0 }, 0.14, Seed + 7u, Leaf, 8, true);
				break;
			}
			case EFloraShape::Pine:
			case EFloraShape::Fir:
			{
				const bool bFir = Shape == EFloraShape::Fir;
				const double H = bFir ? Rand(1, 1200.0, 1450.0) : Rand(1, 1000.0, 1250.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, H * 0.9 }, { bFir ? 20.0 : 24.0, 8.0 }, 0.06, Seed, Bark, 6, false);
				const int32 Tiers = bFir ? 7 : 5;
				for (int32 t = 0; t < Tiers; ++t)
				{
					const double U = static_cast<double>(t) / (Tiers - 1);
					const double Z0 = H * FMath::Lerp(bFir ? 0.12 : 0.22, 0.8, U);
					const double Th = H * (bFir ? 0.2 : 0.28);
					const double R = FMath::Lerp(bFir ? 200.0 : 280.0, bFir ? 45.0 : 75.0, U) * Rand(10 + t, 0.88, 1.12);
					TNFloraLathe(M, FVector::ZeroVector, { Z0, Z0 + Th * 0.25, Z0 + Th }, { R, R * 0.72, 6.0 }, 0.12, Seed + t, Leaf * (0.9f + 0.05f * t), 8, true);
				}
				break;
			}
			case EFloraShape::Willow:
			{
				const double H = Rand(1, 780.0, 900.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 250.0, H * 0.6 }, { 42.0, 32.0, 24.0 }, 0.08, Seed, Bark, 7, false);
				for (int32 b = 0; b < 4; ++b)
				{
					const FVector2D D = DirAt(b, 4);
					M.AddBeam(FVector(0.0, 0.0, H * 0.55), FVector(D.X * 170.0, D.Y * 170.0, H * 0.78), 9.0, Bark);
				}
				TNFloraBlob(M, FVector(0.0, 0.0, H * 0.82), 300.0, 170.0, Seed + 3u, LeafAlt, 8);
				const int32 NS = 18;
				for (int32 s = 0; s < NS; ++s)
				{
					const FVector2D D = DirAt(s + 4, NS);
					const double R = Rand(10 + s, 180.0, 290.0);
					const FVector Top(D.X * R, D.Y * R, H * 0.8 + Rand(30 + s, -40.0, 40.0));
					M.AddBeam(Top, Top + FVector(D.X * 30.0, D.Y * 30.0, -Rand(50 + s, 350.0, 520.0)), 9.0, (s % 2) ? LeafAlt : Leaf * 1.1f);
				}
				break;
			}
			case EFloraShape::Acacia:
			{
				const double H = Rand(1, 600.0, 720.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, H * 0.45 }, { 22.0, 17.0 }, 0.08, Seed, Bark, 6, false);
				for (int32 b = 0; b < 4; ++b)
				{
					const FVector2D D = DirAt(b, 4);
					const double R = Rand(10 + b, 170.0, 240.0);
					M.AddBeam(FVector(0.0, 0.0, H * 0.43), FVector(D.X * R, D.Y * R, H * 0.86), 8.0, Bark);
				}
				for (int32 c = 0; c < 4; ++c)
				{
					const FVector2D D = DirAt(c + 6, 4);
					const double R = Rand(20 + c, 90.0, 180.0);
					TNFloraBlob(M, FVector(D.X * R, D.Y * R, H * Rand(30 + c, 0.88, 0.95)), Rand(40 + c, 220.0, 280.0), Rand(50 + c, 55.0, 70.0), Seed + c, (c % 2) ? Leaf : LeafAlt, 8);
				}
				TNFloraBlob(M, FVector(0.0, 0.0, H * 0.94), 250.0, 60.0, Seed + 99u, Leaf, 8);
				break;
			}
			case EFloraShape::DeadTree:
			case EFloraShape::CharredTree:
			{
				const bool bCharred = Shape == EFloraShape::CharredTree;
				const double H = Rand(1, 600.0, 800.0) * (bCharred ? 0.85 : 1.0);
				const FLinearColor Wood = bCharred ? Pal.Bark : Pal.Dry;
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 150.0, H * 0.7 }, { 30.0, 22.0, bCharred ? 16.0 : 11.0 }, 0.12, Seed, Wood, 7, false);
				TNFloraTwigs(M, bCharred ? 4 : 6, H * 0.4, H * 0.7, bCharred ? 150.0 : 230.0, bCharred ? 8.0 : 7.0, Seed, Wood * 0.95f);
				if (bCharred)
				{
					// Brasas en las grietas del tronco.
					for (int32 e = 0; e < 5; ++e)
					{
						const FVector2D D = DirAt(e, 5);
						const double Z = Rand(10 + e, 40.0, H * 0.6);
						TNFloraBead(M, FVector(D.X * 26.0, D.Y * 26.0, Z), Rand(20 + e, 9.0, 14.0), Seed + e, Pal.Accent);
					}
				}
				break;
			}
			case EFloraShape::Ornamental:
			{
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 300.0 }, { 14.0, 11.0 }, 0.05, Seed, Bark, 6, false);
				TNFloraLathe(M, FVector(0.0, 0.0, -5.0), { 0.0, 60.0 }, { 15.5, 15.0 }, 0.0, Seed, FLinearColor(0.85f, 0.83f, 0.78f), 6, false);
				TNFloraBlob(M, FVector(0.0, 0.0, 420.0), 170.0, 160.0, Seed + 1u, Leaf, 8);
				for (int32 c = 0; c < 2; ++c)
				{
					const FVector2D D = DirAt(c, 2);
					TNFloraBlob(M, FVector(D.X * 90.0, D.Y * 90.0, 380.0 + 40.0 * c), 95.0, 85.0, Seed + 10u + c, LeafAlt, 7);
				}
				break;
			}
			case EFloraShape::Fern:
			{
				const int32 NF = 7 + Variant;
				for (int32 f = 0; f < NF; ++f)
				{
					TNFloraFrond(M, FVector(0.0, 0.0, 8.0), DirAt(f, NF), Rand(10 + f, 90.0, 130.0), Rand(20 + f, 22.0, 30.0), Rand(30 + f, 45.0, 70.0), Rand(40 + f, 60.0, 90.0), (f % 2) ? Leaf : LeafAlt, 4);
				}
				TNFloraBlob(M, FVector(0.0, 0.0, 6.0), 12.0, 8.0, Seed + 1u, Leaf * 0.7f, 5);
				break;
			}
			case EFloraShape::Bush:
			case EFloraShape::AshBush:
			{
				const bool bAsh = Shape == EFloraShape::AshBush;
				const int32 NB = 3 + Variant % 2 + (bAsh ? 0 : 1);
				for (int32 b = 0; b < NB; ++b)
				{
					const FVector2D D = DirAt(b, NB);
					const double R = b == 0 ? 0.0 : Rand(10 + b, 30.0, 60.0);
					TNFloraBlob(M, FVector(D.X * R, D.Y * R, Rand(20 + b, 45.0, 80.0)), Rand(30 + b, 55.0, 90.0), Rand(40 + b, 45.0, 70.0), Seed + b, (b % 2) ? Leaf : LeafAlt, 6);
				}
				if (bAsh) { TNFloraTwigs(M, 4, 20.0, 50.0, 70.0, 2.5, Seed, Pal.Dry); }
				else if (Variant == 2)
				{
					// Bayas o flores sueltas por encima.
					for (int32 k = 0; k < 7; ++k)
					{
						const FVector2D D = DirAt(k + 3, 7);
						TNFloraBead(M, FVector(D.X * Rand(50 + k, 30.0, 75.0), D.Y * Rand(50 + k, 30.0, 75.0), Rand(60 + k, 85.0, 125.0)), 8.0, Seed + 20u + k, Pal.Accent);
					}
				}
				break;
			}
			case EFloraShape::Grass:
			case EFloraShape::Flowers:
			{
				const bool bFlowers = Shape == EFloraShape::Flowers;
				const int32 NB = bFlowers ? 8 : 11;
				for (int32 b = 0; b < NB; ++b)
				{
					const FVector2D D = DirAt(b, NB);
					const double R = Rand(10 + b, 3.0, 20.0);
					TNFloraBlade(M, FVector(D.X * R, D.Y * R, -4.0), D, Rand(20 + b, 45.0, 88.0), Rand(30 + b, 7.0, 10.0), Rand(40 + b, 12.0, 32.0), Grass * (0.88f + 0.24f * static_cast<float>(0.5 + 0.5 * TNProcHashNoise(b, 9, Seed))));
				}
				if (bFlowers)
				{
					static const FLinearColor Petals[3] = { FLinearColor(0.9f, 0.18f, 0.2f), FLinearColor(0.97f, 0.82f, 0.2f), FLinearColor(0.7f, 0.45f, 0.95f) };
					for (int32 k = 0; k < 5; ++k)
					{
						const FVector2D D = DirAt(k + 13, 5);
						const FVector Head(D.X * Rand(50 + k, 8.0, 28.0), D.Y * Rand(50 + k, 8.0, 28.0), Rand(60 + k, 35.0, 58.0));
						M.AddBeam(FVector(Head.X * 0.5, Head.Y * 0.5, 0.0), Head, 1.3, Grass * 0.8f);
						TNFloraBead(M, Head, 8.5, Seed + 30u + k, Petals[(Variant + k) % 3]);
					}
				}
				break;
			}
			case EFloraShape::Reeds:
			{
				const FLinearColor Stalk = TNProcLerpColor(Grass, FLinearColor(0.42f, 0.5f, 0.22f), 0.5f);
				for (int32 s = 0; s < 14; ++s)
				{
					const FVector2D D = DirAt(s, 14);
					const double R = Rand(10 + s, 2.0, 26.0);
					const double Ht = Rand(20 + s, 150.0, 250.0);
					const FVector Top(D.X * (R + Rand(30 + s, 8.0, 25.0)), D.Y * (R + Rand(30 + s, 8.0, 25.0)), Ht);
					M.AddBeam(FVector(D.X * R, D.Y * R, -10.0), Top, 2.2, Stalk * (0.9f + 0.1f * (s % 3)));
					if (s % 3 == 0)
					{
						TNFloraBlob(M, Top - FVector(0.0, 0.0, 24.0), 4.5, 14.0, Seed + s, FLinearColor(0.3f, 0.19f, 0.1f), 5);
					}
				}
				for (int32 b = 0; b < 5; ++b)
				{
					const FVector2D D = DirAt(b + 20, 5);
					TNFloraBlade(M, FVector(D.X * 8.0, D.Y * 8.0, -8.0), D, Rand(40 + b, 100.0, 140.0), 6.0, Rand(50 + b, 25.0, 45.0), Stalk);
				}
				break;
			}
			case EFloraShape::Saguaro:
			{
				const double H = Rand(1, 460.0, 620.0);
				const FLinearColor Cactus = TNFloraVary(FLinearColor(0.2f, 0.4f, 0.17f), Variant);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 40.0, H * 0.8, H * 0.95, H }, { 36.0, 40.0, 38.0, 30.0, 12.0 }, 0.03, Seed, Cactus, 10, false, 0.86);
				const int32 Arms = 1 + Variant;
				for (int32 a = 0; a < Arms; ++a)
				{
					const FVector2D D = DirAt(a + 2, Arms);
					const double Z = H * Rand(10 + a, 0.3, 0.5);
					const double Out = Rand(20 + a, 70.0, 95.0);
					M.AddBeam(FVector(D.X * 25.0, D.Y * 25.0, Z), FVector(D.X * Out, D.Y * Out, Z + 25.0), 17.0, Cactus * 0.95f);
					TNFloraLathe(M, FVector(D.X * Out, D.Y * Out, Z + 10.0), { 0.0, Rand(30 + a, 120.0, 220.0) }, { 21.0, 15.0 }, 0.03, Seed + a, Cactus, 8, false, 0.86);
				}
				if (Variant == 1)
				{
					for (int32 k = 0; k < 3; ++k)
					{
						const FVector2D D = DirAt(k, 3);
						TNFloraBead(M, FVector(D.X * 12.0, D.Y * 12.0, H + 2.0), 8.0, Seed + 40u + k, FLinearColor(0.95f, 0.93f, 0.85f));
					}
				}
				break;
			}
			case EFloraShape::Barrel:
			{
				const FLinearColor Cactus = TNFloraVary(FLinearColor(0.22f, 0.42f, 0.18f), Variant);
				TNFloraLathe(M, FVector(0.0, 0.0, -5.0), { 0.0, 20.0, 50.0, 70.0 }, { 34.0, 42.0, 36.0, 16.0 }, 0.03, Seed, Cactus, 12, false, 0.84);
				TNFloraBlob(M, FVector(0.0, 0.0, 70.0), 11.0, 7.0, Seed + 1u, Pal.Accent, 6);
				break;
			}
			case EFloraShape::DryBush:
			{
				TNFloraTwigs(M, 10, 4.0, 20.0, 60.0, 2.6, Seed, TNFloraVary(Pal.Dry, Variant));
				TNFloraBlob(M, FVector(0.0, 0.0, 10.0), 22.0, 12.0, Seed + 1u, Pal.Dry * 0.8f, 5);
				break;
			}
			case EFloraShape::Hedge:
			{
				M.AddBox(FVector(0.0, 0.0, 50.0), FVector(1.0, 0.0, 0.0), FVector(110.0, 45.0, 55.0), Leaf);
				for (int32 k = 0; k < 3; ++k)
				{
					TNFloraBlob(M, FVector(-70.0 + 70.0 * k, 0.0, 105.0), 55.0, 22.0, Seed + k, LeafAlt, 6);
				}
				break;
			}
			case EFloraShape::Umbrella:
			{
				static const FLinearColor Cloth[3] = { FLinearColor(0.85f, 0.15f, 0.12f), FLinearColor(0.1f, 0.35f, 0.8f), FLinearColor(0.95f, 0.7f, 0.1f) };
				M.AddBox(FVector(0.0, 0.0, 8.0), FVector(1.0, 0.0, 0.0), FVector(22.0, 22.0, 8.0), FLinearColor(0.4f, 0.4f, 0.42f));
				M.AddBeam(FVector(0.0, 0.0, 0.0), FVector(0.0, 0.0, 240.0), 3.5, FLinearColor(0.9f, 0.9f, 0.88f));
				const FVector Apex(0.0, 0.0, 250.0);
				const int32 NS = 8;
				for (int32 s = 0; s < NS; ++s)
				{
					const double A0 = TNProcMap::TwoPi * s / NS, A1 = TNProcMap::TwoPi * (s + 1) / NS;
					const FVector P0(FMath::Cos(A0) * 150.0, FMath::Sin(A0) * 150.0, 205.0);
					const FVector P1(FMath::Cos(A1) * 150.0, FMath::Sin(A1) * 150.0, 205.0);
					const FLinearColor C = (s % 2) ? FLinearColor(0.95f, 0.95f, 0.92f) : Cloth[Variant % 3];
					M.AddTri(Apex, P0, P1, FVector::UpVector, C);
					M.AddTri(Apex, P1, P0, -FVector::UpVector, C * 0.7f);
				}
				break;
			}
			case EFloraShape::Creeper:
			{
				// Manta de hojas aplastada (la instancia la pega a la pared con su normal).
				const FLinearColor Moss = TNFloraVary(Pal.Moss, Variant);
				const int32 NB = 6 + Variant;
				for (int32 b = 0; b < NB; ++b)
				{
					const FVector2D D = DirAt(b, NB);
					const double R = b == 0 ? 0.0 : Rand(10 + b, 25.0, 70.0);
					TNFloraBlob(M, FVector(D.X * R, D.Y * R, 3.0), Rand(20 + b, 30.0, 55.0), Rand(30 + b, 7.0, 12.0), Seed + b, (b % 2) ? Moss : Moss * 1.15f, 6);
				}
				break;
			}
			case EFloraShape::BananaPlant:
			{
				// Platanera: pseudotallo y hojas enormes en pala, algunas rasgadas; racimo en una variante.
				const double H = Rand(1, 190.0, 240.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, H * 0.5, H }, { 20.0, 16.0, 12.0 }, 0.06, Seed, TNProcLerpColor(Pal.Bark, LeafAlt, 0.55f), 7, false);
				const int32 NL = 7 + Variant;
				for (int32 l = 0; l < NL; ++l)
				{
					const FLinearColor C = (l % 3 == 0) ? LeafAlt * 1.12f : Leaf * 1.05f;
					TNFloraFrond(M, FVector(0.0, 0.0, H - 10.0 + 12.0 * (l % 2)), DirAt(l, NL), Rand(10 + l, 170.0, 230.0), Rand(20 + l, 58.0, 72.0), Rand(30 + l, 50.0, 90.0), Rand(40 + l, 90.0, 150.0), C, 5);
				}
				if (Variant == 2)
				{
					for (int32 b = 0; b < 6; ++b)
					{
						TNFloraBead(M, FVector(24.0 + 5.0 * (b % 2), 8.0 * (b % 3), H - 40.0 - 12.0 * b), 10.0, Seed + 30u + b, FLinearColor(0.85f, 0.75f, 0.15f));
					}
				}
				break;
			}
			case EFloraShape::Bamboo:
			{
				// Mata de bambú: cañas finas con nudos, algo inclinadas, y penachos de hojas arriba.
				const FLinearColor Cane = TNFloraVary(FLinearColor(0.42f, 0.52f, 0.18f), Variant);
				const int32 NC = 6 + 2 * Variant;
				for (int32 c = 0; c < NC; ++c)
				{
					const FVector2D D = DirAt(c, NC);
					const double R0 = Rand(10 + c, 5.0, 45.0);
					const double Ht = Rand(20 + c, 620.0, 1050.0);
					const FVector Base(D.X * R0, D.Y * R0, -20.0);
					const FVector Top = Base + FVector(D.X * Ht * 0.08, D.Y * Ht * 0.08, Ht);
					for (int32 s = 0; s < 4; ++s)
					{
						const FVector A = FMath::Lerp(Base, Top, s / 4.0);
						const FVector B = FMath::Lerp(Base, Top, (s + 1) / 4.0);
						TNProcAddCylinder(M, A, B - (B - A).GetSafeNormal() * 6.0, 6.0, 5.5, 5, s % 2 ? Cane : Cane * 0.88f, false);
						TNProcAddCylinder(M, B - (B - A).GetSafeNormal() * 6.0, B, 7.0, 7.0, 5, Cane * 0.7f, false);
					}
					for (int32 l = 0; l < 3; ++l)
					{
						TNFloraFrond(M, FMath::Lerp(Base, Top, 0.7 + 0.1 * l), DirAt(c * 3 + l, NC * 3), Rand(40 + c * 3 + l, 70.0, 110.0), 16.0, 20.0, 45.0, (l % 2) ? Leaf : LeafAlt, 3);
					}
				}
				break;
			}
			case EFloraShape::TreeFern:
			{
				// Helecho arbóreo: tronco fibroso y una corona de frondas enormes que se arquean.
				const double H = Rand(1, 280.0, 420.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, 40.0, H }, { 26.0, 19.0, 16.0 }, 0.14, Seed, Pal.Bark * 0.8f, 7, false);
				const int32 NF = 9 + Variant;
				for (int32 f = 0; f < NF; ++f)
				{
					TNFloraFrond(M, FVector(0.0, 0.0, H), DirAt(f, NF), Rand(10 + f, 190.0, 250.0), Rand(20 + f, 40.0, 52.0), Rand(30 + f, 60.0, 90.0), Rand(40 + f, 130.0, 190.0), (f % 2) ? Leaf : LeafAlt, 5);
				}
				TNFloraBlob(M, FVector(0.0, 0.0, H + 5.0), 22.0, 14.0, Seed + 3u, Leaf * 0.7f, 6);
				break;
			}
			case EFloraShape::SeaGrape:
			{
				// Uva de playa: varios troncos cortos retorcidos y copa baja de hojas redondas grandes.
				const double H = Rand(1, 220.0, 320.0);
				for (int32 t = 0; t < 3; ++t)
				{
					const FVector2D D = DirAt(t, 3);
					M.AddBeam(FVector(D.X * 15.0, D.Y * 15.0, -10.0), FVector(D.X * Rand(10 + t, 60.0, 110.0), D.Y * Rand(10 + t, 60.0, 110.0), H * 0.6), 8.0, Pal.Bark * 0.9f);
				}
				const int32 NB = 6 + Variant;
				for (int32 b = 0; b < NB; ++b)
				{
					const FVector2D D = DirAt(b + 5, NB);
					const double R = b == 0 ? 0.0 : Rand(20 + b, 50.0, 120.0);
					const FLinearColor C = (b % 4 == 3) ? FLinearColor(0.55f, 0.25f, 0.12f) : ((b % 2) ? LeafAlt : Leaf);
					TNFloraBlob(M, FVector(D.X * R, D.Y * R, H * Rand(30 + b, 0.6, 0.9)), Rand(40 + b, 70.0, 110.0), Rand(50 + b, 45.0, 65.0), Seed + b, C, 7);
				}
				break;
			}
			case EFloraShape::Pandanus:
			{
				// Pándano: raíces zancudas bajo un tronco inclinado que se ramifica en penachos de hojas
				// largas y afiladas, con algún fruto.
				const double H = Rand(1, 420.0, 560.0);
				const FVector TrunkBase(0.0, 0.0, 110.0);
				for (int32 r = 0; r < 5; ++r)
				{
					const FVector2D D = DirAt(r, 5);
					M.AddBeam(TrunkBase + FVector(D.X * 8.0, D.Y * 8.0, 0.0), FVector(D.X * Rand(10 + r, 70.0, 110.0), D.Y * Rand(10 + r, 70.0, 110.0), -20.0), 5.5, Pal.Bark * 0.85f);
				}
				const FVector Fork = TrunkBase + FVector(Rand(2, -60.0, 60.0), Rand(3, -60.0, 60.0), H * 0.55);
				TNProcAddCylinder(M, TrunkBase, Fork, 14.0, 11.0, 6, Pal.Bark, false);
				const int32 Branches = 2 + Variant % 2;
				for (int32 b = 0; b < Branches; ++b)
				{
					const FVector2D D = DirAt(b + 7, Branches);
					const FVector Tip = Fork + FVector(D.X * Rand(20 + b, 90.0, 150.0), D.Y * Rand(20 + b, 90.0, 150.0), Rand(30 + b, 70.0, 140.0));
					TNProcAddCylinder(M, Fork, Tip, 9.0, 7.0, 5, Pal.Bark * 0.95f, false);
					for (int32 l = 0; l < 9; ++l)
					{
						TNFloraFrond(M, Tip, DirAt(l + b * 9, 9), Rand(40 + l + b * 9, 150.0, 210.0), 18.0, Rand(50 + l, 40.0, 70.0), Rand(60 + l, 50.0, 110.0), (l % 2) ? Leaf : LeafAlt, 3);
					}
					if (b == 0) { TNFloraBead(M, Tip - FVector(0.0, 0.0, 22.0), 16.0, Seed + 70u, FLinearColor(0.85f, 0.45f, 0.1f)); }
				}
				break;
			}
			case EFloraShape::FanPalm:
			{
				// Palmito: tronco corto y hojas en abanico sobre pecíolos.
				const double H = Rand(1, 70.0, 150.0);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, H }, { 22.0, 17.0 }, 0.12, Seed, Pal.Bark * 0.85f, 7, false);
				const int32 NL = 7 + Variant;
				for (int32 l = 0; l < NL; ++l)
				{
					const FVector2D D = DirAt(l, NL);
					const FVector Stalk = FVector(0.0, 0.0, H) + FVector(D.X * Rand(10 + l, 60.0, 90.0), D.Y * Rand(10 + l, 60.0, 90.0), Rand(20 + l, 50.0, 110.0));
					M.AddBeam(FVector(0.0, 0.0, H), Stalk, 2.5, Leaf * 0.8f);
					// Abanico: cinco cuñas finas desde la punta del pecíolo.
					const FVector Out = FVector(D.X, D.Y, 0.6).GetSafeNormal();
					const FVector Side = FVector(-D.Y, D.X, 0.0);
					for (int32 w = 0; w < 5; ++w)
					{
						const double A0 = -0.9 + 0.36 * w, A1 = A0 + 0.36;
						const FVector P0 = Stalk + (Out * FMath::Cos(A0) + Side * FMath::Sin(A0)) * 70.0;
						const FVector P1 = Stalk + (Out * FMath::Cos(A1) + Side * FMath::Sin(A1)) * 70.0;
						const FVector Nrm = FVector::CrossProduct(P0 - Stalk, P1 - Stalk);
						const FLinearColor C = (w % 2) ? Leaf : LeafAlt;
						M.AddTri(Stalk, P0, P1, Nrm, C);
						M.AddTri(Stalk, P1, P0, -Nrm, C * 0.8f);
					}
				}
				break;
			}
			case EFloraShape::Casuarina:
			{
				// Casuarina: tronco esbelto y copa de ramillas finas colgantes, verde azulada.
				const double H = Rand(1, 850.0, 1150.0);
				const FLinearColor Needle = TNFloraVary(FLinearColor(0.16f, 0.3f, 0.22f), Variant);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, H * 0.4, H * 0.85 }, { 22.0, 16.0, 8.0 }, 0.08, Seed, Pal.Bark, 6, false);
				for (int32 c = 0; c < 7; ++c)
				{
					const double T = FMath::Lerp(0.35, 0.95, c / 6.0);
					const double R = FMath::Lerp(200.0, 70.0, T) * Rand(10 + c, 0.85, 1.15);
					const FVector2D D = DirAt(c, 7);
					TNFloraBlob(M, FVector(D.X * R * 0.35, D.Y * R * 0.35, H * T), R, R * 0.55, Seed + c, (c % 2) ? Needle : Needle * 1.12f, 7);
				}
				for (int32 s = 0; s < 10; ++s)
				{
					const FVector2D D = DirAt(s + 3, 10);
					const FVector Top(D.X * Rand(40 + s, 110.0, 180.0), D.Y * Rand(40 + s, 110.0, 180.0), H * Rand(50 + s, 0.4, 0.75));
					M.AddBeam(Top, Top + FVector(D.X * 15.0, D.Y * 15.0, -Rand(60 + s, 120.0, 200.0)), 6.0, Needle * 0.9f);
				}
				break;
			}
			case EFloraShape::JoshuaTree:
			{
				// Árbol de Josué: tronco peludo que se bifurca y penachos de hojas en punta al final de cada rama.
				const double H = Rand(1, 420.0, 600.0);
				const FLinearColor Shaggy(0.38f, 0.3f, 0.2f);
				const FLinearColor Spike = TNFloraVary(FLinearColor(0.33f, 0.42f, 0.16f), Variant);
				const FVector Fork(0.0, 0.0, H * 0.45);
				TNFloraLathe(M, FVector::ZeroVector, { 0.0, H * 0.45 }, { 24.0, 18.0 }, 0.18, Seed, Shaggy, 7, false);
				const int32 Arms = 3 + Variant;
				for (int32 a = 0; a < Arms; ++a)
				{
					const FVector2D D = DirAt(a, Arms);
					const FVector Mid = Fork + FVector(D.X * Rand(10 + a, 60.0, 110.0), D.Y * Rand(10 + a, 60.0, 110.0), Rand(20 + a, 80.0, 140.0));
					const FVector Tip = Mid + FVector(D.X * Rand(30 + a, 30.0, 80.0), D.Y * Rand(30 + a, 30.0, 80.0), Rand(40 + a, 60.0, 120.0));
					TNProcAddCylinder(M, Fork, Mid, 14.0, 11.0, 6, Shaggy * 0.95f, false);
					TNProcAddCylinder(M, Mid, Tip, 11.0, 9.0, 6, Shaggy, false);
					for (int32 s = 0; s < 10; ++s)
					{
						const double A = TNProcMap::TwoPi * s / 10.0;
						const FVector Out = FVector(FMath::Cos(A), FMath::Sin(A), 0.9 + 0.5 * TNProcHashNoise(a, s, Seed)).GetSafeNormal();
						M.AddBeam(Tip, Tip + Out * Rand(50 + s + a * 10, 55.0, 85.0), 4.5, (s % 2) ? Spike : Spike * 1.15f);
					}
				}
				break;
			}
			case EFloraShape::Birch:
			{
				// Abedul: tronco blanco con marcas negras y copa ligera de hojas pequeñas (dorada en otoño).
				const double H = Rand(1, 850.0, 1150.0);
				const FLinearColor White(0.86f, 0.84f, 0.78f);
				constexpr int32 Bands = 6;
				for (int32 b = 0; b < Bands; ++b)
				{
					const double Z0 = H * 0.75 * b / Bands, Z1 = H * 0.75 * (b + 1) / Bands;
					const double R0 = FMath::Lerp(22.0, 9.0, static_cast<double>(b) / Bands), R1 = FMath::Lerp(22.0, 9.0, static_cast<double>(b + 1) / Bands);
					TNFloraLathe(M, FVector(0.0, 0.0, Z0), { 0.0, Z1 - Z0 }, { R0, R1 }, 0.05, Seed + b, (b % 2) ? White : White * 0.35f + FLinearColor(0.05f, 0.05f, 0.05f), 6, false);
				}
				const FLinearColor Foliage = Variant == 2 ? FLinearColor(0.75f, 0.55f, 0.12f) : LeafAlt * 1.1f;
				for (int32 c = 0; c < 6; ++c)
				{
					const FVector2D D = DirAt(c, 6);
					const double R = Rand(10 + c, 60.0, 130.0);
					TNFloraBlob(M, FVector(D.X * R, D.Y * R, H * Rand(20 + c, 0.6, 0.95)), Rand(30 + c, 90.0, 140.0), Rand(40 + c, 110.0, 170.0), Seed + c, (c % 2) ? Foliage : Foliage * 0.88f, 7);
				}
				break;
			}
			case EFloraShape::Rock:
			{
				const double Ht = Variant == 1 ? 55.0 : (Variant == 2 ? 130.0 : 90.0);
				TNProcAddBoulder(M, FVector::ZeroVector, 100.0, Ht, Seed, Pal.Rock);
				if (Variant == 1) { TNProcAddBoulder(M, FVector(95.0, 40.0, 0.0), 45.0, 40.0, Seed + 1u, Pal.Rock * 0.9f); }
				break;
			}
			case EFloraShape::Stones:
			{
				const int32 NS = 4 + Variant;
				for (int32 k = 0; k < NS; ++k)
				{
					const FVector2D D = DirAt(k, NS);
					const double R = k == 0 ? 0.0 : Rand(10 + k, 18.0, 40.0);
					const double Sz = Rand(20 + k, 12.0, 28.0);
					const double Ht = Sz * Rand(30 + k, 0.55, 0.95);
					TNFloraLathe(M, FVector(D.X * R, D.Y * R, 0.0), { -4.0, Ht * 0.5, Ht }, { Sz, Sz * 0.9, Sz * 0.45 }, 0.25, Seed + k, Pal.Rock * (0.85f + 0.1f * (k % 3)), 6, false);
				}
				break;
			}
			default:
				break;
		}
		// Hojas finas (hierba, flores, juncos, helechos): normales casi hacia arriba, así se iluminan
		// como el suelo y no salen negras vistas de canto.
		if (Shape == EFloraShape::Grass || Shape == EFloraShape::Flowers || Shape == EFloraShape::Reeds || Shape == EFloraShape::Fern)
		{
			for (FVector& N : M.Normals) { N = (N * 0.3 + FVector::UpVector * 0.7).GetSafeNormal(); }
		}
	}

	/** Cómo se ve cada forma: distancia de culling (cm, 0 = nunca), sombra y si su altura varía aparte. */
	struct FTNFloraLook
	{
		float Cull = 0.f;
		bool bShadow = true;
		bool bStretch = true;
	};

	inline FTNFloraLook TNFloraLookOf(EFloraShape Shape)
	{
		switch (Shape)
		{
			case EFloraShape::Grass:
			case EFloraShape::Flowers:  return { 5000.f, false, true };
			case EFloraShape::Stones:   return { 6000.f, false, false };
			case EFloraShape::Reeds:    return { 7000.f, false, true };
			case EFloraShape::Creeper:  return { 7000.f, false, false };
			case EFloraShape::BananaPlant:
			case EFloraShape::FanPalm:
			case EFloraShape::SeaGrape: return { 30000.f, true, true };
			case EFloraShape::Fern:
			case EFloraShape::DryBush:  return { 8000.f, false, true };
			case EFloraShape::Bush:
			case EFloraShape::AshBush:
			case EFloraShape::Barrel:   return { 10000.f, false, true };
			case EFloraShape::Hedge:
			case EFloraShape::Umbrella: return { 15000.f, true, false };
			case EFloraShape::Rock:     return { 30000.f, true, false };
			case EFloraShape::Prop:     return { 9000.f, true, false };   // cada prop lleva la suya (TNPropLookOf)
			case EFloraShape::Ceiba:
			case EFloraShape::YoungSequoia: return { 150000.f, true, true };   // siluetas del horizonte
			default:                    return { 90000.f, true, true };   // árboles: se ven desde lejos
		}
	}
}
