#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapMeshKit.h"

/**
 * Mallas low-poly de caras planas del gusano de arena gigante (ATN_BeachSandWorm), solo geometría (sin motor): el anillo
 * del cuerpo, la cabeza con la boca redonda y sus coronas de dientes, el labio que se abre como el pétalo de una flor, el
 * cráter de arena que deja y el remolino del aviso. Medidas en cm de juego (la tortuga mide ~1,4 m: boca de 6 m de
 * diámetro); colores en sRGB. Los ejes: el cuerpo, la cabeza y el labio van a lo largo de su X local.
 */
namespace TNSandWormMeshes
{
	using TNProcMesh::FTNProcMeshBuffers;

	/** Radio del borde de la boca (6 m de diámetro) y del cuerpo. */
	constexpr double MouthRadius = 300.0;
	constexpr double BodyRadius = 270.0;
	/** Del borde de la boca a la unión con el cuerpo, y largo de la malla de cada anillo (se solapan en la columna). */
	constexpr double HeadLength = 280.0;
	constexpr double SegmentLength = 300.0;
	/** Labios de la boca y largo de cada uno (con el labio cerrado, la punta llega al centro). */
	constexpr int32 LipCount = 4;
	constexpr double LipLength = MouthRadius * 1.22;
	/** Radio del borde en el que van las bisagras de los labios. */
	constexpr double LipHingeRadius = MouthRadius * 1.0;

	inline FLinearColor WormRgb(float R, float G, float B)
	{
		return FLinearColor(R, G, B, 1.f);
	}

	/** Colores de un gusano. */
	struct FWormLook
	{
		/** Piel arenosa, surcos entre anillos, verrugas. */
		FLinearColor Skin = WormRgb(0.8f, 0.63f, 0.42f);
		FLinearColor Band = WormRgb(0.52f, 0.38f, 0.25f);
		FLinearColor Wart = WormRgb(0.66f, 0.49f, 0.32f);
		/** Carne de la boca y de los labios, garganta (casi negra) y dientes. */
		FLinearColor Flesh = WormRgb(0.9f, 0.4f, 0.42f);
		FLinearColor Throat = WormRgb(0.22f, 0.04f, 0.06f);
		FLinearColor Tooth = WormRgb(0.98f, 0.95f, 0.84f);
	};

	/** Tres paletas: arena tostada, rosa de lombriz y gris de duna. */
	inline FWormLook WormPalette(int32 Index)
	{
		FWormLook L;
		switch (((Index % 3) + 3) % 3)
		{
		case 1:
			L.Skin = WormRgb(0.86f, 0.58f, 0.52f); L.Band = WormRgb(0.6f, 0.34f, 0.32f); L.Wart = WormRgb(0.74f, 0.46f, 0.42f);
			L.Flesh = WormRgb(0.95f, 0.48f, 0.5f);
			break;
		case 2:
			L.Skin = WormRgb(0.68f, 0.64f, 0.52f); L.Band = WormRgb(0.42f, 0.4f, 0.32f); L.Wart = WormRgb(0.55f, 0.52f, 0.42f);
			L.Flesh = WormRgb(0.86f, 0.36f, 0.38f);
			break;
		default:
			break;
		}
		return L;
	}

	/** Tono estable por cara (0,9..1,1) para que la piel no sea plana. */
	inline float FaceTone(int32 Index, uint32 Seed)
	{
		return 1.f + (TNProcMesh::TNProcTone(Index, Seed) - 1.f) * 0.55f;
	}

	/** Verruga: pirámide de 4 caras que sale de Base hacia Dir. */
	inline void AddWart(FTNProcMeshBuffers& M, const FVector& Base, const FVector& Dir, double Size, const FLinearColor& Color)
	{
		TNProcMesh::TNProcAddCylinder(M, Base - Dir * (Size * 0.3), Base + Dir * Size, Size * 0.75, Size * 0.12, 4, Color, false);
	}

	/** Diente: cono de 4 caras de Base a Tip. */
	inline void AddTooth(FTNProcMeshBuffers& M, const FVector& Base, const FVector& Tip, double BaseRadius, const FLinearColor& Color)
	{
		TNProcMesh::TNProcAddCylinder(M, Base, Tip, BaseRadius, BaseRadius * 0.08, 4, Color, true);
	}

