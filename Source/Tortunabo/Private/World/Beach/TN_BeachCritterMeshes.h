#pragma once

#include "CoreMinimal.h"
#include "TN_BeachEnemyMeshes.h"

/**
 * Mallas low-poly de caras planas de los enemigos de la ronda 3 de la carrera (solo geometría, sin motor): el cangrejo
 * ermitaño con su caracola y sus piezas, el pulpo de poza (cuerpo, tramo de tentáculo, silueta y charco), la pulga de
 * arena y el tanque de juguete teledirigido por piezas (casco, rueda, torreta, cañón, antena, banderita y bolita de
 * espuma). A TNBeach::Scale veces su tamaño real. Colores en sRGB (la caché de TNBeachKit los decodifica).
 */
namespace TNBeachCritterMeshes
{
	using TNProcMesh::FTNProcMeshBuffers;
	using TNBeachMeshes::Rgb;

	// ─────────────────────────────────────────────────────────────────────────
	// Utilidades
	// ─────────────────────────────────────────────────────────────────────────

	/** Añade Src a Dst transformado por Xf (posiciones y normales; los colores y UV tal cual). */
	inline void AppendTransformed(FTNProcMeshBuffers& Dst, const FTNProcMeshBuffers& Src, const FTransform& Xf)
	{
		const int32 Base = Dst.Verts.Num();
		for (int32 i = 0; i < Src.Verts.Num(); ++i)
		{
			Dst.Verts.Add(Xf.TransformPosition(Src.Verts[i]));
			Dst.Normals.Add(Xf.TransformVectorNoScale(Src.Normals[i]).GetSafeNormal());
			Dst.UVs.Add(Src.UVs.IsValidIndex(i) ? Src.UVs[i] : FVector2D::ZeroVector);
			Dst.Colors.Add(Src.Colors[i]);
		}
		for (const int32 Index : Src.Tris)
		{
			Dst.Tris.Add(Base + Index);
		}
	}

	/** Bulto (TNFaunaBlob) girado: centro, semiejes en su propio marco y giro. */
	inline void AddTiltedBlob(FTNProcMeshBuffers& M, const FVector& Center, const FVector& Radii, const FRotator& Rot, const FLinearColor& Top,
		const FLinearColor& Belly, int32 Seg = 8, int32 Bands = 4)
	{
		FTNProcMeshBuffers Tmp;
		TNFauna::TNFaunaBlob(Tmp, FVector::ZeroVector, Radii, Top, Belly, Seg, Bands);
		AppendTransformed(M, Tmp, FTransform(Rot, Center));
	}

	/** Prisma convexo de un polígono del plano XZ extruido a lo largo de Y, de Y0 a Y1. */
	inline void AddExtrudeY(FTNProcMeshBuffers& M, const TArray<FVector2D>& PolyXZ, double Y0, double Y1, const FLinearColor& Color, const FLinearColor& SideColor)
	{
		const int32 Num = PolyXZ.Num();
		if (Num < 3)
		{
			return;
		}
		FVector2D C = FVector2D::ZeroVector;
		for (const FVector2D& P : PolyXZ)
		{
			C += P;
		}
		C /= static_cast<double>(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			const FVector2D& A = PolyXZ[i];
			const FVector2D& B = PolyXZ[(i + 1) % Num];
			const FVector2D Mid = (A + B) * 0.5 - C;
			M.AddQuad(FVector(A.X, Y0, A.Y), FVector(B.X, Y0, B.Y), FVector(B.X, Y1, B.Y), FVector(A.X, Y1, A.Y), FVector(Mid.X, 0.0, Mid.Y), Color);
			M.AddTri(FVector(C.X, Y0, C.Y), FVector(A.X, Y0, A.Y), FVector(B.X, Y0, B.Y), FVector(0.0, -1.0, 0.0), SideColor);
			M.AddTri(FVector(C.X, Y1, C.Y), FVector(A.X, Y1, A.Y), FVector(B.X, Y1, B.Y), FVector(0.0, 1.0, 0.0), SideColor);
		}
	}

	/** Estrella de cinco puntas plana (radio R) en Center, de cara a Normal y con una punta hacia Up. */
	inline void AddStar(FTNProcMeshBuffers& M, const FVector& Center, const FVector& Normal, const FVector& Up, double R, const FLinearColor& Color)
	{
		const FVector N = Normal.GetSafeNormal();
		FVector U = (Up - N * FVector::DotProduct(Up, N)).GetSafeNormal();
		if (U.IsNearlyZero())
		{
			U = FVector::CrossProduct(N, FVector::ForwardVector).GetSafeNormal();
		}
		const FVector S = FVector::CrossProduct(N, U);
		for (int32 k = 0; k < 10; ++k)
		{
			const double A0 = UE_DOUBLE_TWO_PI * k / 10.0;
			const double A1 = UE_DOUBLE_TWO_PI * (k + 1) / 10.0;
			const double R0 = (k % 2) ? R * 0.42 : R;
			const double R1 = (k % 2) ? R : R * 0.42;
			M.AddTri(Center, Center + (U * FMath::Cos(A0) + S * FMath::Sin(A0)) * R0, Center + (U * FMath::Cos(A1) + S * FMath::Sin(A1)) * R1, N,
				(k % 2) ? Color : Color * 0.88f);
		}
	}

