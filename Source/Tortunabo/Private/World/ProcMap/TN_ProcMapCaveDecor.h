#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapCaves.h"
#include "TN_ProcMapCaveMeshes.h"
#include "TN_ProcMapPropMeshes.h"
#include "TN_ProcMapRockMeshes.h"
#include "../../Art/TN_ArtPieces.h"

/**
 * Interior y bocas de las cuevas (low-poly de caras planas con color de vértice), según su estilo:
 * - Caliza (rocoso): estalagmitas, columnas que unen suelo y techo, estalactitas grandes, poza con
 *   borde de piedras, lucernario con su haz de luz y pinturas ocres en paredes y estelas.
 * - Cristales (rocoso): racimos de cristales gigantes que brillan, con luces de su color.
 * - Selva: raíces que bajan de la bóveda por las paredes, lianas, setas que brillan, musgo, símbolos
 *   verdes que brillan y cortina de lianas en las bocas.
 * - Templo (desierto): pilares de arenisca hasta la bóveda, pilastras, antorchas, vasijas y huesos,
 *   estelas grabadas y portada tallada con la cabeza de la tortuga en las bocas.
 * - Tubo de lava (volcán): esquirlas de obsidiana, grietas incandescentes junto a las paredes y
 *   columnas de basalto en las bocas (el río de lava de la cámara lo pone el hueco).
 * Todas llevan la estatua de la tortuga (con ofrendas) en la cámara más ancha y símbolos en las
 * paredes. Nada invade el carril central: lo del suelo va contra las paredes (en las cámaras, en su
 * franja de fuera) y lo que cuelga de la bóveda no baja de 2,6 m sobre el suelo.
 */
namespace TNCaveDecor
{
	using namespace TNProcMesh;
	using namespace TNCaveMesh;
	using TNProcMap::LerpD;

	enum class ECaveStyle : uint8 { Limestone, Crystal, Jungle, Temple, LavaTube };

	inline ECaveStyle TNCaveStyleFor(ETNProcBiome Biome, uint32 Seed)
	{
		switch (Biome)
		{
			case ETNProcBiome::Jungle:   return ECaveStyle::Jungle;
			case ETNProcBiome::Desert:   return ECaveStyle::Temple;
			case ETNProcBiome::Volcanic: return ECaveStyle::LavaTube;
			default:                     return (Seed % 5u) < 2u ? ECaveStyle::Crystal : ECaveStyle::Limestone;
		}
	}

	/** Luz sin sombras que pone el actor: puntual, o foco hacia abajo (lucernario). */
	struct FTNCaveLight
	{
		FVector P = FVector::ZeroVector;
		FLinearColor Color = FLinearColor::White;
		float Lumens = 1500.f;
		float Radius = 1500.f;
		bool bSpot = false;
	};

	/** Lago de magma de una cueva que atraviesa un volcán (TNProcMap::BuildCaveVolcanoes). */
	struct FTNCaveMagma
	{
		FVector Center = FVector::ZeroVector;
		double Radius = 250.0;
		/** Lado del camino (respecto a su normal izquierda) en el que está. */
		double Side = 1.0;
	};

	/**
	 * Lo que sale de una cueva, por material: se añade al final de los buffers de las secciones del mapa (los de
	 * ATN_ProcMapGenerator::BuildStructures), así cada objeto de la decoración se puede marcar como pieza de arte en ellos.
	 */
	struct FTNCaveDecorOut
	{
		FTNCaveDecorOut(FTNProcMeshBuffers& InSolid, FTNProcMeshBuffers& InDetail, FTNProcMeshBuffers& InGlow, FTNProcMeshBuffers& InEmber,
			FTNProcMeshBuffers& InBeam, FTNProcMeshBuffers& InWater)
			: Solid(InSolid), Detail(InDetail), Glow(InGlow), Ember(InEmber), Beam(InBeam), Water(InWater)
		{
		}

		FTNProcMeshBuffers& Solid;   ///< Con colisión: estatua, columnas, pilares, estalagmitas, estelas, peñascos.
		FTNProcMeshBuffers& Detail;  ///< Sin colisión: símbolos, raíces, lianas, huesos, vasijas, musgo, piedras sueltas.
		FTNProcMeshBuffers& Glow;    ///< Emisivo del color del vértice: setas, cristales, ojos, llamas, cielo del lucernario.
		FTNProcMeshBuffers& Ember;   ///< Material de lava: grietas incandescentes.
		FTNProcMeshBuffers& Beam;    ///< Translúcido: haz del lucernario (opacidad en el alfa del vértice).
		FTNProcMeshBuffers& Water;   ///< Pozas.
		TArray<FTNCaveLight> Lights;
		TArray<FVector> Flames;     ///< Antorchas: brasas que suben.
		TArray<FVector> Motes;      ///< Polvo que flota en el haz del lucernario.
		/** Registro de las piezas de arte (Docs/Arte_Assets.md) en el que se marca cada objeto; nulo, nada. */
		TNArt::FPieceLog* Log = nullptr;
	};

	/** Colores de un estilo. */
	struct FTNCavePalette
	{
		FLinearColor Formation;   ///< Estalagmitas, columnas, pilares.
		FLinearColor Statue;      ///< Piedra de la estatua y de las estelas.
		FLinearColor Accent;      ///< Escudos del caparazón, incrustaciones, corona.
		FLinearColor Paint;       ///< Símbolos.
		bool bGlowPaint = false;  ///< Los símbolos brillan (van en Glow).
		FLinearColor Glow;        ///< Setas, cristales, ojos de la estatua.
		FLinearColor Light;       ///< Luces del túnel.
		float Lumens = 1500.f;
	};

	inline FTNCavePalette TNCavePaletteFor(ECaveStyle Style, const FTNCaveLook& Look, uint32 Seed)
	{
		FTNCavePalette P;
		switch (Style)
		{
			case ECaveStyle::Crystal:
			{
				const FLinearColor Hues[3] = { FLinearColor(0.25f, 0.85f, 1.f), FLinearColor(0.72f, 0.38f, 1.f), FLinearColor(1.f, 0.42f, 0.72f) };
				P.Glow = Hues[(Seed / 7u) % 3u];
				P.Formation = Look.Inner * 1.5f;
				P.Statue = FLinearColor(0.46f, 0.48f, 0.56f);
				P.Accent = P.Glow;
				P.Paint = P.Glow;
				P.bGlowPaint = true;
				P.Light = TNProcLerpColor(P.Glow, FLinearColor::White, 0.25f);
				P.Lumens = 1300.f;
				break;
			}
			case ECaveStyle::Jungle:
				P.Formation = FLinearColor(0.36f, 0.38f, 0.28f);
				P.Statue = FLinearColor(0.3f, 0.46f, 0.38f);
				P.Accent = FLinearColor(0.5f, 0.66f, 0.3f);
				P.Paint = FLinearColor(0.35f, 1.f, 0.55f);
				P.bGlowPaint = true;
				P.Glow = FLinearColor(0.3f, 1.f, 0.7f);
				P.Light = FLinearColor(0.5f, 1.f, 0.72f);
				P.Lumens = 1200.f;
				break;
			case ECaveStyle::Temple:
				P.Formation = FLinearColor(0.8f, 0.62f, 0.4f);
				P.Statue = FLinearColor(0.78f, 0.6f, 0.38f);
				P.Accent = FLinearColor(1.f, 0.76f, 0.2f);
				P.Paint = FLinearColor(0.42f, 0.26f, 0.14f);
				P.Glow = FLinearColor(1.f, 0.6f, 0.18f);
				P.Light = FLinearColor(1.f, 0.64f, 0.34f);
				P.Lumens = 900.f;
				break;
			case ECaveStyle::LavaTube:
				P.Formation = FLinearColor(0.06f, 0.055f, 0.07f);
				P.Statue = FLinearColor(0.09f, 0.08f, 0.1f);
				P.Accent = FLinearColor(0.16f, 0.14f, 0.17f);
				P.Paint = FLinearColor(1.f, 0.42f, 0.08f);
				P.bGlowPaint = true;
				P.Glow = FLinearColor(1.f, 0.45f, 0.1f);
				P.Light = FLinearColor(1.f, 0.45f, 0.2f);
				P.Lumens = 2500.f;
				break;
			default:
				P.Formation = FLinearColor(0.74f, 0.7f, 0.62f);
				P.Statue = FLinearColor(0.6f, 0.58f, 0.54f);
				P.Accent = FLinearColor(0.42f, 0.5f, 0.48f);
				P.Paint = FLinearColor(0.7f, 0.3f, 0.12f);
				P.Glow = FLinearColor(0.55f, 0.85f, 1.f);
				P.Light = FLinearColor(0.72f, 0.8f, 1.f);
				P.Lumens = 1500.f;
				break;
		}
		return P;
	}

	namespace CaveDecorDetail
	{
		/** Aleatorio estable y barato (xorshift) para colocar la decoración. */
		struct FDecorRng
		{
			uint32 S;
			explicit FDecorRng(uint32 Seed) : S(Seed * 2654435761u + 0x9E3779B9u) { if (S == 0u) { S = 1u; } }
			uint32 Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
			double Unit() { return static_cast<double>(Next() & 0xFFFFFFu) / 16777215.0; }
			double Range(double A, double B) { return A + (B - A) * Unit(); }
			int32 RangeInt(int32 A, int32 B) { return A + static_cast<int32>(Next() % static_cast<uint32>(B - A + 1)); }
			bool Chance(double P) { return Unit() < P; }
			double Sign() { return Chance(0.5) ? 1.0 : -1.0; }
		};

		/** Sección del túnel en una estación: ejes y medidas de la bóveda (como TNCaveBuildRoof, sin el ruido). */
		struct FFrame
		{
			FVector F = FVector::ZeroVector;   ///< Suelo en el eje.
			FVector D = FVector::ForwardVector; ///< A lo largo del camino.
			FVector N = FVector(0.0, 1.0, 0.0); ///< Normal izquierda.
			double Hw = 300.0;                  ///< Medio ancho del suelo.
			double Cl = 500.0;                  ///< Altura libre en la clave.
			double Spring = 250.0;              ///< Arranque de la bóveda.

			FVector At(double Y, double Z, double X = 0.0) const { return F + N * Y + D * X + FVector(0.0, 0.0, Z); }

			/** Altura de la cara de dentro de la bóveda a la distancia Y del eje (la roca puede entrar hasta 55 cm). */
			double RoofZ(double Y) const
			{
				const double C = FMath::Clamp(FMath::Abs(Y) / (Hw + 45.0), 0.0, 1.0);
				return Spring + FMath::Sqrt(1.0 - C * C) * (Cl - Spring);
			}
		};

		inline double YawOf(const FVector& Face) { return FMath::RadiansToDegrees(FMath::Atan2(Face.Y, Face.X)); }
	}

	// ── Símbolos ─────────────────────────────────────────────────────────────────

