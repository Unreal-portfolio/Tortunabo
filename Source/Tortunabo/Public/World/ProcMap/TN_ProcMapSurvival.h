#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapGenerate.h"

/**
 * Mapa de Supervivencia (#273, Docs/Mapa_Supervivencia.md): el generador del Coop con un perfil propio. Un mapa
 * alargado de unos 160 × 400 m (2 × 5 módulos de 80 m), la salida al sur y la meta al norte, sin pasadas de cruce,
 * poco sinuoso y con la dificultad de entrada 1–5 (el nivel N de la partida pide min(N, 5)). El Coop no cambia.
 */

namespace TNProcMap
{
	constexpr int32 SurvivalMinDifficulty = 1;
	constexpr int32 SurvivalMaxDifficulty = 5;
	/** Cota del agua en el formato del banco (WATER_M de Scripts/terrain_vol): el mar del generador está a 0. */
	constexpr double SurvivalBenchWaterM = -4.0;

	/** Parámetros del mapa de Supervivencia para una semilla y una dificultad 1–5 (GenerateLayout los sanea). */
	inline FGenParams MakeSurvivalParams(uint32 Seed, int32 Difficulty)
	{
		const int32 D = FMath::Clamp(Difficulty, SurvivalMinDifficulty, SurvivalMaxDifficulty);
		const double T = static_cast<double>(D - SurvivalMinDifficulty) / (SurvivalMaxDifficulty - SurvivalMinDifficulty);

		FGenParams P;
		P.Seed = Seed;
		P.GridSize = 5;
		P.GridSizeX = 2;
		P.ModuleSize = 8000.0;
		P.CellSize = 200.0;
		P.SampleSpacing = 200.0;

		// Más o menos lineal: la ruta avanza hacia la meta, sin cruces ni lazos, y el camino serpentea poco.
		P.Coverage = 0.6;
		P.bMonotonicRoute = true;
		P.NumCrossings = 0;
		P.Sinuosity = LerpD(1.15, 1.35, T);
		P.NumLanes = 0;
		P.NumBranches = 1 + D;
		P.BranchMaxModules = 1;
		P.bRiver = false;
		P.NumBiomeRegions = 2;

		// El camino nunca baja de 3 m (la especificación); la dificultad lo estrecha y pone más huecos y más largos.
		P.PathWidthMin = LerpD(700.0, 400.0, T);
		P.PathWidthMax = LerpD(1600.0, 1000.0, T);
		P.PortalWidthMin = P.PathWidthMin;
		P.PortalWidthMax = P.PathWidthMax;
		P.NarrowChance = LerpD(0.1, 0.35, T);
		P.GapsPerKm = LerpD(8.0, 24.0, T);
		P.GapMax = LerpD(260.0, 390.0, T);
		P.Difficulty01 = T;
		P.EggNestEveryNPortals = 2;
		// La salida y la meta a unos 12 m de los extremos: claro de salida pequeño y la costa unos 12 m pasado el
		// borde norte, para que la orilla quede al final del mapa. SanitizeParams reduce a la mitad los dos en
		// módulos pequeños, y se aplica una sola vez, en GenerateLayout.
		P.StartClearingRadius = 1000.0;
		P.CoastInset = -2400.0;
		return P;
	}

	/**
	 * Alturas del mapa en el formato del banco (Scripts/terrain_survival/mapa.py): una muestra por metro, filas =
	 * ancho (X del mapa) y columnas = avance (Y), así que la salida queda al oeste y la meta al este. Una ventana de
	 * 150 × 400 m centrada en el ancho: lo que sobra a los lados es muro del borde. Los huecos de salto van aparte
	 * (Jumps): el banco los cruza saltando si el salto más largo cabe en el dive (decisión pendiente en #273).
	 */
	struct FSurvivalTop
	{
		static constexpr int32 Rows = 151;
		static constexpr int32 Cols = 401;
		/** Cota (m) de cada muestra, fila a fila: Top[Row * Cols + Col]. */
		TArray<float> Top;
		/** (fila, columna) de la salida y de la meta. */
		FIntPoint Start = FIntPoint::ZeroValue;
		FIntPoint Goal = FIntPoint::ZeroValue;
		/** Un hueco de salto: (fila, columna) de un borde y del otro, y el salto más largo para cruzarlo (m). */
		struct FJump { FIntPoint From; FIntPoint To; double LeapM = 0.0; };
		TArray<FJump> Jumps;
	};

