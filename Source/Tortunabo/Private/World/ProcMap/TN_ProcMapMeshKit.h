#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapMath.h"

/**
 * Malla procedural de caras planas (estilo low-poly) compartida por las estructuras del mapa
 * (TN_ProcMapGenerator_Build.cpp) y la vegetación (TN_ProcMapGenerator_Flora.cpp): buffers de
 * una sección, ruido estable por índices y cuerpos básicos (torno, peñasco, tronco).
 */
namespace TNProcMesh
{
	/** Buffers de una sección de malla procedural con caras planas orientadas. */
	struct FTNProcMeshBuffers
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;

		bool IsEmpty() const { return Tris.Num() == 0; }

		/**
		 * Añade un triángulo cuya cara visible mira hacia Hint. En UE la cara frontal
		 * de (A,B,C) es la de normal (C-A)x(B-A) (la que calculan MeshUtilities y
		 * CalculateTangentsForMesh): se orienta N = (B-A)x(C-A) hacia Hint y se emite
		 * A, C, B.
		 */
		void AddTri(const FVector& A, const FVector& B, const FVector& C, const FVector& Hint, const FLinearColor& Color)
		{
			FVector N = FVector::CrossProduct(B - A, C - A);
			if (N.SizeSquared() < 1e-4) { return; }
			N.Normalize();
			const bool bFlip = FVector::DotProduct(N, Hint) < 0.0;
			const FVector P1 = bFlip ? C : B;
			const FVector P2 = bFlip ? B : C;
			if (bFlip) { N = -N; }
			const int32 Base = Verts.Num();
			const FVector Pts[3] = { A, P1, P2 };
			for (const FVector& P : Pts)
			{
				Verts.Add(P);
				Normals.Add(N);
				// UV triplanar simple según la orientación de la cara.
				const double Ax = FMath::Abs(N.X), Ay = FMath::Abs(N.Y), Az = FMath::Abs(N.Z);
				FVector2D UV = Az >= Ax && Az >= Ay ? FVector2D(P.X, P.Y) : (Ax >= Ay ? FVector2D(P.Y, P.Z) : FVector2D(P.X, P.Z));
				UVs.Add(UV / 400.0);
				Colors.Add(Color);
			}
			Tris.Add(Base);
			Tris.Add(Base + 2);
			Tris.Add(Base + 1);
		}

