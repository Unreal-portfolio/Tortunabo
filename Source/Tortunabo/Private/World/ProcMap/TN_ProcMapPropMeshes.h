#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapFlora.h"
#include "TN_ProcMapMeshKit.h"

/**
 * Props low-poly del mapa (color de vértice, caras planas): objetos sueltos junto al camino que se
 * instancian como la vegetación (cajas, barriles, vallas, conos, pacas, bancos, farolas, buzones,
 * sacos, macetas, conchas, estrellas de mar, cubos de playa, toallas, tablas de surf, sombrillas,
 * troncos a la deriva, cocos, salvavidas, setas, vasijas, antorchas, calaveras, huesos, ánforas,
 * ruedas de carro, postes indicadores, plantas rodadoras, cristales, tocones, hitos de piedra,
 * faroles y nasas) y las piezas que se juntan en los obstáculos del camino. Local: base en el
 * origen, Z arriba, X al frente, cm a escala 1. Sin dependencias del motor más allá de CoreMinimal.
 */
namespace TNPropMesh
{
	using namespace TNProcMesh;
	using TNProcMap::LerpD;
	using TNProcMap::EPropKind;
	using TNProcMap::EPathProp;

	namespace PropColors
	{
		const FLinearColor Wood(0.46f, 0.3f, 0.16f);
		const FLinearColor WoodDark(0.28f, 0.18f, 0.09f);
		const FLinearColor WoodLight(0.66f, 0.5f, 0.3f);
		const FLinearColor Driftwood(0.72f, 0.66f, 0.56f);
		const FLinearColor Iron(0.2f, 0.2f, 0.22f);
		const FLinearColor Steel(0.55f, 0.57f, 0.6f);
		const FLinearColor Rope(0.72f, 0.62f, 0.42f);
		const FLinearColor Red(0.78f, 0.1f, 0.07f);
		const FLinearColor White(0.92f, 0.92f, 0.9f);
		const FLinearColor Yellow(0.97f, 0.76f, 0.1f);
		const FLinearColor Orange(0.97f, 0.42f, 0.06f);
		const FLinearColor Blue(0.12f, 0.36f, 0.78f);
		const FLinearColor Teal(0.08f, 0.6f, 0.62f);
		const FLinearColor Green(0.16f, 0.56f, 0.2f);
		const FLinearColor Pink(0.96f, 0.5f, 0.58f);
		const FLinearColor Purple(0.45f, 0.2f, 0.62f);
		const FLinearColor Terracotta(0.72f, 0.36f, 0.2f);
		const FLinearColor Bone(0.88f, 0.84f, 0.72f);
		const FLinearColor Straw(0.86f, 0.7f, 0.33f);
		const FLinearColor Burlap(0.64f, 0.53f, 0.36f);
		const FLinearColor Stone(0.55f, 0.53f, 0.5f);
		const FLinearColor Sand(0.9f, 0.79f, 0.55f);
		const FLinearColor Black(0.04f, 0.04f, 0.05f);
		const FLinearColor Glow(1.0f, 0.82f, 0.4f);
		const FLinearColor Flame(1.0f, 0.55f, 0.08f);
		const FLinearColor Leaf(0.2f, 0.5f, 0.16f);
	}

	/** Número pseudoaleatorio estable en [Lo, Hi]. */
	inline double TNPropRand(uint32 Seed, int32 K, double Lo, double Hi)
	{
		return LerpD(Lo, Hi, 0.5 + 0.5 * TNProcHashNoise(K, 71, Seed));
	}