	/**
	 * Anillo del cuerpo a lo largo de X, centrado en el origen: surcos oscuros en los dos extremos, panza en medio, caras
	 * con tonos sueltos y cuatro verrugas.
	 */
	inline void BuildSegment(FTNProcMeshBuffers& M, const FWormLook& L, uint32 Seed)
	{
		constexpr int32 Sides = 11;
		constexpr int32 NumRings = 7;
		const double R = BodyRadius;
		const double H = SegmentLength * 0.5;
		const double RingX[NumRings] = { -H, -H * 0.78, -H * 0.42, 0.0, H * 0.42, H * 0.78, H };
		const double RingK[NumRings] = { 0.84, 0.95, 1.0, 1.035, 1.0, 0.95, 0.84 };
		TArray<FVector> Rings[NumRings];
		for (int32 r = 0; r < NumRings; ++r)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				const double A = UE_DOUBLE_TWO_PI * k / Sides;
				const double Rad = R * RingK[r] * (1.0 + 0.035 * TNProcMesh::TNProcHashNoise(r, k, Seed));
				Rings[r].Add(FVector(RingX[r], FMath::Cos(A) * Rad, FMath::Sin(A) * Rad));
			}
		}
		for (int32 r = 0; r + 1 < NumRings; ++r)
		{
			const bool bGroove = r == 0 || r == NumRings - 2;
			for (int32 k = 0; k < Sides; ++k)
			{
				const int32 K1 = (k + 1) % Sides;
				const FVector Mid = (Rings[r][k] + Rings[r][K1] + Rings[r + 1][k] + Rings[r + 1][K1]) * 0.25;
				const FVector Out(0.0, Mid.Y, Mid.Z);
				const FLinearColor Color = bGroove ? L.Band : L.Skin * FaceTone(r * 31 + k, Seed);
				M.AddQuad(Rings[r][k], Rings[r][K1], Rings[r + 1][K1], Rings[r + 1][k], Out, Color);
			}
		}
		// Tapas oscuras (no se ven: los anillos se solapan, pero así nunca hay un agujero).
		for (int32 End = 0; End < 2; ++End)
		{
			const TArray<FVector>& Ring = End == 0 ? Rings[0] : Rings[NumRings - 1];
			const FVector Center(End == 0 ? -H : H, 0.0, 0.0);
			const FVector Facing(End == 0 ? -1.0 : 1.0, 0.0, 0.0);
			for (int32 k = 0; k < Sides; ++k)
			{
				M.AddTri(Center, Ring[k], Ring[(k + 1) % Sides], Facing, L.Band * 0.8f);
			}
		}
		// Verrugas repartidas por la panza del anillo.
		for (int32 w = 0; w < 4; ++w)
		{
			const double A = UE_DOUBLE_TWO_PI * (w + 0.5 * TNProcMesh::TNProcHashNoise(w, 3, Seed + 17u)) / 4.0;
			const double X = H * 0.35 * TNProcMesh::TNProcHashNoise(w, 5, Seed + 23u);
			const FVector Dir(0.0, FMath::Cos(A), FMath::Sin(A));
			AddWart(M, FVector(X, 0.0, 0.0) + Dir * (R * 1.02), Dir, R * 0.11, L.Wart);
		}
	}

	/**
	 * Cabeza: collar exterior de la unión con el cuerpo (x = -HeadLength) al borde de la boca (x = 0), encía, pared de
	 * dentro que se estrecha hacia la garganta oscura y tres coronas de dientes que apuntan hacia dentro.
	 */
	inline void BuildHead(FTNProcMeshBuffers& M, const FWormLook& L, uint32 Seed)
	{
		constexpr int32 Sides = 14;
		const double R = MouthRadius;
		const double HL = HeadLength;
		auto RingAt = [](double X, double Radius, int32 Count, double Twist, TArray<FVector>& Out)
		{
			Out.Reset();
			for (int32 k = 0; k < Count; ++k)
			{
				const double A = UE_DOUBLE_TWO_PI * (k + Twist) / Count;
				Out.Add(FVector(X, FMath::Cos(A) * Radius, FMath::Sin(A) * Radius));
			}
		};
		// Collar exterior.
		constexpr int32 NumOuter = 5;
		const double OuterX[NumOuter] = { -HL, -HL * 0.7, -HL * 0.38, -HL * 0.1, 0.0 };
		const double OuterK[NumOuter] = { 0.88, 0.98, 1.05, 1.1, 1.06 };
		TArray<FVector> Outer[NumOuter];
		for (int32 r = 0; r < NumOuter; ++r)
		{
			RingAt(OuterX[r], R * OuterK[r], Sides, 0.0, Outer[r]);
		}
		for (int32 r = 0; r + 1 < NumOuter; ++r)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				const int32 K1 = (k + 1) % Sides;
				const FVector Mid = (Outer[r][k] + Outer[r][K1]) * 0.5;
				const FLinearColor Color = r == 0 ? L.Band : L.Skin * FaceTone(r * 17 + k, Seed + 5u);
				M.AddQuad(Outer[r][k], Outer[r][K1], Outer[r + 1][K1], Outer[r + 1][k], FVector(0.0, Mid.Y, Mid.Z), Color);
			}
		}
		// Encía (del borde exterior al interior) y pared de dentro, cada vez más oscura hacia la garganta.
		constexpr int32 NumInner = 5;
		const double InnerX[NumInner] = { 0.0, -HL * 0.04, -HL * 0.3, -HL * 0.6, -HL * 0.85 };
		const double InnerK[NumInner] = { 1.06, 0.86, 0.74, 0.58, 0.44 };
		TArray<FVector> Inner[NumInner];
		for (int32 r = 0; r < NumInner; ++r)
		{
			RingAt(InnerX[r], R * InnerK[r], Sides, 0.0, Inner[r]);
		}
		for (int32 r = 0; r + 1 < NumInner; ++r)
		{
			const float Depth = static_cast<float>(r) / static_cast<float>(NumInner - 2);
			const FLinearColor Color = r == 0 ? L.Flesh * 1.08f : TNProcMesh::TNProcLerpColor(L.Flesh, L.Throat, Depth * 0.8f);
			for (int32 k = 0; k < Sides; ++k)
			{
				const int32 K1 = (k + 1) % Sides;
				const FVector Mid = (Inner[r][k] + Inner[r][K1] + Inner[r + 1][k] + Inner[r + 1][K1]) * 0.25;
				// Hacia el eje y un poco hacia fuera de la boca: se ve desde delante.
				const FVector Hint(0.35, -Mid.Y, -Mid.Z);
				M.AddQuad(Inner[r][k], Inner[r][K1], Inner[r + 1][K1], Inner[r + 1][k], r == 0 ? FVector(1.0, 0.0, 0.0) : Hint, Color);
			}
		}
		// Garganta: embudo oscuro que se cierra.
		const FVector ThroatTip(-HL * 0.98, 0.0, 0.0);
		const TArray<FVector>& Last = Inner[NumInner - 1];
		for (int32 k = 0; k < Sides; ++k)
		{
			M.AddTri(ThroatTip, Last[k], Last[(k + 1) % Sides], FVector(1.0, 0.0, 0.0), L.Throat);
		}
		// Coronas de dientes: la del borde grande, las de dentro más pequeñas y a medio paso.
		struct FToothRing
		{
			double X;
			double Radius;
			int32 Count;
			double Length;
		};
		const FToothRing ToothRings[3] = {
			{ -HL * 0.06, R * 0.86, 16, R * 0.3 },
			{ -HL * 0.32, R * 0.73, 12, R * 0.24 },
			{ -HL * 0.6, R * 0.57, 9, R * 0.18 },
		};
		for (int32 j = 0; j < 3; ++j)
		{
			const FToothRing& TR = ToothRings[j];
			for (int32 t = 0; t < TR.Count; ++t)
			{
				const double A = UE_DOUBLE_TWO_PI * (t + 0.5 * (j % 2)) / TR.Count;
				const FVector Radial(0.0, FMath::Cos(A), FMath::Sin(A));
				const FVector Base = FVector(TR.X, 0.0, 0.0) + Radial * (TR.Radius + 8.0);
				const FVector Tip = Base - Radial * (TR.Length * 0.85) + FVector(TR.Length * 0.5, 0.0, 0.0);
				AddTooth(M, Base, Tip, TR.Length * 0.3, L.Tooth * (0.92f + 0.08f * static_cast<float>(t % 2)));
			}
		}
		// Verrugas del collar.
		for (int32 w = 0; w < 6; ++w)
		{
			const double A = UE_DOUBLE_TWO_PI * (w + 0.4 * TNProcMesh::TNProcHashNoise(w, 1, Seed + 29u)) / 6.0;
			const FVector Dir(0.0, FMath::Cos(A), FMath::Sin(A));
			AddWart(M, FVector(-HL * 0.45, 0.0, 0.0) + Dir * (R * 1.06), Dir, R * 0.1, L.Wart);
		}
	}

	/**
	 * Labio (pétalo): de la bisagra (x = 0) a la punta (x = LipLength), de ancho que se estrecha hasta la punta, abombado
	 * hacia +Z (la piel) y con la carne rosa hacia -Z; la punta se curva hacia fuera. Dientes en los bordes de la cara de
	 * carne. Cerrados, los labios forman una cúpula sobre la boca; abiertos, una flor con la carne y los dientes a la vista.
	 */
	inline void BuildLip(FTNProcMeshBuffers& M, const FWormLook& L, uint32 Seed)
	{
		constexpr int32 NX = 6;
		constexpr int32 NY = 4;
		const double R = MouthRadius;
		const double Len = LipLength;
		const double HalfW = UE_DOUBLE_PI * LipHingeRadius / LipCount * 1.08;
		const double Thick = R * 0.13;
		FVector Outer[NX + 1][NY + 1];
		FVector Inner[NX + 1][NY + 1];
		double Width[NX + 1];
		double MidZ[NX + 1];
		for (int32 i = 0; i <= NX; ++i)
		{
			const double U = static_cast<double>(i) / NX;
			Width[i] = HalfW * FMath::Pow(FMath::Max(0.0, 1.0 - FMath::Pow(U, 1.7)), 0.7);
			const double Bulge = R * 0.16 * (1.0 - 0.5 * U);
			const double Curl = R * 0.2 * U * U;
			const double T = Thick * (1.0 - 0.7 * U);
			MidZ[i] = Curl;
			for (int32 j = 0; j <= NY; ++j)
			{
				const double V = -1.0 + 2.0 * j / NY;
				const double Z = Bulge * (1.0 - V * V) + Curl;
				const double X = Len * U;
				const double Y = Width[i] * V;
				Outer[i][j] = FVector(X, Y, Z + T * 0.5);
				Inner[i][j] = FVector(X, Y, Z - T * 0.5);
			}
		}
		for (int32 i = 0; i < NX; ++i)
		{
			for (int32 j = 0; j < NY; ++j)
			{
				const bool bRidge = j == NY / 2 - 1 || j == NY / 2;
				const FLinearColor SkinColor = (bRidge && i < NX - 1 ? L.Skin * 0.92f : L.Skin) * FaceTone(i * 13 + j, Seed + 41u);
				M.AddQuad(Outer[i][j], Outer[i + 1][j], Outer[i + 1][j + 1], Outer[i][j + 1], FVector(0.0, 0.0, 1.0), SkinColor);
				const FLinearColor FleshColor = L.Flesh * (0.9f + 0.1f * static_cast<float>((i + j) % 2));
				M.AddQuad(Inner[i][j], Inner[i + 1][j], Inner[i + 1][j + 1], Inner[i][j + 1], FVector(0.0, 0.0, -1.0), FleshColor);
			}
			// Bordes de los lados.
			M.AddQuad(Outer[i][0], Outer[i + 1][0], Inner[i + 1][0], Inner[i][0], FVector(0.0, -1.0, 0.0), L.Band);
			M.AddQuad(Outer[i][NY], Outer[i + 1][NY], Inner[i + 1][NY], Inner[i][NY], FVector(0.0, 1.0, 0.0), L.Band);
		}
		// Borde de la bisagra.
		for (int32 j = 0; j < NY; ++j)
		{
			M.AddQuad(Outer[0][j], Outer[0][j + 1], Inner[0][j + 1], Inner[0][j], FVector(-1.0, 0.0, 0.0), L.Band);
		}
		// Dientes en los dos bordes de la carne, hacia dentro de la boca y hacia el centro del labio; uno en la punta.
		const double ToothLen = R * 0.17;
		for (int32 i = 1; i < NX; ++i)
		{
			for (const double Side : { -1.0, 1.0 })
			{
				const int32 j = Side < 0.0 ? 0 : NY;
				const FVector Base = Inner[i][j] + FVector(0.0, -Side * Width[i] * 0.12, 4.0);
				const FVector Tip = Base + FVector(ToothLen * 0.15, -Side * ToothLen * 0.35, -ToothLen);
				AddTooth(M, Base, Tip, ToothLen * 0.28, L.Tooth);
			}
		}
		const FVector TipBase(Len * 0.92, 0.0, MidZ[NX] - Thick * 0.1);
		AddTooth(M, TipBase, TipBase + FVector(ToothLen * 0.3, 0.0, -ToothLen * 1.1), ToothLen * 0.3, L.Tooth);
	}

	/**
	 * Cráter de radio 100 cm (se escala): falda que se mete en la arena, reborde abultado y más claro, y dentro un fondo
	 * casi plano, a ras de arena, cada vez más oscuro hacia el centro. El terreno no se agujerea: el hoyo es el oscuro (lo
	 * que quedara por debajo de la arena no se vería).
	 */
	inline void BuildCrater(FTNProcMeshBuffers& M, uint32 Seed)
	{
		constexpr int32 Sides = 22;
		constexpr int32 NumRings = 8;
		const FLinearColor Sand = WormRgb(0.86f, 0.76f, 0.52f);
		const FLinearColor Light = WormRgb(0.93f, 0.84f, 0.61f);
		const FLinearColor Damp = WormRgb(0.6f, 0.48f, 0.31f);
		const FLinearColor Deep = WormRgb(0.2f, 0.14f, 0.08f);
		const double RingR[NumRings] = { 1.25, 1.0, 0.84, 0.7, 0.58, 0.45, 0.28, 0.12 };
		const double RingZ[NumRings] = { -6.0, 0.0, 16.0, 22.0, 12.0, 3.0, 2.0, 1.5 };
		const FLinearColor RingColor[NumRings] = { Sand, Sand, Light, Light, Sand * 0.9f, Damp, Damp * 0.6f, Deep };
		TArray<FVector> Rings[NumRings];
		for (int32 r = 0; r < NumRings; ++r)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				const double A = UE_DOUBLE_TWO_PI * k / Sides;
				const double Rad = 100.0 * RingR[r] * (1.0 + (r > 0 ? 0.07 : 0.02) * TNProcMesh::TNProcHashNoise(r, k, Seed));
				const double Z = RingZ[r] + (r > 0 && r < 5 ? 4.0 * TNProcMesh::TNProcHashNoise(k, r, Seed + 3u) : 0.0);
				Rings[r].Add(FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, Z));
			}
		}
		for (int32 r = 0; r + 1 < NumRings; ++r)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				const int32 K1 = (k + 1) % Sides;
				const FLinearColor Color = TNProcMesh::TNProcLerpColor(RingColor[r], RingColor[r + 1], 0.5f) * FaceTone(r * 7 + k, Seed + 9u);
				M.AddQuad(Rings[r][k], Rings[r][K1], Rings[r + 1][K1], Rings[r + 1][k], FVector::UpVector, Color);
			}
		}
		const FVector Hole(0.0, 0.0, 1.2);
		for (int32 k = 0; k < Sides; ++k)
		{
			M.AddTri(Hole, Rings[NumRings - 1][k], Rings[NumRings - 1][(k + 1) % Sides], FVector::UpVector, Deep * 0.8f);
		}
	}

	/**
	 * Remolino del aviso (radio 100 cm): disco a ras de arena con brazos de espiral de dos tonos que se oscurecen hacia el
	 * centro (parece que se hunde; lo que se hunde de verdad es la tortuga, dentro de la arena). Se gira entero.
	 */
	inline void BuildWhirl(FTNProcMeshBuffers& M)
	{
		constexpr int32 Sides = 24;
		constexpr int32 NumRings = 6;
		const FLinearColor Light = WormRgb(0.9f, 0.8f, 0.57f);
		const FLinearColor Dark = WormRgb(0.62f, 0.5f, 0.33f);
		TArray<FVector> Rings[NumRings + 1];
		for (int32 r = 0; r <= NumRings; ++r)
		{
			const double Rr = 1.0 - static_cast<double>(r) / NumRings;
			// Un pelo más alto en el borde (la arena que se escurre) y casi plano dentro.
			const double Z = 1.0 + 2.5 * Rr * Rr;
			for (int32 k = 0; k < Sides; ++k)
			{
				// Cada anillo va algo girado respecto al de fuera: los brazos se enroscan hacia el centro.
				const double A = UE_DOUBLE_TWO_PI * (k + 0.9 * r) / Sides;
				Rings[r].Add(FVector(FMath::Cos(A) * 100.0 * Rr, FMath::Sin(A) * 100.0 * Rr, Z));
			}
		}
		for (int32 r = 0; r < NumRings; ++r)
		{
			const float Shade = 1.f - 0.11f * static_cast<float>(r);
			for (int32 k = 0; k < Sides; ++k)
			{
				const int32 K1 = (k + 1) % Sides;
				const bool bArm = ((k / 3) % 2) == 0;
				M.AddQuad(Rings[r][k], Rings[r][K1], Rings[r + 1][K1], Rings[r + 1][k], FVector::UpVector, (bArm ? Light : Dark) * Shade);
			}
		}
	}
}