		/** Quad A-B-C-D en orden de contorno. */
		void AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Hint, const FLinearColor& Color)
		{
			AddTri(A, B, C, Hint, Color);
			AddTri(A, C, D, Hint, Color);
		}

		/** Caja orientada: AxisX horizontal unitario, Z arriba. */
		void AddBox(const FVector& Center, const FVector& AxisX, const FVector& Half, const FLinearColor& Color)
		{
			const FVector X = AxisX.GetSafeNormal2D().IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : AxisX.GetSafeNormal2D();
			const FVector Y(-X.Y, X.X, 0.0);
			const FVector Z(0.0, 0.0, 1.0);
			auto P = [&](double Sx, double Sy, double Sz) { return Center + X * (Sx * Half.X) + Y * (Sy * Half.Y) + Z * (Sz * Half.Z); };
			AddQuad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Z, Color);
			AddQuad(P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), -Z, Color);
			AddQuad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), X, Color);
			AddQuad(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), -X, Color);
			AddQuad(P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), Y, Color);
			AddQuad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), -Y, Color);
		}

		/** Viga de sección cuadrada (semilado Half) entre dos puntos cualesquiera: cuerdas, cables, péndolas. */
		void AddBeam(const FVector& A, const FVector& B, double Half, const FLinearColor& Color)
		{
			const FVector D = B - A;
			const double Len = D.Size();
			if (Len < 1.0) { return; }
			const FVector X = D / Len;
			FVector Y = FVector::CrossProduct(FVector::UpVector, X);
			if (Y.SizeSquared() < 1e-6) { Y = FVector(0.0, 1.0, 0.0); }
			Y.Normalize();
			const FVector Z = FVector::CrossProduct(X, Y);
			const FVector C[4] = { (Y + Z) * Half, (Z - Y) * Half, (-Y - Z) * Half, (Y - Z) * Half };
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector& P0 = C[k];
				const FVector& P1 = C[(k + 1) % 4];
				AddQuad(A + P0, A + P1, B + P1, B + P0, P0 + P1, Color);
			}
		}

		/** Prisma vertical de un polígono (isletas, pozas). */
		void AddPrism(const TArray<FVector2D>& Poly, double ZTop, double ZBottom, const FLinearColor& Color, bool bSides = true)
		{
			if (Poly.Num() < 3) { return; }
			FVector2D C = FVector2D::ZeroVector;
			for (const FVector2D& V : Poly) { C += V; }
			C = C / static_cast<double>(Poly.Num());
			const FVector Top(C.X, C.Y, ZTop);
			for (int32 i = 0; i < Poly.Num(); ++i)
			{
				const FVector2D& A = Poly[i];
				const FVector2D& B = Poly[(i + 1) % Poly.Num()];
				AddTri(Top, FVector(A.X, A.Y, ZTop), FVector(B.X, B.Y, ZTop), FVector::UpVector, Color);
				if (bSides)
				{
					const FVector2D Mid = (A + B) * 0.5 - C;
					AddQuad(FVector(A.X, A.Y, ZBottom), FVector(B.X, B.Y, ZBottom), FVector(B.X, B.Y, ZTop), FVector(A.X, A.Y, ZTop),
						FVector(Mid.X, Mid.Y, 0.0), Color * 0.8f);
				}
			}
		}

		/**
		 * Barrido: Rings[i][k] son los puntos del perfil en la muestra i. Cada cara
		 * mira hacia fuera del centroide de su anillo.
		 */
		void AddSweep(const TArray<TArray<FVector>>& Rings, bool bClosedProfile, const FLinearColor& Color)
		{
			for (int32 i = 0; i + 1 < Rings.Num(); ++i)
			{
				const TArray<FVector>& R0 = Rings[i];
				const TArray<FVector>& R1 = Rings[i + 1];
				FVector C0 = FVector::ZeroVector;
				for (const FVector& V : R0) { C0 += V; }
				C0 /= static_cast<double>(FMath::Max(1, R0.Num()));
				const int32 Count = bClosedProfile ? R0.Num() : R0.Num() - 1;
				for (int32 k = 0; k < Count; ++k)
				{
					const int32 K1 = (k + 1) % R0.Num();
					const FVector Mid = (R0[k] + R0[K1]) * 0.5;
					AddQuad(R0[k], R0[K1], R1[K1], R1[k], Mid - C0, Color);
				}
			}
		}
	};

	inline FLinearColor TNProcLerpColor(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return A + (B - A) * FMath::Clamp(T, 0.f, 1.f);
	}

	/** Tono aleatorio estable por índice (vetas de los tablones). */
	inline float TNProcTone(int32 Index, uint32 Seed)
	{
		uint32 H = static_cast<uint32>(Index) * 2654435761u ^ Seed;
		H ^= H >> 15; H *= 2246822519u; H ^= H >> 13;
		return 0.82f + 0.36f * static_cast<float>(H & 0xFFFF) / 65535.f;
	}

	/** Ruido de valor estable en [-1, 1] por índices (forma de rocas y troncos). */
	inline double TNProcHashNoise(int32 A, int32 B, uint32 Seed)
	{
		uint32 H = static_cast<uint32>(A) * 73856093u ^ static_cast<uint32>(B) * 19349663u ^ Seed * 83492791u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return static_cast<double>(H & 0xFFFF) / 32767.5 - 1.0;
	}

	/**
	 * Cuerpo de revolución irregular: anillos de Seg vértices a las alturas Z (sobre Base) con
	 * radios R, cada vértice con ruido radial; se cierra por arriba. Para rocas, agujas y troncos
	 * de árbol (eje vertical).
	 */
	inline void TNProcAddLathe(FTNProcMeshBuffers& Mesh, const FVector& Base, const TArray<double>& Z, const TArray<double>& R, double Jitter,
		uint32 Seed, const FLinearColor& Color, int32 Seg = 10, double CapRise = 0.3)
	{
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r < Z.Num(); ++r)
		{
			TArray<FVector> Ring;
			for (int32 k = 0; k < Seg; ++k)
			{
				const double A = TNProcMap::TwoPi * (k + 0.37 * TNProcHashNoise(r, k, Seed + 5u)) / Seg;
				const double Rad = FMath::Max(1.0, R[r] * (1.0 + Jitter * TNProcHashNoise(r, k, Seed)));
				Ring.Add(Base + FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, Z[r] + Jitter * 0.3 * R[r] * TNProcHashNoise(k, r, Seed + 9u)));
			}
			Rings.Add(Ring);
		}
		Mesh.AddSweep(Rings, true, Color);
		const TArray<FVector>& Top = Rings.Last();
		FVector C = FVector::ZeroVector;
		for (const FVector& V : Top) { C += V; }
		C /= static_cast<double>(Top.Num());
		// La tapa sube CapRise veces el último radio (0 = techo plano: terrazas, mesas, pozas).
		C.Z += R.Last() * CapRise;
		for (int32 k = 0; k < Top.Num(); ++k) { Mesh.AddTri(C, Top[k], Top[(k + 1) % Top.Num()], FVector::UpVector, Color); }
	}

	/** Peñasco: elipsoide irregular hundido 25 cm en el suelo. */
	inline void TNProcAddBoulder(FTNProcMeshBuffers& Mesh, const FVector& Base, double Radius, double Height, uint32 Seed, const FLinearColor& Color)
	{
		TArray<double> Z, R;
		for (int32 r = 0; r <= 5; ++r)
		{
			const double T = r / 6.0;
			Z.Add(-25.0 + Height * (0.5 - 0.5 * FMath::Cos(T * PI)) * 1.1);
			R.Add(Radius * FMath::Sin(FMath::Lerp(0.35, 0.92, T) * PI));
		}
		TNProcAddLathe(Mesh, Base, Z, R, 0.22, Seed, Color * (0.85f + 0.3f * static_cast<float>(0.5 + 0.5 * TNProcHashNoise(1, 2, Seed))), 9);
	}

	/** Tronco caído (cilindro irregular) entre A y B, con tapas y muñones de ramas. */
	inline void TNProcAddLog(FTNProcMeshBuffers& Mesh, const FVector& A, const FVector& B, double Radius, uint32 Seed, const FLinearColor& Bark, const FLinearColor& Cut)
	{
		const FVector Axis = (B - A).GetSafeNormal();
		if (Axis.IsNearlyZero()) { return; }
		FVector U = FVector::CrossProduct(Axis, FVector::UpVector);
		if (U.SizeSquared() < 1e-6) { U = FVector(1.0, 0.0, 0.0); }
		U.Normalize();
		const FVector V = FVector::CrossProduct(Axis, U);
		constexpr int32 Seg = 9;
		constexpr int32 Stations = 6;
		TArray<TArray<FVector>> Rings;
		for (int32 St = 0; St <= Stations; ++St)
		{
			const FVector C = FMath::Lerp(A, B, static_cast<double>(St) / Stations);
			TArray<FVector> Ring;
			for (int32 k = 0; k < Seg; ++k)
			{
				const double Ang = TNProcMap::TwoPi * k / Seg;
				const double Rad = Radius * (1.0 + 0.1 * TNProcHashNoise(St, k, Seed));
				Ring.Add(C + (U * FMath::Cos(Ang) + V * FMath::Sin(Ang)) * Rad);
			}
			Rings.Add(Ring);
		}
		Mesh.AddSweep(Rings, true, Bark);
		for (int32 End = 0; End < 2; ++End)
		{
			const TArray<FVector>& Ring = End == 0 ? Rings[0] : Rings.Last();
			const FVector C = End == 0 ? A : B;
			const FVector Out = End == 0 ? -Axis : Axis;
			for (int32 k = 0; k < Seg; ++k) { Mesh.AddTri(C, Ring[k], Ring[(k + 1) % Seg], Out, Cut); }
		}
		for (int32 b = 0; b < 2; ++b)
		{
			const double T = 0.3 + 0.4 * (0.5 + 0.5 * TNProcHashNoise(b, 7, Seed));
			const double Ang = PI * TNProcHashNoise(b, 3, Seed);
			const FVector Dir = (U * FMath::Cos(Ang) + V * FMath::Sin(Ang) + Axis * 0.3).GetSafeNormal();
			const FVector P0 = FMath::Lerp(A, B, T);
			Mesh.AddBeam(P0, P0 + Dir * (Radius * 2.2), Radius * 0.28, Bark * 0.9f);
		}
	}

	/** Cilindro (o tronco de cono) de Seg lados entre A y B, con tapas opcionales. */
	inline void TNProcAddCylinder(FTNProcMeshBuffers& M, const FVector& A, const FVector& B, double RA, double RB, int32 Seg, const FLinearColor& Color, bool bCaps = true)
	{
		const FVector Ax = (B - A).GetSafeNormal();
		if (Ax.IsNearlyZero()) { return; }
		const FVector U = FVector::CrossProduct(Ax, FMath::Abs(Ax.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Ax, U);
		TArray<FVector> RingA, RingB;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double Ang = TNProcMap::TwoPi * k / Seg;
			const FVector Off = U * FMath::Cos(Ang) + V * FMath::Sin(Ang);
			RingA.Add(A + Off * RA);
			RingB.Add(B + Off * RB);
		}
		for (int32 k = 0; k < Seg; ++k)
		{
			const int32 K1 = (k + 1) % Seg;
			const FVector Mid = (RingA[k] + RingA[K1]) * 0.5 - A;
			M.AddQuad(RingA[k], RingA[K1], RingB[K1], RingB[k], Mid, Color);
		}
		if (bCaps)
		{
			for (int32 k = 0; k < Seg; ++k)
			{
				const int32 K1 = (k + 1) % Seg;
				M.AddTri(A, RingA[k], RingA[K1], -Ax, Color * 0.9f);
				M.AddTri(B, RingB[k], RingB[K1], Ax, Color * 1.05f);
			}
		}
	}
}