	/** Caja con ejes cualesquiera (ortonormales) y semilados Half. */
	inline void TNPropBox(FTNProcMeshBuffers& M, const FVector& C, const FVector& Ax, const FVector& Ay, const FVector& Az, const FVector& Half, const FLinearColor& Color)
	{
		auto P = [&](double Sx, double Sy, double Sz) { return C + Ax * (Sx * Half.X) + Ay * (Sy * Half.Y) + Az * (Sz * Half.Z); };
		M.AddQuad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Az, Color);
		M.AddQuad(P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), -Az, Color);
		M.AddQuad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), Ax, Color);
		M.AddQuad(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), -Ax, Color);
		M.AddQuad(P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), Ay, Color);
		M.AddQuad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), -Ay, Color);
	}

	/** Caja alineada con los ejes locales (Z arriba) girada Yaw grados. */
	inline void TNPropYawBox(FTNProcMeshBuffers& M, const FVector& C, double YawDeg, const FVector& Half, const FLinearColor& Color)
	{
		const double A = FMath::DegreesToRadians(YawDeg);
		M.AddBox(C, FVector(FMath::Cos(A), FMath::Sin(A), 0.0), Half, Color);
	}

	/** Toro de eje Axis (aro, salvavidas, rueda): NMaj tramos alrededor y NMin en la sección; colores alternos en Stripes grupos. */
	inline void TNPropTorus(FTNProcMeshBuffers& M, const FVector& C, const FVector& Axis, double R, double Rs, int32 NMaj, int32 NMin,
		const FLinearColor& A, const FLinearColor& B, int32 Stripes)
	{
		const FVector N = Axis.GetSafeNormal();
		const FVector U = FVector::CrossProduct(N, FMath::Abs(N.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(N, U);
		auto P = [&](int32 i, int32 j)
		{
			const double Ta = TNProcMap::TwoPi * i / NMaj;
			const double Tb = TNProcMap::TwoPi * j / NMin;
			const FVector Radial = U * FMath::Cos(Ta) + V * FMath::Sin(Ta);
			return C + Radial * (R + Rs * FMath::Cos(Tb)) + N * (Rs * FMath::Sin(Tb));
		};
		for (int32 i = 0; i < NMaj; ++i)
		{
			const double Ta = TNProcMap::TwoPi * (i + 0.5) / NMaj;
			const FVector Radial = U * FMath::Cos(Ta) + V * FMath::Sin(Ta);
			const FLinearColor& Col = (Stripes > 0 && ((i * Stripes / NMaj) % 2)) ? B : A;
			for (int32 j = 0; j < NMin; ++j)
			{
				const FVector Mid = (P(i, j) + P(i + 1, j + 1)) * 0.5;
				const FVector Ring = C + Radial * R;
				M.AddQuad(P(i, j), P(i + 1, j), P(i + 1, j + 1), P(i, j + 1), Mid - Ring, Col);
			}
		}
	}

	/** Esfera de caras planas (Seg lados, Rings anillos). */
	inline void TNPropBall(FTNProcMeshBuffers& M, const FVector& C, double R, const FLinearColor& Color, int32 Seg = 7, int32 Rings = 4, double Squash = 1.0)
	{
		TArray<double> Z, Rad;
		for (int32 k = 0; k <= Rings; ++k)
		{
			const double A = -HALF_PI + PI * k / Rings;
			Z.Add(R * Squash * (1.0 + FMath::Sin(A)));
			Rad.Add(FMath::Max(0.5, R * FMath::Cos(A)));
		}
		TNProcAddLathe(M, C - FVector(0.0, 0.0, R * Squash), Z, Rad, 0.0, 0u, Color, Seg);
	}

	/** Copia Src en Dst girado Yaw (grados), escalado y desplazado a Offset. */
	inline void TNPropAppend(FTNProcMeshBuffers& Dst, const FTNProcMeshBuffers& Src, const FVector& Offset, double YawDeg, double Scale = 1.0)
	{
		const double A = FMath::DegreesToRadians(YawDeg);
		const double Ca = FMath::Cos(A), Sa = FMath::Sin(A);
		auto Rot = [&](const FVector& V) { return FVector(V.X * Ca - V.Y * Sa, V.X * Sa + V.Y * Ca, V.Z); };
		const int32 Base = Dst.Verts.Num();
		for (int32 i = 0; i < Src.Verts.Num(); ++i)
		{
			Dst.Verts.Add(Offset + Rot(Src.Verts[i]) * Scale);
			Dst.Normals.Add(Rot(Src.Normals[i]));
			Dst.UVs.Add(Src.UVs[i]);
			Dst.Colors.Add(Src.Colors[i]);
		}
		for (const int32 T : Src.Tris) { Dst.Tris.Add(Base + T); }
	}

	// ── Recetas ──────────────────────────────────────────────────────────────────

	/** Caja de madera de ~1 m: cuerpo, cantoneras y aspa en dos caras. */
	inline void TNPropCrate(FTNProcMeshBuffers& M, double Size, const FLinearColor& Body, const FLinearColor& Frame)
	{
		const double H = Size * 0.5;
		M.AddBox(FVector(0.0, 0.0, H), FVector(1.0, 0.0, 0.0), FVector(H - 3.0, H - 3.0, H - 3.0), Body);
		const double E = Size * 0.06;
		for (const double Sx : { -1.0, 1.0 })
		{
			for (const double Sy : { -1.0, 1.0 })
			{
				M.AddBox(FVector(Sx * (H - E), Sy * (H - E), H), FVector(1.0, 0.0, 0.0), FVector(E, E, H), Frame);
				M.AddBox(FVector(Sx * (H - E), 0.0, H + Sy * (H - E)), FVector(1.0, 0.0, 0.0), FVector(E, H, E), Frame);
				M.AddBox(FVector(0.0, Sx * (H - E), H + Sy * (H - E)), FVector(1.0, 0.0, 0.0), FVector(H, E, E), Frame);
			}
		}
		for (const double Sy : { -1.0, 1.0 })
		{
			const double Y = Sy * (H - 1.0);
			M.AddBeam(FVector(-H + E, Y, E * 1.5), FVector(H - E, Y, Size - E * 1.5), E * 0.7, Frame);
		}
	}

	inline void TNPropWoodBarrel(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const FLinearColor Staves = Variant == 2 ? Blue : (Variant == 1 ? WoodDark : Wood);
		const TArray<double> Z = { 0.0, 18.0, 45.0, 72.0, 92.0, 104.0 };
		const TArray<double> R = { 31.0, 36.0, 40.0, 38.0, 34.0, 30.0 };
		TNProcAddLathe(M, FVector::ZeroVector, Z, R, 0.0, 0u, Staves, 12);
		for (const double Hz : { 14.0, 90.0 })
		{
			TNProcAddCylinder(M, FVector(0.0, 0.0, Hz - 3.0), FVector(0.0, 0.0, Hz + 3.0), 36.5 + (Hz > 50.0 ? -1.0 : 0.0), 36.5, 12, Iron, false);
		}
		if (Variant == 2)
		{
			TNProcAddCylinder(M, FVector(0.0, 0.0, 50.0), FVector(0.0, 0.0, 58.0), 40.5, 40.5, 12, White, false);
		}
	}

	/** Valla de obra: tablero a franjas rojas y blancas sobre dos caballetes, con luz naranja. */
	inline void TNPropBarricade(FTNProcMeshBuffers& M, double Len)
	{
		using namespace PropColors;
		const double Hl = Len * 0.5;
		for (const double Sx : { -1.0, 1.0 })
		{
			const double X = Sx * (Hl - 25.0);
			M.AddBeam(FVector(X, -28.0, 0.0), FVector(X, 0.0, 95.0), 3.5, White);
			M.AddBeam(FVector(X, 28.0, 0.0), FVector(X, 0.0, 95.0), 3.5, White);
			M.AddBeam(FVector(X, -18.0, 35.0), FVector(X, 18.0, 35.0), 2.5, White);
		}
		const int32 N = FMath::Max(3, FMath::RoundToInt32(Len / 45.0));
		for (int32 i = 0; i < N; ++i)
		{
			const double X0 = -Hl + Len * i / N;
			const double X1 = -Hl + Len * (i + 1) / N;
			const FLinearColor& Col = (i % 2) ? White : Red;
			// Franjas en diagonal: cada tramo es un paralelogramo (inclinación de un tramo).
			const double K = (X1 - X0) * 0.5;
			for (const double Sy : { -1.0, 1.0 })
			{
				const double Y = Sy * 3.0;
				M.AddQuad(FVector(X0, Y, 75.0), FVector(X1, Y, 75.0), FVector(X1 + K, Y, 105.0), FVector(X0 + K, Y, 105.0), FVector(0.0, Sy, 0.0), Col);
			}
		}
		M.AddBox(FVector(Hl * 0.5, 0.0, 70.0 + 3.0), FVector(1.0, 0.0, 0.0), FVector(Hl * 0.5 + 2.0, 3.0, 2.0), White);
		TNProcAddCylinder(M, FVector(-Hl + 25.0, 0.0, 106.0), FVector(-Hl + 25.0, 0.0, 122.0), 7.0, 6.0, 8, Orange);
	}

	/** Cono de tráfico naranja con franjas blancas y base cuadrada. */
	inline void TNPropTrafficCone(FTNProcMeshBuffers& M)
	{
		using namespace PropColors;
		M.AddBox(FVector(0.0, 0.0, 2.5), FVector(1.0, 0.0, 0.0), FVector(22.0, 22.0, 2.5), Black);
		const double Zs[] = { 5.0, 20.0, 28.0, 40.0, 48.0, 70.0 };
		const double Rs[] = { 16.0, 12.8, 11.2, 8.8, 7.2, 3.0 };
		for (int32 k = 0; k + 1 < 6; ++k)
		{
			TNProcAddCylinder(M, FVector(0.0, 0.0, Zs[k]), FVector(0.0, 0.0, Zs[k + 1]), Rs[k], Rs[k + 1], 10, (k == 1 || k == 3) ? White : Orange, k == 4);
		}
	}

	/** Paca de paja: redonda tumbada (variante 0) o rectangular con cordeles. */
	inline void TNPropHayBale(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		if (Variant == 0)
		{
			TNProcAddCylinder(M, FVector(0.0, -60.0, 70.0), FVector(0.0, 60.0, 70.0), 70.0, 70.0, 12, Straw);
			for (const double Y : { -35.0, 0.0, 35.0 })
			{
				TNProcAddCylinder(M, FVector(0.0, Y - 2.0, 70.0), FVector(0.0, Y + 2.0, 70.0), 71.0, 71.0, 12, Straw * 0.8f, false);
			}
			return;
		}
		const int32 Layers = Variant == 1 ? 1 : 2;
		for (int32 l = 0; l < Layers; ++l)
		{
			const double Z = l * 45.0 + 22.5;
			const double Yaw = l * 90.0;
			TNPropYawBox(M, FVector(0.0, 0.0, Z), Yaw, FVector(55.0, 28.0, 22.0), Straw);
			for (const double Sx : { -20.0, 20.0 })
			{
				const double A = FMath::DegreesToRadians(Yaw);
				TNPropYawBox(M, FVector(Sx * FMath::Cos(A), Sx * FMath::Sin(A), Z), Yaw, FVector(1.5, 29.0, 23.0), Straw * 0.6f);
			}
		}
	}

	/** Banco de parque: tres tablas de asiento, respaldo de dos y patas de hierro. */
	inline void TNPropBench(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const double Len = Variant == 2 ? 120.0 : 170.0;
		const FLinearColor Boards = Variant == 1 ? Green : Wood;
		for (int32 b = 0; b < 3; ++b)
		{
			M.AddBox(FVector(0.0, -14.0 + b * 14.0, 45.0), FVector(1.0, 0.0, 0.0), FVector(Len * 0.5, 6.0, 2.5), Boards);
		}
		for (int32 b = 0; b < 2; ++b)
		{
			M.AddBox(FVector(0.0, 24.0, 62.0 + b * 16.0), FVector(1.0, 0.0, 0.0), FVector(Len * 0.5, 2.5, 6.0), Boards);
		}
		for (const double Sx : { -1.0, 1.0 })
		{
			const double X = Sx * (Len * 0.5 - 15.0);
			M.AddBeam(FVector(X, -18.0, 0.0), FVector(X, -18.0, 43.0), 3.0, Iron);
			M.AddBeam(FVector(X, 20.0, 0.0), FVector(X, 24.0, 88.0), 3.0, Iron);
			M.AddBeam(FVector(X, -18.0, 42.0), FVector(X, 22.0, 42.0), 2.5, Iron);
		}
	}

	/** Farola: poste, brazo y farol cálido. */
	inline void TNPropLampPost(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const double H = 330.0 + 30.0 * Variant;
		TNProcAddCylinder(M, FVector(0.0, 0.0, 0.0), FVector(0.0, 0.0, 30.0), 14.0, 11.0, 8, Iron);
		TNProcAddCylinder(M, FVector(0.0, 0.0, 30.0), FVector(0.0, 0.0, H), 6.0, 4.5, 8, Iron);
		if (Variant == 1)
		{
			M.AddBeam(FVector(0.0, 0.0, H - 10.0), FVector(45.0, 0.0, H + 5.0), 2.5, Iron);
			M.AddBox(FVector(45.0, 0.0, H - 22.0), FVector(1.0, 0.0, 0.0), FVector(12.0, 12.0, 16.0), Glow);
			TNProcAddCylinder(M, FVector(45.0, 0.0, H - 6.0), FVector(45.0, 0.0, H + 6.0), 17.0, 3.0, 4, Iron);
			return;
		}
		M.AddBox(FVector(0.0, 0.0, H + 18.0), FVector(1.0, 0.0, 0.0), FVector(13.0, 13.0, 18.0), Glow);
		for (const double Sx : { -1.0, 1.0 })
		{
			for (const double Sy : { -1.0, 1.0 })
			{
				M.AddBeam(FVector(Sx * 13.0, Sy * 13.0, H), FVector(Sx * 13.0, Sy * 13.0, H + 36.0), 1.5, Iron);
			}
		}
		TNProcAddCylinder(M, FVector(0.0, 0.0, H + 36.0), FVector(0.0, 0.0, H + 52.0), 20.0, 3.0, 4, Iron);
	}

	/** Buzón de poste con techo redondo y banderita. */
	inline void TNPropMailbox(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const FLinearColor Box = Variant == 0 ? Blue : (Variant == 1 ? Red : Green);
		M.AddBeam(FVector(0.0, 0.0, 0.0), FVector(0.0, 0.0, 105.0), 5.0, Wood);
		M.AddBox(FVector(0.0, 0.0, 118.0), FVector(1.0, 0.0, 0.0), FVector(24.0, 12.0, 12.0), Box);
		TNProcAddCylinder(M, FVector(-24.0, 0.0, 130.0), FVector(24.0, 0.0, 130.0), 12.0, 12.0, 8, Box);
		M.AddBeam(FVector(10.0, 13.0, 118.0), FVector(10.0, 13.0, 150.0), 1.5, Red);
		M.AddBox(FVector(16.0, 13.5, 145.0), FVector(1.0, 0.0, 0.0), FVector(7.0, 0.8, 5.0), Red);
	}

	/** Dos o tres sacos de arpillera atados. */
	inline void TNPropSacks(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		const int32 N = 2 + Variant % 2;
		for (int32 s = 0; s < N; ++s)
		{
			const double A = TNProcMap::TwoPi * s / N + 0.4;
			const FVector C(FMath::Cos(A) * 26.0 * (N > 1), FMath::Sin(A) * 26.0 * (N > 1), 0.0);
			const double H = TNPropRand(Seed, s, 55.0, 75.0);
			TNProcAddLathe(M, C, { 0.0, 10.0, H * 0.55, H * 0.85, H * 0.95, H }, { 20.0, 27.0, 26.0, 16.0, 7.0, 9.0 }, 0.12, Seed + s, s == 1 ? Burlap * 0.85f : Burlap, 7);
			TNProcAddCylinder(M, C + FVector(0.0, 0.0, H * 0.9), C + FVector(0.0, 0.0, H * 0.93), 8.0, 8.0, 6, Rope, false);
		}
	}

	/** Maceta de barro con un arbusto redondo y flores. */
	inline void TNPropFlowerPot(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		TNProcAddLathe(M, FVector::ZeroVector, { 0.0, 40.0, 44.0, 46.0 }, { 22.0, 30.0, 33.0, 30.0 }, 0.0, 0u, Terracotta, 8);
		TNPropBall(M, FVector(0.0, 0.0, 62.0), 30.0, Leaf * (0.85f + 0.1f * Variant), 7, 4, 0.85);
		const FLinearColor Petal = Variant == 0 ? Pink : (Variant == 1 ? Yellow : Purple);
		for (int32 f = 0; f < 6; ++f)
		{
			const double A = TNProcMap::TwoPi * f / 6 + TNPropRand(Seed, f, 0.0, 0.8);
			TNPropBall(M, FVector(FMath::Cos(A) * 22.0, FMath::Sin(A) * 22.0, 72.0 + TNPropRand(Seed, f + 9, -6.0, 8.0)), 5.5, Petal, 5, 2);
		}
	}

	/** Concha: caracola (0), vieira en abanico (1) o grupo de conchitas (2). */
	inline void TNPropShell(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		if (Variant == 0)
		{
			// Caracola: huso de vueltas alternas, apuntado por los dos extremos, con el labio rosa abierto.
			const FVector Axis = FVector(1.0, 0.0, 0.25).GetSafeNormal();
			const double Rad[] = { 1.5, 9.0, 14.0, 13.0, 9.0, 5.0, 1.0 };
			const double Len = 42.0;
			for (int32 k = 0; k + 1 < 7; ++k)
			{
				const FVector A = Axis * (-Len * 0.5 + Len * k / 6.0) + FVector(0.0, 0.0, 12.0);
				const FVector B = Axis * (-Len * 0.5 + Len * (k + 1) / 6.0) + FVector(0.0, 0.0, 12.0);
				TNProcAddCylinder(M, A, B, Rad[k], Rad[k + 1], 8, (k % 2) ? Sand * 1.05f : Sand * 0.85f + Orange * 0.1f, false);
			}
			const FVector Lip = Axis * (-4.0) + FVector(0.0, 0.0, 12.0);
			M.AddTri(Lip + FVector(0.0, 13.0, -9.0), Lip + FVector(12.0, 17.0, 2.0), Lip + FVector(-12.0, 16.0, 6.0), FVector(0.0, 1.0, 0.3), Pink);
			M.AddTri(Lip + FVector(0.0, 13.0, -9.0), Lip + FVector(12.0, 17.0, 2.0), Lip + FVector(-12.0, 16.0, 6.0), FVector(0.0, -1.0, -0.3), Pink * 0.8f);
			return;
		}
		// Vieiras: abanico de costillas en cúpula.
		const int32 N = Variant == 1 ? 1 : 3;
		for (int32 s = 0; s < N; ++s)
		{
			const FVector C = N == 1 ? FVector::ZeroVector : FVector(TNPropRand(Seed, s, -20.0, 20.0), TNPropRand(Seed, s + 5, -20.0, 20.0), 0.0);
			const double R = N == 1 ? 20.0 : TNPropRand(Seed, s + 11, 8.0, 12.0);
			const double Turn = TNPropRand(Seed, s + 17, 0.0, TNProcMap::TwoPi);
			const FLinearColor Col = (s % 3 == 1) ? Pink : ((s % 3 == 2) ? White * 0.95f : Orange * 0.85f + White * 0.15f);
			const FVector Hinge = C + FVector(0.0, 0.0, 1.0);
			const int32 Ribs = 8;
			for (int32 r = 0; r < Ribs; ++r)
			{
				const double A0 = Turn + FMath::DegreesToRadians(-75.0 + 150.0 * r / Ribs);
				const double A1 = Turn + FMath::DegreesToRadians(-75.0 + 150.0 * (r + 1) / Ribs);
				const double Am = 0.5 * (A0 + A1);
				const FVector P0 = Hinge + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.05) * R;
				const FVector P1 = Hinge + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.05) * R;
				const FVector Q = Hinge + FVector(FMath::Cos(Am) * 0.72, FMath::Sin(Am) * 0.72, 0.32) * R;
				M.AddTri(Hinge, P0, Q, FVector::UpVector, Col);
				M.AddTri(Hinge, Q, P1, FVector::UpVector, Col * 0.9f);
				M.AddTri(P0, P1, Q, FVector(FMath::Cos(Am), FMath::Sin(Am), 0.6), Col * 0.95f);
			}
		}
	}

	/** Estrella de mar de cinco brazos, algo abombada. */
	inline void TNPropStarfish(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const FLinearColor Col = Variant == 0 ? Orange : (Variant == 1 ? Red * 1.1f : Purple);
		const double R = 22.0;
		const FVector Top(0.0, 0.0, 6.0);
		for (int32 k = 0; k < 5; ++k)
		{
			const double A = TNProcMap::TwoPi * k / 5;
			const double B = TNProcMap::TwoPi * (k + 0.5) / 5;
			const double Bp = TNProcMap::TwoPi * (k - 0.5) / 5;
			const FVector Tip(FMath::Cos(A) * R, FMath::Sin(A) * R, 1.0);
			const FVector In1(FMath::Cos(B) * R * 0.38, FMath::Sin(B) * R * 0.38, 2.0);
			const FVector In0(FMath::Cos(Bp) * R * 0.38, FMath::Sin(Bp) * R * 0.38, 2.0);
			M.AddTri(Top, In0, Tip, FVector::UpVector, Col);
			M.AddTri(Top, Tip, In1, FVector::UpVector, Col * 0.88f);
		}
	}

	/** Cubo de playa con asa y pala al lado, junto a un montoncito de arena. */
	inline void TNPropSandBucket(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const FLinearColor Col = Variant == 0 ? Red : (Variant == 1 ? Blue : Yellow);
		TNProcAddLathe(M, FVector::ZeroVector, { 0.0, 26.0, 28.0 }, { 12.0, 16.0, 16.5 }, 0.0, 0u, Col, 10);
		for (int32 k = 0; k < 6; ++k)
		{
			const double A0 = PI * k / 6.0, A1 = PI * (k + 1) / 6.0;
			M.AddBeam(FVector(0.0, FMath::Cos(A0) * 16.0, 28.0 + FMath::Sin(A0) * 12.0), FVector(0.0, FMath::Cos(A1) * 16.0, 28.0 + FMath::Sin(A1) * 12.0), 0.8, Col * 0.8f);
		}
		const FLinearColor Spade = Variant == 1 ? Yellow : Green;
		M.AddBeam(FVector(22.0, 8.0, 2.0), FVector(52.0, 18.0, 3.0), 1.6, Spade);
		M.AddBox(FVector(58.0, 20.0, 2.5), FVector(1.0, 0.33, 0.0), FVector(9.0, 6.0, 1.0), Spade);
		TNProcAddLathe(M, FVector(-30.0, -18.0, 0.0), { 0.0, 8.0, 12.0 }, { 20.0, 12.0, 2.0 }, 0.2, 7u + static_cast<uint32>(Variant), Sand, 7);
	}

	/** Toalla de playa a franjas, con un borde doblado. */
	inline void TNPropBeachTowel(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const FLinearColor A = Variant == 0 ? Red : (Variant == 1 ? Teal : Yellow);
		const FLinearColor B = Variant == 2 ? Orange : White;
		const int32 N = 6;
		for (int32 i = 0; i < N; ++i)
		{
			const double X0 = -90.0 + 180.0 * i / N, X1 = -90.0 + 180.0 * (i + 1) / N;
			M.AddBox(FVector((X0 + X1) * 0.5, 0.0, 1.2), FVector(1.0, 0.0, 0.0), FVector((X1 - X0) * 0.5, 45.0, 1.2), (i % 2) ? B : A);
		}
		M.AddBox(FVector(-84.0, 0.0, 5.0), FVector(1.0, 0.0, 0.0), FVector(6.0, 45.0, 4.0), A * 0.9f);
	}

	/** Tabla de surf clavada en la arena, algo inclinada. */
	inline void TNPropSurfboard(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const FLinearColor Col = Variant == 0 ? Teal : (Variant == 1 ? Orange : White);
		const FLinearColor Stripe = Variant == 2 ? Blue : White;
		const double Lean = FMath::DegreesToRadians(8.0 + 4.0 * Variant);
		const FVector Up(FMath::Sin(Lean), 0.0, FMath::Cos(Lean));
		const FVector Side(0.0, 1.0, 0.0);
		const FVector Out = FVector::CrossProduct(Up, Side).GetSafeNormal();
		const double Len = 210.0;
		const int32 N = 8;
		auto Half = [&](double T) { return 27.0 * FMath::Sin(PI * FMath::Clamp(0.08 + 0.9 * T, 0.0, 1.0)) * (T > 0.7 ? 1.0 - (T - 0.7) * 1.4 : 1.0); };
		for (int32 i = 0; i < N; ++i)
		{
			const double T0 = static_cast<double>(i) / N, T1 = static_cast<double>(i + 1) / N;
			const FVector C0 = Up * (Len * T0 - 30.0), C1 = Up * (Len * T1 - 30.0);
			const FLinearColor& Col2 = (i == 4) ? Stripe : Col;
			for (const double S : { -1.0, 1.0 })
			{
				const FVector O = Out * (S * 3.0);
				M.AddQuad(C0 + O - Side * Half(T0), C0 + O + Side * Half(T0), C1 + O + Side * Half(T1), C1 + O - Side * Half(T1), Out * S, Col2);
			}
		}
		M.AddBeam(Up * 10.0 - Side * 0.0, Up * 175.0, 1.0, Stripe);
	}

	/** Sombrilla de playa a gajos con hamaca. */
	inline void TNPropParasol(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const FLinearColor A = Variant == 0 ? Red : (Variant == 1 ? Blue : Yellow);
		const FLinearColor B = White;
		const double Tilt = FMath::DegreesToRadians(8.0);
		const FVector Top(FMath::Sin(Tilt) * 230.0, 0.0, FMath::Cos(Tilt) * 230.0);
		M.AddBeam(FVector(0.0, 0.0, -20.0), Top, 2.2, White);
		const int32 N = 10;
		const double R = 125.0;
		const FVector Apex = Top + FVector(0.0, 0.0, 12.0);
		for (int32 k = 0; k < N; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / N, A1 = TNProcMap::TwoPi * (k + 1) / N;
			const FVector P0 = Top + FVector(FMath::Cos(A0) * R, FMath::Sin(A0) * R, -32.0);
			const FVector P1 = Top + FVector(FMath::Cos(A1) * R, FMath::Sin(A1) * R, -32.0);
			const FLinearColor& Col = (k % 2) ? B : A;
			M.AddTri(Apex, P0, P1, FVector::UpVector, Col);
			M.AddTri(Apex, P0, P1, -FVector::UpVector, Col * 0.7f);
		}
		// Hamaca: bastidor y tela.
		const FVector Chair(95.0, 30.0, 0.0);
		const FLinearColor Cloth = Variant == 1 ? Yellow : Teal;
		for (const double Sy : { -1.0, 1.0 })
		{
			M.AddBeam(Chair + FVector(-40.0, Sy * 26.0, 0.0), Chair + FVector(35.0, Sy * 26.0, 30.0), 2.0, WoodLight);
			M.AddBeam(Chair + FVector(35.0, Sy * 26.0, 30.0), Chair + FVector(55.0, Sy * 26.0, 85.0), 2.0, WoodLight);
			M.AddBeam(Chair + FVector(25.0, Sy * 26.0, 0.0), Chair + FVector(5.0, Sy * 26.0, 30.0), 2.0, WoodLight);
		}
		M.AddQuad(Chair + FVector(-38.0, -24.0, 3.0), Chair + FVector(-38.0, 24.0, 3.0), Chair + FVector(33.0, 24.0, 28.0), Chair + FVector(33.0, -24.0, 28.0), FVector(-0.3, 0.0, 1.0), Cloth);
		M.AddQuad(Chair + FVector(33.0, -24.0, 28.0), Chair + FVector(33.0, 24.0, 28.0), Chair + FVector(53.0, 24.0, 82.0), Chair + FVector(53.0, -24.0, 82.0), FVector(-1.0, 0.0, 0.4), Cloth);
	}

	/** Tronco a la deriva, blanqueado por el sol. */
	inline void TNPropDriftwood(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		const double Len = 140.0 + 60.0 * Variant;
		TNProcAddLog(M, FVector(-Len * 0.5, 0.0, 12.0), FVector(Len * 0.5, 8.0, 14.0), 13.0 + 3.0 * Variant, Seed, Driftwood, Driftwood * 1.1f);
		M.AddBeam(FVector(Len * 0.2, 4.0, 18.0), FVector(Len * 0.35, 40.0, 30.0), 3.5, Driftwood * 0.95f);
		if (Variant == 2) { M.AddBeam(FVector(-Len * 0.2, 0.0, 16.0), FVector(-Len * 0.35, -38.0, 22.0), 3.0, Driftwood * 0.9f); }
	}

	/** Cocos: dos a cuatro y uno abierto. */
	inline void TNPropCoconuts(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		const int32 N = 2 + Variant;
		for (int32 c = 0; c < N; ++c)
		{
			const FVector C(TNPropRand(Seed, c, -22.0, 22.0), TNPropRand(Seed, c + 7, -22.0, 22.0), 11.0);
			TNPropBall(M, C, 11.0, WoodDark * 1.2f, 7, 4, 0.9);
		}
		TNProcAddLathe(M, FVector(30.0, 10.0, 0.0), { 0.0, 7.0, 10.0 }, { 5.0, 10.0, 11.0 }, 0.0, 0u, WoodDark * 1.2f, 7);
		TNProcAddCylinder(M, FVector(30.0, 10.0, 9.5), FVector(30.0, 10.0, 10.5), 9.5, 9.5, 7, White, true);
	}

	/** Salvavidas colgado de un poste. */
	inline void TNPropLifebuoy(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		M.AddBeam(FVector(0.0, 0.0, -20.0), FVector(0.0, 0.0, 150.0), 6.0, Variant == 1 ? White : Wood);
		M.AddBox(FVector(0.0, 0.0, 158.0), FVector(1.0, 0.0, 0.0), FVector(9.0, 9.0, 8.0), Red);
		TNPropTorus(M, FVector(9.0, 0.0, 112.0), FVector(1.0, 0.0, 0.0), 30.0, 9.0, 16, 6, Variant == 2 ? Orange : Red, White, 8);
		M.AddBeam(FVector(6.0, 0.0, 140.0), FVector(9.0, 0.0, 145.0), 1.2, Rope);
	}

	/** Setas: grupo de tres a seis, rojas con motas, pardas o moradas. */
	inline void TNPropMushrooms(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		const FLinearColor Cap = Variant == 0 ? Red : (Variant == 1 ? WoodLight : Purple);
		const int32 N = 3 + Variant + (Seed % 2);
		for (int32 s = 0; s < N; ++s)
		{
			const double A = TNProcMap::TwoPi * s / N + TNPropRand(Seed, s, 0.0, 0.9);
			const double D = s == 0 ? 0.0 : TNPropRand(Seed, s + 3, 14.0, 30.0);
			const double Sz = s == 0 ? 1.0 : TNPropRand(Seed, s + 7, 0.45, 0.8);
			const FVector C(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.0);
			const double H = 34.0 * Sz, R = 22.0 * Sz;
			TNProcAddCylinder(M, C, C + FVector(0.0, 0.0, H), 5.0 * Sz + 1.0, 4.0 * Sz + 1.0, 7, Bone);
			TNProcAddLathe(M, C + FVector(0.0, 0.0, H - 2.0), { 0.0, R * 0.25, R * 0.55, R * 0.7 }, { R * 0.4, R, R * 0.75, 1.0 }, 0.0, 0u, Cap, 9);
			if (Variant == 0)
			{
				for (int32 d = 0; d < 4; ++d)
				{
					const double B = TNProcMap::TwoPi * d / 4 + A;
					TNPropBall(M, C + FVector(FMath::Cos(B) * R * 0.55, FMath::Sin(B) * R * 0.55, H + R * 0.38), 2.4 * Sz + 0.8, White, 5, 2);
				}
			}
		}
	}

	/** Vasija de barro (variante 2: rota, con trozos). */
	inline void TNPropClayPot(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		const FLinearColor Col = Variant == 1 ? Terracotta * 0.8f : Terracotta;
		if (Variant == 2)
		{
			TNProcAddLathe(M, FVector::ZeroVector, { 0.0, 16.0, 26.0 }, { 14.0, 22.0, 21.0 }, 0.18, Seed, Col, 8);
			for (int32 k = 0; k < 4; ++k)
			{
				const double A = TNProcMap::TwoPi * k / 4 + 0.3;
				TNPropYawBox(M, FVector(FMath::Cos(A) * 36.0, FMath::Sin(A) * 36.0, 2.0), A * 57.3, FVector(8.0, 5.0, 1.5), Col * 0.9f);
			}
			return;
		}
		TNProcAddLathe(M, FVector::ZeroVector, { 0.0, 12.0, 30.0, 44.0, 52.0, 58.0 }, { 13.0, 22.0, 25.0, 16.0, 10.0, 12.0 }, 0.0, 0u, Col, 9);
		TNProcAddCylinder(M, FVector(0.0, 0.0, 30.0), FVector(0.0, 0.0, 34.0), 25.5, 25.2, 9, Black * 2.5f, false);
	}

	/** Antorcha tiki: caña, cesta y llama. */
	inline void TNPropTikiTorch(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const double H = 170.0 + 20.0 * Variant;
		TNProcAddCylinder(M, FVector(0.0, 0.0, -20.0), FVector(0.0, 0.0, H), 6.0, 5.0, 6, Straw * 0.85f);
		for (int32 k = 1; k < 5; ++k) { TNProcAddCylinder(M, FVector(0.0, 0.0, H * k / 5.0 - 2.5), FVector(0.0, 0.0, H * k / 5.0 + 2.5), 6.8, 6.8, 6, WoodDark, false); }
		TNProcAddLathe(M, FVector(0.0, 0.0, H), { 0.0, 18.0, 28.0 }, { 7.0, 15.0, 16.0 }, 0.0, 0u, Straw * 0.7f, 7);
		TNProcAddLathe(M, FVector(0.0, 0.0, H + 28.0), { 0.0, 14.0, 38.0 + 8.0 * Variant }, { 14.0, 10.0, 0.5 }, 0.15, 3u + static_cast<uint32>(Variant), Flame, 6);
		TNProcAddLathe(M, FVector(0.0, 0.0, H + 30.0), { 0.0, 8.0, 22.0 }, { 8.0, 6.0, 0.5 }, 0.1, 5u, Yellow, 5);
	}

	/** Poste tribal con calavera y plumas. */
	inline void TNPropSkullPost(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const double H = 150.0 + 25.0 * Variant;
		M.AddBeam(FVector(0.0, 0.0, -20.0), FVector(0.0, 0.0, H), 7.0, WoodDark);
		TNPropBall(M, FVector(0.0, 0.0, H + 20.0), 20.0, Bone, 8, 4, 0.95);
		M.AddBox(FVector(13.0, 0.0, H + 4.0), FVector(1.0, 0.0, 0.0), FVector(9.0, 11.0, 6.0), Bone);
		for (const double Sy : { -1.0, 1.0 }) { M.AddBox(FVector(17.0, Sy * 7.5, H + 22.0), FVector(1.0, 0.0, 0.0), FVector(2.5, 4.5, 4.5), Black); }
		M.AddBox(FVector(19.5, 0.0, H + 12.0), FVector(1.0, 0.0, 0.0), FVector(1.5, 2.0, 3.0), Black);
		// Plumas colgando bajo la calavera y una cinta roja.
		TNProcAddCylinder(M, FVector(0.0, 0.0, H - 14.0), FVector(0.0, 0.0, H - 6.0), 8.2, 8.2, 6, Red, false);
		const FLinearColor Feather[4] = { Red, Yellow, Teal, White };
		for (int32 f = 0; f < 4; ++f)
		{
			const double A = TNProcMap::TwoPi * f / 4 + 0.4;
			const FVector D(FMath::Cos(A), FMath::Sin(A), 0.0);
			const FVector Base = FVector(0.0, 0.0, H - 12.0) + D * 8.0;
			const FVector Tip = Base + D * 16.0 + FVector(0.0, 0.0, -55.0);
			const FVector Side = FVector(-D.Y, D.X, 0.0) * 7.0;
			M.AddTri(Base - Side, Base + Side, Tip, D, Feather[f]);
			M.AddTri(Base - Side, Base + Side, Tip, -D, Feather[f] * 0.8f);
		}
	}

	/** Calavera de vaca con cuernos, tirada en el suelo. */
	inline void TNPropCattleSkull(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const double S = Variant == 2 ? 1.4 : 1.0;
		TNProcAddLathe(M, FVector(0.0, 0.0, 0.0), { 0.0, 14.0 * S, 24.0 * S }, { 16.0 * S, 17.0 * S, 12.0 * S }, 0.08, 9u, Bone, 7);
		const FVector Snout(28.0 * S, 0.0, 8.0 * S);
		TNPropBox(M, Snout, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(16.0 * S, 9.0 * S, 7.0 * S), Bone * 0.95f);
		for (const double Sy : { -1.0, 1.0 })
		{
			M.AddBox(FVector(12.0 * S, Sy * 9.0 * S, 17.0 * S), FVector(1.0, 0.0, 0.0), FVector(4.0 * S, 3.5 * S, 3.5 * S), Black);
			FVector Prev(-2.0 * S, Sy * 15.0 * S, 18.0 * S);
			for (int32 k = 1; k <= 4; ++k)
			{
				const double T = k / 4.0;
				const FVector P(-2.0 * S - 6.0 * S * T, Sy * (15.0 + 32.0 * T) * S, (18.0 + 18.0 * T * T) * S);
				M.AddBeam(Prev, P, (4.0 - 2.6 * T) * S, Bone * 0.9f);
				Prev = P;
			}
		}
	}

	/** Huesos sueltos: costillar en arco y fémures. */
	inline void TNPropBones(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		const FLinearColor Col = Bone * (Variant == 1 ? 0.85f : 1.0f);
		// Costillar tumbado de lado: la columna en el suelo con una vértebra por costilla, y cada
		// costilla sale de ella en arco por encima y cae hacia el otro lado sin llegar a tocarlo.
		const int32 Ribs = 3 + Variant;
		const double X0 = -30.0, X1 = -30.0 + 18.0 * (Ribs - 1);
		M.AddBeam(FVector(X0 - 14.0, -24.0, 4.0), FVector(X1 + 18.0, -24.0, 4.0), 3.2, Col * 0.92f);
		for (int32 r = 0; r < Ribs; ++r)
		{
			const double X = X0 + 18.0 * r;
			TNPropBall(M, FVector(X, -24.0, 5.0), 5.5, Col, 6, 3, 0.8);
			FVector Prev(X, -24.0, 6.0);
			const double Size = 1.0 - 0.12 * FMath::Abs(r - (Ribs - 1) * 0.5);
			for (int32 k = 1; k <= 6; ++k)
			{
				const double A = PI * 0.92 * k / 6.0;
				const FVector P(X + 3.0 * k / 6.0, -24.0 * FMath::Cos(A) * Size, 6.0 + 30.0 * FMath::Sin(A) * Size);
				M.AddBeam(Prev, P, FMath::Lerp(2.4, 1.5, k / 6.0), Col);
				Prev = P;
			}
		}
		for (int32 b = 0; b < 2; ++b)
		{
			const double A = TNPropRand(Seed, b, 0.0, 3.1);
			const FVector C(TNPropRand(Seed, b + 4, 10.0, 40.0), TNPropRand(Seed, b + 8, 25.0, 45.0), 3.0);
			const FVector D(FMath::Cos(A) * 22.0, FMath::Sin(A) * 22.0, 0.0);
			M.AddBeam(C - D, C + D, 2.8, Col);
			TNPropBall(M, C - D, 4.5, Col, 5, 2);
			TNPropBall(M, C + D, 4.5, Col, 5, 2);
		}
	}

	/** Ánfora de dos asas (variante 1 tumbada). */
	inline void TNPropAmphora(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		FTNProcMeshBuffers Local;
		const FLinearColor Col = Variant == 2 ? Terracotta * 1.15f : Terracotta;
		TNProcAddLathe(Local, FVector::ZeroVector, { 0.0, 8.0, 30.0, 52.0, 62.0, 72.0, 76.0 }, { 4.0, 14.0, 21.0, 18.0, 8.0, 7.0, 9.0 }, 0.0, 0u, Col, 9);
		for (const double Sy : { -1.0, 1.0 })
		{
			Local.AddBeam(FVector(0.0, Sy * 7.0, 68.0), FVector(0.0, Sy * 17.0, 64.0), 2.0, Col * 0.9f);
			Local.AddBeam(FVector(0.0, Sy * 17.0, 64.0), FVector(0.0, Sy * 17.0, 52.0), 2.0, Col * 0.9f);
		}
		TNProcAddCylinder(Local, FVector(0.0, 0.0, 34.0), FVector(0.0, 0.0, 40.0), 21.3, 21.0, 9, Black * 2.0f, false);
		if (Variant != 1)
		{
			TNPropAppend(M, Local, FVector::ZeroVector, 0.0);
			return;
		}
		// Tumbada: gira 80° sobre Y.
		const double A = FMath::DegreesToRadians(80.0);
		const double Ca = FMath::Cos(A), Sa = FMath::Sin(A);
		const int32 Base = M.Verts.Num();
		for (int32 i = 0; i < Local.Verts.Num(); ++i)
		{
			const FVector& V = Local.Verts[i];
			const FVector& N = Local.Normals[i];
			M.Verts.Add(FVector(V.X * Ca + V.Z * Sa - 30.0, V.Y, -V.X * Sa + V.Z * Ca + 18.0));
			M.Normals.Add(FVector(N.X * Ca + N.Z * Sa, N.Y, -N.X * Sa + N.Z * Ca));
			M.UVs.Add(Local.UVs[i]);
			M.Colors.Add(Local.Colors[i]);
		}
		for (const int32 T : Local.Tris) { M.Tris.Add(Base + T); }
	}

	/** Rueda de carro apoyada (variante 0), tumbada (1) o rota (2). */
	inline void TNPropWagonWheel(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const double R = 55.0;
		const FVector C = Variant == 1 ? FVector(0.0, 0.0, 6.0) : FVector(0.0, 0.0, R - 2.0);
		const FVector Axis = Variant == 1 ? FVector(0.0, 0.0, 1.0) : FVector(0.1, 1.0, 0.25).GetSafeNormal();
		TNPropTorus(M, C, Axis, R, 5.0, 18, 4, WoodDark, Iron, 0);
		const FVector U = FVector::CrossProduct(Axis, FMath::Abs(Axis.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Axis, U);
		const int32 Spokes = Variant == 2 ? 5 : 8;
		for (int32 s = 0; s < Spokes; ++s)
		{
			const double A = TNProcMap::TwoPi * s / 8.0;
			M.AddBeam(C, C + (U * FMath::Cos(A) + V * FMath::Sin(A)) * (R - 3.0), 2.6, Wood);
		}
		TNProcAddCylinder(M, C - Axis * 9.0, C + Axis * 9.0, 9.0, 9.0, 8, WoodDark);
	}

	/** Poste indicador con dos o tres flechas. */
	inline void TNPropSignpost(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		M.AddBeam(FVector(0.0, 0.0, -25.0), FVector(0.0, 0.0, 200.0), 5.5, WoodDark);
		const int32 N = 2 + (Variant > 0 ? 1 : 0);
		for (int32 b = 0; b < N; ++b)
		{
			const double Yaw = TNPropRand(Seed, b, -80.0, 80.0) + b * 110.0;
			const double A = FMath::DegreesToRadians(Yaw);
			const FVector D(FMath::Cos(A), FMath::Sin(A), 0.0);
			const double Z = 180.0 - b * 28.0;
			const FLinearColor Col = (Variant == 2 && b == 1) ? Red : WoodLight;
			M.AddBox(FVector(0.0, 0.0, Z) + D * 30.0, D, FVector(30.0, 2.5, 10.0), Col);
			const FVector Tip = FVector(0.0, 0.0, Z) + D * 72.0;
			const FVector Nrm(-D.Y, D.X, 0.0);
			for (const double S : { -1.0, 1.0 })
			{
				const FVector O = Nrm * (S * 2.5);
				M.AddTri(FVector(0.0, 0.0, Z + 12.0) + D * 60.0 + O, Tip + O, FVector(0.0, 0.0, Z - 12.0) + D * 60.0 + O, Nrm * S, Col);
			}
		}
	}

	/** Planta rodadora: bola de ramitas secas. */
	inline void TNPropTumbleweed(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		const double R = 28.0 + 8.0 * Variant;
		const FVector C(0.0, 0.0, R * 0.9);
		const FLinearColor Col = Straw * 0.75f;
		for (int32 k = 0; k < 16; ++k)
		{
			const double A = TNPropRand(Seed, k, 0.0, TNProcMap::TwoPi);
			const double B = TNPropRand(Seed, k + 40, -1.2, 1.2);
			const FVector D(FMath::Cos(A) * FMath::Cos(B), FMath::Sin(A) * FMath::Cos(B), FMath::Sin(B));
			const FVector E = FVector::CrossProduct(D, FVector(0.3, 0.5, 0.8)).GetSafeNormal();
			M.AddBeam(C - D * R, C + D * R * 0.2 + E * R * 0.7, 0.9, Col);
			M.AddBeam(C + D * R * 0.2 + E * R * 0.7, C + D * R, 0.9, Col * 0.9f);
		}
	}

	/** Cristales: grupo de agujas de caras planas (color del bioma). */
	inline void TNPropCrystals(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed, const FLinearColor& Col)
	{
		const int32 N = 4 + Variant * 2;
		for (int32 c = 0; c < N; ++c)
		{
			const double A = TNPropRand(Seed, c, 0.0, TNProcMap::TwoPi);
			const double Tilt = c == 0 ? 0.0 : TNPropRand(Seed, c + 20, 0.25, 0.75);
			const FVector Dir = FVector(FMath::Cos(A) * Tilt, FMath::Sin(A) * Tilt, 1.0).GetSafeNormal();
			const double L = (c == 0 ? 70.0 : TNPropRand(Seed, c + 40, 25.0, 55.0)) * (1.0 + 0.3 * Variant);
			const double R = L * 0.16;
			const FVector Base = FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * (c == 0 ? 0.0 : R * 1.2);
			TNProcAddCylinder(M, Base - FVector(0.0, 0.0, 5.0), Base + Dir * L, R, R * 0.85, 6, c % 2 ? Col * 1.15f : Col, false);
			TNProcAddLathe(M, Base + Dir * L, { 0.0, R * 1.4 }, { R * 0.85, 0.5 }, 0.0, 0u, Col * 1.3f, 6);
		}
	}

	/** Tocón con raíces y anillos (carbonizado en el volcán). */
	inline void TNPropStump(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed, bool bCharred)
	{
		using namespace PropColors;
		const FLinearColor Bark = bCharred ? Black * 1.6f : WoodDark;
		const FLinearColor Cut = bCharred ? FLinearColor(0.35f, 0.12f, 0.05f) : WoodLight;
		const double R = 26.0 + 6.0 * Variant;
		const double H = 30.0 + 18.0 * Variant;
		TNProcAddLathe(M, FVector(0.0, 0.0, -8.0), { 0.0, H * 0.4, H }, { R * 1.25, R, R * 0.95 }, 0.1, Seed, Bark, 9);
		TNProcAddCylinder(M, FVector(0.0, 0.0, H - 8.0 - 1.0), FVector(0.0, 0.0, H - 8.0 + 0.5), R * 0.93, R * 0.93, 9, Cut, true);
		for (int32 k = 0; k < 4; ++k)
		{
			const double A = TNProcMap::TwoPi * k / 4 + TNPropRand(Seed, k, 0.0, 0.7);
			M.AddBeam(FVector(FMath::Cos(A) * R * 0.8, FMath::Sin(A) * R * 0.8, 8.0), FVector(FMath::Cos(A) * R * 2.0, FMath::Sin(A) * R * 2.0, -4.0), 5.0, Bark);
		}
	}

	/** Hito de piedras apiladas. */
	inline void TNPropCairn(FTNProcMeshBuffers& M, int32 Variant, uint32 Seed)
	{
		using namespace PropColors;
		const int32 N = 4 + Variant;
		double Z = 0.0;
		for (int32 s = 0; s < N; ++s)
		{
			const double T = static_cast<double>(s) / N;
			const double R = LerpD(30.0, 11.0, T) * (1.0 + 0.15 * Variant);
			const double H = R * 0.7;
			const FVector C(TNPropRand(Seed, s, -3.0, 3.0), TNPropRand(Seed, s + 9, -3.0, 3.0), Z);
			TNProcAddBoulder(M, C, R, H, Seed + static_cast<uint32>(s) * 31u, Stone * (0.85f + 0.08f * (s % 3)));
			Z += H * 0.85;
		}
	}

	/** Farol colgado de un poste con brazo. */
	inline void TNPropLantern(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const double H = 190.0 + 20.0 * Variant;
		M.AddBeam(FVector(0.0, 0.0, -20.0), FVector(0.0, 0.0, H), 5.0, WoodDark);
		M.AddBeam(FVector(0.0, 0.0, H - 12.0), FVector(42.0, 0.0, H - 4.0), 3.0, WoodDark);
		M.AddBeam(FVector(40.0, 0.0, H - 6.0), FVector(40.0, 0.0, H - 26.0), 0.8, Iron);
		M.AddBox(FVector(40.0, 0.0, H - 40.0), FVector(1.0, 0.0, 0.0), FVector(9.0, 9.0, 13.0), Glow);
		TNProcAddCylinder(M, FVector(40.0, 0.0, H - 27.0), FVector(40.0, 0.0, H - 20.0), 12.0, 3.0, 4, Iron);
		M.AddBox(FVector(40.0, 0.0, H - 54.0), FVector(1.0, 0.0, 0.0), FVector(10.0, 10.0, 2.0), Iron);
	}

	/** Nasa de pescador: jaula de listones con red y un flotador. */
	inline void TNPropCrabTrap(FTNProcMeshBuffers& M, int32 Variant)
	{
		using namespace PropColors;
		const double L = 70.0, W = 50.0, H = 40.0 + 5.0 * Variant;
		for (const double Sx : { -1.0, 1.0 })
		{
			for (const double Sy : { -1.0, 1.0 })
			{
				M.AddBeam(FVector(Sx * L * 0.5, Sy * W * 0.5, 0.0), FVector(Sx * L * 0.5, Sy * W * 0.5, H), 2.0, Wood);
				M.AddBeam(FVector(-L * 0.5, Sy * W * 0.5, (Sx > 0.0) ? H : 0.0), FVector(L * 0.5, Sy * W * 0.5, (Sx > 0.0) ? H : 0.0), 2.0, Wood);
				M.AddBeam(FVector(Sx * L * 0.5, -W * 0.5, (Sy > 0.0) ? H : 0.0), FVector(Sx * L * 0.5, W * 0.5, (Sy > 0.0) ? H : 0.0), 2.0, Wood);
			}
		}
		for (int32 k = 1; k < 4; ++k)
		{
			const double X = -L * 0.5 + L * k / 4.0;
			for (const double Sy : { -1.0, 1.0 }) { M.AddBeam(FVector(X, Sy * W * 0.5, 0.0), FVector(X, Sy * W * 0.5, H), 0.7, Rope); }
			M.AddBeam(FVector(X, -W * 0.5, H), FVector(X, W * 0.5, H), 0.7, Rope);
		}
		M.AddBeam(FVector(L * 0.5, 0.0, H), FVector(L * 0.5 + 40.0, 12.0, 4.0), 0.8, Rope);
		TNPropBall(M, FVector(L * 0.5 + 44.0, 14.0, 9.0), 9.0, Variant == 1 ? Yellow : Orange, 6, 3);
	}

	/** Receta de un prop suelto. Crystal: color de los cristales del bioma; bCharred: tocón carbonizado. */
	inline void TNPropBuild(FTNProcMeshBuffers& M, EPropKind Kind, int32 Variant, uint32 Seed, const FLinearColor& Crystal, bool bCharred)
	{
		using namespace PropColors;
		switch (Kind)
		{
			case EPropKind::Crate:
				TNPropCrate(M, 80.0 + 20.0 * Variant, Variant == 2 ? WoodDark * 1.3f : WoodLight, Variant == 2 ? WoodDark : Wood);
				break;
			case EPropKind::WoodBarrel:  TNPropWoodBarrel(M, Variant); break;
			case EPropKind::Barricade:   TNPropBarricade(M, 170.0 + 40.0 * Variant); break;
			case EPropKind::TrafficCone: TNPropTrafficCone(M); if (Variant > 0) { FTNProcMeshBuffers C; TNPropTrafficCone(C); TNPropAppend(M, C, FVector(45.0, 20.0 * Variant, 0.0), 30.0); } break;
			case EPropKind::HayBale:     TNPropHayBale(M, Variant); break;
			case EPropKind::Bench:       TNPropBench(M, Variant); break;
			case EPropKind::LampPost:    TNPropLampPost(M, Variant); break;
			case EPropKind::Mailbox:     TNPropMailbox(M, Variant); break;
			case EPropKind::Sacks:       TNPropSacks(M, Variant, Seed); break;
			case EPropKind::FlowerPot:   TNPropFlowerPot(M, Variant, Seed); break;
			case EPropKind::Shell:       TNPropShell(M, Variant, Seed); break;
			case EPropKind::Starfish:    TNPropStarfish(M, Variant); break;
			case EPropKind::SandBucket:  TNPropSandBucket(M, Variant); break;
			case EPropKind::BeachTowel:  TNPropBeachTowel(M, Variant); break;
			case EPropKind::Surfboard:   TNPropSurfboard(M, Variant); break;
			case EPropKind::Parasol:     TNPropParasol(M, Variant); break;
			case EPropKind::Driftwood:   TNPropDriftwood(M, Variant, Seed); break;
			case EPropKind::Coconuts:    TNPropCoconuts(M, Variant, Seed); break;
			case EPropKind::Lifebuoy:    TNPropLifebuoy(M, Variant); break;
			case EPropKind::Mushrooms:   TNPropMushrooms(M, Variant, Seed); break;
			case EPropKind::ClayPot:     TNPropClayPot(M, Variant, Seed); break;
			case EPropKind::TikiTorch:   TNPropTikiTorch(M, Variant); break;
			case EPropKind::SkullPost:   TNPropSkullPost(M, Variant); break;
			case EPropKind::CattleSkull: TNPropCattleSkull(M, Variant); break;
			case EPropKind::Bones:       TNPropBones(M, Variant, Seed); break;
			case EPropKind::Amphora:     TNPropAmphora(M, Variant); break;
			case EPropKind::WagonWheel:  TNPropWagonWheel(M, Variant); break;
			case EPropKind::Signpost:    TNPropSignpost(M, Variant, Seed); break;
			case EPropKind::Tumbleweed:  TNPropTumbleweed(M, Variant, Seed); break;
			case EPropKind::Crystals:    TNPropCrystals(M, Variant, Seed, Crystal); break;
			case EPropKind::Stump:       TNPropStump(M, Variant, Seed, bCharred); break;
			case EPropKind::Cairn:       TNPropCairn(M, Variant, Seed); break;
			case EPropKind::Lantern:     TNPropLantern(M, Variant); break;
			case EPropKind::CrabTrap:    TNPropCrabTrap(M, Variant); break;
			default: break;
		}
	}

	/** Cómo se ve un prop: distancia de culling (cm) y si proyecta sombra. */
	struct FTNPropLook
	{
		float Cull = 9000.f;
		bool bShadow = true;
	};

	inline FTNPropLook TNPropLookOf(EPropKind Kind)
	{
		switch (Kind)
		{
			case EPropKind::Shell:
			case EPropKind::Starfish:
			case EPropKind::Coconuts:
			case EPropKind::SandBucket:
			case EPropKind::BeachTowel:
			case EPropKind::Bones:       return { 5500.f, false };
			case EPropKind::LampPost:
			case EPropKind::Parasol:
			case EPropKind::Surfboard:
			case EPropKind::Signpost:
			case EPropKind::Lantern:
			case EPropKind::TikiTorch:
			case EPropKind::SkullPost:
			case EPropKind::Lifebuoy:    return { 15000.f, true };
			default:                     return { 9000.f, true };
		}
	}

	/** Props macizos: llevan colisión (una caja con su tamaño); el resto se atraviesa. */
	inline bool TNPropSolid(EPropKind Kind)
	{
		switch (Kind)
		{
			case EPropKind::Crate: case EPropKind::WoodBarrel: case EPropKind::HayBale: case EPropKind::Sacks: case EPropKind::FlowerPot:
			case EPropKind::Cairn: case EPropKind::Stump: case EPropKind::ClayPot: case EPropKind::Amphora: case EPropKind::CrabTrap:
			case EPropKind::Bench: case EPropKind::Barricade: case EPropKind::TrafficCone: case EPropKind::LampPost: case EPropKind::Mailbox:
				return true;
			default:
				return false;
		}
	}

	/** Color de los cristales de cada bioma: obsidiana, amatista, cuarzo o aguamarina. */
	inline FLinearColor TNPropCrystalColor(ETNProcBiome Biome)
	{
		switch (Biome)
		{
			case ETNProcBiome::Volcanic: return FLinearColor(0.1f, 0.07f, 0.14f);
			case ETNProcBiome::Desert:   return FLinearColor(0.55f, 0.3f, 0.75f);
			case ETNProcBiome::Rocky:    return FLinearColor(0.8f, 0.85f, 0.95f);
			default:                     return FLinearColor(0.2f, 0.7f, 0.65f);
		}
	}

	// ── Obstáculos de objetos del camino (EFeature::PathProp) ────────────────────

	struct FTNPathPropParams
	{
		/** Semiancho de la huella a lo ancho del camino, alto y largo de los alargados (cm). */
		double Radius = 120.0;
		double Height = 120.0;
		double Length = 0.0;
		uint32 Seed = 0;
		/** Color de los cristales del bioma; tocones y huesos carbonizados en el volcán. */
		FLinearColor Crystal = FLinearColor(0.2f, 0.7f, 0.65f);
		bool bCharred = false;
	};

	/** Copia Src con sus ejes locales llevados a Ax, Ay, Az (base ortonormal con Az = Ax x Ay: un giro, no un reflejo), escalado y desplazado. */
	inline void TNPropAppendXf(FTNProcMeshBuffers& Dst, const FTNProcMeshBuffers& Src, const FVector& Offset, const FVector& Ax, const FVector& Ay, const FVector& Az, double Scale = 1.0)
	{
		const int32 Base = Dst.Verts.Num();
		for (int32 i = 0; i < Src.Verts.Num(); ++i)
		{
			const FVector& V = Src.Verts[i];
			const FVector& N = Src.Normals[i];
			Dst.Verts.Add(Offset + (Ax * V.X + Ay * V.Y + Az * V.Z) * Scale);
			Dst.Normals.Add((Ax * N.X + Ay * N.Y + Az * N.Z).GetSafeNormal());
			Dst.UVs.Add(Src.UVs[i]);
			Dst.Colors.Add(Src.Colors[i]);
		}
		for (const int32 T : Src.Tris) { Dst.Tris.Add(Base + T); }
	}

	/** Un prop suelto construido aparte y colocado (girado Yaw, escalado) sobre el suelo en (X, Y). */
	template <typename FGround>
	void TNPropPlace(FTNProcMeshBuffers& M, EPropKind Kind, int32 Variant, uint32 Seed, const FTNPathPropParams& P, FGround& Ground,
		double X, double Y, double YawDeg, double Scale = 1.0, double Lift = 0.0)
	{
		FTNProcMeshBuffers Local;
		TNPropBuild(Local, Kind, Variant, Seed, P.Crystal, P.bCharred);
		TNPropAppend(M, Local, FVector(X, Y, Ground(X, Y) + Lift), YawDeg, Scale);
	}

	/** Barca de remos volcada (quilla arriba), con los remos al lado. */
	inline void TNPathPropRowboat(FTNProcMeshBuffers& M, double Len, uint32 Seed)
	{
		using namespace PropColors;
		const FLinearColor Hull = (Seed % 3 == 0) ? White : ((Seed % 3 == 1) ? Teal : Red * 0.9f + White * 0.1f);
		const FLinearColor Band = (Seed % 3 == 0) ? Blue : White;
		const double Wd = 70.0, H = 70.0;
		const int32 N = 10;
		// Perfil (y, z) de media sección con la barca derecha: borda, pantoque y quilla; volcada, z' = H - z.
		const double Prof[][2] = { { 1.0, 1.0 }, { 0.97, 0.62 }, { 0.8, 0.3 }, { 0.45, 0.08 }, { 0.0, 0.0 } };
		TArray<TArray<FVector>> Rings;
		for (int32 i = 0; i <= N; ++i)
		{
			const double T = static_cast<double>(i) / N;
			const double X = -Len * 0.5 + Len * T;
			const double Beam = Wd * FMath::Pow(FMath::Max(0.0, FMath::Sin(PI * LerpD(0.02, 0.98, T))), 0.55) * (T > 0.8 ? 1.0 - (T - 0.8) * 1.5 : 1.0);
			TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
			for (int32 k = 0; k < 5; ++k) { Ring.Add(FVector(X, -Prof[k][0] * Beam, H - Prof[k][1] * H * (0.85 + 0.15 * FMath::Sin(PI * T)))); }
			for (int32 k = 3; k >= 0; --k) { Ring.Add(FVector(X, Prof[k][0] * Beam, H - Prof[k][1] * H * (0.85 + 0.15 * FMath::Sin(PI * T)))); }
		}
		// Casco a franjas: la banda de color junto a la borda (que queda abajo).
		for (int32 i = 0; i < N; ++i)
		{
			const TArray<FVector>& R0 = Rings[i];
			const TArray<FVector>& R1 = Rings[i + 1];
			const FVector Mid = (R0[4] + R1[4]) * 0.5 - FVector(0.0, 0.0, H * 0.6);
			for (int32 k = 0; k + 1 < R0.Num(); ++k)
			{
				const FLinearColor& Col = (k == 0 || k == R0.Num() - 2) ? Band : Hull * (k % 2 ? 0.93f : 1.0f);
				M.AddQuad(R0[k], R0[k + 1], R1[k + 1], R1[k], (R0[k] + R0[k + 1]) * 0.5 - Mid, Col);
			}
		}
		M.AddBeam(FVector(-Len * 0.5, 0.0, H + 2.0), FVector(Len * 0.5, 0.0, H + 2.0), 5.0, WoodDark);
		for (const double Sy : { -1.0, 1.0 })
		{
			const FVector A(-Len * 0.3, Sy * (Wd + 30.0), 4.0), B(Len * 0.35, Sy * (Wd + 42.0), 4.0);
			M.AddBeam(A, B, 3.0, WoodLight);
			M.AddBox(B + FVector(18.0, Sy * 3.0, 0.0), FVector(1.0, 0.1 * Sy, 0.0), FVector(22.0, 9.0, 1.5), WoodLight);
		}
	}

	/** Castillo de arena: muralla baja, cuatro torres de cubo almenadas, torre del homenaje y banderita. */
	template <typename FGround>
	void TNPathPropSandcastle(FTNProcMeshBuffers& M, const FTNPathPropParams& P, FGround& Ground)
	{
		using namespace PropColors;
		const double R = P.Radius, H = P.Height;
		const double Z0 = Ground(0.0, 0.0);
		const FLinearColor Wet = Sand * 0.9f;
		TNProcAddLathe(M, FVector(0.0, 0.0, Z0 - 10.0), { 0.0, 22.0, 30.0 }, { R * 1.05, R * 0.95, R * 0.8 }, 0.08, P.Seed, Sand * 0.97f, 12, 0.02);
		const double Half = R * 0.62;
		auto Tower = [&](const FVector& Base, double Rt, double Ht, bool bKeep)
		{
			TNProcAddCylinder(M, Base, Base + FVector(0.0, 0.0, Ht), Rt, Rt * 0.82, 10, Wet);
			for (int32 c = 0; c < 6; ++c)
			{
				const double A = TNProcMap::TwoPi * c / 6;
				M.AddBox(Base + FVector(FMath::Cos(A) * Rt * 0.72, FMath::Sin(A) * Rt * 0.72, Ht + 6.0), FVector(FMath::Cos(A), FMath::Sin(A), 0.0), FVector(6.0, 5.0, 6.0), Wet);
			}
			if (bKeep)
			{
				M.AddBeam(Base + FVector(0.0, 0.0, Ht), Base + FVector(0.0, 0.0, Ht + 45.0), 1.2, WoodLight);
				M.AddTri(Base + FVector(0.0, 0.0, Ht + 45.0), Base + FVector(0.0, 0.0, Ht + 30.0), Base + FVector(0.0, 24.0, Ht + 38.0), FVector(1.0, 0.0, 0.0), Red);
				M.AddTri(Base + FVector(0.0, 0.0, Ht + 45.0), Base + FVector(0.0, 0.0, Ht + 30.0), Base + FVector(0.0, 24.0, Ht + 38.0), FVector(-1.0, 0.0, 0.0), Red * 0.8f);
			}
		};
		for (int32 k = 0; k < 4; ++k)
		{
			const double A = HALF_PI * k + PI * 0.25;
			const FVector C(FMath::Cos(A) * Half * 1.41, FMath::Sin(A) * Half * 1.41, Z0 + 26.0);
			Tower(C, R * 0.2, H * 0.45, false);
			const double B = A + HALF_PI;
			const FVector D(FMath::Cos(B) * Half * 1.41, FMath::Sin(B) * Half * 1.41, Z0 + 26.0);
			const FVector Mid = (C + D) * 0.5;
			M.AddBox(Mid + FVector(0.0, 0.0, H * 0.12), D - C, FVector((D - C).Size() * 0.5, 7.0, H * 0.12), Wet * 1.05f);
		}
		Tower(FVector(0.0, 0.0, Z0 + 26.0), R * 0.32, H * 0.62, false);
		Tower(FVector(0.0, 0.0, Z0 + 26.0 + H * 0.62), R * 0.17, H * 0.3, true);
		for (int32 s = 0; s < 5; ++s)
		{
			const double A = TNPropRand(P.Seed, s, 0.0, TNProcMap::TwoPi);
			const FVector C(FMath::Cos(A) * R * 0.85, FMath::Sin(A) * R * 0.85, Z0 + 16.0);
			M.AddBox(C, FVector(FMath::Cos(A), FMath::Sin(A), 0.0), FVector(4.0, 3.0, 1.5), s % 2 ? Pink : White);
		}
	}

	/** Tótem tallado: bloques pintados con caras, alas y pico arriba. */
	inline void TNPathPropTotem(FTNProcMeshBuffers& M, double R, double H, uint32 Seed)
	{
		using namespace PropColors;
		const FLinearColor Paints[4] = { Red, Teal, Yellow, WoodLight };
		const int32 N = FMath::Max(2, FMath::RoundToInt32(H / 95.0));
		const double Seg = H / N;
		const double Hw = R * 0.75;
		for (int32 s = 0; s < N; ++s)
		{
			const double Z = Seg * (s + 0.5);
			const FLinearColor Body = Paints[(s + Seed) % 4] * 0.85f + Wood * 0.15f;
			M.AddBox(FVector(0.0, 0.0, Z), FVector(1.0, 0.0, 0.0), FVector(Hw, Hw, Seg * 0.5 - 2.0), Body);
			M.AddBox(FVector(0.0, 0.0, Seg * s + 2.0), FVector(1.0, 0.0, 0.0), FVector(Hw + 3.0, Hw + 3.0, 3.0), WoodDark);
			for (const double Sx : { -1.0, 1.0 })
			{
				const double Fx = Sx * (Hw + 1.0);
				for (const double Sy : { -1.0, 1.0 })
				{
					M.AddBox(FVector(Fx, Sy * Hw * 0.42, Z + Seg * 0.14), FVector(1.0, 0.0, 0.0), FVector(3.0, Hw * 0.24, Seg * 0.1), White);
					M.AddBox(FVector(Fx + Sx * 2.0, Sy * Hw * 0.42, Z + Seg * 0.14), FVector(1.0, 0.0, 0.0), FVector(2.0, Hw * 0.1, Seg * 0.07), Black);
					M.AddBox(FVector(Fx, Sy * Hw * 0.42, Z + Seg * 0.29), FVector(1.0, 0.0, 0.0), FVector(4.0, Hw * 0.28, Seg * 0.035), Black);
				}
				M.AddBox(FVector(Fx + Sx * 8.0, 0.0, Z - Seg * 0.02), FVector(1.0, 0.0, 0.0), FVector(9.0, Hw * 0.14, Seg * 0.14), Body * 0.8f);
				M.AddBox(FVector(Fx, 0.0, Z - Seg * 0.27), FVector(1.0, 0.0, 0.0), FVector(3.0, Hw * 0.5, Seg * 0.07), (s % 2) ? Red : Black);
			}
		}
		for (const double Sy : { -1.0, 1.0 })
		{
			const FVector Root(0.0, Sy * Hw, H - Seg * 0.3);
			M.AddQuad(Root, Root + FVector(0.0, Sy * R * 1.6, Seg * 0.35), Root + FVector(0.0, Sy * R * 1.9, -Seg * 0.15), Root + FVector(0.0, 0.0, -Seg * 0.35), FVector(1.0, 0.0, 0.0), Paints[(Seed + 1) % 4]);
			M.AddQuad(Root, Root + FVector(0.0, Sy * R * 1.6, Seg * 0.35), Root + FVector(0.0, Sy * R * 1.9, -Seg * 0.15), Root + FVector(0.0, 0.0, -Seg * 0.35), FVector(-1.0, 0.0, 0.0), Paints[(Seed + 1) % 4] * 0.8f);
		}
		M.AddBox(FVector(0.0, 0.0, H + 6.0), FVector(1.0, 0.0, 0.0), FVector(Hw * 0.8, Hw * 0.8, 6.0), Yellow);
	}

	/** Columna en ruinas: basa, fuste acanalado roto y tambores caídos alrededor. */
	inline void TNPathPropRuinColumn(FTNProcMeshBuffers& M, double R, double H, uint32 Seed)
	{
		using namespace PropColors;
		const FLinearColor St = Stone * 1.1f;
		const double Rc = R * 0.42;
		M.AddBox(FVector(0.0, 0.0, 15.0), FVector(1.0, 0.0, 0.0), FVector(Rc * 1.45, Rc * 1.45, 15.0), St * 0.92f);
		const double Top = FMath::Max(90.0, H * TNPropRand(Seed, 1, 0.65, 0.95));
		const int32 Drums = FMath::Max(2, FMath::RoundToInt32(Top / 70.0));
		for (int32 d = 0; d < Drums; ++d)
		{
			const double Z0 = 30.0 + (Top - 30.0) * d / Drums, Z1 = 30.0 + (Top - 30.0) * (d + 1) / Drums;
			TNProcAddCylinder(M, FVector(TNPropRand(Seed, d, -2.0, 2.0), 0.0, Z0), FVector(TNPropRand(Seed, d + 1, -2.0, 2.0), 0.0, Z1), Rc, Rc * 0.98, 12, d % 2 ? St : St * 0.94f, d == Drums - 1);
		}
		TNProcAddBoulder(M, FVector(0.0, 0.0, Top - 6.0), Rc * 0.8, Rc * 0.32, Seed + 7u, St * 0.9f);
		M.AddBox(FVector(Rc * 0.3, Rc * 0.2, Top + Rc * 0.1), FVector(0.4, 1.0, 0.0), FVector(Rc * 0.5, Rc * 0.3, Rc * 0.15), Leaf * 0.9f);
		for (int32 f = 0; f < 2; ++f)
		{
			const double A = TNPropRand(Seed, 10 + f, 0.0, TNProcMap::TwoPi);
			const FVector C(FMath::Cos(A) * R * 0.95, FMath::Sin(A) * R * 0.95, Rc * 0.95);
			const FVector D(-FMath::Sin(A + 0.6), FMath::Cos(A + 0.6), 0.0);
			TNProcAddCylinder(M, C - D * 35.0, C + D * 35.0, Rc * 0.95, Rc * 0.95, 12, St * 0.96f);
		}
		M.AddBox(FVector(-R * 0.7, R * 0.6, 20.0), FVector(0.8, 0.6, 0.0), FVector(Rc * 1.4, Rc * 0.9, 20.0), St * 0.88f);
	}

	/** Vagoneta de mina sobre sus vías, con mineral y un pico apoyado. */
	inline void TNPathPropMineCart(FTNProcMeshBuffers& M, double Len, const FLinearColor& Ore, uint32 Seed)
	{
		using namespace PropColors;
		for (const double Sy : { -40.0, 40.0 }) { M.AddBox(FVector(0.0, Sy, 6.0), FVector(1.0, 0.0, 0.0), FVector(Len * 0.5 + 60.0, 3.5, 3.5), Steel); }
		for (double X = -Len * 0.5 - 40.0; X <= Len * 0.5 + 40.0; X += 60.0) { M.AddBox(FVector(X, 0.0, 2.5), FVector(1.0, 0.0, 0.0), FVector(9.0, 60.0, 2.5), WoodDark); }
		const double L2 = 75.0, W2 = 55.0, Zb = 30.0, Zt = 105.0;
		const FVector B[4] = { FVector(-L2 * 0.8, -W2 * 0.8, Zb), FVector(L2 * 0.8, -W2 * 0.8, Zb), FVector(L2 * 0.8, W2 * 0.8, Zb), FVector(-L2 * 0.8, W2 * 0.8, Zb) };
		const FVector T[4] = { FVector(-L2, -W2, Zt), FVector(L2, -W2, Zt), FVector(L2, W2, Zt), FVector(-L2, W2, Zt) };
		const FLinearColor Bin = (Seed % 2) ? Red * 0.7f : Iron * 1.6f;
		for (int32 k = 0; k < 4; ++k)
		{
			const int32 J = (k + 1) % 4;
			const FVector Out = ((B[k] + B[J] + T[k] + T[J]) * 0.25 - FVector(0.0, 0.0, (Zb + Zt) * 0.5)).GetSafeNormal();
			M.AddQuad(B[k], B[J], T[J], T[k], Out, Bin);
			M.AddQuad(B[k], B[J], T[J], T[k], -Out, Bin * 0.6f);
			M.AddBeam(T[k] + FVector(0.0, 0.0, 2.0), T[J] + FVector(0.0, 0.0, 2.0), 3.0, Iron);
		}
		M.AddQuad(B[0], B[1], B[2], B[3], FVector(0.0, 0.0, 1.0), Bin * 0.5f);
		for (const double Sx : { -1.0, 1.0 })
		{
			for (const double Sy : { -1.0, 1.0 })
			{
				TNProcAddCylinder(M, FVector(Sx * L2 * 0.55, Sy * 36.0, 22.0), FVector(Sx * L2 * 0.55, Sy * 46.0, 22.0), 16.0, 16.0, 10, Iron);
			}
		}
		for (int32 o = 0; o < 5; ++o)
		{
			TNProcAddBoulder(M, FVector(TNPropRand(Seed, o, -40.0, 40.0), TNPropRand(Seed, o + 5, -25.0, 25.0), Zt - 30.0), TNPropRand(Seed, o + 9, 16.0, 24.0), 28.0, Seed + static_cast<uint32>(o), Stone * 0.6f);
		}
		FTNProcMeshBuffers Cr;
		TNPropCrystals(Cr, 1, Seed, Ore);
		TNPropAppend(M, Cr, FVector(10.0, 0.0, Zt - 12.0), 20.0, 0.6);
		M.AddBeam(FVector(L2 + 10.0, 30.0, 0.0), FVector(L2 + 30.0, 30.0, 95.0), 2.5, WoodLight);
		M.AddBeam(FVector(L2 + 30.0, 5.0, 95.0), FVector(L2 + 30.0, 55.0, 90.0), 3.0, Iron);
	}

	/** Puesto de mercado: cuatro postes, toldo a franjas, mostrador con fruta y cajas debajo. */
	inline void TNPathPropMarketStall(FTNProcMeshBuffers& M, double R, double H, uint32 Seed)
	{
		using namespace PropColors;
		const double Wd = R * 0.95, Dp = R * 0.55;
		const FLinearColor Stripe = (Seed % 3 == 0) ? Red : ((Seed % 3 == 1) ? Green : Blue);
		for (const double Sx : { -1.0, 1.0 })
		{
			for (const double Sy : { -1.0, 1.0 })
			{
				M.AddBeam(FVector(Sx * Dp, Sy * Wd, 0.0), FVector(Sx * Dp, Sy * Wd, Sx > 0.0 ? H - 40.0 : H), 4.0, Wood);
			}
		}
		M.AddBox(FVector(-Dp * 0.3, 0.0, 45.0), FVector(1.0, 0.0, 0.0), FVector(Dp * 0.55, Wd - 8.0, 45.0), WoodLight);
		M.AddBox(FVector(-Dp * 0.3, 0.0, 92.0), FVector(1.0, 0.0, 0.0), FVector(Dp * 0.62, Wd - 2.0, 3.0), WoodDark);
		const int32 N = 8;
		for (int32 k = 0; k < N; ++k)
		{
			const double Y0 = -Wd - 10.0 + (2.0 * Wd + 20.0) * k / N, Y1 = -Wd - 10.0 + (2.0 * Wd + 20.0) * (k + 1) / N;
			const FLinearColor& Col = (k % 2) ? White : Stripe;
			const FVector A(-Dp - 15.0, Y0, H + 5.0), B(-Dp - 15.0, Y1, H + 5.0), C(Dp + 30.0, Y1, H - 50.0), D(Dp + 30.0, Y0, H - 50.0);
			M.AddQuad(A, B, C, D, FVector(0.4, 0.0, 1.0), Col);
			M.AddQuad(A, B, C, D, FVector(-0.4, 0.0, -1.0), Col * 0.75f);
			M.AddTri(D, C, (C + D) * 0.5 + FVector(0.0, 0.0, -22.0), FVector(1.0, 0.0, 0.0), Col);
			M.AddTri(D, C, (C + D) * 0.5 + FVector(0.0, 0.0, -22.0), FVector(-1.0, 0.0, 0.0), Col * 0.75f);
		}
		const FLinearColor Fruit[4] = { Orange, Red, Green * 1.2f, Yellow };
		for (int32 b = 0; b < 3; ++b)
		{
			const double Y = -Wd * 0.6 + Wd * 0.6 * b;
			M.AddBox(FVector(-Dp * 0.3, Y, 104.0), FVector(1.0, 0.0, 0.0), FVector(Dp * 0.4, Wd * 0.26, 9.0), WoodLight * 0.9f);
			for (int32 f = 0; f < 6; ++f)
			{
				TNPropBall(M, FVector(-Dp * 0.3 + TNPropRand(Seed, b * 10 + f, -Dp * 0.3, Dp * 0.3), Y + TNPropRand(Seed, b * 10 + f + 50, -Wd * 0.18, Wd * 0.18), 118.0), 7.0, Fruit[(b + Seed) % 4], 6, 3);
			}
		}
		FTNProcMeshBuffers Cr;
		TNPropCrate(Cr, 55.0, WoodLight, Wood);
		TNPropAppend(M, Cr, FVector(Dp * 0.55, Wd * 0.55, 0.0), 12.0);
		TNPropAppend(M, Cr, FVector(Dp * 0.55, -Wd * 0.4, 0.0), -8.0);
	}

	/**
	 * Construye un obstáculo de objetos (local: X = Dir de la feature, Z arriba, origen en el suelo del
	 * camino). Ground(X, Y) da la cota del suelo relativa al origen.
	 */
	template <typename FGround>
	void TNPathPropBuild(FTNProcMeshBuffers& M, EPathProp Kind, const FTNPathPropParams& P, FGround&& Ground)
	{
		using namespace PropColors;
		const double R = P.Radius, H = P.Height;
		const uint32 S = P.Seed;
		switch (Kind)
		{
			case EPathProp::CrateStack:
			{
				const int32 Base = R < 140.0 ? 2 : 3;
				TArray<double> Sizes;
				for (int32 b = 0; b < Base; ++b)
				{
					const double Sz = TNPropRand(S, b, 85.0, 110.0);
					Sizes.Add(Sz);
					FTNProcMeshBuffers Cr;
					TNPropCrate(Cr, Sz, (b + S) % 3 == 2 ? WoodDark * 1.3f : WoodLight, (b + S) % 3 == 2 ? WoodDark : Wood);
					const double Y = (b - (Base - 1) * 0.5) * 108.0;
					TNPropAppend(M, Cr, FVector(TNPropRand(S, b + 10, -15.0, 15.0), Y, Ground(0.0, Y)), TNPropRand(S, b + 20, -12.0, 12.0));
				}
				if (H > 130.0)
				{
					for (int32 b = 0; b + 1 < Base; ++b)
					{
						const double Y = (b + 0.5 - (Base - 1) * 0.5) * 108.0;
						FTNProcMeshBuffers Cr;
						TNPropCrate(Cr, 85.0, WoodLight * 1.05f, Wood);
						TNPropAppend(M, Cr, FVector(0.0, Y, Ground(0.0, Y) + FMath::Min(Sizes[b], Sizes[b + 1]) - 2.0), TNPropRand(S, b + 30, -20.0, 20.0));
					}
				}
				if (H > 200.0 && Base == 3)
				{
					FTNProcMeshBuffers Cr;
					TNPropCrate(Cr, 75.0, WoodDark * 1.3f, WoodDark);
					TNPropAppend(M, Cr, FVector(0.0, 0.0, Ground(0.0, 0.0) + 85.0 + FMath::Min(Sizes[0], Sizes[1]) - 4.0), 25.0);
				}
				break;
			}
			case EPathProp::BarrelGroup:
			{
				const int32 N = R < 125.0 ? 3 : 4;
				for (int32 b = 0; b < N; ++b)
				{
					const double A = TNProcMap::TwoPi * b / N + 0.3;
					const double X = FMath::Cos(A) * R * 0.45, Y = FMath::Sin(A) * R * 0.45;
					TNPropPlace(M, EPropKind::WoodBarrel, (b + S) % 3, S + b, P, Ground, X, Y, TNPropRand(S, b, 0.0, 360.0));
				}
				FTNProcMeshBuffers Br;
				TNPropWoodBarrel(Br, S % 3);
				// Tumbado a lo largo de X (giro propio: los ejes han de conservar la orientación de las caras).
				TNPropAppendXf(M, Br, FVector(-R * 0.95 + 52.0, R * 0.6, Ground(-R * 0.6, R * 0.6) + 38.0), FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(1.0, 0.0, 0.0));
				break;
			}
			case EPathProp::Barricade:
			{
				FTNProcMeshBuffers Bc;
				TNPropBarricade(Bc, P.Length);
				TNPropAppend(M, Bc, FVector(0.0, 0.0, Ground(0.0, 0.0)), 0.0);
				TNPropPlace(M, EPropKind::TrafficCone, 0, S, P, Ground, P.Length * 0.5 + 35.0, 30.0, 10.0);
				TNPropPlace(M, EPropKind::TrafficCone, 0, S + 1, P, Ground, -P.Length * 0.5 - 35.0, -25.0, 40.0);
				break;
			}
			case EPathProp::HayBales:
			{
				TNPropPlace(M, EPropKind::HayBale, 0, S, P, Ground, -R * 0.35, -R * 0.35, TNPropRand(S, 1, 0.0, 180.0));
				TNPropPlace(M, EPropKind::HayBale, 2, S + 1, P, Ground, R * 0.3, R * 0.3, TNPropRand(S, 2, 0.0, 180.0));
				if (R > 150.0) { TNPropPlace(M, EPropKind::HayBale, 1, S + 2, P, Ground, R * 0.45, -R * 0.5, TNPropRand(S, 3, 0.0, 180.0)); }
				break;
			}
			case EPathProp::Sandcastle:
				TNPathPropSandcastle(M, P, Ground);
				TNPropPlace(M, EPropKind::SandBucket, S % 3, S, P, Ground, -R * 1.05, R * 0.4, 30.0);
				break;
			case EPathProp::Rowboat:
			{
				FTNProcMeshBuffers Bt;
				TNPathPropRowboat(Bt, P.Length, S);
				TNPropAppend(M, Bt, FVector(0.0, 0.0, Ground(0.0, 0.0) - 4.0), TNPropRand(S, 1, -12.0, 12.0));
				break;
			}
			case EPathProp::BeachSet:
			{
				TNPropPlace(M, EPropKind::Parasol, S % 3, S, P, Ground, -R * 0.2, -R * 0.35, TNPropRand(S, 1, 0.0, 360.0));
				TNPropPlace(M, EPropKind::BeachTowel, (S + 1) % 3, S + 1, P, Ground, R * 0.35, R * 0.35, TNPropRand(S, 2, -30.0, 30.0) + 90.0);
				TNPropPlace(M, EPropKind::SandBucket, (S + 2) % 3, S + 2, P, Ground, R * 0.6, -R * 0.45, TNPropRand(S, 3, 0.0, 360.0));
				const double Cx = -R * 0.55, Cy = R * 0.5;
				const double Z = Ground(Cx, Cy);
				M.AddBox(FVector(Cx, Cy, Z + 22.0), FVector(0.9, 0.4, 0.0), FVector(30.0, 20.0, 22.0), Blue);
				M.AddBox(FVector(Cx, Cy, Z + 47.0), FVector(0.9, 0.4, 0.0), FVector(32.0, 22.0, 4.0), White);
				break;
			}
			case EPathProp::Totem:
			{
				FTNProcMeshBuffers Tm;
				TNPathPropTotem(Tm, R, H, S);
				TNPropAppend(M, Tm, FVector(0.0, 0.0, Ground(0.0, 0.0) - 5.0), TNPropRand(S, 1, -10.0, 10.0));
				break;
			}
			case EPathProp::RuinColumn:
			{
				FTNProcMeshBuffers Rc;
				TNPathPropRuinColumn(Rc, R, H, S);
				TNPropAppend(M, Rc, FVector(0.0, 0.0, Ground(0.0, 0.0) - 6.0), TNPropRand(S, 1, 0.0, 360.0));
				break;
			}
			case EPathProp::GiantMushrooms:
				TNPropPlace(M, EPropKind::Mushrooms, S % 3, S, P, Ground, 0.0, 0.0, TNPropRand(S, 1, 0.0, 360.0), H / 34.0);
				break;
			case EPathProp::SkullRock:
			{
				const FLinearColor RockC = P.bCharred ? Black * 3.0f : Stone * 0.95f;
				for (int32 b = 0; b < 3; ++b)
				{
					const double A = TNProcMap::TwoPi * b / 3 + 0.5;
					const double X = FMath::Cos(A) * R * 0.45, Y = FMath::Sin(A) * R * 0.45;
					TNProcAddBoulder(M, FVector(X, Y, Ground(X, Y) - 10.0), R * TNPropRand(S, b, 0.45, 0.6), H * 0.45, S + static_cast<uint32>(b) * 17u, RockC);
				}
				TNPropPlace(M, EPropKind::CattleSkull, 2, S, P, Ground, R * 0.05, 0.0, TNPropRand(S, 5, -30.0, 30.0) + 180.0, R / 38.0, H * 0.3);
				break;
			}
			case EPathProp::PotteryJars:
			{
				const int32 N = 4 + static_cast<int32>(S % 3);
				for (int32 j = 0; j < N; ++j)
				{
					const double A = TNProcMap::TwoPi * j / N + TNPropRand(S, j, 0.0, 0.6);
					const double D = j == 0 ? 0.0 : R * TNPropRand(S, j + 10, 0.45, 0.85);
					const double Sc = j == 0 ? H / 60.0 : TNPropRand(S, j + 20, 0.8, 1.3);
					const EPropKind Kd = (j % 2) ? EPropKind::Amphora : EPropKind::ClayPot;
					TNPropPlace(M, Kd, (j == N - 1) ? (Kd == EPropKind::Amphora ? 1 : 2) : (j % 2), S + j, P, Ground, FMath::Cos(A) * D, FMath::Sin(A) * D, TNPropRand(S, j + 30, 0.0, 360.0), Sc);
				}
				break;
			}
			case EPathProp::CrystalSpikes:
				TNPropPlace(M, EPropKind::Crystals, 2, S, P, Ground, 0.0, 0.0, TNPropRand(S, 1, 0.0, 360.0), H / 95.0, -8.0);
				break;
			case EPathProp::Cairn:
				TNPropPlace(M, EPropKind::Cairn, 2, S, P, Ground, 0.0, 0.0, TNPropRand(S, 1, 0.0, 360.0), H / 100.0, -5.0);
				break;
			case EPathProp::MineCart:
			{
				FTNProcMeshBuffers Mc;
				TNPathPropMineCart(Mc, P.Length, P.Crystal, S);
				TNPropAppend(M, Mc, FVector(0.0, 0.0, Ground(0.0, 0.0) - 3.0), 0.0);
				break;
			}
			case EPathProp::CrabTraps:
			{
				TNPropPlace(M, EPropKind::CrabTrap, 0, S, P, Ground, -R * 0.3, -R * 0.2, 10.0);
				TNPropPlace(M, EPropKind::CrabTrap, 1, S + 1, P, Ground, R * 0.35, -R * 0.15, -8.0);
				if (H > 100.0) { TNPropPlace(M, EPropKind::CrabTrap, 2, S + 2, P, Ground, 0.0, -R * 0.2, 25.0, 1.0, 44.0); }
				TNPropTorus(M, FVector(R * 0.2, R * 0.55, Ground(R * 0.2, R * 0.55) + 5.0), FVector(0.0, 0.0, 1.0), 26.0, 4.0, 12, 4, Rope, Rope, 0);
				TNPropTorus(M, FVector(R * 0.2, R * 0.55, Ground(R * 0.2, R * 0.55) + 13.0), FVector(0.0, 0.0, 1.0), 22.0, 4.0, 12, 4, Rope, Rope, 0);
				break;
			}
			case EPathProp::MarketStall:
			{
				FTNProcMeshBuffers St;
				TNPathPropMarketStall(St, R, H, S);
				TNPropAppend(M, St, FVector(0.0, 0.0, Ground(0.0, 0.0) - 3.0), 0.0);
				break;
			}
			case EPathProp::ConeLine:
			{
				const int32 N = FMath::Max(3, FMath::RoundToInt32(P.Length / 75.0));
				FVector Prev = FVector::ZeroVector;
				for (int32 c = 0; c < N; ++c)
				{
					const double X = -P.Length * 0.5 + P.Length * c / (N - 1);
					TNPropPlace(M, EPropKind::TrafficCone, 0, S + c, P, Ground, X, 0.0, TNPropRand(S, c, -20.0, 20.0));
					const FVector Top(X, 0.0, Ground(X, 0.0) + 58.0);
					if (c > 0)
					{
						const int32 K = 4;
						for (int32 k = 0; k < K; ++k)
						{
							M.AddBeam(FMath::Lerp(Prev, Top, static_cast<double>(k) / K), FMath::Lerp(Prev, Top, static_cast<double>(k + 1) / K), 1.2, (k % 2) ? White : Red);
						}
					}
					Prev = Top;
				}
				break;
			}
			default:
				break;
		}
	}

	/** Nombre de un obstáculo de objetos (herramientas). */
	inline const char* TNPathPropName(EPathProp Kind)
	{
		static const char* Names[] = { "CrateStack", "BarrelGroup", "Barricade", "HayBales", "Sandcastle", "Rowboat", "BeachSet", "Totem", "RuinColumn",
			"GiantMushrooms", "SkullRock", "PotteryJars", "CrystalSpikes", "Cairn", "MineCart", "CrabTraps", "MarketStall", "ConeLine" };
		static_assert(sizeof(Names) / sizeof(Names[0]) == static_cast<int32>(EPathProp::Count), "TNPathPropName desfasado");
		return Names[static_cast<int32>(Kind)];
	}
}
