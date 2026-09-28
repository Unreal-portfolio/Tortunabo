#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapCaves.h"
#include "TN_ProcMapMeshKit.h"
#include "TN_ProcMapFormationMeshes.h"

/**
 * Techo de roca de las cuevas (low-poly de caras planas con color de vértice): bóveda irregular de
 * pie a pie sobre el suelo del camino, con la cara de dentro mirando al túnel, un grueso de roca
 * por encima que se funde con la loma del terreno, bocas en los extremos, estalactitas colgando
 * y, fuera del volcán, grupos de cristales en las paredes.
 */
namespace TNCaveMesh
{
	using namespace TNProcMesh;
	using namespace TNFormMesh;

	/** Una sección del túnel: centro del suelo, dirección del camino y medio ancho. */
	struct FTNCaveStation
	{
		FVector Floor = FVector::ZeroVector;
		FVector2D Dir = FVector2D(1.0, 0.0);
		double HalfWidth = 300.0;
	};

	struct FTNCaveLook
	{
		FLinearColor Inner;     ///< Roca de dentro (oscura).
		FLinearColor Outer;     ///< Roca de fuera.
		FLinearColor Moss;      ///< Lo que mira arriba por fuera.
		FLinearColor Crystal;   ///< Cristales (fuera del volcán) o brasas.
		bool bCrystals = true;
	};