	/** Escarapela de Tortunavy (disco azul marino, aro y estrella dorados) de radio R pegada a una cara. */
	inline void AddRoundel(FTNProcMeshBuffers& M, const FVector& Center, const FVector& Normal, const FVector& Up, double R)
	{
		const FLinearColor Navy = Rgb(0.07f, 0.19f, 0.36f);
		const FLinearColor Gold = Rgb(1.f, 0.8f, 0.24f);
		const FVector N = Normal.GetSafeNormal();
		FVector U = (Up - N * FVector::DotProduct(Up, N)).GetSafeNormal();
		if (U.IsNearlyZero())
		{
			U = FVector::CrossProduct(N, FVector::ForwardVector).GetSafeNormal();
		}
		const FVector S = FVector::CrossProduct(N, U);
		const FVector Disc = Center + N * 1.5;
		constexpr int32 Seg = 12;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = UE_DOUBLE_TWO_PI * k / Seg;
			const double A1 = UE_DOUBLE_TWO_PI * (k + 1) / Seg;
			const FVector E0 = U * FMath::Cos(A0) + S * FMath::Sin(A0);
			const FVector E1 = U * FMath::Cos(A1) + S * FMath::Sin(A1);
			M.AddTri(Disc, Disc + E0 * (R * 0.84), Disc + E1 * (R * 0.84), N, Navy);
			M.AddQuad(Disc + E0 * (R * 0.84), Disc + E1 * (R * 0.84), Disc + E1 * R, Disc + E0 * R, N, Gold);
		}
		AddStar(M, Center + N * 3.0, N, U, R * 0.66, Gold);
	}

	/** Mezcla de dos colores. */
	inline FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return A + (B - A) * FMath::Clamp(T, 0.f, 1.f);
	}

	/** Triángulo con el alfa de sus vértices dado por AlphaOf(posición) (mallas translúcidas por el alfa del vértice). */
	template <typename TAlphaFn>
	inline void AddTriA(FTNProcMeshBuffers& M, const FVector& A, const FVector& B, const FVector& C, const FVector& Hint, const FLinearColor& Color, TAlphaFn AlphaOf)
	{
		const int32 Before = M.Verts.Num();
		M.AddTri(A, B, C, Hint, Color);
		for (int32 i = Before; i < M.Verts.Num(); ++i)
		{
			M.Colors[i].A = AlphaOf(M.Verts[i]);
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Cangrejo ermitaño bola
	// ─────────────────────────────────────────────────────────────────────────

	/** Radio real de la caracola (cm): a escala, una bola de ~2,2 m (la tortuga mide 1,4 m). */
	constexpr double HermitShellReal = 4.0;

	/** Radio de rodar de la caracola (cm de juego, con SizeScale 1). */
	inline double HermitRadius()
	{
		return HermitShellReal * TNBeach::Scale;
	}

	/** Centro de la boca de la caracola en su espacio (el ermitaño asoma por aquí, hacia +X). */
	inline FVector HermitAperture()
	{
		return FVector(HermitRadius() * 0.8, 0.0, -HermitRadius() * 0.04);
	}

	struct FHermitLook
	{
		FLinearColor ShellA = Rgb(0.95f, 0.86f, 0.7f);
		FLinearColor ShellB = Rgb(0.72f, 0.38f, 0.2f);
		FLinearColor Lip = Rgb(0.98f, 0.7f, 0.72f);
		FLinearColor Hole = Rgb(0.25f, 0.1f, 0.14f);
		FLinearColor Knob = Rgb(0.98f, 0.92f, 0.8f);
		FLinearColor Body = Rgb(0.95f, 0.42f, 0.2f);
		FLinearColor Claw = Rgb(0.55f, 0.25f, 0.62f);
		FLinearColor ClawTip = Rgb(0.98f, 0.85f, 0.6f);
		FLinearColor Leg = Rgb(0.9f, 0.38f, 0.18f);
	};

	/** Caracola de bandas crema y óxido (ermitaño naranja de pinza violeta), turbante gris verdoso, rosa y leonada. */
	inline FHermitLook HermitPalette(int32 Index)
	{
		FHermitLook L;
		switch (((Index % 4) + 4) % 4)
		{
		case 1:
			L.ShellA = Rgb(0.62f, 0.7f, 0.6f); L.ShellB = Rgb(0.3f, 0.38f, 0.34f); L.Lip = Rgb(0.95f, 0.9f, 0.75f); L.Hole = Rgb(0.12f, 0.14f, 0.12f);
			L.Knob = Rgb(0.8f, 0.85f, 0.78f); L.Body = Rgb(0.92f, 0.3f, 0.22f); L.Claw = Rgb(0.96f, 0.52f, 0.16f); L.ClawTip = Rgb(1.f, 0.92f, 0.75f);
			L.Leg = Rgb(0.86f, 0.28f, 0.2f);
			break;
		case 2:
			L.ShellA = Rgb(0.98f, 0.8f, 0.82f); L.ShellB = Rgb(0.82f, 0.36f, 0.55f); L.Lip = Rgb(1.f, 0.95f, 0.9f); L.Hole = Rgb(0.35f, 0.12f, 0.25f);
			L.Knob = Rgb(1.f, 0.9f, 0.92f); L.Body = Rgb(0.36f, 0.46f, 0.9f); L.Claw = Rgb(0.96f, 0.76f, 0.2f); L.ClawTip = Rgb(1.f, 0.98f, 0.9f);
			L.Leg = Rgb(0.3f, 0.4f, 0.84f);
			break;
		case 3:
			L.ShellA = Rgb(0.86f, 0.72f, 0.5f); L.ShellB = Rgb(0.46f, 0.3f, 0.2f); L.Lip = Rgb(0.96f, 0.82f, 0.56f); L.Hole = Rgb(0.2f, 0.12f, 0.08f);
			L.Knob = Rgb(0.98f, 0.95f, 0.88f); L.Body = Rgb(0.98f, 0.7f, 0.2f); L.Claw = Rgb(0.86f, 0.24f, 0.24f); L.ClawTip = Rgb(1.f, 0.9f, 0.72f);
			L.Leg = Rgb(0.92f, 0.62f, 0.18f);
			break;
		default:
			break;
		}
		return L;
	}

	/**
	 * Caracola centrada en su centro de rodar: cuerpo de revolución a lo largo de X (la punta de la espira atrás, la boca
	 * delante) con un surco en espiral, bandas de dos colores que siguen la espiral, nudos en el hombro y la boca con su
	 * labio y el hueco oscuro.
	 */
	inline void BuildHermitShell(FTNProcMeshBuffers& M, const FHermitLook& L, uint32 Seed)
	{
		const double R = HermitRadius();
		static constexpr int32 NumRings = 10;
		static constexpr double Xs[NumRings] = { -1.1, -0.95, -0.78, -0.58, -0.34, -0.08, 0.2, 0.45, 0.66, 0.8 };
		static constexpr double Rs[NumRings] = { 0.0, 0.2, 0.38, 0.58, 0.8, 0.97, 1.0, 0.9, 0.7, 0.5 };
		constexpr int32 Seg = 14;
		// Coordenada de la espiral: 2,3 vueltas de la punta a la boca.
		auto SpiralU = [](double XRel, double Theta)
		{
			return (XRel + 1.1) * 2.3 + Theta / UE_DOUBLE_TWO_PI;
		};
		auto RingPoint = [&](int32 Ring, int32 K)
		{
			const double XRel = Xs[Ring];
			const double Theta = UE_DOUBLE_TWO_PI * K / Seg + XRel * 0.35;
			const double U = SpiralU(XRel, Theta);
			const double F = U - FMath::FloorToDouble(U);
			// Surco en espiral: la sección se hunde un 8 % donde pasa la línea entre dos vueltas.
			const double Groove = 1.0 - TNProcMap::SmoothStep(0.0, 0.16, FMath::Min(F, 1.0 - F));
			const double Rad = Rs[Ring] * R * (1.0 - 0.08 * Groove) * (1.0 + 0.03 * TNProcMesh::TNProcHashNoise(Ring, K, Seed));
			return FVector(XRel * R, FMath::Cos(Theta) * Rad, FMath::Sin(Theta) * Rad);
		};
		for (int32 Ring = 0; Ring + 1 < NumRings; ++Ring)
		{
			for (int32 K = 0; K < Seg; ++K)
			{
				const FVector P00 = RingPoint(Ring, K);
				const FVector P01 = RingPoint(Ring, K + 1);
				const FVector P11 = RingPoint(Ring + 1, K + 1);
				const FVector P10 = RingPoint(Ring + 1, K);
				const FVector Mid = (P00 + P01 + P11 + P10) * 0.25;
				const double XRel = Mid.X / R;
				const double Theta = FMath::Atan2(Mid.Z, Mid.Y);
				const double U = SpiralU(XRel, Theta);
				const int32 Band = FMath::FloorToInt32(U * 2.0);
				const FLinearColor Col = (Band % 2 == 0) ? L.ShellA : L.ShellB;
				const float Shade = Mid.Z < -R * 0.4 ? 0.88f : 1.f;
				M.AddQuad(P00, P01, P11, P10, Mid - FVector(Mid.X, 0.0, 0.0), Col * Shade);
			}
		}
		// Boca: labio que se abre hacia fuera y hueco oscuro hundido.
		const double LipX0 = Xs[NumRings - 1] * R;
		auto Ring2 = [R](double X, double Rad, int32 K)
		{
			const double Theta = UE_DOUBLE_TWO_PI * K / 14 + (X / R) * 0.35;
			return FVector(X, FMath::Cos(Theta) * Rad, FMath::Sin(Theta) * Rad);
		};
		for (int32 K = 0; K < Seg; ++K)
		{
			const FVector A0 = RingPoint(NumRings - 1, K);
			const FVector A1 = RingPoint(NumRings - 1, K + 1);
			const FVector B0 = Ring2(LipX0 + R * 0.07, R * 0.62, K);
			const FVector B1 = Ring2(LipX0 + R * 0.07, R * 0.62, K + 1);
			const FVector C0 = Ring2(LipX0 + R * 0.06, R * 0.44, K);
			const FVector C1 = Ring2(LipX0 + R * 0.06, R * 0.44, K + 1);
			const FVector D0 = Ring2(LipX0 - R * 0.1, R * 0.4, K);
			const FVector D1 = Ring2(LipX0 - R * 0.1, R * 0.4, K + 1);
			const FVector Mid = (A0 + A1 + B0 + B1) * 0.25;
			M.AddQuad(A0, A1, B1, B0, FVector(0.3, Mid.Y, Mid.Z), L.Lip);
			M.AddQuad(B0, B1, C1, C0, FVector(1.0, 0.0, 0.0), L.Lip * 0.95f);
			M.AddQuad(C0, C1, D1, D0, -FVector(0.0, (C0 + C1).Y, (C0 + C1).Z), L.Hole * 1.3f);
			M.AddTri(FVector(LipX0 - R * 0.1, 0.0, 0.0), D0, D1, FVector(1.0, 0.0, 0.0), L.Hole);
		}
		// Nudos en el hombro de la última vuelta (caracola de turbante).
		for (int32 K = 0; K < 7; ++K)
		{
			const double Theta = UE_DOUBLE_TWO_PI * (K + 0.3) / 7.0;
			const double X = -0.3 * R;
			const double Rad = 0.84 * R;
			const FVector At(X, FMath::Cos(Theta) * Rad, FMath::Sin(Theta) * Rad);
			TNFauna::TNFaunaBlob(M, At, FVector(R * 0.1, R * 0.09, R * 0.09), L.Knob, L.Knob * 0.9f, 5, 3);
		}
	}

	/** Cabeza y tórax del ermitaño (espacio del cangrejo: origen en la boca de la caracola, +X hacia fuera). */
	inline void BuildHermitHead(FTNProcMeshBuffers& M, const FHermitLook& L)
	{
		const double R = HermitRadius();
		TNFauna::TNFaunaBlob(M, FVector(R * 0.08, 0.0, 0.0), FVector(R * 0.2, R * 0.25, R * 0.17), L.Body, L.Body * 0.85f, 8, 4);
		// Escudo de la cabeza, algo más claro, y la boca con sus apéndices.
		TNFauna::TNFaunaBlob(M, FVector(R * 0.16, 0.0, R * 0.08), FVector(R * 0.1, R * 0.14, R * 0.07), L.Body * 1.12f, L.Body, 6, 3);
		for (const double Side : { -1.0, 1.0 })
		{
			M.AddBeam(FVector(R * 0.24, Side * R * 0.05, -R * 0.06), FVector(R * 0.32, Side * R * 0.08, -R * 0.16), R * 0.018, L.Leg);
			M.AddBeam(FVector(R * 0.22, Side * R * 0.1, -R * 0.08), FVector(R * 0.3, Side * R * 0.14, -R * 0.2), R * 0.016, L.Leg * 0.9f);
		}
	}

	/** Antenas largas y antenitas (pivote en su base). */
	inline void BuildHermitAntennae(FTNProcMeshBuffers& M, const FHermitLook& L)
	{
		const double R = HermitRadius();
		for (const double Side : { -1.0, 1.0 })
		{
			const FVector Mid(R * 0.35, Side * R * 0.2, R * 0.24);
			const FVector Tip(R * 0.82, Side * R * 0.46, R * 0.32);
			M.AddBeam(FVector(0.0, Side * R * 0.04, 0.0), Mid, R * 0.02, L.Leg);
			M.AddBeam(Mid, Tip, R * 0.014, L.Leg * 1.1f);
			const FVector Short(R * 0.2, Side * R * 0.08, R * 0.18);
			M.AddBeam(FVector(0.0, Side * R * 0.02, 0.0), Short, R * 0.016, L.Body);
			M.AddBeam(Short, Short + FVector(R * 0.06, Side * R * 0.04, -R * 0.05), R * 0.02, L.Body * 0.8f);
		}
	}

	/** Pedúnculo con su ojo negro de dibujo y un brillo (pivote en la base; vale para los dos lados). */
	inline void BuildHermitEye(FTNProcMeshBuffers& M, const FHermitLook& L)
	{
		const double R = HermitRadius();
		const FVector Top(R * 0.05, 0.0, R * 0.32);
		M.AddBeam(FVector::ZeroVector, Top, R * 0.035, L.Body);
		const FVector Ball = Top + FVector(R * 0.02, 0.0, R * 0.07);
		TNFauna::TNFaunaBlob(M, Ball, FVector(R * 0.08, R * 0.075, R * 0.095), Rgb(0.07f, 0.06f, 0.08f), Rgb(0.05f, 0.04f, 0.06f), 7, 3);
		M.AddBox(Ball + FVector(R * 0.065, R * 0.02, R * 0.04), FVector::ForwardVector, FVector(R * 0.02, R * 0.025, R * 0.025), Rgb(1.f, 1.f, 1.f));
	}

	/** Pinza grande (la izquierda, como en los ermitaños de verdad), con el pivote en el hombro. Scale la achica para la otra. */
	inline void BuildHermitClaw(FTNProcMeshBuffers& M, const FHermitLook& L, double Scale)
	{
		const double R = HermitRadius() * Scale;
		const FVector Wrist(R * 0.2, 0.0, -R * 0.04);
		M.AddBeam(FVector::ZeroVector, Wrist, R * 0.06, L.Claw * 0.9f);
		TNFauna::TNFaunaBlob(M, Wrist, FVector(R * 0.08, R * 0.08, R * 0.08), L.Claw * 0.95f, L.Claw * 0.85f, 6, 3);
		TNFauna::TNFaunaBlob(M, FVector(R * 0.38, 0.0, 0.0), FVector(R * 0.2, R * 0.11, R * 0.16), L.Claw, L.Claw * 0.82f, 8, 4);
		// Granos de la palma.
		for (int32 k = 0; k < 4; ++k)
		{
			M.AddBox(FVector(R * (0.3 + 0.06 * k), R * (k % 2 ? 0.03 : -0.03), R * 0.15), FVector::ForwardVector, FVector(R * 0.025, R * 0.025, R * 0.02), L.Claw * 1.2f);
		}
		TNProcMesh::TNProcAddCylinder(M, FVector(R * 0.52, 0.0, -R * 0.06), FVector(R * 0.72, 0.0, -R * 0.1), R * 0.07, R * 0.015, 5, L.ClawTip, true);
		TNProcMesh::TNProcAddCylinder(M, FVector(R * 0.5, 0.0, R * 0.05), FVector(R * 0.7, 0.0, R * 0.01), R * 0.06, R * 0.012, 5, L.ClawTip * 0.95f, true);
	}

	/** Pata de andar de dos tramos hacia Side (+1 derecha); pivote en el costado del cuerpo, el pie cerca del suelo. */
	inline void BuildHermitLeg(FTNProcMeshBuffers& M, const FHermitLook& L, double Side)
	{
		const double R = HermitRadius();
		const FVector Knee(R * 0.08, Side * R * 0.3, R * 0.1);
		const FVector Ankle(R * 0.22, Side * R * 0.44, -R * 0.45);
		const FVector Foot(R * 0.3, Side * R * 0.46, -R * 0.74);
		M.AddBeam(FVector::ZeroVector, Knee, R * 0.035, L.Leg);
		TNFauna::TNFaunaBlob(M, Knee, FVector(R * 0.045, R * 0.045, R * 0.045), L.Leg, L.Leg * 0.9f, 5, 3);
		M.AddBeam(Knee, Ankle, R * 0.03, L.Leg * 0.92f);
		TNProcMesh::TNProcAddCylinder(M, Ankle, Foot, R * 0.03, R * 0.008, 4, L.ClawTip * 0.85f, true);
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Pulpo de poza
	// ─────────────────────────────────────────────────────────────────────────

	/** Largo real del cuerpo (cm): a escala, ~1,5 m de cuerpo y brazos de unos 3,7 m (cabe tumbado en una poza de 1,4 m). */
	constexpr double OctoMantleReal = 5.5;

	inline double OctoSize()
	{
		return OctoMantleReal * TNBeach::Scale;
	}

	/** Largo de un brazo (cm de juego) y cuántos tramos lo forman. */
	inline double OctoArmLength()
	{
		return OctoSize() * 2.4;
	}
	constexpr int32 OctoArmSegments = 6;

	struct FOctoLook
	{
		FLinearColor Skin = Rgb(0.9f, 0.36f, 0.24f);
		FLinearColor SkinDark = Rgb(0.7f, 0.22f, 0.16f);
		FLinearColor Belly = Rgb(0.98f, 0.7f, 0.6f);
		FLinearColor Sucker = Rgb(1.f, 0.86f, 0.78f);
		FLinearColor Iris = Rgb(1.f, 0.85f, 0.2f);
		FLinearColor Spot = Rgb(0.6f, 0.16f, 0.12f);
	};

	/** Rojo anaranjado, violeta, amarillo de anillos azules y verde azulado moteado. */
	inline FOctoLook OctoPalette(int32 Index)
	{
		FOctoLook L;
		switch (((Index % 4) + 4) % 4)
		{
		case 1:
			L.Skin = Rgb(0.56f, 0.3f, 0.72f); L.SkinDark = Rgb(0.38f, 0.18f, 0.52f); L.Belly = Rgb(0.85f, 0.7f, 0.92f); L.Sucker = Rgb(0.96f, 0.86f, 1.f);
			L.Iris = Rgb(0.96f, 0.95f, 0.4f); L.Spot = Rgb(0.3f, 0.12f, 0.45f);
			break;
		case 2:
			L.Skin = Rgb(0.96f, 0.8f, 0.36f); L.SkinDark = Rgb(0.78f, 0.6f, 0.2f); L.Belly = Rgb(1.f, 0.92f, 0.7f); L.Sucker = Rgb(1.f, 0.95f, 0.85f);
			L.Iris = Rgb(0.95f, 0.5f, 0.15f); L.Spot = Rgb(0.15f, 0.45f, 0.96f);
			break;
		case 3:
			L.Skin = Rgb(0.3f, 0.62f, 0.55f); L.SkinDark = Rgb(0.18f, 0.42f, 0.38f); L.Belly = Rgb(0.75f, 0.9f, 0.82f); L.Sucker = Rgb(0.9f, 0.98f, 0.92f);
			L.Iris = Rgb(1.f, 0.62f, 0.15f); L.Spot = Rgb(0.12f, 0.3f, 0.28f);
			break;
		default:
			break;
		}
		return L;
	}

	/**
	 * Cabeza y manto del pulpo: origen en el anillo donde nacen los brazos, +X hacia donde mira. Ojos saltones de pupila
	 * de barra, cejas, manchas, sifón y la membrana de los brazos.
	 */
	inline void BuildOctoBody(FTNProcMeshBuffers& M, const FOctoLook& L, uint32 Seed)
	{
		const double S = OctoSize();
		// Membrana y cabeza.
		TNFauna::TNFaunaBlob(M, FVector(0.0, 0.0, S * 0.02), FVector(S * 0.24, S * 0.24, S * 0.08), L.SkinDark, L.Belly, 10, 3);
		TNFauna::TNFaunaBlob(M, FVector(S * 0.04, 0.0, S * 0.16), FVector(S * 0.2, S * 0.23, S * 0.17), L.Skin, L.Belly, 10, 4);
		// Manto que sube hacia atrás.
		AddTiltedBlob(M, FVector(-S * 0.24, 0.0, S * 0.42), FVector(S * 0.38, S * 0.28, S * 0.3), FRotator(-28.f, 0.f, 0.f), L.Skin, L.SkinDark, 10, 5);
		// Manchas (o anillos del pulpo de anillos azules).
		for (int32 k = 0; k < 9; ++k)
		{
			const double U = TNProcMesh::TNProcHashNoise(k, 1, Seed);
			const double V = TNProcMesh::TNProcHashNoise(k, 2, Seed);
			const FVector At(-S * (0.2 + 0.2 * U), S * 0.26 * V, S * (0.5 + 0.12 * U));
			const FVector Out = (At - FVector(-S * 0.24, 0.0, S * 0.42)).GetSafeNormal();
			const FVector Surface = FVector(-S * 0.24, 0.0, S * 0.42) + Out * (S * 0.3);
			M.AddBox(Surface, Out, FVector(S * 0.035, S * 0.035, S * 0.02), L.Spot);
		}
		// Ojos saltones: blanco, iris y pupila de barra, con su ceja.
		for (const double Side : { -1.0, 1.0 })
		{
			const FVector Eye(S * 0.15, Side * S * 0.15, S * 0.3);
			TNFauna::TNFaunaBlob(M, Eye, FVector(S * 0.075, S * 0.07, S * 0.08), Rgb(0.98f, 0.97f, 0.93f), Rgb(0.9f, 0.88f, 0.84f), 7, 3);
			M.AddBox(Eye + FVector(S * 0.06, Side * S * 0.012, 0.0), FVector::ForwardVector, FVector(S * 0.022, S * 0.045, S * 0.05), L.Iris);
			M.AddBox(Eye + FVector(S * 0.08, Side * S * 0.012, 0.0), FVector::ForwardVector, FVector(S * 0.012, S * 0.036, S * 0.012), Rgb(0.04f, 0.03f, 0.04f));
			M.AddBeam(Eye + FVector(-S * 0.02, -Side * S * 0.06, S * 0.08), Eye + FVector(S * 0.06, Side * S * 0.05, S * 0.07), S * 0.02, L.SkinDark);
		}
		// Sifón a un costado.
		TNProcMesh::TNProcAddCylinder(M, FVector(-S * 0.02, S * 0.2, S * 0.12), FVector(S * 0.02, S * 0.29, S * 0.06), S * 0.045, S * 0.03, 6, L.SkinDark, true);
	}

	/**
	 * Tramo de brazo de radio 1 y largo 100 a lo largo de +X (se escala por instancia: X = largo / 100, Y y Z = radio en
	 * cm): tubo de seis caras que se estrecha, con la cara de abajo clara y dos ventosas.
	 */
	inline void BuildOctoArmSegment(FTNProcMeshBuffers& M, const FOctoLook& L)
	{
		constexpr int32 Seg = 6;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = UE_DOUBLE_TWO_PI * (k + 0.5) / Seg;
			const double A1 = UE_DOUBLE_TWO_PI * (k + 1.5) / Seg;
			const FVector D0(0.0, FMath::Cos(A0), FMath::Sin(A0));
			const FVector D1(0.0, FMath::Cos(A1), FMath::Sin(A1));
			const FVector Mid = (D0 + D1) * 0.5;
			const FLinearColor Col = Mid.Z < -0.4 ? L.Sucker : (Mid.Z > 0.6 ? L.SkinDark : L.Skin);
			M.AddQuad(D0, D1, FVector(100.0, 0.0, 0.0) + D1 * 0.82, FVector(100.0, 0.0, 0.0) + D0 * 0.82, Mid, Col);
		}
		for (const double X : { 28.0, 70.0 })
		{
			M.AddBox(FVector(X, 0.0, -0.92), FVector::ForwardVector, FVector(9.0, 0.34, 0.16), L.Belly);
		}
	}

	/** Silueta oscura del pulpo vista desde arriba (translúcida por el alfa del vértice; radio de los brazos ~260). */
	inline void BuildOctoSilhouette(FTNProcMeshBuffers& M)
	{
		const FLinearColor Dark(0.03f, 0.04f, 0.06f, 1.f);
		auto Alpha = [](const FVector& P)
		{
			const double Rn = P.Size2D();
			return Rn < 60.0 ? 0.34f : static_cast<float>(FMath::Lerp(0.3, 0.04, FMath::Clamp((Rn - 60.0) / 200.0, 0.0, 1.0)));
		};
		constexpr int32 DiscSeg = 12;
		for (int32 k = 0; k < DiscSeg; ++k)
		{
			const double A0 = UE_DOUBLE_TWO_PI * k / DiscSeg;
			const double A1 = UE_DOUBLE_TWO_PI * (k + 1) / DiscSeg;
			AddTriA(M, FVector::ZeroVector, FVector(FMath::Cos(A0) * 70.0, FMath::Sin(A0) * 60.0, 0.0),
				FVector(FMath::Cos(A1) * 70.0, FMath::Sin(A1) * 60.0, 0.0), FVector::UpVector, Dark, Alpha);
		}
		for (int32 Arm = 0; Arm < 8; ++Arm)
		{
			const double Base = UE_DOUBLE_TWO_PI * (Arm + 0.5) / 8.0;
			FVector Prev = FVector(FMath::Cos(Base), FMath::Sin(Base), 0.0) * 45.0;
			for (int32 k = 1; k <= 5; ++k)
			{
				const double T = k / 5.0;
				const double Ang = Base + 0.35 * FMath::Sin(T * 3.0 + Arm);
				const FVector Next = FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.0) * (45.0 + 215.0 * T);
				const FVector Dir = (Next - Prev).GetSafeNormal2D();
				const FVector Side(-Dir.Y, Dir.X, 0.0);
				const double W0 = FMath::Lerp(22.0, 4.0, (k - 1) / 5.0);
				const double W1 = FMath::Lerp(22.0, 4.0, T);
				AddTriA(M, Prev - Side * W0, Prev + Side * W0, Next + Side * W1, FVector::UpVector, Dark, Alpha);
				AddTriA(M, Prev - Side * W0, Next + Side * W1, Next - Side * W1, FVector::UpVector, Dark, Alpha);
				Prev = Next;
			}
		}
	}

	/** Charco (translúcido) de radio 100 para el pulpo fuera de una poza: agua clara y un borde de espuma. */
	inline void BuildPuddle(FTNProcMeshBuffers& M)
	{
		const FLinearColor Water(0.3f, 0.6f, 0.76f, 1.f);
		const FLinearColor Foam(0.92f, 0.96f, 0.98f, 1.f);
		constexpr int32 Seg = 24;
		auto WaterAlpha = [](const FVector&) { return 0.62f; };
		auto FoamAlpha = [](const FVector& P) { return P.Size2D() > 99.0 ? 0.f : 0.55f; };
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = UE_DOUBLE_TWO_PI * k / Seg;
			const double A1 = UE_DOUBLE_TWO_PI * (k + 1) / Seg;
			const double W0 = 1.0 + 0.06 * FMath::Sin(A0 * 3.0);
			const double W1 = 1.0 + 0.06 * FMath::Sin(A1 * 3.0);
			const FVector I0(FMath::Cos(A0) * 90.0 * W0, FMath::Sin(A0) * 90.0 * W0, 0.0);
			const FVector I1(FMath::Cos(A1) * 90.0 * W1, FMath::Sin(A1) * 90.0 * W1, 0.0);
			const FVector O0(FMath::Cos(A0) * 100.0 * W0, FMath::Sin(A0) * 100.0 * W0, 0.5);
			const FVector O1(FMath::Cos(A1) * 100.0 * W1, FMath::Sin(A1) * 100.0 * W1, 0.5);
			AddTriA(M, FVector::ZeroVector, I0, I1, FVector::UpVector, Water, WaterAlpha);
			AddTriA(M, I0, O0, O1, FVector::UpVector, Foam, FoamAlpha);
			AddTriA(M, I0, O1, I1, FVector::UpVector, Foam, FoamAlpha);
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Pulga de arena
	// ─────────────────────────────────────────────────────────────────────────

	/** Largo real de una pulga de arena (cm): a escala, un puntito de ~18 cm que salta. */
	constexpr double FleaReal = 0.65;

	/** Pulga (anfípodo) de lado: cuerpo curvado de dos bultos, antenas y las patas de saltar. Origen en el suelo. */
	inline void BuildFlea(FTNProcMeshBuffers& M, uint32 Variant)
	{
		const double S = FleaReal * TNBeach::Scale / 18.0;
		const FLinearColor Body = Variant % 2 ? Rgb(0.42f, 0.36f, 0.28f) : Rgb(0.34f, 0.3f, 0.25f);
		const FLinearColor Back = Body * 0.75f;
		TNFauna::TNFaunaBlob(M, FVector(3.0, 0.0, 5.0) * S, FVector(6.0, 2.8, 4.2) * S, Back, Body, 6, 3);
		TNFauna::TNFaunaBlob(M, FVector(-4.0, 0.0, 4.2) * S, FVector(5.5, 2.6, 3.8) * S, Back, Body, 6, 3);
		for (const double Side : { -1.0, 1.0 })
		{
			M.AddBeam(FVector(7.5, Side * 1.0, 6.0) * S, FVector(13.0, Side * 3.0, 10.0) * S, 0.6 * S, Body * 1.2f);
			M.AddBeam(FVector(-3.0, Side * 2.0, 3.0) * S, FVector(-8.5, Side * 4.0, 0.3) * S, 0.8 * S, Body * 1.1f);
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Tanque de juguete teledirigido (espacio del casco: X adelante, Y derecha, Z arriba; origen en el suelo)
	// ─────────────────────────────────────────────────────────────────────────

	/** Medidas del tanque ya a escala (cm de juego): un tanque de juguete de 16 cm de largo. */
	struct FTankDims
	{
		double HalfLength = 8.0 * TNBeach::Scale;
		double TrackY = 3.2 * TNBeach::Scale;
		double TrackHalfWidth = 1.0 * TNBeach::Scale;
		double TrackHeight = 3.2 * TNBeach::Scale;
		double HullTop = 4.6 * TNBeach::Scale;
		double WheelRadius = 1.1 * TNBeach::Scale;
		/** Pivote de la torreta sobre el casco. */
		FVector TurretPivot = FVector(-0.6 * TNBeach::Scale, 0.0, 4.6 * TNBeach::Scale);
		/** Pivote del cañón en la torreta (el mantelete) y largo del cañón. */
		FVector BarrelPivot = FVector(3.4 * TNBeach::Scale, 0.0, 1.2 * TNBeach::Scale);
		double BarrelLength = 9.6 * TNBeach::Scale;
		/** Pivote de la antena en la torreta y su largo. */
		FVector AntennaPivot = FVector(-2.4 * TNBeach::Scale, -1.9 * TNBeach::Scale, 2.4 * TNBeach::Scale);
		double AntennaLength = 11.5 * TNBeach::Scale;
		/** Radio de la bolita de espuma. */
		double FoamRadius = 0.95 * TNBeach::Scale;
	};

	inline FTankDims TankDims()
	{
		return FTankDims();
	}

	/** Posición de la rueda Index (0..4 a la izquierda de delante a atrás, 5..9 a la derecha), espacio del casco. */
	inline FVector TankWheelAt(int32 Index)
	{
		const FTankDims D = TankDims();
		const double X = (2 - (Index % 5)) * 2.6 * TNBeach::Scale;
		const double Y = (Index < 5 ? -1.0 : 1.0) * (D.TrackY + D.TrackHalfWidth + 6.0);
		return FVector(X, Y, D.WheelRadius + 2.0);
	}

	struct FTankLook
	{
		FLinearColor Body = Rgb(0.37f, 0.43f, 0.22f);
		FLinearColor BodyDark = Rgb(0.27f, 0.32f, 0.16f);
		FLinearColor BodyLight = Rgb(0.5f, 0.56f, 0.32f);
		FLinearColor Rubber = Rgb(0.14f, 0.14f, 0.15f);
		FLinearColor Steel = Rgb(0.5f, 0.53f, 0.55f);
		FLinearColor Light = Rgb(1.f, 0.93f, 0.6f);
		bool bCamo = false;
	};

	/** Verde oliva, arena del desierto, gris de Tortunavy y camuflaje. */
	inline FTankLook TankPalette(int32 Index)
	{
		FTankLook L;
		switch (((Index % 4) + 4) % 4)
		{
		case 1:
			L.Body = Rgb(0.8f, 0.7f, 0.46f); L.BodyDark = Rgb(0.62f, 0.52f, 0.32f); L.BodyLight = Rgb(0.9f, 0.82f, 0.6f);
			break;
		case 2:
			L.Body = Rgb(0.36f, 0.43f, 0.52f); L.BodyDark = Rgb(0.24f, 0.3f, 0.38f); L.BodyLight = Rgb(0.5f, 0.57f, 0.65f);
			break;
		case 3:
			L.bCamo = true;
			break;
		default:
			break;
		}
		return L;
	}

	/** Color de una cara con camuflaje (manchas oscuras y claras al azar por posición). */
	inline FLinearColor TankSkin(const FTankLook& L, const FVector& At, uint32 Seed)
	{
		if (!L.bCamo)
		{
			return L.Body;
		}
		const double N = TNProcMesh::TNProcHashNoise(FMath::FloorToInt32(At.X / 90.0), FMath::FloorToInt32((At.Y + At.Z) / 80.0), Seed);
		return N > 0.35 ? L.BodyDark : (N < -0.45 ? Rgb(0.55f, 0.48f, 0.3f) : L.Body);
	}

	/**
	 * Casco del tanque (sin las ruedas ni la torreta): orugas de goma con eslabones, guardabarros, casco con el glacis
	 * inclinado, rejilla del motor, faros, escarapelas de Tortunavy en los costados, tubos de escape y ganchos.
	 */
	inline void BuildTankHull(FTNProcMeshBuffers& M, const FTankLook& L, uint32 Seed)
	{
		const FTankDims D = TankDims();
		const double S = TNBeach::Scale;
		const double HL = D.HalfLength;
		// Orugas: caja con los extremos redondeados y eslabones claros arriba y abajo.
		for (const double Side : { -1.0, 1.0 })
		{
			const double Y = Side * D.TrackY;
			const double R = D.TrackHeight * 0.5;
			M.AddBox(FVector(0.0, Y, R), FVector::ForwardVector, FVector(HL - R, D.TrackHalfWidth, R), L.Rubber);
			for (const double End : { -1.0, 1.0 })
			{
				TNProcMesh::TNProcAddCylinder(M, FVector(End * (HL - R), Y - D.TrackHalfWidth, R), FVector(End * (HL - R), Y + D.TrackHalfWidth, R), R, R, 10, L.Rubber, true);
			}
			for (int32 k = 0; k < 14; ++k)
			{
				const double X = -HL + R + (k + 0.5) * (2.0 * (HL - R)) / 14.0;
				M.AddBox(FVector(X, Y, D.TrackHeight + 1.0), FVector::ForwardVector, FVector(9.0, D.TrackHalfWidth * 1.02, 3.0), L.Steel * 0.6f);
				M.AddBox(FVector(X, Y, -0.5), FVector::ForwardVector, FVector(9.0, D.TrackHalfWidth * 1.02, 2.5), L.Steel * 0.5f);
			}
			// Guardabarros sobre la oruga.
			M.AddBox(FVector(0.0, Y + Side * 4.0, D.TrackHeight + 7.0), FVector::ForwardVector, FVector(HL + 8.0, D.TrackHalfWidth + 10.0, 4.0), L.BodyDark);
		}
		// Casco bajo (entre las orugas) y superestructura con glacis delante y rampa atrás.
		M.AddBox(FVector(0.0, 0.0, 1.9 * S), FVector::ForwardVector, FVector(HL - 0.3 * S, D.TrackY - D.TrackHalfWidth, 0.9 * S), L.BodyDark);
		TArray<FVector2D> Upper;
		Upper.Add(FVector2D(-HL + 0.2 * S, D.TrackHeight + 0.1 * S));
		Upper.Add(FVector2D(HL - 0.1 * S, D.TrackHeight + 0.1 * S));
		Upper.Add(FVector2D(HL - 0.2 * S, D.TrackHeight + 0.6 * S));
		Upper.Add(FVector2D(HL - 2.4 * S, D.HullTop));
		Upper.Add(FVector2D(-HL + 1.0 * S, D.HullTop));
		Upper.Add(FVector2D(-HL + 0.2 * S, D.HullTop - 0.5 * S));
		{
			FTNProcMeshBuffers Tmp;
			AddExtrudeY(Tmp, Upper, -(D.TrackY + D.TrackHalfWidth + 6.0), D.TrackY + D.TrackHalfWidth + 6.0, L.Body, L.Body * 0.92f);
			// Camuflaje por cara.
			for (int32 t = 0; t + 2 < Tmp.Tris.Num(); t += 3)
			{
				const FVector Mid = (Tmp.Verts[Tmp.Tris[t]] + Tmp.Verts[Tmp.Tris[t + 1]] + Tmp.Verts[Tmp.Tris[t + 2]]) / 3.0;
				const FLinearColor Col = TankSkin(L, Mid, Seed) * (Tmp.Colors[Tmp.Tris[t]].R < L.Body.R * 0.95f ? 0.92f : 1.f);
				for (int32 j = 0; j < 3; ++j)
				{
					Tmp.Colors[Tmp.Tris[t + j]] = Col;
				}
			}
			AppendTransformed(M, Tmp, FTransform::Identity);
		}
		// Rejilla del motor (atrás, arriba).
		for (int32 k = 0; k < 5; ++k)
		{
			M.AddBox(FVector(-HL + 1.6 * S + k * 0.45 * S, 0.0, D.HullTop + 1.5), FVector::ForwardVector, FVector(0.12 * S, 1.6 * S, 2.0), L.Rubber);
		}
		// Faros, escapes, ganchos y escarapelas.
		for (const double Side : { -1.0, 1.0 })
		{
			TNFauna::TNFaunaBlob(M, FVector(HL - 1.0 * S, Side * 2.7 * S, D.HullTop - 0.35 * S), FVector(0.42 * S, 0.42 * S, 0.42 * S), L.Light, L.Light * 0.8f, 7, 3);
			TNProcMesh::TNProcAddCylinder(M, FVector(-HL + 0.1 * S, Side * 1.5 * S, D.HullTop - 0.9 * S), FVector(-HL - 0.6 * S, Side * 1.5 * S, D.HullTop - 0.8 * S),
				0.3 * S, 0.3 * S, 8, L.Steel * 0.7f, true);
			M.AddBox(FVector(HL + 0.2 * S, Side * 1.6 * S, D.TrackHeight + 0.2 * S), FVector::ForwardVector, FVector(0.3 * S, 0.25 * S, 0.25 * S), L.Steel * 0.6f);
			AddRoundel(M, FVector(-0.4 * S, Side * (D.TrackY + D.TrackHalfWidth + 6.5), D.TrackHeight + 0.85 * S), FVector(0.0, Side, 0.0), FVector::UpVector, 0.5 * S);
		}
		// Número de serie de juguete: una franja clara en el glacis.
		M.AddBox(FVector(HL - 1.2 * S, 0.0, D.TrackHeight + 1.0 * S), FVector::ForwardVector, FVector(0.1 * S, 1.6 * S, 0.12 * S), L.BodyLight);
	}

	/** Rueda de la oruga: disco a lo largo de Y con el cubo y un radio claro para que se vea girar (pivote en el eje). */
	inline void BuildTankWheel(FTNProcMeshBuffers& M, const FTankLook& L)
	{
		const FTankDims D = TankDims();
		const double R = D.WheelRadius;
		TNProcMesh::TNProcAddCylinder(M, FVector(0.0, -9.0, 0.0), FVector(0.0, 9.0, 0.0), R, R, 10, L.Rubber * 1.3f, true);
		TNProcMesh::TNProcAddCylinder(M, FVector(0.0, -11.0, 0.0), FVector(0.0, 11.0, 0.0), R * 0.55, R * 0.55, 8, L.BodyDark, true);
		for (const double Side : { -1.0, 1.0 })
		{
			M.AddBox(FVector(0.0, Side * 12.0, 0.0), FVector::ForwardVector, FVector(R * 0.5, 1.5, R * 0.1), L.BodyLight);
		}
	}

	/** Torreta (pivote en su aro): cuerpo de ocho lados que se estrecha, escotilla, periscopio, mantelete, cesta y estrellas. */
	inline void BuildTankTurret(FTNProcMeshBuffers& M, const FTankLook& L, uint32 Seed)
	{
		const double S = TNBeach::Scale;
		const FTankDims D = TankDims();
		static constexpr double Px[8] = { 3.3, 2.4, -1.2, -3.0, -3.3, -3.0, -1.2, 2.4 };
		static constexpr double Py[8] = { 0.0, 2.6, 2.9, 2.1, 0.0, -2.1, -2.9, -2.6 };
		const double H = 2.4 * S;
		auto Pt = [&](int32 K, double Z, double Shrink)
		{
			return FVector(Px[K % 8] * S * Shrink, Py[K % 8] * S * Shrink, Z);
		};
		const FVector TopC(0.0, 0.0, H);
		for (int32 K = 0; K < 8; ++K)
		{
			const FVector A0 = Pt(K, 0.0, 1.0);
			const FVector A1 = Pt(K + 1, 0.0, 1.0);
			const FVector B0 = Pt(K, H, 0.84);
			const FVector B1 = Pt(K + 1, H, 0.84);
			const FVector Mid = (A0 + A1 + B0 + B1) * 0.25;
			M.AddQuad(A0, A1, B1, B0, FVector(Mid.X, Mid.Y, 0.25 * Mid.Size2D()), TankSkin(L, Mid, Seed + 3u));
			M.AddTri(TopC, B0, B1, FVector::UpVector, TankSkin(L, (B0 + B1) * 0.5, Seed + 5u) * 1.04f);
			M.AddTri(FVector::ZeroVector, A0, A1, -FVector::UpVector, L.BodyDark);
		}
		// Escotilla y periscopio.
		TNProcMesh::TNProcAddCylinder(M, FVector(-0.8 * S, 0.7 * S, H), FVector(-0.8 * S, 0.7 * S, H + 0.4 * S), 0.9 * S, 0.85 * S, 10, L.BodyLight, true);
		M.AddBox(FVector(0.6 * S, -1.0 * S, H + 0.25 * S), FVector::ForwardVector, FVector(0.3 * S, 0.3 * S, 0.3 * S), L.BodyDark);
		M.AddBox(FVector(0.82 * S, -1.0 * S, H + 0.3 * S), FVector::ForwardVector, FVector(0.05 * S, 0.24 * S, 0.12 * S), Rgb(0.4f, 0.75f, 0.95f));
		// Mantelete delante (el cañón sale de aquí).
		M.AddBox(FVector(D.BarrelPivot.X - 0.25 * S, 0.0, D.BarrelPivot.Z), FVector::ForwardVector, FVector(0.5 * S, 1.2 * S, 0.9 * S), L.BodyDark);
		// Cesta de atrás con una lona.
		M.AddBox(FVector(-3.6 * S, 0.0, 1.2 * S), FVector::ForwardVector, FVector(0.5 * S, 2.0 * S, 0.7 * S), L.BodyDark * 0.9f);
		TNFauna::TNFaunaBlob(M, FVector(-3.6 * S, 0.3 * S, 2.0 * S), FVector(0.45 * S, 1.3 * S, 0.35 * S), Rgb(0.72f, 0.66f, 0.46f), Rgb(0.6f, 0.55f, 0.38f), 6, 3);
		// Estrellas doradas a los lados.
		for (const double Side : { -1.0, 1.0 })
		{
			AddStar(M, FVector(0.2 * S, Side * 2.62 * S, 1.2 * S), FVector(0.0, Side, 0.0), FVector::UpVector, 0.7 * S, Rgb(1.f, 0.8f, 0.24f));
		}
	}

	/** Cañón (pivote en el mantelete, a lo largo de +X): tubo, freno de boca y la punta naranja de los juguetes. */
	inline void BuildTankBarrel(FTNProcMeshBuffers& M, const FTankLook& L)
	{
		const FTankDims D = TankDims();
		const double S = TNBeach::Scale;
		const double Len = D.BarrelLength;
		TNProcMesh::TNProcAddCylinder(M, FVector(-0.2 * S, 0.0, 0.0), FVector(Len - 0.9 * S, 0.0, 0.0), 0.62 * S, 0.54 * S, 8, L.Body, true);
		TNProcMesh::TNProcAddCylinder(M, FVector(Len - 0.9 * S, 0.0, 0.0), FVector(Len - 0.1 * S, 0.0, 0.0), 0.8 * S, 0.8 * S, 8, L.BodyDark, true);
		TNProcMesh::TNProcAddCylinder(M, FVector(Len - 0.1 * S, 0.0, 0.0), FVector(Len + 0.4 * S, 0.0, 0.0), 0.82 * S, 0.82 * S, 8, Rgb(1.f, 0.45f, 0.08f), true);
		// Boca oscura.
		TNProcMesh::TNProcAddCylinder(M, FVector(Len + 0.35 * S, 0.0, 0.0), FVector(Len + 0.42 * S, 0.0, 0.0), 0.45 * S, 0.45 * S, 8, Rgb(0.08f, 0.08f, 0.08f), true);
	}

	/** Antena de látigo (pivote en su base, hacia +Z) con su soporte y una bolita en la punta. */
	inline void BuildTankAntenna(FTNProcMeshBuffers& M, const FTankLook& L)
	{
		const FTankDims D = TankDims();
		const double S = TNBeach::Scale;
		TNProcMesh::TNProcAddCylinder(M, FVector::ZeroVector, FVector(0.0, 0.0, 0.5 * S), 0.35 * S, 0.3 * S, 6, L.Steel * 0.6f, true);
		TNProcMesh::TNProcAddCylinder(M, FVector(0.0, 0.0, 0.5 * S), FVector(0.0, 0.0, D.AntennaLength), 0.11 * S, 0.06 * S, 4, L.Steel, false);
		TNFauna::TNFaunaBlob(M, FVector(0.0, 0.0, D.AntennaLength), FVector(0.18 * S, 0.18 * S, 0.18 * S), Rgb(0.1f, 0.1f, 0.1f), Rgb(0.1f, 0.1f, 0.1f), 5, 3);
	}

	/** Banderita de Tortunavy (azul marino con la estrella dorada por las dos caras), pivote en el mástil y hacia -X. */
	inline void BuildTankFlag(FTNProcMeshBuffers& M)
	{
		const double S = TNBeach::Scale;
		const double W = 2.6 * S;
		const double H = 1.6 * S;
		const FLinearColor Navy = Rgb(0.07f, 0.19f, 0.36f);
		const FLinearColor Gold = Rgb(1.f, 0.8f, 0.24f);
		for (const double Side : { -1.0, 1.0 })
		{
			const FVector N(0.0, Side, 0.0);
			const FVector Off = N * 0.6;
			M.AddQuad(Off, FVector(-W * 0.5, 0.0, 0.08 * H) + Off, FVector(-W * 0.5, 0.0, -H * 0.92) + Off, FVector(0.0, 0.0, -H) + Off, N, Navy);
			M.AddQuad(FVector(-W * 0.5, 0.0, 0.08 * H) + Off, FVector(-W, 0.0, 0.15 * H) + Off, FVector(-W, 0.0, -H * 0.85) + Off,
				FVector(-W * 0.5, 0.0, -H * 0.92) + Off, N, Navy * 0.92f);
			AddStar(M, FVector(-W * 0.42, Side * 1.6, -H * 0.42), N, FVector::UpVector, H * 0.3, Gold);
		}
	}

	/** Bolita de espuma naranja con una franja amarilla (centrada en su origen). */
	inline void BuildFoamBall(FTNProcMeshBuffers& M)
	{
		const FTankDims D = TankDims();
		const double R = D.FoamRadius;
		TNFauna::TNFaunaBlob(M, FVector::ZeroVector, FVector(R, R, R), Rgb(1.f, 0.5f, 0.1f), Rgb(1.f, 0.42f, 0.08f), 8, 5);
		TNProcMesh::TNProcAddCylinder(M, FVector(-R * 0.12, 0.0, 0.0), FVector(R * 0.12, 0.0, 0.0), R * 1.02, R * 1.02, 10, Rgb(1.f, 0.9f, 0.25f), false);
	}
}
