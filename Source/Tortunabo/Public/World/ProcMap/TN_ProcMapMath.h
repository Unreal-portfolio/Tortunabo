#pragma once

#include "CoreMinimal.h"

/**
 * Utilidades matemáticas PURAS del mapa procedural: RNG determinista, ruido
 * coherente y geometría de polilíneas. Sin UWorld ni UObject — mismo contrato
 * que TN_GridPathDecisions.h, para que los tests de Automation cubran el código
 * real.
 *
 * Determinismo: servidor y clientes generan el mismo mapa a partir de la semilla
 * replicada, así que aquí no se usa FMath::Rand ni nada con estado global. El RNG
 * es SplitMix64 y el ruido es de gradiente con hash propio.
 */

namespace TNProcMap
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double TwoPi = 6.28318530717958647692;

	// ─────────────────────────────────────────────────────────────────────────
	// Escalares
	// ─────────────────────────────────────────────────────────────────────────

	inline double Saturate(double X) { return FMath::Clamp(X, 0.0, 1.0); }

	/** Hermite 0..1 entre Edge0 y Edge1 (sin depender de la firma de FMath::SmoothStep). */
	inline double SmoothStep(double Edge0, double Edge1, double X)
	{
		if (Edge1 == Edge0) { return X < Edge0 ? 0.0 : 1.0; }
		const double T = Saturate((X - Edge0) / (Edge1 - Edge0));
		return T * T * (3.0 - 2.0 * T);
	}

	inline double LerpD(double A, double B, double T) { return A + (B - A) * T; }

	/** Ángulo envuelto a (-Pi, Pi]. */
	inline double WrapAngle(double A)
	{
		while (A > Pi) { A -= TwoPi; }
		while (A <= -Pi) { A += TwoPi; }
		return A;
	}

	inline FVector2D DirFromAngle(double A) { return FVector2D(FMath::Cos(A), FMath::Sin(A)); }
	inline double AngleOf(const FVector2D& V) { return FMath::Atan2(V.Y, V.X); }

	/** Normal izquierda (rotación +90°) de una dirección. */
	inline FVector2D LeftNormal(const FVector2D& D) { return FVector2D(-D.Y, D.X); }

	// ─────────────────────────────────────────────────────────────────────────
	// RNG determinista
	// ─────────────────────────────────────────────────────────────────────────

	/** SplitMix64: rápido, sin estado global, idéntico en todas las plataformas. */
	struct FRng
	{
		uint64 State = 0;

		explicit FRng(uint64 Seed) : State(Seed ^ 0x9E3779B97F4A7C15ull) { Next(); }

		uint64 Next()
		{
			uint64 Z = (State += 0x9E3779B97F4A7C15ull);
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}

		/** Uniforme en [0, 1). */
		double Unit() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }

		double Range(double Min, double Max) { return Min + (Max - Min) * Unit(); }

		/** Entero uniforme en [Min, Max], ambos inclusive. */
		int32 RangeInt(int32 Min, int32 Max)
		{
			if (Max <= Min) { return Min; }
			const uint64 Span = static_cast<uint64>(static_cast<int64>(Max) - static_cast<int64>(Min) + 1);
			return Min + static_cast<int32>(Next() % Span);
		}

		bool Chance(double P) { return Unit() < P; }

		/** Sub-generador independiente: cada fase de la generación usa el suyo, así
		 *  cambiar cuántos números consume una fase no altera las demás. */
		FRng Fork(uint64 Salt) const { return FRng(State ^ (Salt * 0xD6E8FEB86659FD93ull + 0x632BE59BD9B4E019ull)); }

		template<typename T>
		void Shuffle(TArray<T>& Items)
		{
			for (int32 i = Items.Num() - 1; i > 0; --i)
			{
				const int32 j = RangeInt(0, i);
				if (i != j) { Items.Swap(i, j); }
			}
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Ruido de gradiente
	// ─────────────────────────────────────────────────────────────────────────

	inline uint32 Hash32(uint32 X)
	{
		X ^= X >> 16; X *= 0x7FEB352Du;
		X ^= X >> 15; X *= 0x846CA68Bu;
		X ^= X >> 16;
		return X;
	}

	inline uint32 HashCell(uint32 Seed, int32 X, int32 Y)
	{
		return Hash32(Seed ^ Hash32(static_cast<uint32>(X) * 0x8DA6B343u ^ Hash32(static_cast<uint32>(Y) * 0xD8163841u)));
	}

	inline double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }

	/** Ruido de gradiente 2D en ~[-1, 1]. Frecuencia 1 = una celda por unidad. */
	inline double Noise2(uint32 Seed, double X, double Y)
	{
		static const double GX[8] = { 1.0, -1.0, 0.0, 0.0, 0.70710678, -0.70710678, 0.70710678, -0.70710678 };
		static const double GY[8] = { 0.0, 0.0, 1.0, -1.0, 0.70710678, 0.70710678, -0.70710678, -0.70710678 };

		const double Fx = FMath::FloorToDouble(X);
		const double Fy = FMath::FloorToDouble(Y);
		const int32 Ix = static_cast<int32>(Fx);
		const int32 Iy = static_cast<int32>(Fy);
		const double Dx = X - Fx;
		const double Dy = Y - Fy;

		auto Dot = [&](int32 Cx, int32 Cy, double Ox, double Oy)
		{
			const uint32 H = HashCell(Seed, Cx, Cy) & 7u;
			return GX[H] * Ox + GY[H] * Oy;
		};

		const double N00 = Dot(Ix, Iy, Dx, Dy);
		const double N10 = Dot(Ix + 1, Iy, Dx - 1.0, Dy);
		const double N01 = Dot(Ix, Iy + 1, Dx, Dy - 1.0);
		const double N11 = Dot(Ix + 1, Iy + 1, Dx - 1.0, Dy - 1.0);
		const double U = Fade(Dx);
		const double V = Fade(Dy);
		const double R = LerpD(LerpD(N00, N10, U), LerpD(N01, N11, U), V);
		return FMath::Clamp(R * 1.41421356, -1.0, 1.0);
	}

	/** Ruido de gradiente 1D en ~[-1, 1]. */
	inline double Noise1(uint32 Seed, double X)
	{
		const double Fx = FMath::FloorToDouble(X);
		const int32 Ix = static_cast<int32>(Fx);
		const double Dx = X - Fx;
		auto Grad = [&](int32 C, double O)
		{
			const uint32 H = HashCell(Seed, C, 0x5BD1E995);
			return ((static_cast<double>(H & 0xFFFFu) / 65535.0) * 2.0 - 1.0) * O;
		};
		const double R = LerpD(Grad(Ix, Dx), Grad(Ix + 1, Dx - 1.0), Fade(Dx));
		return FMath::Clamp(R * 2.0, -1.0, 1.0);
	}

	/** Suma fractal normalizada a ~[-1, 1]. */
	inline double Fbm2(uint32 Seed, double X, double Y, int32 Octaves = 4, double Lacunarity = 2.0, double Gain = 0.5)
	{
		double Sum = 0.0, Amp = 1.0, Norm = 0.0, Freq = 1.0;
		for (int32 o = 0; o < Octaves; ++o)
		{
			Sum += Amp * Noise2(Seed + static_cast<uint32>(o) * 1013u, X * Freq, Y * Freq);
			Norm += Amp;
			Amp *= Gain;
			Freq *= Lacunarity;
		}
		return Norm > 0.0 ? Sum / Norm : 0.0;
	}

	inline double Fbm1(uint32 Seed, double X, int32 Octaves = 3)
	{
		double Sum = 0.0, Amp = 1.0, Norm = 0.0, Freq = 1.0;
		for (int32 o = 0; o < Octaves; ++o)
		{
			Sum += Amp * Noise1(Seed + static_cast<uint32>(o) * 7919u, X * Freq);
			Norm += Amp;
			Amp *= 0.5;
			Freq *= 2.0;
		}
		return Norm > 0.0 ? Sum / Norm : 0.0;
	}

	/** Ruido "crestado" en [0, 1]: crestas afiladas para roca y dunas. */
	inline double Ridged2(uint32 Seed, double X, double Y, int32 Octaves = 3)
	{
		double Sum = 0.0, Amp = 1.0, Norm = 0.0, Freq = 1.0;
		for (int32 o = 0; o < Octaves; ++o)
		{
			const double N = 1.0 - FMath::Abs(Noise2(Seed + static_cast<uint32>(o) * 3571u, X * Freq, Y * Freq));
			Sum += Amp * N * N;
			Norm += Amp;
			Amp *= 0.5;
			Freq *= 2.0;
		}
		return Norm > 0.0 ? Sum / Norm : 0.0;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Geometría de polilíneas
	// ─────────────────────────────────────────────────────────────────────────

	/** Distancia de P al segmento AB; OutT = parámetro [0,1] del punto más cercano. */
	inline double DistPointSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B, double& OutT)
	{
		const FVector2D AB = B - A;
		const double Len2 = AB.SizeSquared();
		OutT = Len2 > 1e-9 ? Saturate(FVector2D::DotProduct(P - A, AB) / Len2) : 0.0;
		return FVector2D::Distance(P, A + AB * OutT);
	}

	inline double PolylineLength(const TArray<FVector2D>& Pts)
	{
		double L = 0.0;
		for (int32 i = 1; i < Pts.Num(); ++i) { L += FVector2D::Distance(Pts[i - 1], Pts[i]); }
		return L;
	}

	/** Remuestrea a espaciado uniforme conservando los extremos. */
	inline TArray<FVector2D> ResamplePolyline(const TArray<FVector2D>& Pts, double Spacing)
	{
		TArray<FVector2D> Out;
		if (Pts.Num() < 2 || Spacing <= 0.0) { Out = Pts; return Out; }

		const double Total = PolylineLength(Pts);
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Total / Spacing));
		const double Step = Total / static_cast<double>(Count);
		Out.Reserve(Count + 1);
		Out.Add(Pts[0]);

		int32 Seg = 1;
		double SegStart = 0.0;
		double SegLen = FVector2D::Distance(Pts[0], Pts[1]);
		for (int32 k = 1; k < Count; ++k)
		{
			const double Target = Step * static_cast<double>(k);
			while (Seg < Pts.Num() - 1 && SegStart + SegLen < Target)
			{
				SegStart += SegLen;
				++Seg;
				SegLen = FVector2D::Distance(Pts[Seg - 1], Pts[Seg]);
			}
			const double T = SegLen > 1e-9 ? (Target - SegStart) / SegLen : 0.0;
			Out.Add(Pts[Seg - 1] + (Pts[Seg] - Pts[Seg - 1]) * Saturate(T));
		}
		Out.Add(Pts.Last());
		return Out;
	}

	/** Suavizado de Chaikin que conserva los extremos. */
	inline TArray<FVector2D> ChaikinSmooth(const TArray<FVector2D>& Pts, int32 Iterations)
	{
		TArray<FVector2D> Cur = Pts;
		for (int32 It = 0; It < Iterations && Cur.Num() >= 3; ++It)
		{
			TArray<FVector2D> Next;
			Next.Reserve(Cur.Num() * 2);
			Next.Add(Cur[0]);
			for (int32 i = 0; i < Cur.Num() - 1; ++i)
			{
				const FVector2D& A = Cur[i];
				const FVector2D& B = Cur[i + 1];
				Next.Add(A * 0.75 + B * 0.25);
				Next.Add(A * 0.25 + B * 0.75);
			}
			Next.Add(Cur.Last());
			Cur = Next;
		}
		return Cur;
	}

	/** Media móvil de un array escalar, ventana [-Radius, Radius], extremos recortados. */
	inline TArray<double> SmoothScalars(const TArray<double>& In, int32 Radius)
	{
		TArray<double> Out;
		Out.SetNum(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			double Sum = 0.0;
			int32 N = 0;
			for (int32 k = FMath::Max(0, i - Radius); k <= FMath::Min(In.Num() - 1, i + Radius); ++k)
			{
				Sum += In[k];
				++N;
			}
			Out[i] = N > 0 ? Sum / static_cast<double>(N) : In[i];
		}
		return Out;
	}

	/** Intersección de segmentos AB y CD (propia, sin contar extremos compartidos). */
	inline bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, FVector2D* OutPoint = nullptr)
	{
		const FVector2D R = B - A;
		const FVector2D S = D - C;
		const double Den = FVector2D::CrossProduct(R, S);
		if (FMath::Abs(Den) < 1e-9) { return false; }
		const double T = FVector2D::CrossProduct(C - A, S) / Den;
		const double U = FVector2D::CrossProduct(C - A, R) / Den;
		if (T <= 1e-9 || T >= 1.0 - 1e-9 || U <= 1e-9 || U >= 1.0 - 1e-9) { return false; }
		if (OutPoint) { *OutPoint = A + R * T; }
		return true;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Montículo binario mínimo (Dijkstra) sin depender de la API de heap de TArray
	// ─────────────────────────────────────────────────────────────────────────

	struct FHeapItem
	{
		double Key = 0.0;
		int32 Value = 0;
	};

	struct FMinHeap
	{
		TArray<FHeapItem> Items;

		bool IsEmpty() const { return Items.Num() == 0; }

		void Push(double Key, int32 Value)
		{
			FHeapItem It;
			It.Key = Key;
			It.Value = Value;
			Items.Add(It);
			int32 i = Items.Num() - 1;
			while (i > 0)
			{
				const int32 P = (i - 1) / 2;
				if (Items[P].Key <= Items[i].Key) { break; }
				Items.Swap(P, i);
				i = P;
			}
		}

		FHeapItem Pop()
		{
			const FHeapItem Top = Items[0];
			const FHeapItem LastItem = Items.Last();
			Items.Pop();
			if (Items.Num() > 0)
			{
				Items[0] = LastItem;
				int32 i = 0;
				const int32 N = Items.Num();
				for (;;)
				{
					const int32 L = i * 2 + 1;
					const int32 R = L + 1;
					int32 Best = i;
					if (L < N && Items[L].Key < Items[Best].Key) { Best = L; }
					if (R < N && Items[R].Key < Items[Best].Key) { Best = R; }
					if (Best == i) { break; }
					Items.Swap(i, Best);
					i = Best;
				}
			}
			return Top;
		}
	};
}