	/**
	 * Techo de la cueva sobre las estaciones St. ClearFactor escala la altura libre
	 * (TNProcMap::CaveDetail::Clearance), Roof es el grueso de la roca por encima. OutInner, si se
	 * pide, recibe por estación los puntos de la cara de dentro (pie, pared, bóveda de izquierda a
	 * derecha, pared y pie), con su ruido: la decoración se apoya en ellos.
	 */
	inline void TNCaveBuildRoof(FTNProcMeshBuffers& M, const TArray<FTNCaveStation>& St, double ClearFactor, double Roof, uint32 Seed, const FTNCaveLook& Look,
		TArray<TArray<FVector>>* OutInner = nullptr)
	{
		if (St.Num() < 2) { return; }
		constexpr int32 ArchPts = 9;
		constexpr int32 Side = ArchPts + 4;          // puntos de la cara de dentro (y de la de fuera)
		constexpr int32 Ring = Side * 2;
		TArray<TArray<FVector>> Rings;
		TArray<FVector> Axis;
		for (int32 s = 0; s < St.Num(); ++s)
		{
			const FTNCaveStation& S = St[s];
			const double Hw = S.HalfWidth;
			const double Cl = TNProcMap::CaveDetail::Clearance(Hw * 2.0, ClearFactor);
			const double Spring = Cl * 0.5;
			const FVector N(-S.Dir.Y, S.Dir.X, 0.0);
			auto At = [&](double Y, double Z) { return S.Floor + N * Y + FVector(0.0, 0.0, Z); };
			TArray<FVector2D> Prof;
			// Dentro, de izquierda a derecha: pie, pared, arranque y bóveda.
			Prof.Add(FVector2D(-(Hw + 70.0), -60.0));
			Prof.Add(FVector2D(-(Hw + 60.0), Spring * 0.5));
			for (int32 k = 0; k < ArchPts; ++k)
			{
				const double A = PI * (1.0 - static_cast<double>(k) / (ArchPts - 1));
				Prof.Add(FVector2D(FMath::Cos(A) * (Hw + 45.0), Spring + FMath::Sin(A) * (Cl - Spring)));
			}
			Prof.Add(FVector2D(Hw + 60.0, Spring * 0.5));
			Prof.Add(FVector2D(Hw + 70.0, -60.0));
			// Fuera, de derecha a izquierda, con el grueso de la roca.
			Prof.Add(FVector2D(Hw + 70.0 + Roof, -150.0));
			Prof.Add(FVector2D(Hw + 70.0 + Roof, Spring + Roof * 0.3));
			for (int32 k = 0; k < ArchPts; ++k)
			{
				const double A = PI * static_cast<double>(k) / (ArchPts - 1);
				Prof.Add(FVector2D(FMath::Cos(A) * (Hw + 45.0 + Roof), Spring + FMath::Sin(A) * (Cl - Spring + Roof)));
			}
			Prof.Add(FVector2D(-(Hw + 70.0 + Roof), Spring + Roof * 0.3));
			Prof.Add(FVector2D(-(Hw + 70.0 + Roof), -150.0));
			// Roca irregular: cada punto se desplaza hacia fuera o hacia dentro de la sección.
			const FVector2D Ctr(0.0, Spring);
			TArray<FVector> R;
			for (int32 k = 0; k < Ring; ++k)
			{
				const bool bInner = k < Side;
				const FVector2D Out = (Prof[k] - Ctr).GetSafeNormal();
				const double Amp = bInner ? FMath::Min(55.0, 0.08 * (Hw + Cl)) : 90.0;
				const double J = TNProcHashNoise(s, k, Seed) * Amp;
				const FVector2D Q = Prof[k] + Out * (bInner ? -FMath::Abs(J) * 0.5 + J * 0.5 : J);
				R.Add(At(Q.X, Q.Y));
			}
			Rings.Add(R);
			Axis.Add(At(0.0, Spring));
			if (OutInner)
			{
				TArray<FVector>& In = OutInner->AddDefaulted_GetRef();
				for (int32 k = 0; k < Side; ++k) { In.Add(R[k]); }
			}
		}

		for (int32 s = 0; s + 1 < Rings.Num(); ++s)
		{
			const FVector Mid = (Axis[s] + Axis[s + 1]) * 0.5;
			for (int32 k = 0; k < Ring; ++k)
			{
				const int32 K1 = (k + 1) % Ring;
				const FVector& A = Rings[s][k];
				const FVector& B = Rings[s][K1];
				const FVector& C = Rings[s + 1][K1];
				const FVector& D = Rings[s + 1][k];
				const FVector Q = (A + B + C + D) * 0.25;
				const bool bInner = k < Side - 1;
				const bool bBottom = k == Side - 1 || k == Ring - 1;
				FVector Hint = bInner ? Mid - Q : Q - Mid;
				if (bBottom) { Hint = FVector(0.0, 0.0, -1.0); }
				FLinearColor Col;
				if (bInner)
				{
					const float Tone = 0.8f + 0.35f * static_cast<float>(0.5 + 0.5 * TNProcHashNoise(s / 2, k, Seed + 3u));
					Col = Look.Inner * Tone;
				}
				else
				{
					const FVector Nrm = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
					Col = FMath::Abs(Nrm.Z) > 0.6 ? Look.Moss : Look.Outer * (0.9f + 0.2f * static_cast<float>(0.5 + 0.5 * TNProcHashNoise(s, k, Seed + 5u)));
				}
				M.AddQuad(A, B, C, D, Hint, Col);
			}
		}

		// Bocas: el anillo de roca entre la bóveda y la cara de fuera, mirando fuera del túnel.
		for (int32 End = 0; End <= 1; ++End)
		{
			const TArray<FVector>& R = End ? Rings.Last() : Rings[0];
			const FVector2D D = End ? St.Last().Dir : St[0].Dir;
			const FVector Face = FVector(D.X, D.Y, 0.0) * (End ? 1.0 : -1.0);
			for (int32 k = 0; k + 1 < Side; ++k)
			{
				M.AddQuad(R[k], R[k + 1], R[Ring - 2 - k], R[Ring - 1 - k], Face, Look.Outer * 0.85f);
			}
		}

		// Estalactitas en la clave y, fuera del volcán, cristales en las paredes.
		for (int32 s = 1; s + 1 < St.Num(); ++s)
		{
			const FVector Crown = Rings[s][2 + ArchPts / 2];
			const FVector N(-St[s].Dir.Y, St[s].Dir.X, 0.0);
			const int32 Hang = static_cast<int32>(3.0 * (0.5 + 0.5 * TNProcHashNoise(s, 70, Seed))) - 1;
			for (int32 h = 0; h < Hang; ++h)
			{
				const FVector Top = Crown + N * (TNProcHashNoise(s, 80 + h, Seed) * St[s].HalfWidth * 0.5) + FVector(0.0, 0.0, 30.0);
				const double Len = 60.0 + 110.0 * (0.5 + 0.5 * TNProcHashNoise(s, 90 + h, Seed));
				TNFormCylinder(M, Top, Top - FVector(0.0, 0.0, Len), 14.0 + Len * 0.12, 2.0, 6, Look.Inner * 1.2f);
			}
			if (Look.bCrystals && TNProcHashNoise(s, 60, Seed) > 0.35)
			{
				const double SideSign = TNProcHashNoise(s, 61, Seed) > 0.0 ? 1.0 : -1.0;
				const FVector Base = St[s].Floor + N * (SideSign * (St[s].HalfWidth + 10.0)) + FVector(0.0, 0.0, 20.0);
				for (int32 c = 0; c < 4; ++c)
				{
					const FVector Tilt = FVector(TNProcHashNoise(c, s, Seed + 7u) * 0.4, TNProcHashNoise(s, c, Seed + 8u) * 0.4, 1.0).GetSafeNormal() - N * (SideSign * 0.35);
					const double L = 50.0 + 60.0 * (0.5 + 0.5 * TNProcHashNoise(c, s, Seed + 9u));
					const FVector P0 = Base + FVector(TNProcHashNoise(c, 1, Seed) * 40.0, TNProcHashNoise(c, 2, Seed) * 40.0, 0.0);
					TNFormCylinder(M, P0, P0 + Tilt.GetSafeNormal() * L, 10.0, 1.5, 5, Look.Crystal);
				}
			}
		}
	}
}