	enum class EGlyph : uint8 { Turtle, Spiral, Sun, Waves, Eye, Honeycomb, Count };

	/** Trazos del símbolo G como polilíneas en [-1, 1]² (V arriba). */
	inline void TNGlyphStrokes(EGlyph G, TArray<TArray<FVector2D>>& Out)
	{
		auto Ring = [&Out](const FVector2D& C, double Rx, double Ry, int32 N, double Phase = 0.0)
		{
			TArray<FVector2D> S;
			for (int32 k = 0; k <= N; ++k)
			{
				const double A = Phase + TNProcMap::TwoPi * k / N;
				S.Add(C + FVector2D(FMath::Cos(A) * Rx, FMath::Sin(A) * Ry));
			}
			Out.Add(S);
		};
		auto Line = [&Out](const FVector2D& A, const FVector2D& B) { Out.Add({ A, B }); };
		switch (G)
		{
			case EGlyph::Turtle:
				// Caparazón con su escudo central, cabeza arriba, cuatro aletas y cola.
				Ring(FVector2D(0.0, -0.08), 0.46, 0.56, 14);
				Ring(FVector2D(0.0, -0.08), 0.2, 0.22, 6, PI / 6.0);
				Ring(FVector2D(0.0, 0.7), 0.14, 0.16, 8);
				Line(FVector2D(0.36, 0.22), FVector2D(0.72, 0.5));
				Line(FVector2D(-0.36, 0.22), FVector2D(-0.72, 0.5));
				Line(FVector2D(0.34, -0.42), FVector2D(0.64, -0.72));
				Line(FVector2D(-0.34, -0.42), FVector2D(-0.64, -0.72));
				Line(FVector2D(0.0, -0.64), FVector2D(0.0, -0.9));
				break;
			case EGlyph::Spiral:
			{
				TArray<FVector2D> S;
				for (int32 k = 0; k <= 30; ++k)
				{
					const double T = k / 30.0;
					const double A = T * TNProcMap::TwoPi * 2.4;
					S.Add(FVector2D(FMath::Cos(A), FMath::Sin(A)) * (0.08 + 0.84 * T));
				}
				Out.Add(S);
				break;
			}
			case EGlyph::Sun:
				Ring(FVector2D::ZeroVector, 0.36, 0.36, 12);
				for (int32 k = 0; k < 8; ++k)
				{
					const double A = TNProcMap::TwoPi * k / 8.0;
					const FVector2D D(FMath::Cos(A), FMath::Sin(A));
					Line(D * 0.52, D * 0.9);
				}
				break;
			case EGlyph::Waves:
				for (int32 r = 0; r < 3; ++r)
				{
					TArray<FVector2D> S;
					for (int32 k = 0; k <= 16; ++k)
					{
						const double X = -0.9 + 1.8 * k / 16.0;
						S.Add(FVector2D(X, -0.55 + 0.55 * r + 0.14 * FMath::Sin(X * 7.0 + r)));
					}
					Out.Add(S);
				}
				break;
			case EGlyph::Eye:
			{
				TArray<FVector2D> Up, Dn;
				for (int32 k = 0; k <= 12; ++k)
				{
					const double X = -0.9 + 1.8 * k / 12.0;
					const double Y = 0.45 * FMath::Cos(X / 0.9 * HALF_PI);
					Up.Add(FVector2D(X, Y));
					Dn.Add(FVector2D(X, -Y));
				}
				Out.Add(Up);
				Out.Add(Dn);
				Ring(FVector2D::ZeroVector, 0.2, 0.2, 8);
				break;
			}
			default:
				// Panal de siete hexágonos (el caparazón visto de cerca).
				for (int32 h = 0; h < 7; ++h)
				{
					const double A = TNProcMap::TwoPi * h / 6.0 + PI / 6.0;
					const FVector2D C = h == 0 ? FVector2D::ZeroVector : FVector2D(FMath::Cos(A), FMath::Sin(A)) * 0.52;
					Ring(C, 0.3, 0.3, 6);
				}
				break;
		}
	}

	/**
	 * Símbolo G de lado 2·Size con centro en C sobre el plano de ejes U (derecha) y V (arriba), en relieve
	 * hacia N: cada trazo, una barra fina.
	 */
	inline void TNCarveGlyph(FTNProcMeshBuffers& M, const FVector& C, const FVector& U, const FVector& V, const FVector& N, double Size, EGlyph G, const FLinearColor& Col)
	{
		TArray<TArray<FVector2D>> Strokes;
		TNGlyphStrokes(G, Strokes);
		const double W = FMath::Max(2.5, Size * 0.075);
		for (const TArray<FVector2D>& S : Strokes)
		{
			for (int32 k = 0; k + 1 < S.Num(); ++k)
			{
				const FVector A = C + U * (S[k].X * Size) + V * (S[k].Y * Size);
				const FVector B = C + U * (S[k + 1].X * Size) + V * (S[k + 1].Y * Size);
				const double Len = FVector::Distance(A, B);
				if (Len < 0.5) { continue; }
				const FVector Ax = (B - A) / Len;
				const FVector Ay = FVector::CrossProduct(N, Ax).GetSafeNormal();
				TNPropMesh::TNPropBox(M, (A + B) * 0.5 + N * (W * 0.25), Ax, Ay, N, FVector(Len * 0.5 + W * 0.5, W * 0.5, W * 0.25), Col);
			}
		}
	}