	/** Salto más largo (cm) para cruzar un hueco: entero en los de borde y de panzazo, entre filas en los de postes. */
	inline double GapLongestLeap(const FLayout& L, const FFeature& F)
	{
		switch (GapStyleOf(F))
		{
			case EGapStyle::Beam: return 0.0;   // la viga se cruza andando
			case EGapStyle::Posts:
			{
				// Mismo reparto de filas que los postes del actor (TN_ProcMapGenerator_Build.cpp).
				const double MaxJump = LerpD(FMath::Min(L.Params.GapMax, 200.0), L.Params.GapMax, Saturate(L.Params.Difficulty01)) * 0.8;
				const int32 Rows = FMath::Max(1, FMath::CeilToInt(F.Length / FMath::Max(150.0, MaxJump)) - 1);
				return F.Length / (Rows + 1);
			}
			default: return F.Length;
		}
	}

	inline void SampleSurvivalTop(const FLayout& L, FSurvivalTop& Out)
	{
		const double Spacing = 100.0;
		const FVector2D Origin((L.WorldSizeX - (FSurvivalTop::Rows - 1) * Spacing) * 0.5, 0.0);
		// El mallado del terreno va en X (ancho) por filas de Y: NX = filas del banco, NY = columnas.
		FTerrainBuilder TB;
		TB.Build(L, Origin, Spacing, FSurvivalTop::Rows, FSurvivalTop::Cols);
		TArray<float> H;
		TArray<uint8> Mask;
		H.SetNum(FSurvivalTop::Rows * FSurvivalTop::Cols);
		Mask.SetNum(H.Num());
		TB.ComputeRows(0, FSurvivalTop::Cols, H, Mask);

		Out.Top.SetNum(H.Num());
		for (int32 iy = 0; iy < FSurvivalTop::Cols; ++iy)
		{
			for (int32 ix = 0; ix < FSurvivalTop::Rows; ++ix)
			{
				Out.Top[ix * FSurvivalTop::Cols + iy] = static_cast<float>(H[iy * FSurvivalTop::Rows + ix] / 100.0 + SurvivalBenchWaterM);
			}
		}
		auto ToIndex = [&Origin, Spacing](const FVector2D& P)
		{
			return FIntPoint(FMath::Clamp(FMath::RoundToInt((P.X - Origin.X) / Spacing), 0, FSurvivalTop::Rows - 1),
				FMath::Clamp(FMath::RoundToInt((P.Y - Origin.Y) / Spacing), 0, FSurvivalTop::Cols - 1));
		};
		Out.Start = ToIndex(L.StartPoint);
		// Meta: el último punto seco del camino, en la playa (la línea de llegada ya está dentro del agua).
		FVector2D Goal = L.EndPoint;
		for (int32 i = L.Main.Num() - 1; i >= 0; --i)
		{
			if (L.Main[i].Z > SeaLevel + 30.0 && (L.Main[i].Flags & PathFlags::Gap) == 0) { Goal = L.Main[i].P; break; }
		}
		Out.Goal = ToIndex(Goal);

		// Huecos de salto: los labios (mallas del actor, no terreno) reducen la zanja al hueco exacto. Se estampan a
		// la cota del camino, desde el borde del salto hasta pasada la zanja, y el salto va de labio a labio.
		Out.Jumps.Reset();
		for (const FFeature& F : L.Features)
		{
			if (F.Type != EFeature::Gap) { continue; }
			const FVector2D C(F.Location.X, F.Location.Y);
			const FVector2D N = LeftNormal(F.Dir);
			const double Inner = F.Length * 0.5;
			const double Outer = F.Height * 0.5 + 150.0;
			const float LipM = static_cast<float>(F.Location.Z / 100.0 + SurvivalBenchWaterM);
			const double Reach = Outer + F.Width * 0.5;
			const FIntPoint Lo = ToIndex(C - FVector2D(Reach, Reach));
			const FIntPoint Hi = ToIndex(C + FVector2D(Reach, Reach));
			for (int32 Row = Lo.X; Row <= Hi.X; ++Row)
			{
				for (int32 Col = Lo.Y; Col <= Hi.Y; ++Col)
				{
					const FVector2D Rel = Origin + FVector2D(Row * Spacing, Col * Spacing) - C;
					const double Along = FMath::Abs(FVector2D::DotProduct(Rel, F.Dir));
					if (Along >= Inner && Along <= Outer && FMath::Abs(FVector2D::DotProduct(Rel, N)) <= F.Width * 0.5)
					{
						float& T = Out.Top[Row * FSurvivalTop::Cols + Col];
						T = FMath::Max(T, LipM);
					}
				}
			}
			const FVector2D Half = F.Dir * (Inner + 50.0);
			Out.Jumps.Add({ ToIndex(C - Half), ToIndex(C + Half), GapLongestLeap(L, F) / 100.0 });
		}
	}
}