	/**
	 * Estela: losa de piedra de canto redondeado sobre un zócalo, con un marco y dos o tres símbolos en
	 * la cara que mira a Face (planta). Base en el suelo.
	 */
	inline void TNStele(FTNProcMeshBuffers& Solid, FTNProcMeshBuffers& Ink, const FVector& Base, const FVector& Face, double W, double H, uint32 Seed,
		const FLinearColor& Stone, const FLinearColor& InkCol)
	{
		CaveDecorDetail::FDecorRng Rng(Seed);
		const FVector N = FVector(Face.X, Face.Y, 0.0).GetSafeNormal();
		const FVector U(-N.Y, N.X, 0.0);
		const FVector Up = FVector::UpVector;
		const double T = 16.0;
		TNPropMesh::TNPropBox(Solid, Base + Up * 12.0, U, N, Up, FVector(W * 0.62, T * 1.1, 12.0), Stone * 0.8f);
		TNPropMesh::TNPropBox(Solid, Base + Up * (12.0 + H * 0.5), U, N, Up, FVector(W * 0.5, T * 0.5, H * 0.5), Stone);
		// Canto redondeado: medio disco extruido.
		const FVector TopC = Base + Up * (12.0 + H);
		constexpr int32 Seg = 8;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = PI * k / Seg, A1 = PI * (k + 1) / Seg;
			const FVector P0 = TopC + U * (FMath::Cos(A0) * W * 0.5) + Up * (FMath::Sin(A0) * W * 0.32);
			const FVector P1 = TopC + U * (FMath::Cos(A1) * W * 0.5) + Up * (FMath::Sin(A1) * W * 0.32);
			Solid.AddTri(TopC + N * (T * 0.5), P0 + N * (T * 0.5), P1 + N * (T * 0.5), N, Stone);
			Solid.AddTri(TopC - N * (T * 0.5), P0 - N * (T * 0.5), P1 - N * (T * 0.5), -N, Stone);
			Solid.AddQuad(P0 + N * (T * 0.5), P1 + N * (T * 0.5), P1 - N * (T * 0.5), P0 - N * (T * 0.5), (P0 + P1) * 0.5 - TopC, Stone * 0.92f);
		}
		// Marco y símbolos en la cara.
		const FVector Face0 = Base + Up * 12.0 + N * (T * 0.5);
		const double Fw = W * 0.42, Fb = H * 0.06, Ft = H * 0.97;
		const FVector Corners[4] = { Face0 + U * -Fw + Up * Fb, Face0 + U * Fw + Up * Fb, Face0 + U * Fw + Up * Ft, Face0 + U * -Fw + Up * Ft };
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector A = Corners[k], B = Corners[(k + 1) % 4];
			const double Len = FVector::Distance(A, B);
			const FVector Ax = (B - A) / Len;
			TNPropMesh::TNPropBox(Ink, (A + B) * 0.5 + N * 1.0, Ax, FVector::CrossProduct(N, Ax), N, FVector(Len * 0.5 + 2.0, 2.0, 1.0), InkCol);
		}
		const int32 Count = H > 190.0 ? 3 : 2;
		const double Size = FMath::Min(W * 0.3, (Ft - Fb) / (Count * 2.3));
		for (int32 g = 0; g < Count; ++g)
		{
			const EGlyph Gl = g == 0 && Rng.Chance(0.6) ? EGlyph::Turtle : static_cast<EGlyph>(Rng.RangeInt(0, static_cast<int32>(EGlyph::Count) - 1));
			const double Z = LerpD(Fb, Ft, (g + 0.5) / Count);
			TNCarveGlyph(Ink, Face0 + Up * Z, U, Up, N, Size, Gl, InkCol);
		}
	}

	// ── Piezas ───────────────────────────────────────────────────────────────────

	/** Estalagmita: cono irregular de punta fina (base en el suelo). */
	inline void TNStalagmite(FTNProcMeshBuffers& M, const FVector& Base, double H, double R, uint32 Seed, const FLinearColor& Col)
	{
		TNProcAddLathe(M, Base, { -15.0, H * 0.18, H * 0.45, H * 0.72, H * 0.9 }, { R * 1.15, R * 0.9, R * 0.6, R * 0.32, R * 0.12 }, 0.15, Seed, Col, 7,
			0.1 * H / FMath::Max(1.0, 0.12 * R));
	}

	/** Columna: estalactita y estalagmita unidas, estrecha en medio, hasta la altura H (dentro del techo). */
	inline void TNCaveColumn(FTNProcMeshBuffers& M, const FVector& Base, double H, double R, uint32 Seed, const FLinearColor& Col)
	{
		const double F[9] = { -0.02, 0.08, 0.22, 0.4, 0.5, 0.6, 0.76, 0.9, 1.0 };
		const double S[9] = { 1.6, 1.2, 0.85, 0.62, 0.55, 0.66, 0.9, 1.25, 1.7 };
		TArray<double> Z, Rr;
		for (int32 k = 0; k < 9; ++k)
		{
			Z.Add(F[k] * H);
			Rr.Add(R * S[k] * (1.0 + 0.1 * TNProcHashNoise(k, 11, Seed)));
		}
		TNProcAddLathe(M, Base, Z, Rr, 0.12, Seed, Col, 8, 0.0);
	}

	/** Cristal: prisma hexagonal con punta, desde Base a lo largo de Axis. */
	inline void TNCrystal(FTNProcMeshBuffers& M, const FVector& Base, const FVector& Axis, double Len, double R, const FLinearColor& Col)
	{
		const FVector Top = Base + Axis * Len;
		TNProcAddCylinder(M, Base - Axis * 12.0, Top, R, R * 0.92, 6, Col, false);
		TNProcAddCylinder(M, Top, Top + Axis * (R * 1.7), R * 0.92, 0.5, 6, Col * 1.15f, false);
	}

	/**
	 * Racimo de cristales que salen del suelo inclinados hacia Out (fuera de la pared). Collider, si se
	 * da, recibe una copia algo más fina de cada cristal (queda dentro, no se ve): su colisión.
	 */
	inline void TNCrystalCluster(FTNProcMeshBuffers& M, const FVector& Base, const FVector& Out, double Size, CaveDecorDetail::FDecorRng& Rng, const FLinearColor& Col,
		FTNProcMeshBuffers* Collider = nullptr)
	{
		const int32 Count = Rng.RangeInt(4, 7);
		for (int32 c = 0; c < Count; ++c)
		{
			const FVector Tilt = (FVector(Rng.Range(-0.4, 0.4), Rng.Range(-0.4, 0.4), 1.0) + Out * Rng.Range(0.2, 0.7)).GetSafeNormal();
			const double L = Size * (c == 0 ? 1.0 : Rng.Range(0.4, 0.85));
			const FVector P0 = Base + FVector(Rng.Range(-0.22, 0.22) * Size, Rng.Range(-0.22, 0.22) * Size, 0.0);
			const double R = L * Rng.Range(0.12, 0.19);
			TNCrystal(M, P0, Tilt, L, R, Col * static_cast<float>(Rng.Range(0.85, 1.15)));
			if (Collider) { TNCrystal(*Collider, P0, Tilt, L * 0.97, R * 0.88, Col * 0.5f); }
		}
	}

	/** Setas que brillan: pie claro (Stem) y sombrero luminoso (Glow). */
	inline void TNGlowMushrooms(FTNProcMeshBuffers& Stem, FTNProcMeshBuffers& Glow, const FVector& Base, double Size, CaveDecorDetail::FDecorRng& Rng, const FLinearColor& Col)
	{
		const int32 Count = Rng.RangeInt(3, 6);
		for (int32 c = 0; c < Count; ++c)
		{
			const FVector P = Base + FVector(Rng.Range(-0.6, 0.6) * Size, Rng.Range(-0.6, 0.6) * Size, 0.0);
			const double H = Size * Rng.Range(0.3, 1.0);
			const double R = H * Rng.Range(0.35, 0.55);
			TNProcAddCylinder(Stem, P - FVector(0.0, 0.0, 5.0), P + FVector(0.0, 0.0, H), H * 0.09, H * 0.07, 6, FLinearColor(0.82f, 0.8f, 0.7f));
			TNPropMesh::TNPropBall(Glow, P + FVector(0.0, 0.0, H), R, Col * static_cast<float>(Rng.Range(0.8, 1.2)), 8, 3, 0.45);
		}
	}

	/** Liana que cuelga desde Top Len cm, con hojitas. */
	inline void TNVine(FTNProcMeshBuffers& M, const FVector& Top, double Len, CaveDecorDetail::FDecorRng& Rng, const FLinearColor& Col, const FLinearColor& Leaf)
	{
		TArray<FVector> Pts;
		TArray<double> Radii;
		const FVector Sway(Rng.Range(-1.0, 1.0), Rng.Range(-1.0, 1.0), 0.0);
		constexpr int32 Seg = 5;
		for (int32 k = 0; k <= Seg; ++k)
		{
			const double T = static_cast<double>(k) / Seg;
			Pts.Add(Top + FVector(0.0, 0.0, -Len * T) + Sway * (18.0 * FMath::Sin(T * PI)));
			Radii.Add(LerpD(3.2, 1.6, T));
		}
		TNFormTube(M, Pts, Radii, 4, Col);
		for (int32 l = 1; l < Seg * 2; ++l)
		{
			const double T = l / (Seg * 2.0);
			const FVector P = Top + FVector(0.0, 0.0, -Len * T) + Sway * (18.0 * FMath::Sin(T * PI));
			const double A = Rng.Range(0.0, TNProcMap::TwoPi);
			M.AddBox(P + FVector(FMath::Cos(A) * 7.0, FMath::Sin(A) * 7.0, 0.0), FVector(FMath::Cos(A), FMath::Sin(A), 0.0), FVector(7.0, 4.0, 1.2), Leaf * static_cast<float>(Rng.Range(0.8, 1.2)));
		}
	}

	/** Pilar de templo (base, fuste con dos bandas, capitel) de Z = 0 a Top; roto si bBroken. */
	inline void TNTemplePillar(FTNProcMeshBuffers& M, const FVector& Base, double Top, double Half, const FVector& Along, bool bBroken, uint32 Seed,
		const FLinearColor& Stone, const FLinearColor& Trim)
	{
		const double H = bBroken ? Top * LerpD(0.35, 0.6, 0.5 + 0.5 * TNProcHashNoise(1, 1, Seed)) : Top;
		M.AddBox(Base + FVector(0.0, 0.0, 14.0), Along, FVector(Half * 1.35, Half * 1.35, 16.0), Stone * 0.85f);
		M.AddBox(Base + FVector(0.0, 0.0, 30.0 + (H - 30.0 - (bBroken ? 0.0 : 44.0)) * 0.5), Along, FVector(Half, Half, (H - 30.0 - (bBroken ? 0.0 : 44.0)) * 0.5), Stone);
		for (const double F : { 0.3, 0.62 })
		{
			if (F * Top > H - 20.0) { continue; }
			M.AddBox(Base + FVector(0.0, 0.0, F * Top), Along, FVector(Half * 1.08, Half * 1.08, 7.0), Trim);
		}
		if (!bBroken)
		{
			M.AddBox(Base + FVector(0.0, 0.0, H - 34.0), Along, FVector(Half * 1.3, Half * 1.3, 14.0), Stone * 0.9f);
			M.AddBox(Base + FVector(0.0, 0.0, H - 10.0), Along, FVector(Half * 1.5, Half * 1.5, 10.0), Stone * 0.82f);
			return;
		}
		// Rotura: tres pedazos inclinados arriba y un tambor caído al lado.
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector P = Base + FVector(TNProcHashNoise(k, 2, Seed) * Half * 0.5, TNProcHashNoise(k, 3, Seed) * Half * 0.5, H + 6.0);
			M.AddBox(P, FVector(FMath::Cos(k * 1.3), FMath::Sin(k * 1.3), 0.0), FVector(Half * 0.4, Half * 0.3, 10.0 + 6.0 * k), Stone * 0.95f);
		}
		const FVector Fall = Base + Along * (Half * 2.6) + FVector(0.0, 0.0, Half);
		TNProcAddCylinder(M, Fall - FVector::CrossProduct(Along, FVector::UpVector) * Half * 1.2, Fall + FVector::CrossProduct(Along, FVector::UpVector) * Half * 1.2, Half, Half, 8, Stone * 0.9f);
	}

	/** Antorcha de pared en P (pie del soporte) que asoma hacia In; llama en Glow. Devuelve la punta de la llama. */
	inline FVector TNWallTorch(FTNProcMeshBuffers& Detail, FTNProcMeshBuffers& Glow, const FVector& P, const FVector& In)
	{
		const FLinearColor Iron(0.14f, 0.13f, 0.13f), Wood(0.4f, 0.26f, 0.14f);
		Detail.AddBox(P, In, FVector(8.0, 10.0, 16.0), Iron);
		const FVector Top = P + In * 26.0 + FVector(0.0, 0.0, 36.0);
		TNProcAddCylinder(Detail, P + In * 4.0, Top, 3.5, 4.5, 5, Wood);
		TNProcAddCylinder(Detail, Top, Top + FVector(0.0, 0.0, 9.0), 8.0, 10.5, 6, Iron);
		const FVector F0 = Top + FVector(0.0, 0.0, 8.0);
		TNProcAddCylinder(Glow, F0, F0 + FVector(0.0, 0.0, 30.0), 8.5, 0.5, 6, FLinearColor(1.f, 0.5f, 0.1f), false);
		TNProcAddCylinder(Glow, F0, F0 + FVector(0.0, 0.0, 19.0), 5.5, 0.5, 5, FLinearColor(1.f, 0.88f, 0.45f), false);
		return F0 + FVector(0.0, 0.0, 24.0);
	}

	/** Poza: disco de agua con orilla de espuma y un borde de piedras planas. */
	inline void TNCavePool(FTNProcMeshBuffers& Water, FTNProcMeshBuffers& Rim, const FVector& C, double R, CaveDecorDetail::FDecorRng& Rng, const FLinearColor& Stone)
	{
		constexpr int32 Seg = 16;
		TArray<double> Radii;
		for (int32 k = 0; k < Seg; ++k) { Radii.Add(R * Rng.Range(0.85, 1.1)); }
		const FVector Ctr = C + FVector(0.0, 0.0, 4.0);
		const FLinearColor Deep(0.1f, 0.34f, 0.5f, 0.9f), Edge(0.8f, 0.92f, 0.96f, 0.7f);
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg, A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			const double R0 = Radii[k], R1 = Radii[(k + 1) % Seg];
			const FVector I0 = Ctr + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.0) * (R0 * 0.8), I1 = Ctr + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.0) * (R1 * 0.8);
			const FVector O0 = Ctr + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.0) * R0, O1 = Ctr + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.0) * R1;
			Water.AddTri(Ctr, I0, I1, FVector::UpVector, Deep);
			const int32 Base = Water.Verts.Num();
			Water.AddQuad(I0, I1, O1, O0, FVector::UpVector, Deep);
			// Espuma en el borde de fuera: color por vértice según su radio.
			for (int32 v = Base; v < Water.Verts.Num(); ++v)
			{
				const double Rv = FVector::Dist2D(Water.Verts[v], Ctr);
				Water.Colors[v] = Rv > (R0 + R1) * 0.45 ? Edge : Deep;
			}
		}
		for (int32 k = 0; k < Seg; k += 2)
		{
			const double A = TNProcMap::TwoPi * (k + Rng.Range(-0.3, 0.3)) / Seg;
			const FVector P = C + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * (Radii[k] + Rng.Range(8.0, 26.0));
			TNProcAddBoulder(Rim, P, Rng.Range(18.0, 38.0), Rng.Range(18.0, 34.0), Rng.Next(), Stone * static_cast<float>(Rng.Range(0.85, 1.1)));
		}
	}

	// ── Piezas de arte (Docs/Arte_Assets.md) ─────────────────────────────────────
	// Las llamadas que llevan varias tiradas del aleatorio en sus argumentos no se tocan (el orden en que se evalúan no
	// cambia): lo que acaban de añadir se marca después como pieza, con el pivote sacado de lo construido.

	/**
	 * Marca como la pieza Slot lo que se ha añadido a M desde Verts0/Tris0, sin cambiar nada: saca esa cola y la vuelve a
	 * poner igual dentro del ámbito de la pieza (los buffers quedan idénticos). PivotOf(Cola) da su pivote.
	 */
	template <typename FPivotOf>
	void TNCaveMarkTail(TNArt::FPieceLog* Log, FName Slot, FTNProcMeshBuffers& M, int32 Verts0, int32 Tris0, FPivotOf&& PivotOf)
	{
		if (!Log || M.Tris.Num() <= Tris0) { return; }
		FTNProcMeshBuffers Tail;
		const int32 NumV = M.Verts.Num() - Verts0;
		Tail.Verts.Append(M.Verts.GetData() + Verts0, NumV);
		Tail.Normals.Append(M.Normals.GetData() + Verts0, NumV);
		Tail.UVs.Append(M.UVs.GetData() + Verts0, NumV);
		Tail.Colors.Append(M.Colors.GetData() + Verts0, NumV);
		Tail.Tris.Append(M.Tris.GetData() + Tris0, M.Tris.Num() - Tris0);
		const FTransform Pivot = PivotOf(Tail);
		M.Verts.SetNum(Verts0, EAllowShrinking::No);
		M.Normals.SetNum(Verts0, EAllowShrinking::No);
		M.UVs.SetNum(Verts0, EAllowShrinking::No);
		M.Colors.SetNum(Verts0, EAllowShrinking::No);
		M.Tris.SetNum(Tris0, EAllowShrinking::No);
		TNArt::FPieceScope Piece(Log, Slot, Pivot, { &M });
		M.Verts.Append(Tail.Verts);
		M.Normals.Append(Tail.Normals);
		M.UVs.Append(Tail.UVs);
		M.Colors.Append(Tail.Colors);
		M.Tris.Append(Tail.Tris);
	}

	/**
	 * Pivote de un peñasco (TNProcAddBoulder: hundido 25 en el suelo) sacado de su caja: centro de la base, sin giro; escala 1
	 * = radio y alto RefSize.
	 */
	inline FTransform TNCaveBoulderPivot(const FTNProcMeshBuffers& Tail, double RefSize)
	{
		const FBox Box(Tail.Verts);
		const FVector Half = Box.GetExtent();
		const double Rxy = 0.5 * (Half.X + Half.Y);
		return TNArt::PiecePivot(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z + 25.0), 0.0,
			FVector(Rxy / RefSize, Rxy / RefSize, FMath::Max(1.0, Box.Max.Z - Box.Min.Z) / (1.1 * RefSize)));
	}

	/** Pivote de una columna de basalto (TNRockHexColumn: enterrada 20) sacado de su caja: pie; escala 1 = radio 36 y 250 de alto. */
	inline FTransform TNCaveBasaltPivot(const FTNProcMeshBuffers& Tail)
	{
		const FBox Box(Tail.Verts);
		const FVector Half = Box.GetExtent();
		const double Rxy = 0.5 * (Half.X + Half.Y);
		return TNArt::PiecePivot(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z + 20.0), 0.0,
			FVector(Rxy / 36.0, Rxy / 36.0, FMath::Max(1.0, Box.Max.Z - Box.Min.Z - 20.0) / 250.0));
	}

	/**
	 * Pivote de un prop copiado con TNPropAppend(Dst, Src, Offset, Yaw, Scale) sacado de dos de sus vértices (Src en sus
	 * locales y Tail ya colocado): Offset, girado Yaw, con la escala Scale.
	 */
	inline FTransform TNCavePropPivot(const FTNProcMeshBuffers& Src, const FTNProcMeshBuffers& Tail, double Scale)
	{
		if (Src.Verts.Num() == 0 || Tail.Verts.Num() != Src.Verts.Num()) { return FTransform::Identity; }
		double YawDeg = 0.0;
		for (int32 i = 1; i < Src.Verts.Num(); ++i)
		{
			const FVector2D Local(Src.Verts[i].X - Src.Verts[0].X, Src.Verts[i].Y - Src.Verts[0].Y);
			if (Local.SizeSquared() < 1.0) { continue; }
			const FVector2D Placed(Tail.Verts[i].X - Tail.Verts[0].X, Tail.Verts[i].Y - Tail.Verts[0].Y);
			YawDeg = FMath::RadiansToDegrees(FMath::Atan2(Placed.Y, Placed.X) - FMath::Atan2(Local.Y, Local.X));
			break;
		}
		const FVector Turned = FRotator(0.0, YawDeg, 0.0).RotateVector(Src.Verts[0]) * Scale;
		return TNArt::PiecePivot(Tail.Verts[0] - Turned, YawDeg, FVector(Scale));
	}

	// ── Cueva completa ───────────────────────────────────────────────────────────

	/**
	 * Decoración de la cueva sobre las estaciones St (las del techo) con la cara de dentro de su bóveda
	 * (Inner, de TNCaveBuildRoof: por estación, pie, pared, bóveda de izquierda a derecha, pared y pie).
	 * NoFloor marca las estaciones sin suelo (el río de lava): ahí no va nada en el suelo.
	 */
	inline void TNCaveBuildDecor(FTNCaveDecorOut& Out, const TArray<FTNCaveStation>& St, const TArray<TArray<FVector>>& Inner, const TArray<uint8>& NoFloor,
		double ClearFactor, uint32 Seed, ECaveStyle Style, const FTNCaveLook& Look, const FTNCaveMagma* Magma = nullptr)
	{
		using namespace CaveDecorDetail;
		const int32 Num = St.Num();
		if (Num < 6 || Inner.Num() != Num || NoFloor.Num() != Num) { return; }
		const int32 Side = Inner[0].Num();
		FDecorRng Rng(Seed ^ 0x0DEC0u);
		const FTNCavePalette Pal = TNCavePaletteFor(Style, Look, Seed);
		FTNProcMeshBuffers& PaintBuf = Pal.bGlowPaint ? Out.Glow : Out.Detail;

		TArray<FFrame> Fr;
		for (const FTNCaveStation& S : St)
		{
			FFrame F;
			F.F = S.Floor;
			F.D = FVector(S.Dir.X, S.Dir.Y, 0.0);
			F.N = FVector(-S.Dir.Y, S.Dir.X, 0.0);
			F.Hw = S.HalfWidth;
			F.Cl = TNProcMap::CaveDetail::Clearance(S.HalfWidth * 2.0, ClearFactor);
			F.Spring = F.Cl * 0.5;
			Fr.Add(F);
		}

		// Cámaras: tramos con más de 13 m de suelo; C, la estación más ancha.
		struct FChamber { int32 A = 0, B = 0, C = 0; };
		TArray<FChamber> Chambers;
		for (int32 s = 1; s < Num - 1; ++s)
		{
			if (Fr[s].Hw < 650.0) { continue; }
			FChamber Ch;
			Ch.A = s;
			while (s + 1 < Num - 1 && Fr[s + 1].Hw >= 650.0) { ++s; }
			Ch.B = s;
			Ch.C = Ch.A;
			for (int32 k = Ch.A; k <= Ch.B; ++k) { if (Fr[k].Hw > Fr[Ch.C].Hw) { Ch.C = k; } }
			Chambers.Add(Ch);
		}
		auto InChamber = [&Chambers](int32 s) { for (const FChamber& Ch : Chambers) { if (s >= Ch.A && s <= Ch.B) { return true; } } return false; };

		// Lo reservado por lado (estatua, poza, estela): no se pone nada más del suelo ahí.
		TArray<uint8> Busy;
		Busy.Init(0, Num * 2);
		auto SideIdx = [](double Sd) { return Sd > 0.0 ? 1 : 0; };
		auto IsBusy = [&](int32 s, double Sd) { return s < 0 || s >= Num || Busy[s * 2 + SideIdx(Sd)] != 0; };
		auto Reserve = [&](int32 s0, int32 s1, double Sd) { for (int32 s = FMath::Max(0, s0); s <= FMath::Min(Num - 1, s1); ++s) { Busy[s * 2 + SideIdx(Sd)] = 1; } };
		auto FloorOk = [&](int32 s0, int32 s1) { for (int32 s = FMath::Max(0, s0); s <= FMath::Min(Num - 1, s1); ++s) { if (NoFloor[s]) { return false; } } return true; };

		// ── Cámara de magma (cueva dentro de un volcán): lo del lado del lago queda libre ──
		int32 MagmaS = INDEX_NONE;
		if (Magma)
		{
			double Best = 1e300;
			for (int32 s = 0; s < Num; ++s)
			{
				const double D = FMath::Square(Fr[s].F.X - Magma->Center.X) + FMath::Square(Fr[s].F.Y - Magma->Center.Y);
				if (D < Best) { Best = D; MagmaS = s; }
			}
			const int32 Span = FMath::CeilToInt((Magma->Radius + 500.0) / 400.0);
			Reserve(MagmaS - Span, MagmaS + Span, Magma->Side);
		}

		// ── Estatua de la tortuga en la cámara más ancha, mirando a quien llega (lejos del magma) ──
		int32 StatueS = INDEX_NONE;
		double StatueSide = Rng.Sign();
		if (Magma) { StatueSide = -Magma->Side; }
		{
			int32 Best = INDEX_NONE;
			for (int32 c = 0; c < Chambers.Num(); ++c) { if (Best == INDEX_NONE || Fr[Chambers[c].C].Hw > Fr[Chambers[Best].C].Hw) { Best = c; } }
			if (Best != INDEX_NONE)
			{
				// Un poco antes del centro: el río de lava (si lo hay) va en el centro de la primera cámara.
				int32 S = Chambers[Best].C;
				for (int32 Try = 0; Try < 6 && !FloorOk(S - 1, S + 1); ++Try) { S = FMath::Max(Chambers[Best].A, S - 1); }
				if (FloorOk(S - 1, S + 1))
				{
					StatueS = S;
					const FFrame& F = Fr[S];
					const double Scale = FMath::Clamp((F.Hw - 500.0) / 500.0, 0.85, 1.25);
					const FVector Face = (-F.N * StatueSide * 0.85 - F.D * 0.5).GetSafeNormal();
					const FVector Pos = F.At(StatueSide * (F.Hw - 150.0 * Scale), -4.0);
					FTNProcMeshBuffers Solid, Glow;
					const int32 Crown = Style == ECaveStyle::Temple ? 1 : (Style == ECaveStyle::Jungle ? 2 : (Style == ECaveStyle::Crystal ? 3 : 0));
					const FLinearColor Eyes = Style == ECaveStyle::Temple || Style == ECaveStyle::Limestone ? FLinearColor(0.35f, 0.9f, 1.f) : Pal.Glow;
					TNTurtleStatue(Solid, Glow, Seed, Pal.Statue, Pal.Accent, Eyes, Crown, Style == ECaveStyle::Jungle);
					// Pieza de arte: centro de la base de la estatua, +X hacia donde mira; escala 1 = la de la cámara normal.
					TNArt::FPieceScope StatuePiece(Out.Log, TN_ART("ProcMap.Cave.TurtleStatue"), TNArt::PiecePivot(Pos, YawOf(Face), FVector(Scale)),
						{ &Out.Solid, &Out.Glow });
					TNPropMesh::TNPropAppend(Out.Solid, Solid, Pos, YawOf(Face), Scale);
					TNPropMesh::TNPropAppend(Out.Glow, Glow, Pos, YawOf(Face), Scale);
					Reserve(S - 2, S + 2, StatueSide);
					FTNCaveLight L;
					L.P = Pos + Face * (320.0 * Scale) + FVector(0.0, 0.0, 420.0 * Scale);
					L.Color = Style == ECaveStyle::LavaTube ? FLinearColor(1.f, 0.5f, 0.25f) : FLinearColor(1.f, 0.84f, 0.62f);
					L.Lumens = 1100.f;
					L.Radius = 1000.f;
					Out.Lights.Add(L);
				}
			}
		}

		// ── Lucernario (caliza, selva, templo): hueco luminoso en la clave, haz de luz y un claro verde ──
		if (Style == ECaveStyle::Limestone || Style == ECaveStyle::Jungle || Style == ECaveStyle::Temple)
		{
			// En una cámara sin la estatua; si no la hay, delante de ella (mira hacia la entrada).
			int32 S = INDEX_NONE;
			for (const FChamber& Ch : Chambers) { if (StatueS < Ch.A || StatueS > Ch.B) { S = Ch.C; } }
			if (S == INDEX_NONE && StatueS != INDEX_NONE) { S = FMath::Max(1, StatueS - 2); }
			if (S == INDEX_NONE && Chambers.Num() > 0) { S = Chambers[0].C; }
			if (S != INDEX_NONE && FloorOk(S, S))
			{
				const FFrame& F = Fr[S];
				const double Crown = F.RoofZ(0.0);
				const FVector Top = F.At(0.0, Crown - 8.0);
				constexpr int32 Seg = 10;
				const FLinearColor Sky(0.92f, 0.96f, 1.f), RimC = Look.Inner * 1.8f;
				// Pieza de arte (hueco de la clave, haz y claro de hierba con flores): centro del suelo, +X por el túnel; escala 1 =
				// 10 m del suelo a la clave.
				TNArt::FPieceScope SkylightPiece(Out.Log, TN_ART("ProcMap.Cave.Skylight"), TNArt::PiecePivot(F.F, YawOf(F.D), FVector(1.0, 1.0, Crown / 1000.0)),
					{ &Out.Glow, &Out.Detail, &Out.Beam });
				for (int32 k = 0; k < Seg; ++k)
				{
					const double A0 = TNProcMap::TwoPi * k / Seg, A1 = TNProcMap::TwoPi * (k + 1) / Seg;
					const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.0), D1(FMath::Cos(A1), FMath::Sin(A1), 0.0);
					Out.Glow.AddTri(Top, Top + D0 * 85.0, Top + D1 * 85.0, -FVector::UpVector, Sky);
					Out.Detail.AddQuad(Top + D0 * 85.0, Top + D1 * 85.0, Top + D1 * 125.0 + FVector(0.0, 0.0, 30.0), Top + D0 * 125.0 + FVector(0.0, 0.0, 30.0), -FVector::UpVector, RimC);
					// Haz: tronco de cono hasta el suelo, más opaco arriba (dos caras en el material).
					const FVector B0 = F.At(0.0, 4.0) + D0 * 175.0, B1 = F.At(0.0, 4.0) + D1 * 175.0;
					const FVector T0 = Top + D0 * 80.0, T1 = Top + D1 * 80.0;
					const int32 Base = Out.Beam.Verts.Num();
					Out.Beam.AddQuad(T0, T1, B1, B0, (D0 + D1), FLinearColor(1.f, 0.95f, 0.8f, 0.3f));
					for (int32 v = Base; v < Out.Beam.Verts.Num(); ++v)
					{
						const float U = static_cast<float>(FMath::Clamp((Out.Beam.Verts[v].Z - F.F.Z) / FMath::Max(1.0, Crown), 0.0, 1.0));
						Out.Beam.Colors[v] = FLinearColor(1.f, 0.95f, 0.8f, 0.3f * U * U);
					}
				}
				// Claro de hierba y flores donde da la luz.
				TArray<FVector2D> Patch;
				for (int32 k = 0; k < 12; ++k)
				{
					const double A = TNProcMap::TwoPi * k / 12.0;
					Patch.Add(FVector2D(F.F.X, F.F.Y) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (190.0 * Rng.Range(0.8, 1.1)));
				}
				Out.Detail.AddPrism(Patch, F.F.Z + 3.0, F.F.Z - 5.0, FLinearColor(0.3f, 0.52f, 0.18f), false);
				for (int32 k = 0; k < 14; ++k)
				{
					const double A = Rng.Range(0.0, TNProcMap::TwoPi), R = Rng.Range(20.0, 170.0);
					const FVector P = F.F + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0);
					TNProcAddCylinder(Out.Detail, P, P + FVector(Rng.Range(-6.0, 6.0), Rng.Range(-6.0, 6.0), Rng.Range(18.0, 38.0)), 5.0, 0.5, 3, FLinearColor(0.28f, 0.58f, 0.16f));
					if (k % 3 == 0) { TNPropMesh::TNPropBall(Out.Detail, P + FVector(0.0, 0.0, 24.0), 4.5, k % 2 ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(0.95f, 0.95f, 0.95f), 5, 2); }
				}
				FTNCaveLight L;
				L.P = Top - FVector(0.0, 0.0, 30.0);
				L.Color = FLinearColor(1.f, 0.93f, 0.78f);
				L.Lumens = 4500.f;
				L.Radius = static_cast<float>(Crown * 1.7);
				L.bSpot = true;
				Out.Lights.Add(L);
				Out.Motes.Add(F.At(0.0, Crown * 0.45));
			}
		}

		// ── Poza (caliza, cristales), en la cámara, del otro lado de la estatua ──
		if ((Style == ECaveStyle::Limestone || Style == ECaveStyle::Crystal) && Chambers.Num() > 0)
		{
			const FChamber& Ch = Chambers.Last();
			const int32 S = FMath::Clamp(Ch.C + (Chambers.Num() > 1 ? 0 : 2), Ch.A, Ch.B);
			const double Sd = -StatueSide;
			if (FloorOk(S - 1, S + 1) && !IsBusy(S, Sd) && Fr[S].Hw > 700.0)
			{
				const FFrame& F = Fr[S];
				const double R = FMath::Min(240.0, F.Hw * 0.22);
				// Pieza de arte (agua y piedras del borde): centro de la poza; escala 1 = radio 240.
				TNArt::FPieceScope PoolPiece(Out.Log, TN_ART("ProcMap.Cave.Pool"), TNArt::PiecePivot(F.At(Sd * (F.Hw - R - 60.0), 0.0), 0.0, FVector(R / 240.0, R / 240.0, 1.0)),
					{ &Out.Water, &Out.Solid });
				TNCavePool(Out.Water, Out.Solid, F.At(Sd * (F.Hw - R - 60.0), 0.0), R, Rng, Pal.Formation * 0.8f);
				Reserve(S - 1, S + 1, Sd);
			}
		}

		// ── Recorrido: lo que va contra las paredes, estación a estación (en las cámaras, a los dos lados) ──
		int32 NextStele = Rng.RangeInt(3, 6);
		int32 NextGlyph = Rng.RangeInt(2, 4);
		int32 NextTorch = 1;
		int32 TorchLights = 0;
		int32 GlowLights = 0;
		for (int32 s = 2; s < Num - 2; ++s)
		{
			const FFrame& F = Fr[s];
			const bool bChamber = InChamber(s);

			// Luces del túnel cada 5 estaciones.
			if (s % 5 == 2)
			{
				FTNCaveLight L;
				L.P = F.At(0.0, F.Cl * 0.7);
				L.Color = Pal.Light;
				L.Lumens = Pal.Lumens;
				L.Radius = static_cast<float>(FMath::Max(1800.0, F.Hw * 2.4));
				Out.Lights.Add(L);
			}

			// Símbolos pintados en la pared (entre el pie y el arranque de la bóveda).
			if (--NextGlyph <= 0)
			{
				NextGlyph = Rng.RangeInt(4, 7);
				const bool bLeft = Rng.Chance(0.5);
				const int32 Kw = bLeft ? 1 : Side - 2;
				const int32 Ka = bLeft ? 2 : Side - 3;
				const FVector Q[4] = { Inner[s][Kw], Inner[s + 1][Kw], Inner[s + 1][Ka], Inner[s][Ka] };
				const FVector C = (Q[0] + Q[1] + Q[2] + Q[3]) * 0.25;
				FVector N = FVector::CrossProduct(Q[1] - Q[0], Q[3] - Q[0]).GetSafeNormal();
				if (FVector::DotProduct(N, F.At(0.0, F.Spring) - C) < 0.0) { N = -N; }
				const FVector U = (Q[1] - Q[0]).GetSafeNormal();
				const FVector V = FVector::CrossProduct(N, U).GetSafeNormal() * (FVector::CrossProduct(N, U).Z < 0.0 ? -1.0 : 1.0);
				const double Size = FMath::Min(55.0, FMath::Min(FVector::Distance(Q[0], Q[3]), FVector::Distance(Q[0], Q[1])) * 0.3);
				const int32 Count = Rng.RangeInt(1, 3);
				for (int32 g = 0; g < Count; ++g)
				{
					const double Off = (g - (Count - 1) * 0.5) * Size * 2.4;
					TNCarveGlyph(PaintBuf, C + U * Off + N * 12.0, U, V, N, Size, static_cast<EGlyph>(Rng.RangeInt(0, static_cast<int32>(EGlyph::Count) - 1)), Pal.Paint);
				}
			}

			if (NoFloor[s]) { continue; }
			const double First = Rng.Sign();
			for (int32 Si = 0; Si < (bChamber ? 2 : 1); ++Si)
			{
				const double Sd = Si == 0 ? First : -First;
				const FVector In = -F.N * Sd;
				// Franja del suelo junto a la pared: en las cámaras, hasta 3 m; en los pasos, pegado a ella.
				const double Band = bChamber ? FMath::Min(300.0, F.Hw * 0.3) : 45.0;
				auto WallFloor = [&](double Inset, double Along = 0.0) { return F.At(Sd * (F.Hw - Inset), -3.0, Along); };
				if (IsBusy(s, Sd)) { continue; }

				// Estela con símbolos contra la pared.
				if (Si == 0 && --NextStele <= 0)
				{
					NextStele = Rng.RangeInt(8, 12);
					const double W = bChamber ? Rng.Range(110.0, 150.0) : Rng.Range(80.0, 100.0);
					const double H = bChamber ? Rng.Range(170.0, 240.0) : Rng.Range(140.0, 180.0);
					const FLinearColor Ink = Pal.bGlowPaint ? Pal.Paint : (Style == ECaveStyle::Temple ? Pal.Accent : Pal.Paint);
					FTNProcMeshBuffers& InkBuf = Pal.bGlowPaint ? Out.Glow : Out.Detail;
					// Pieza de arte: pie de la estela, +X hacia donde mira su cara; escala 1 = 120 de ancho y 190 de alto.
					TNArt::FPieceScope StelePiece(Out.Log, TN_ART("ProcMap.Cave.Stele"),
						TNArt::PiecePivot(WallFloor(bChamber ? 70.0 : 32.0), YawOf(In), FVector(1.0, W / 120.0, H / 190.0)), { &Out.Solid, &InkBuf });
					TNStele(Out.Solid, InkBuf, WallFloor(bChamber ? 70.0 : 32.0), In, W, H, Rng.Next(), Pal.Statue * 0.95f, Ink);
					Reserve(s, s, Sd);
					continue;
				}

				switch (Style)
				{
					case ECaveStyle::Limestone:
					case ECaveStyle::Crystal:
					{
						if (Style == ECaveStyle::Crystal && Rng.Chance(bChamber ? 0.55 : 0.35))
						{
							const double Size = bChamber ? Rng.Range(160.0, 340.0) : Rng.Range(50.0, 90.0);
							const FVector ClusterBase = WallFloor(bChamber ? Rng.Range(60.0, Band * 0.6) : 25.0);
							{
								// Pieza de arte: pie del racimo, +X hacia fuera de la pared; escala 1 = cristal mayor de 200.
								TNArt::FPieceScope ClusterPiece(Out.Log, TN_ART("ProcMap.Cave.CrystalCluster"), TNArt::PiecePivot(ClusterBase, YawOf(In), FVector(Size / 200.0)),
									{ &Out.Glow, &Out.Solid });
								TNCrystalCluster(Out.Glow, ClusterBase, In, Size, Rng, Pal.Glow, bChamber ? &Out.Solid : nullptr);
							}
							if (bChamber && GlowLights < 6)
							{
								++GlowLights;
								FTNCaveLight L;
								L.P = WallFloor(Size * 0.6) + FVector(0.0, 0.0, Size * 0.8);
								L.Color = Pal.Glow;
								L.Lumens = 1400.f;
								L.Radius = static_cast<float>(Size * 4.0);
								Out.Lights.Add(L);
							}
							break;
						}
						if (bChamber && Rng.Chance(0.25))
						{
							// Columna del suelo a la bóveda.
							const double Y = F.Hw - Rng.Range(90.0, Band * 0.7);
							// Pieza de arte: pie de la columna; escala Z 1 = 600 del suelo a donde entra en la bóveda.
							TNArt::FPieceScope ColumnPiece(Out.Log, TN_ART("ProcMap.Cave.Column"),
								TNArt::PiecePivot(F.At(Sd * Y, -6.0), 0.0, FVector(1.0, 1.0, (F.RoofZ(Y) + 70.0) / 600.0)), { &Out.Solid });
							TNCaveColumn(Out.Solid, F.At(Sd * Y, -6.0), F.RoofZ(Y) + 70.0, Rng.Range(40.0, 75.0), Rng.Next(), Pal.Formation * static_cast<float>(Rng.Range(0.9, 1.05)));
							break;
						}
						if (Rng.Chance(bChamber ? 0.75 : 0.5))
						{
							const int32 Count = bChamber ? Rng.RangeInt(2, 4) : Rng.RangeInt(1, 2);
							for (int32 c = 0; c < Count; ++c)
							{
								const double H = bChamber ? Rng.Range(70.0, 340.0) : Rng.Range(35.0, 80.0);
								const FVector P = WallFloor(bChamber ? Rng.Range(40.0, Band) : Rng.Range(15.0, 35.0), Rng.Range(-150.0, 150.0));
								// Pieza de arte: pie de la estalagmita; escala 1 = 150 de alto.
								TNArt::FPieceScope StalagmitePiece(Out.Log, TN_ART("ProcMap.Cave.Stalagmite"), TNArt::PiecePivot(P, 0.0, FVector(H / 150.0)), { &Out.Solid });
								TNStalagmite(Out.Solid, P, H, H * Rng.Range(0.16, 0.24), Rng.Next(), Pal.Formation * static_cast<float>(Rng.Range(0.85, 1.05)));
							}
						}
						// Estalactitas grandes en la bóveda de las cámaras (no bajan de 2,6 m).
						if (bChamber)
						{
							for (int32 c = 0; c < 2; ++c)
							{
								const double Y = Sd * F.Hw * Rng.Range(0.3, 0.85);
								const double Z = F.RoofZ(Y);
								const double Len = FMath::Min(Rng.Range(90.0, 280.0), Z - 260.0);
								if (Len < 40.0) { continue; }
								const FVector Top = F.At(Y, Z + 50.0, Rng.Range(-180.0, 180.0));
								// Pieza de arte: donde nace en la bóveda (50 por dentro de la roca); escala 1 = 230 hasta la punta.
								TNArt::FPieceScope StalactitePiece(Out.Log, TN_ART("ProcMap.Cave.Stalactite"), TNArt::PiecePivot(Top, 0.0, FVector((Len + 50.0) / 230.0)),
									{ &Out.Detail });
								TNProcAddCylinder(Out.Detail, Top, Top - FVector(0.0, 0.0, Len + 50.0), Len * 0.16, 2.0, 6, Pal.Formation * 0.9f);
							}
						}
						break;
					}
					case ECaveStyle::Jungle:
					{
						// Raíces: bajan por la bóveda y la pared siguiendo la roca y se arrastran por el suelo.
						const int32 Roots = bChamber ? Rng.RangeInt(1, 2) : (Rng.Chance(0.6) ? 1 : 0);
						for (int32 r = 0; r < Roots; ++r)
						{
							TArray<FVector> Pts;
							TArray<double> Radii;
							// Nace en una grieta de la bóveda (no siempre en la clave) y serpentea a lo largo del túnel.
							const int32 Crown = Side / 2;
							const int32 K0 = Sd < 0.0 ? Crown - Rng.RangeInt(-1, 3) : Crown + Rng.RangeInt(-1, 3);
							const int32 Step = Sd < 0.0 ? -1 : 1;
							double Along = Rng.Range(0.0, 1.0);
							const double Drift = Rng.Range(-0.14, 0.14);
							for (int32 k = K0; Sd < 0.0 ? k >= 0 : k <= Side - 1; k += Step)
							{
								Along = FMath::Clamp(Along + Drift + Rng.Range(-0.1, 0.1), 0.0, 1.0);
								const FVector P = FMath::Lerp(Inner[s][k], Inner[s + 1][k], Along);
								const FVector ToAxis = (F.At(0.0, F.Spring, FVector::DotProduct(P - F.F, F.D)) - P).GetSafeNormal();
								Pts.Add(P + ToAxis * Rng.Range(12.0, 30.0));
							}
							if (Pts.Num() < 2) { continue; }
							Pts.Last().Z = F.F.Z + 4.0;
							Pts.Add(WallFloor(Rng.Range(50.0, bChamber ? 180.0 : 60.0), FVector::DotProduct(Pts.Last() - F.F, F.D)) + FVector(0.0, 0.0, 7.0));
							const double R0 = bChamber ? Rng.Range(20.0, 38.0) : Rng.Range(12.0, 22.0);
							for (int32 k = 0; k < Pts.Num(); ++k) { Radii.Add(R0 * (0.55 + 0.45 * FMath::Sin(PI * (k + 0.5) / Pts.Num()))); }
							TNFormTube(Out.Detail, Pts, Radii, 6, FLinearColor(0.3f, 0.2f, 0.12f) * static_cast<float>(Rng.Range(0.85, 1.15)));
						}
						if (Rng.Chance(bChamber ? 0.6 : 0.35))
						{
							const double Size = bChamber ? Rng.Range(60.0, 120.0) : Rng.Range(30.0, 50.0);
							const FVector MushroomBase = WallFloor(bChamber ? Rng.Range(50.0, Band * 0.7) : 25.0);
							{
								// Pieza de arte: centro del corro de setas; escala 1 = 80 (la seta mayor llega a ese alto).
								TNArt::FPieceScope MushroomPiece(Out.Log, TN_ART("ProcMap.Cave.GlowMushrooms"), TNArt::PiecePivot(MushroomBase, 0.0, FVector(Size / 80.0)),
									{ &Out.Detail, &Out.Glow });
								TNGlowMushrooms(Out.Detail, Out.Glow, MushroomBase, Size, Rng, Pal.Glow);
							}
							if (GlowLights < 6 && Rng.Chance(0.45))
							{
								++GlowLights;
								FTNCaveLight L;
								L.P = WallFloor(60.0) + FVector(0.0, 0.0, 90.0);
								L.Color = Pal.Glow;
								L.Lumens = 800.f;
								L.Radius = 700.f;
								Out.Lights.Add(L);
							}
						}
						// Lianas que cuelgan de la bóveda (sin bajar de 2,6 m).
						for (int32 v = 0; v < (bChamber ? 3 : 1); ++v)
						{
							if (!Rng.Chance(0.55)) { continue; }
							const double Y = Sd * F.Hw * Rng.Range(0.15, 0.85);
							const double Z = F.RoofZ(Y);
							const double Len = FMath::Min(Rng.Range(60.0, 220.0), Z - 260.0);
							if (Len > 30.0)
							{
								const FVector VineTop = F.At(Y, Z + 10.0, Rng.Range(-180.0, 180.0));
								// Pieza de arte: donde cuelga de la bóveda; escala 1 = 150 de largo.
								TNArt::FPieceScope VinePiece(Out.Log, TN_ART("ProcMap.Cave.Vine"), TNArt::PiecePivot(VineTop, 0.0, FVector((Len + 10.0) / 150.0)), { &Out.Detail });
								TNVine(Out.Detail, VineTop, Len + 10.0, Rng, FLinearColor(0.2f, 0.34f, 0.12f), FLinearColor(0.22f, 0.5f, 0.16f));
							}
						}
						if (Rng.Chance(0.55))
						{
							TArray<FVector2D> Moss;
							const FVector C = WallFloor(Rng.Range(30.0, bChamber ? 160.0 : 60.0));
							const double R = Rng.Range(50.0, bChamber ? 150.0 : 80.0);
							for (int32 k = 0; k < 8; ++k)
							{
								const double A = TNProcMap::TwoPi * k / 8.0;
								Moss.Add(FVector2D(C.X, C.Y) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (R * Rng.Range(0.7, 1.1)));
							}
							Out.Detail.AddPrism(Moss, C.Z + 5.0, C.Z - 4.0, FLinearColor(0.2f, 0.4f, 0.13f) * static_cast<float>(Rng.Range(0.85, 1.15)), false);
						}
						break;
					}
					case ECaveStyle::Temple:
					{
						if (bChamber && s % 2 == 0)
						{
							// Pilares hasta la bóveda, con antorcha en uno de cada dos.
							const double Y = F.Hw - 75.0;
							const bool bBroken = Rng.Chance(0.2);
							{
								// Pieza de arte (entero o roto, con su tambor caído): pie del pilar, +X por el túnel; escala Z 1 = 600 hasta la bóveda.
								TNArt::FPieceScope PillarPiece(Out.Log, bBroken ? TN_ART("ProcMap.Cave.TemplePillarBroken") : TN_ART("ProcMap.Cave.TemplePillar"),
									TNArt::PiecePivot(F.At(Sd * Y, -4.0), YawOf(F.D), FVector(1.0, 1.0, (F.RoofZ(Y) + 60.0) / 600.0)), { &Out.Solid });
								TNTemplePillar(Out.Solid, F.At(Sd * Y, -4.0), F.RoofZ(Y) + 60.0, 32.0, F.D, bBroken, Rng.Next(), Pal.Formation, Pal.Formation * 0.8f);
							}
							if (!bBroken && TorchLights < 8 && --NextTorch <= 0)
							{
								NextTorch = 2;
								// Pieza de arte: pie del soporte en la pared, +X hacia dentro del túnel.
								TNArt::FPieceScope TorchPiece(Out.Log, TN_ART("ProcMap.Cave.WallTorch"), TNArt::PiecePivot(F.At(Sd * (Y - 45.0), 170.0), YawOf(In)),
									{ &Out.Detail, &Out.Glow });
								const FVector Tip = TNWallTorch(Out.Detail, Out.Glow, F.At(Sd * (Y - 45.0), 170.0), In);
								Out.Flames.Add(Tip);
								++TorchLights;
								FTNCaveLight L;
								L.P = Tip + FVector(0.0, 0.0, 30.0) + In * 40.0;
								L.Color = FLinearColor(1.f, 0.58f, 0.25f);
								L.Lumens = 1500.f;
								L.Radius = 1100.f;
								Out.Lights.Add(L);
							}
							break;
						}
						if (!bChamber && s % 3 == 0)
						{
							// Pilastras pegadas a la pared en los pasos, con antorcha.
							const double Y = F.Hw - 18.0;
							const double Top = F.RoofZ(Y) + 40.0;
							{
								// Pieza de arte: pie de la pilastra, +X por el túnel; escala Z 1 = 600 hasta la bóveda.
								TNArt::FPieceScope PilasterPiece(Out.Log, TN_ART("ProcMap.Cave.Pilaster"), TNArt::PiecePivot(F.At(Sd * Y, -4.0), YawOf(F.D), FVector(1.0, 1.0, Top / 600.0)),
									{ &Out.Solid });
								Out.Solid.AddBox(F.At(Sd * Y, Top * 0.5 - 4.0), F.D, FVector(34.0, 18.0, Top * 0.5), Pal.Formation);
								Out.Solid.AddBox(F.At(Sd * Y, Top - 60.0), F.D, FVector(42.0, 24.0, 14.0), Pal.Formation * 0.85f);
							}
							if (TorchLights < 8 && --NextTorch <= 0)
							{
								NextTorch = 2;
								TNArt::FPieceScope TorchPiece(Out.Log, TN_ART("ProcMap.Cave.WallTorch"), TNArt::PiecePivot(F.At(Sd * (Y - 20.0), 165.0), YawOf(In)),
									{ &Out.Detail, &Out.Glow });
								const FVector Tip = TNWallTorch(Out.Detail, Out.Glow, F.At(Sd * (Y - 20.0), 165.0), In);
								Out.Flames.Add(Tip);
								++TorchLights;
								FTNCaveLight L;
								L.P = Tip + FVector(0.0, 0.0, 30.0) + In * 40.0;
								L.Color = FLinearColor(1.f, 0.58f, 0.25f);
								L.Lumens = 1300.f;
								L.Radius = 900.f;
								Out.Lights.Add(L);
							}
							break;
						}
						if (Rng.Chance(bChamber ? 0.7 : 0.45))
						{
							// Vasijas, ánforas y huesos contra la pared.
							FTNProcMeshBuffers Prop;
							const double Pick = Rng.Unit();
							if (Pick < 0.4) { TNPropMesh::TNPropClayPot(Prop, Rng.RangeInt(0, 2), Rng.Next()); }
							else if (Pick < 0.75) { TNPropMesh::TNPropAmphora(Prop, Rng.RangeInt(0, 2)); }
							else { TNPropMesh::TNPropBones(Prop, Rng.RangeInt(0, 1), Rng.Next()); }
							const int32 PropVerts0 = Out.Detail.Verts.Num();
							const int32 PropTris0 = Out.Detail.Tris.Num();
							TNPropMesh::TNPropAppend(Out.Detail, Prop, WallFloor(bChamber ? Rng.Range(50.0, Band * 0.6) : 30.0, Rng.Range(-100.0, 100.0)), Rng.Range(0.0, 360.0), bChamber ? 1.0 : 0.8);
							// Pieza de arte: pie del objeto, con su giro y su escala (sacados de lo construido).
							const FName PropSlot = Pick < 0.4 ? TN_ART("ProcMap.Cave.ClayPot") : (Pick < 0.75 ? TN_ART("ProcMap.Cave.Amphora") : TN_ART("ProcMap.Cave.Bones"));
							TNCaveMarkTail(Out.Log, PropSlot, Out.Detail, PropVerts0, PropTris0,
								[&Prop, bChamber](const FTNProcMeshBuffers& Tail) { return TNCavePropPivot(Prop, Tail, bChamber ? 1.0 : 0.8); });
						}
						break;
					}
					case ECaveStyle::LavaTube:
					{
						if (Rng.Chance(bChamber ? 0.65 : 0.4))
						{
							// Esquirlas de obsidiana.
							const FVector B = WallFloor(bChamber ? Rng.Range(40.0, Band * 0.6) : 22.0);
							const int32 Count = Rng.RangeInt(3, 6);
							const double Size = bChamber ? Rng.Range(100.0, 260.0) : Rng.Range(40.0, 80.0);
							// Pieza de arte (el grupo): pie de la esquirla mayor, +X hacia dentro del túnel; escala 1 = 150 de largo.
							TNArt::FPieceScope ShardsPiece(Out.Log, TN_ART("ProcMap.Cave.ObsidianShards"), TNArt::PiecePivot(B, YawOf(In), FVector(Size / 150.0)), { &Out.Solid });
							for (int32 c = 0; c < Count; ++c)
							{
								const FVector Tilt = (FVector(Rng.Range(-0.35, 0.35), Rng.Range(-0.35, 0.35), 1.0) + In * Rng.Range(0.1, 0.5)).GetSafeNormal();
								const FVector P = B + FVector(Rng.Range(-0.3, 0.3) * Size, Rng.Range(-0.3, 0.3) * Size, -6.0);
								const double L = Size * (c == 0 ? 1.0 : Rng.Range(0.4, 0.8));
								TNProcAddCylinder(Out.Solid, P, P + Tilt * L, L * Rng.Range(0.14, 0.22), 1.0, 5, Pal.Formation * static_cast<float>(Rng.Range(0.8, 1.4)));
							}
						}
						else if (bChamber && Rng.Chance(0.5))
						{
							// Columnas de basalto contra la pared.
							const int32 Count = Rng.RangeInt(3, 5);
							for (int32 c = 0; c < Count; ++c)
							{
								const int32 ColVerts0 = Out.Solid.Verts.Num();
								const int32 ColTris0 = Out.Solid.Tris.Num();
								TNRockMesh::TNRockHexColumn(Out.Solid, WallFloor(Rng.Range(40.0, Band * 0.7), Rng.Range(-120.0, 120.0)), Rng.Range(26.0, 42.0), Rng.Range(80.0, 260.0), Rng.Next(), FLinearColor(0.12f, 0.11f, 0.12f));
								// Pieza de arte: pie de la columna (sacado de lo construido).
								TNCaveMarkTail(Out.Log, TN_ART("ProcMap.Cave.BasaltColumn"), Out.Solid, ColVerts0, ColTris0, [](const FTNProcMeshBuffers& Tail) { return TNCaveBasaltPivot(Tail); });
							}
						}
						break;
					}
				}

				// Piedras sueltas al pie de la pared.
				if (Rng.Chance(0.45))
				{
					const int32 Count = Rng.RangeInt(1, 3);
					for (int32 c = 0; c < Count; ++c)
					{
						const int32 StoneVerts0 = Out.Detail.Verts.Num();
						const int32 StoneTris0 = Out.Detail.Tris.Num();
						TNProcAddBoulder(Out.Detail, WallFloor(Rng.Range(10.0, 60.0), Rng.Range(-180.0, 180.0)), Rng.Range(12.0, 34.0), Rng.Range(12.0, 30.0), Rng.Next(),
							Look.Inner * static_cast<float>(Rng.Range(1.1, 1.5)));
						// Pieza de arte: centro de la base de la piedra (sacado de lo construido).
						TNCaveMarkTail(Out.Log, TN_ART("ProcMap.Cave.LooseStone"), Out.Detail, StoneVerts0, StoneTris0,
							[](const FTNProcMeshBuffers& Tail) { return TNCaveBoulderPivot(Tail, 25.0); });
					}
				}
			}
		}

		// ── Lago de magma: anillo de basalto, coladas por la pared de su lado, grieta en la clave, luz y brasas ──
		if (Magma && MagmaS != INDEX_NONE)
		{
			const FFrame& F = Fr[MagmaS];
			const int32 Rocks = 9;
			for (int32 k = 0; k < Rocks; ++k)
			{
				const double A = TNProcMap::TwoPi * (k + Rng.Range(-0.2, 0.2)) / Rocks;
				const FVector P = Magma->Center + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * (Magma->Radius + Rng.Range(50.0, 110.0)) - FVector(0.0, 0.0, 10.0);
				const int32 RockVerts0 = Out.Solid.Verts.Num();
				const int32 RockTris0 = Out.Solid.Tris.Num();
				TNProcAddBoulder(Out.Solid, P, Rng.Range(45.0, 85.0), Rng.Range(40.0, 95.0), Rng.Next(), FLinearColor(0.08f, 0.07f, 0.08f) * static_cast<float>(Rng.Range(0.8, 1.3)));
				// Pieza de arte: centro de la base de la roca (sacado de lo construido).
				TNCaveMarkTail(Out.Log, TN_ART("ProcMap.Cave.MagmaRock"), Out.Solid, RockVerts0, RockTris0, [](const FTNProcMeshBuffers& Tail) { return TNCaveBoulderPivot(Tail, 65.0); });
			}
			// Coladas: cintas de lava que bajan por la bóveda y la pared del lado del lago hasta el suelo.
			const int32 Crown = Side / 2;
			for (int32 c = -1; c <= 1; ++c)
			{
				const int32 S0 = FMath::Clamp(MagmaS + c * 2, 0, Num - 2);
				const double Along = Rng.Range(0.2, 0.8);
				const double Wd = Rng.Range(28.0, 55.0);
				const int32 K0 = Magma->Side < 0.0 ? Crown - Rng.RangeInt(1, 2) : Crown + Rng.RangeInt(1, 2);
				const int32 Step = Magma->Side < 0.0 ? -1 : 1;
				FVector Prev = FVector::ZeroVector;
				bool bHasPrev = false;
				for (int32 k = K0; Magma->Side < 0.0 ? k >= 0 : k <= Side - 1; k += Step)
				{
					FVector P = FMath::Lerp(Inner[S0][k], Inner[S0 + 1][k], Along);
					const FVector ToAxis = (Fr[S0].At(0.0, Fr[S0].Spring) - P).GetSafeNormal();
					P += ToAxis * 6.0;
					if (bHasPrev)
					{
						const FVector Across = Fr[S0].D * (Wd * 0.5);
						Out.Ember.AddQuad(Prev - Across, Prev + Across, P + Across, P - Across, ToAxis, FLinearColor::White);
					}
					Prev = P;
					bHasPrev = true;
				}
			}
			// Grieta encendida en la clave, sobre el lago (la chimenea que sube al cráter).
			const double Yc = Magma->Side * F.Hw * 0.35;
			const FVector Vent = F.At(Yc, F.RoofZ(Yc) - 8.0);
			for (int32 k = 0; k < 10; ++k)
			{
				const double A0 = TNProcMap::TwoPi * k / 10.0, A1 = TNProcMap::TwoPi * (k + 1) / 10.0;
				const double R0 = 70.0 + 30.0 * TNProcHashNoise(k, 5, Seed), R1 = 70.0 + 30.0 * TNProcHashNoise(k + 1, 5, Seed);
				Out.Ember.AddTri(Vent, Vent + FVector(FMath::Cos(A0) * R0, FMath::Sin(A0) * R0, 0.0), Vent + FVector(FMath::Cos(A1) * R1, FMath::Sin(A1) * R1, 0.0), -FVector::UpVector, FLinearColor::White);
			}
			for (const double Up : { 120.0, 380.0 })
			{
				FTNCaveLight L;
				L.P = Magma->Center + FVector(0.0, 0.0, Up);
				L.Color = FLinearColor(1.f, 0.38f, 0.1f);
				L.Lumens = 4000.f;
				L.Radius = 2200.f;
				Out.Lights.Add(L);
			}
			Out.Flames.Add(Magma->Center + FVector(0.0, 0.0, 30.0));
		}

		// ── Grietas incandescentes junto a las paredes (tubo de lava) ──
		if (Style == ECaveStyle::LavaTube)
		{
			for (const double Sd : { -1.0, 1.0 })
			{
				for (int32 s = 1; s + 1 < Num - 1; ++s)
				{
					if (NoFloor[s] || NoFloor[s + 1]) { continue; }
					constexpr int32 Sub = 4;
					for (int32 k = 0; k < Sub; ++k)
					{
						const double T0 = static_cast<double>(k) / Sub, T1 = static_cast<double>(k + 1) / Sub;
						auto P = [&](double T, int32 Kk)
						{
							const FFrame& A = Fr[s];
							const FFrame& B = Fr[s + 1];
							const double Hw = LerpD(A.Hw, B.Hw, T);
							const double Jig = 18.0 * TNProcHashNoise(s * Sub + Kk, Sd > 0.0 ? 7 : 8, Seed);
							return FMath::Lerp(A.F, B.F, T) + FMath::Lerp(A.N, B.N, T) * (Sd * (Hw - 26.0 + Jig)) + FVector(0.0, 0.0, 2.0);
						};
						const FVector A = P(T0, k), B = P(T1, k + 1);
						const FVector Across = FVector::CrossProduct(FVector::UpVector, (B - A).GetSafeNormal()) * 5.0;
						Out.Ember.AddQuad(A - Across, B - Across, B + Across, A + Across, FVector::UpVector, FLinearColor::White);
					}
				}
			}
		}

		// ── Bocas ──
		for (int32 End = 0; End <= 1; ++End)
		{
			const int32 S = End ? Num - 1 : 0;
			const FFrame& F = Fr[S];
			const double OutSign = End ? 1.0 : -1.0;
			switch (Style)
			{
				case ECaveStyle::Temple:
				{
					// Portada: dos pilares y un dintel con símbolos y la cabeza de la tortuga en la clave.
					const FVector C = F.F + F.D * (OutSign * 70.0);
					const double Hx = F.Hw + 120.0;
					const double Top = F.Cl + 50.0;
					// Pieza de arte (pilares, dintel, símbolos y cabeza): centro del umbral, +X hacia fuera de la cueva; escala 1 =
					// 700 del eje a cada pilar y 800 de alto bajo el dintel.
					TNArt::FPieceScope GatePiece(Out.Log, TN_ART("ProcMap.Cave.TempleGate"), TNArt::PiecePivot(C, YawOf(F.D * OutSign), FVector(1.0, Hx / 700.0, Top / 800.0)),
						{ &Out.Solid, &Out.Detail, &Out.Glow });
					for (const double Ps : { -1.0, 1.0 })
					{
						TNTemplePillar(Out.Solid, C + F.N * (Ps * Hx) - FVector(0.0, 0.0, 4.0), Top, 40.0, F.D, false, Rng.Next(), Pal.Formation, Pal.Formation * 0.8f);
					}
					const FVector L0 = C + FVector(0.0, 0.0, Top + 40.0);
					Out.Solid.AddBox(L0, F.N, FVector(Hx + 90.0, 45.0, 42.0), Pal.Formation);
					Out.Solid.AddBox(L0 + FVector(0.0, 0.0, 54.0), F.N, FVector(Hx + 115.0, 56.0, 12.0), Pal.Formation * 0.85f);
					const FVector Face = F.D * OutSign;
					const FVector U = F.N * -OutSign;
					const int32 Count = FMath::Clamp(static_cast<int32>(Hx / 110.0), 2, 5);
					for (int32 g = 0; g < Count; ++g)
					{
						const double Off = (g - (Count - 1) * 0.5) * (2.0 * Hx / Count);
						if (FMath::Abs(Off) < 60.0) { continue; }
						TNCarveGlyph(Out.Detail, L0 + Face * 45.0 + U * Off, U, FVector::UpVector, Face, 26.0, static_cast<EGlyph>(Rng.RangeInt(0, static_cast<int32>(EGlyph::Count) - 1)), Pal.Accent);
					}
					// Cabeza de tortuga en la clave, mirando fuera.
					const FVector Head = L0 + Face * 60.0;
					TNPropMesh::TNPropBall(Out.Solid, Head, 38.0, Pal.Statue, 9, 5, 0.8);
					TNPropMesh::TNPropBall(Out.Solid, Head + Face * 34.0 - FVector(0.0, 0.0, 6.0), 18.0, Pal.Statue * 0.95f, 7, 3, 0.8);
					for (const double Sy : { -1.0, 1.0 }) { TNPropMesh::TNPropBall(Out.Glow, Head + Face * 26.0 + F.N * (Sy * 20.0) + FVector(0.0, 0.0, 12.0), 6.0, FLinearColor(0.35f, 0.9f, 1.f), 6, 3); }
					break;
				}
				case ECaveStyle::LavaTube:
				{
					// Columnas de basalto al pie de los dos lados de la boca.
					for (const double Ps : { -1.0, 1.0 })
					{
						const int32 Count = Rng.RangeInt(5, 9);
						for (int32 c = 0; c < Count; ++c)
						{
							const FVector P = F.F + F.D * (OutSign * Rng.Range(40.0, 520.0)) + F.N * (Ps * (F.Hw + Rng.Range(60.0, 170.0)));
							const int32 ColVerts0 = Out.Solid.Verts.Num();
							const int32 ColTris0 = Out.Solid.Tris.Num();
							TNRockMesh::TNRockHexColumn(Out.Solid, P, Rng.Range(28.0, 50.0), Rng.Range(120.0, 480.0), Rng.Next(), FLinearColor(0.12f, 0.11f, 0.12f));
							TNCaveMarkTail(Out.Log, TN_ART("ProcMap.Cave.BasaltColumn"), Out.Solid, ColVerts0, ColTris0, [](const FTNProcMeshBuffers& Tail) { return TNCaveBasaltPivot(Tail); });
						}
					}
					break;
				}
				default:
				{
					// Peñascos a los lados de la boca y, en la selva, cortina de lianas; en la caliza, musgo colgando.
					for (const double Ps : { -1.0, 1.0 })
					{
						const int32 Count = Rng.RangeInt(1, 3);
						for (int32 c = 0; c < Count; ++c)
						{
							const double R = Rng.Range(45.0, 95.0);
							const FVector P = F.F + F.D * (OutSign * Rng.Range(60.0, 480.0)) + F.N * (Ps * (F.Hw + R * 0.9 + 20.0));
							const int32 RockVerts0 = Out.Solid.Verts.Num();
							const int32 RockTris0 = Out.Solid.Tris.Num();
							TNProcAddBoulder(Out.Solid, P, R, R * Rng.Range(0.8, 1.3), Rng.Next(), Look.Outer * static_cast<float>(Rng.Range(0.85, 1.1)));
							// Pieza de arte: centro de la base del peñasco (sacado de lo construido).
							TNCaveMarkTail(Out.Log, TN_ART("ProcMap.Cave.MouthBoulder"), Out.Solid, RockVerts0, RockTris0, [](const FTNProcMeshBuffers& Tail) { return TNCaveBoulderPivot(Tail, 70.0); });
						}
					}
					if (Style == ECaveStyle::Jungle || Style == ECaveStyle::Limestone)
					{
						const bool bJungle = Style == ECaveStyle::Jungle;
						for (int32 k = 2; k < Side - 2; ++k)
						{
							const FVector Top = Inner[S][k] + F.D * (OutSign * 10.0);
							const double Room = Top.Z - F.F.Z - 260.0;
							const int32 Count = bJungle ? 2 : 1;
							for (int32 v = 0; v < Count; ++v)
							{
								const double Len = FMath::Min(Room, Rng.Range(bJungle ? 80.0 : 40.0, bJungle ? 230.0 : 110.0));
								if (Len < 25.0) { continue; }
								const FVector VineTop = Top + F.N * Rng.Range(-40.0, 40.0);
								TNArt::FPieceScope VinePiece(Out.Log, TN_ART("ProcMap.Cave.Vine"), TNArt::PiecePivot(VineTop, 0.0, FVector(Len / 150.0)), { &Out.Detail });
								TNVine(Out.Detail, VineTop, Len, Rng, bJungle ? FLinearColor(0.2f, 0.34f, 0.12f) : FLinearColor(0.3f, 0.36f, 0.22f),
									bJungle ? FLinearColor(0.22f, 0.52f, 0.16f) : FLinearColor(0.34f, 0.44f, 0.22f));
							}
						}
					}
					break;
				}
			}
		}
	}
}
