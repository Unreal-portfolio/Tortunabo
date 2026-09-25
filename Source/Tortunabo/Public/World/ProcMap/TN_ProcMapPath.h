#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"

/**
 * Camino fino: geometría dentro de cada módulo, anchura, perfil de alturas y ramas.
 *
 * Dentro de un módulo el camino lo traza un "caminante" con inercia de rumbo: cada
 * paso de 6 m evalúa unos pocos giros posibles y puntúa alineación con un rumbo
 * deseado que oscila con ruido alrededor de la dirección al portal de salida (el
 * meandro de un río), repulsión del borde del módulo y de su propio recorrido, y
 * una atracción a la salida que crece al agotarse el presupuesto de longitud.
 * Luego se suaviza (Chaikin) y se remuestrea a espaciado fijo.
 */

namespace TNProcMap
{
	namespace PathDetail
	{
		struct FWalkInput
		{
			int32 Module = INDEX_NONE;
			FVector2D Entry = FVector2D::ZeroVector;
			/** Dirección de entrada, apuntando HACIA DENTRO del módulo. */
			FVector2D EntryDir = FVector2D(0.0, 1.0);
			FVector2D Exit = FVector2D::ZeroVector;
			/** Dirección de salida, apuntando HACIA FUERA del módulo. */
			FVector2D ExitDir = FVector2D(0.0, 1.0);
			double TargetLength = 40000.0;
			double Margin = 3000.0;
			uint32 NoiseSeed = 0;
			double MeanderAmp = 1.2;
			double Wavelength = 16000.0;
			/** Si true, el final puede salir del módulo (tramo de costa). */
			bool bFreeExit = false;
		};

		constexpr double WalkStep = 600.0;
		constexpr double PortalRelax = 5000.0;
		constexpr double LeadIn = 2500.0;
		constexpr double LeadOut = 3000.0;

		inline double RelaxFactor(const FWalkInput& In, const FVector2D& P)
		{
			const double D = FMath::Min(FVector2D::Distance(P, In.Entry), FVector2D::Distance(P, In.Exit));
			return Saturate(D / PortalRelax);
		}

		inline bool Walk(const FLayout& L, const FWalkInput& In, TArray<FVector2D>& Out)
		{
			Out.Reset();
			const FVector2D PostEntry = In.Entry + In.EntryDir * LeadIn;
			const FVector2D PreExit = In.Exit - In.ExitDir * LeadOut;
			Out.Add(In.Entry);
			Out.Add(PostEntry);

			FVector2D Pos = PostEntry;
			double Heading = AngleOf(In.EntryDir);
			double Traveled = LeadIn;
			// Fase del meandro: el rumbo deseado oscila como un río (sinuosidad ≈ 1/J0(Amp)).
			double Phase = static_cast<double>(In.NoiseSeed % 6283u) / 1000.0;
			const double MaxTurn = FMath::DegreesToRadians(16.0);
			const double SelfClear = FMath::Max(5200.0, In.Margin * 1.7);
			const int32 SkipRecent = FMath::CeilToInt(SelfClear * 1.6 / WalkStep);
			const int32 MaxSteps = FMath::CeilToInt(In.TargetLength * 2.6 / WalkStep) + 60;

			for (int32 It = 0; It < MaxSteps; ++It)
			{
				const FVector2D ToT = PreExit - Pos;
				const double DistT = ToT.Size();
				if (DistT < WalkStep * 1.5)
				{
					Out.Add(PreExit);
					Out.Add(In.Exit);
					return true;
				}

				const double Remaining = In.TargetLength - LeadOut - Traveled;
				// Amplitud del meandro: se apaga cuando el presupuesto no da para más que ir recto.
				const double Slack = Saturate((Remaining - DistT * 1.1) / FMath::Max(1.0, 0.18 * In.TargetLength));
				const double Amp = In.MeanderAmp * Slack * (0.8 + 0.35 * Noise1(In.NoiseSeed + 11u, Traveled / (In.Wavelength * 2.3)));
				const double AngT = AngleOf(ToT);
				const double Wave = FMath::Sin(Phase) + 0.3 * Noise1(In.NoiseSeed, Traveled / 9000.0);
				const double Desired = AngT + Amp * Wave;
				const double ExitPull = 0.2 + 2.8 * (1.0 - Slack);
				// La longitud de onda varía con ruido para que no quede un zigzag regular.
				Phase += TwoPi * WalkStep / (In.Wavelength * (0.8 + 0.4 * (0.5 + 0.5 * Noise1(In.NoiseSeed + 29u, Traveled / 30000.0))));

				double BestScore = -1e300;
				double BestHeading = Heading;
				bool bFound = false;
				for (int32 k = -3; k <= 3; ++k)
				{
					const double Phi = Heading + MaxTurn * (static_cast<double>(k) / 3.0);
					const FVector2D Next = Pos + DirFromAngle(Phi) * WalkStep;
					const double Relax = RelaxFactor(In, Next);

					const bool bInside = L.ModuleAt(Next) == In.Module;
					if (!bInside && Relax > 0.35 && !In.bFreeExit) { continue; }

					double Score = FMath::Cos(WrapAngle(Phi - Desired)) + ExitPull * FMath::Cos(WrapAngle(Phi - AngT));

					const double Required = In.Margin * Relax;
					const double Bd = bInside ? L.BorderDistAt(Next) : 0.0;
					if (Bd < Required)
					{
						if (Bd < Required * 0.35 && Relax > 0.6 && !In.bFreeExit) { continue; }
						Score -= 3.0 * (Required - Bd) / 1000.0;
					}

					double MinSelf = 1e300;
					for (int32 j = 0; j < Out.Num() - SkipRecent; ++j)
					{
						MinSelf = FMath::Min(MinSelf, FVector2D::DistSquared(Next, Out[j]));
					}
					MinSelf = FMath::Sqrt(MinSelf);
					if (MinSelf < SelfClear)
					{
						if (MinSelf < SelfClear * 0.55) { continue; }
						Score -= 4.0 * (SelfClear - MinSelf) / 1000.0;
					}

					if (Score > BestScore)
					{
						BestScore = Score;
						BestHeading = Phi;
						bFound = true;
					}
				}
				if (!bFound) { return false; }

				Heading = BestHeading;
				Pos = Pos + DirFromAngle(Heading) * WalkStep;
				Out.Add(Pos);
				Traveled += WalkStep;
			}
			return false;
		}

		/** Comprueba que un camino de módulo queda dentro, lejos del borde y sin tocarse a sí mismo. */
		inline bool ValidateModulePath(const FLayout& L, const FWalkInput& In, const TArray<FVector2D>& Pts)
		{
			const double MinBorder = In.Margin * 0.45;
			double S = 0.0;
			const double Total = PolylineLength(Pts);
			TArray<double> Acc;
			Acc.SetNum(Pts.Num());
			for (int32 i = 0; i < Pts.Num(); ++i)
			{
				if (i > 0) { S += FVector2D::Distance(Pts[i - 1], Pts[i]); }
				Acc[i] = S;
				const bool bNearEnd = S < 3500.0 || Total - S < 3500.0;
				if (bNearEnd) { continue; }
				if (!In.bFreeExit || Total - S > 9000.0)
				{
					if (L.ModuleAt(Pts[i]) != In.Module) { return false; }
					if (L.BorderDistAt(Pts[i]) < MinBorder) { return false; }
				}
			}
			for (int32 i = 0; i < Pts.Num(); i += 2)
			{
				for (int32 j = i + 2; j < Pts.Num(); j += 2)
				{
					if (Acc[j] - Acc[i] < 9000.0) { continue; }
					if (FVector2D::DistSquared(Pts[i], Pts[j]) < 3600.0 * 3600.0) { return false; }
				}
			}
			return true;
		}

		inline TArray<FVector2D> CatmullRom(const TArray<FVector2D>& Ctrl, int32 PerSegment)
		{
			TArray<FVector2D> Out;
			if (Ctrl.Num() < 2) { return Ctrl; }
			for (int32 i = 0; i < Ctrl.Num() - 1; ++i)
			{
				const FVector2D P0 = Ctrl[FMath::Max(0, i - 1)];
				const FVector2D P1 = Ctrl[i];
				const FVector2D P2 = Ctrl[i + 1];
				const FVector2D P3 = Ctrl[FMath::Min(Ctrl.Num() - 1, i + 2)];
				for (int32 k = 0; k < PerSegment; ++k)
				{
					const double T = static_cast<double>(k) / PerSegment;
					const double T2 = T * T;
					const double T3 = T2 * T;
					Out.Add((P1 * 2.0 + (P2 - P0) * T + (P0 * 2.0 - P1 * 5.0 + P2 * 4.0 - P3) * T2 + (P1 * 3.0 - P0 - P2 * 3.0 + P3) * T3) * 0.5);
				}
			}
			Out.Add(Ctrl.Last());
			return Out;
		}

		/** Curva de reserva por el interior del módulo cuando el caminante falla. */
		inline TArray<FVector2D> FallbackCurve(const FLayout& L, const FWalkInput& In, bool bThroughInterior)
		{
			TArray<FVector2D> Ctrl;
			Ctrl.Add(In.Entry);
			Ctrl.Add(In.Entry + In.EntryDir * LeadIn);
			if (bThroughInterior)
			{
				// Punto interior con más holgura cerca del punto medio entre portales.
				const FVector2D Mid = (In.Entry + In.Exit) * 0.5;
				FVector2D Best = L.Modules[In.Module].Centroid;
				double BestScore = -1e300;
				const FModule& M = L.Modules[In.Module];
				for (int32 y = 0; y < L.RasterH; y += 3)
				{
					for (int32 x = 0; x < L.RasterW; x += 3)
					{
						if (L.ModuleOfCell[L.CellIndex(x, y)] != M.Id) { continue; }
						const FVector2D C = L.CellCenter(x, y);
						const double Score = L.BorderDist[L.CellIndex(x, y)] - 0.35 * FVector2D::Distance(C, Mid);
						if (Score > BestScore) { BestScore = Score; Best = C; }
					}
				}
				Ctrl.Add(Best);
			}
			Ctrl.Add(In.Exit - In.ExitDir * LeadOut);
			Ctrl.Add(In.Exit);
			return CatmullRom(Ctrl, 12);
		}

		/** Pasada casi recta (módulos de cruce): cuerda con un ligero arco. */
		inline TArray<FVector2D> ChordCurve(const FWalkInput& In, FRng& Rng)
		{
			const FVector2D A = In.Entry + In.EntryDir * LeadIn;
			const FVector2D B = In.Exit - In.ExitDir * LeadOut;
			const FVector2D Mid = (A + B) * 0.5 + LeftNormal((B - A).GetSafeNormal()) * (FVector2D::Distance(A, B) * Rng.Range(-0.05, 0.05));
			TArray<FVector2D> Ctrl;
			Ctrl.Add(In.Entry);
			Ctrl.Add(A);
			Ctrl.Add(Mid);
			Ctrl.Add(B);
			Ctrl.Add(In.Exit);
			return CatmullRom(Ctrl, 10);
		}

		/** Posición del camino principal a una distancia S (interpolada). */
		inline FVector2D MainPointAt(const TArray<FPathSample>& Main, double S, FVector2D* OutDir = nullptr, int32* OutIndex = nullptr)
		{
			if (Main.Num() == 0) { return FVector2D::ZeroVector; }
			int32 Lo = 0, Hi = Main.Num() - 1;
			if (S <= Main[0].S) { if (OutDir) { *OutDir = Main[0].Dir; } if (OutIndex) { *OutIndex = 0; } return Main[0].P; }
			if (S >= Main[Hi].S) { if (OutDir) { *OutDir = Main[Hi].Dir; } if (OutIndex) { *OutIndex = Hi; } return Main[Hi].P; }
			while (Hi - Lo > 1)
			{
				const int32 Mid = (Lo + Hi) / 2;
				if (Main[Mid].S <= S) { Lo = Mid; } else { Hi = Mid; }
			}
			const double T = (S - Main[Lo].S) / FMath::Max(1e-6, Main[Hi].S - Main[Lo].S);
			if (OutDir) { *OutDir = (Main[Lo].Dir * (1.0 - T) + Main[Hi].Dir * T).GetSafeNormal(); }
			if (OutIndex) { *OutIndex = Lo; }
			return Main[Lo].P + (Main[Hi].P - Main[Lo].P) * T;
		}

		/** Recalcula S y Dir de una lista de muestras. */
		inline void FinalizeSamples(TArray<FPathSample>& Samples)
		{
			double S = 0.0;
			for (int32 i = 0; i < Samples.Num(); ++i)
			{
				if (i > 0) { S += FVector2D::Distance(Samples[i - 1].P, Samples[i].P); }
				Samples[i].S = S;
			}
			for (int32 i = 0; i < Samples.Num(); ++i)
			{
				const FVector2D A = Samples[FMath::Max(0, i - 2)].P;
				const FVector2D B = Samples[FMath::Min(Samples.Num() - 1, i + 2)].P;
				const FVector2D D = (B - A).GetSafeNormal();
				Samples[i].Dir = D.IsNearlyZero() ? FVector2D(0.0, 1.0) : D;
			}
		}

		/** Envolvente inferior con pendiente máxima K (cm por cm), in-place en [From, To]. */
		inline void SlopeLimit(TArray<double>& Z, const TArray<FPathSample>& Samples, int32 From, int32 To, double K)
		{
			for (int32 i = From + 1; i <= To; ++i)
			{
				const double Ds = Samples[i].S - Samples[i - 1].S;
				Z[i] = FMath::Min(Z[i], Z[i - 1] + K * Ds);
			}
			for (int32 i = To - 1; i >= From; --i)
			{
				const double Ds = Samples[i + 1].S - Samples[i].S;
				Z[i] = FMath::Min(Z[i], Z[i + 1] + K * Ds);
			}
		}

		/** Rejilla de cubos de muestras para consultas de distancia al camino principal. */
		struct FSampleGrid
		{
			double Cell = 4000.0;
			int32 W = 0, H = 0;
			double Origin = -20000.0;
			TArray<TArray<int32>> Buckets;
			const TArray<FPathSample>* Samples = nullptr;

			void Build(const TArray<FPathSample>& In, double WorldSize)
			{
				Samples = &In;
				W = H = FMath::CeilToInt((WorldSize - 2.0 * Origin) / Cell) + 1;
				Buckets.Reset();
				Buckets.SetNum(W * H);
				for (int32 i = 0; i < In.Num(); ++i)
				{
					const int32 X = FMath::Clamp(FMath::FloorToInt((In[i].P.X - Origin) / Cell), 0, W - 1);
					const int32 Y = FMath::Clamp(FMath::FloorToInt((In[i].P.Y - Origin) / Cell), 0, H - 1);
					Buckets[Y * W + X].Add(i);
				}
			}

			/**
			 * Muestra más cercana dentro de Radius (INDEX_NONE si ninguna). Con MinAlong > 0
			 * ignora las que están a menos de MinAlong de AlongS medido por el camino.
			 */
			int32 Nearest(const FVector2D& P, double Radius, double& OutDist, double AlongS = 0.0, double MinAlong = 0.0) const
			{
				OutDist = 1e300;
				int32 Best = INDEX_NONE;
				const int32 R = FMath::CeilToInt(Radius / Cell);
				const int32 CX = FMath::FloorToInt((P.X - Origin) / Cell);
				const int32 CY = FMath::FloorToInt((P.Y - Origin) / Cell);
				for (int32 Y = FMath::Max(0, CY - R); Y <= FMath::Min(H - 1, CY + R); ++Y)
				{
					for (int32 X = FMath::Max(0, CX - R); X <= FMath::Min(W - 1, CX + R); ++X)
					{
						for (const int32 Idx : Buckets[Y * W + X])
						{
							if (MinAlong > 0.0 && FMath::Abs((*Samples)[Idx].S - AlongS) < MinAlong) { continue; }
							const double D = FVector2D::Distance(P, (*Samples)[Idx].P);
							if (D < OutDist) { OutDist = D; Best = Idx; }
						}
					}
				}
				return OutDist <= Radius ? Best : INDEX_NONE;
			}
		};

		/** Tramos por anchura, de desfiladero a explanada. */
		enum class EWidthKind : uint8 { Narrow, Tight, Normal, Wide, Open };
		constexpr int32 NumWidthKinds = 5;

		/** Reparto de tramos por bioma: cañones en desierto y roca, arenales abiertos en la playa. */
		inline void WidthKindWeights(ETNProcBiome Biome, double NarrowChance, double (&Out)[NumWidthKinds])
		{
			Out[0] = NarrowChance; Out[1] = 0.2; Out[2] = 0.3; Out[3] = 0.2; Out[4] = 0.1;
			switch (Biome)
			{
				case ETNProcBiome::Desert:   Out[0] *= 1.6; Out[1] *= 1.3; Out[4] *= 0.8; break;
				case ETNProcBiome::Rocky:    Out[0] *= 1.8; Out[1] *= 1.4; Out[3] *= 0.7; break;
				case ETNProcBiome::Beach:    Out[0] *= 0.4; Out[3] *= 1.5; Out[4] *= 2.2; break;
				case ETNProcBiome::Jungle:   Out[1] *= 1.5; Out[4] *= 0.8; break;
				case ETNProcBiome::Volcanic: Out[0] *= 1.2; Out[4] *= 1.4; break;
				case ETNProcBiome::Human:    Out[2] *= 1.4; Out[3] *= 1.3; break;
				default: break;
			}
		}

		/** Anchura (U en [0,1]) y longitud de cada tipo de tramo. */
		inline double WidthOfKind(const FGenParams& P, EWidthKind Kind, double U)
		{
			const double Mn = P.PathWidthMin;
			const double Mx = FMath::Max(P.PathWidthMax, Mn * 4.0);
			switch (Kind)
			{
				case EWidthKind::Narrow: return LerpD(Mn * 0.9, Mn * 1.3, U);
				case EWidthKind::Tight:  return LerpD(Mn * 1.6, Mn * 2.6, U);
				case EWidthKind::Normal: return LerpD(Mn * 2.8, FMath::Max(Mn * 3.2, Mx * 0.5), U);
				case EWidthKind::Wide:   return LerpD(Mx * 0.6, Mx, U);
				default:                 return LerpD(Mx * 1.15, Mx * 1.7, U);
			}
		}

		inline double LengthOfKind(EWidthKind Kind, double U)
		{
			switch (Kind)
			{
				case EWidthKind::Narrow: return LerpD(2500.0, 6500.0, U);
				case EWidthKind::Tight:  return LerpD(3000.0, 9000.0, U);
				case EWidthKind::Normal: return LerpD(5000.0, 15000.0, U);
				case EWidthKind::Wide:   return LerpD(4000.0, 11000.0, U);
				default:                 return LerpD(3500.0, 8000.0, U);
			}
		}
	}

	/** Punto de salida (sur) y de llegada (costa norte). */
	inline void ChooseStartAndEnd(FLayout& L, FRng& Rng)
	{
		const FGenParams& P = L.Params;
		const int32 StartModule = L.Route[0].Module;
		const int32 EndModule = L.Route.Last().Module;

		// Salida: celda del módulo inicial con holgura para el claro, lo más al sur posible.
		double BestScore = -1e300;
		FVector2D BestStart = L.Modules[StartModule].Centroid;
		const double Need = P.StartClearingRadius + 1500.0;
		for (int32 y = 0; y < L.RasterH; ++y)
		{
			for (int32 x = 0; x < L.RasterW; ++x)
			{
				const int32 Idx = L.CellIndex(x, y);
				if (L.ModuleOfCell[Idx] != StartModule) { continue; }
				const double Bd = L.BorderDist[Idx];
				const FVector2D C = L.CellCenter(x, y);
				const double Score = (Bd >= Need ? 100000.0 : Bd) - C.Y * 0.6 + Rng.Range(0.0, 300.0);
				if (Score > BestScore) { BestScore = Score; BestStart = C; }
			}
		}
		L.StartPoint = BestStart;

		// Llegada: junto a la costa, en la parte del módulo final más alejada de sus vecinos.
		BestScore = -1e300;
		FVector2D BestEnd = L.Modules[EndModule].Centroid;
		for (int32 y = 0; y < L.RasterH; ++y)
		{
			for (int32 x = 0; x < L.RasterW; ++x)
			{
				const int32 Idx = L.CellIndex(x, y);
				if (L.ModuleOfCell[Idx] != EndModule) { continue; }
				const FVector2D C = L.CellCenter(x, y);
				const double Coast = L.CoastY(C.X);
				if (C.Y < Coast - 5000.0 || C.Y > Coast - 1800.0) { continue; }
				const double EdgeX = FMath::Min(C.X, L.WorldSize - C.X) - P.MapEdgeClearance;
				const double Score = FMath::Min(static_cast<double>(L.ModuleDist[Idx]), EdgeX) + Rng.Range(0.0, 400.0);
				if (Score > BestScore) { BestScore = Score; BestEnd = C; }
			}
		}
		L.EndPoint = BestEnd;
	}

	/** Traza el camino principal módulo a módulo. */
	inline bool BuildMainPath(FLayout& L, FRng Rng)
	{
		using namespace PathDetail;
		const FGenParams& P = L.Params;
		ChooseStartAndEnd(L, Rng);

		L.Main.Reset();
		const int32 NumSteps = L.Route.Num();
		for (int32 k = 0; k < NumSteps; ++k)
		{
			FRouteStep& Step = L.Route[k];
			FWalkInput In;
			In.Module = Step.Module;
			In.Entry = k == 0 ? L.StartPoint : L.Portals[Step.EntryPortal].Point;
			In.Exit = k == NumSteps - 1 ? L.EndPoint : L.Portals[Step.ExitPortal].Point;
			In.ExitDir = k == NumSteps - 1 ? FVector2D(0.0, 1.0) : L.Portals[Step.ExitPortal].Dir;
			In.EntryDir = k == 0 ? (In.Exit - In.Entry).GetSafeNormal() : L.Portals[Step.EntryPortal].Dir;
			In.bFreeExit = k == NumSteps - 1;
			In.Margin = 3200.0;
			In.NoiseSeed = P.Seed ^ Hash32(static_cast<uint32>(k) * 2654435761u);
			const double Straight = FVector2D::Distance(In.Entry, In.Exit);
			In.TargetLength = FMath::Max(Straight * P.Sinuosity * Rng.Range(0.85, 1.15), Straight + LeadIn + LeadOut);
			In.MeanderAmp = Rng.Range(1.2, 1.55);
			// Longitud de onda del meandro grande: una o dos curvas amplias por módulo.
			In.Wavelength = Rng.Range(0.8, 1.4) * P.ModuleSize;

			TArray<FVector2D> Pts;
			const bool bCrossModule = Step.CrossingIndex != INDEX_NONE;
			bool bOk = false;
			if (bCrossModule)
			{
				Pts = ChordCurve(In, Rng);
				bOk = true;
			}
			else
			{
				for (int32 Attempt = 0; Attempt < 6 && !bOk; ++Attempt)
				{
					FWalkInput Try = In;
					Try.NoiseSeed = In.NoiseSeed + static_cast<uint32>(Attempt) * 7717u;
					Try.MeanderAmp = In.MeanderAmp * (1.0 - 0.15 * Attempt);
					Try.TargetLength = FMath::Max(Straight + LeadIn + LeadOut, In.TargetLength * (1.0 - 0.1 * Attempt));
					TArray<FVector2D> Raw;
					if (Walk(L, Try, Raw))
					{
						Pts = ResamplePolyline(ChaikinSmooth(Raw, 2), P.SampleSpacing);
						bOk = ValidateModulePath(L, Try, Pts);
					}
				}
				if (!bOk)
				{
					++L.WalkFallbacks;
					Pts = FallbackCurve(L, In, true);
					bOk = ValidateModulePath(L, In, ResamplePolyline(Pts, P.SampleSpacing));
					if (!bOk) { Pts = FallbackCurve(L, In, false); }
				}
			}
			Pts = ResamplePolyline(Pts, P.SampleSpacing);

			Step.FirstSample = L.Main.Num() > 0 ? L.Main.Num() - 1 : 0;
			for (int32 i = (k == 0 ? 0 : 1); i < Pts.Num(); ++i)
			{
				FPathSample Sm;
				Sm.P = Pts[i];
				Sm.Step = k;
				Sm.Module = Step.Module;
				Sm.Biome = L.Modules[Step.Module].Biome;
				L.Main.Add(Sm);
			}
			Step.LastSample = L.Main.Num() - 1;
			L.Main[Step.LastSample].Flags |= (k < NumSteps - 1) ? PathFlags::Portal : PathFlags::None;
		}

		// Bajada final al mar: la meta es entrar en el agua.
		{
			const FVector2D From = L.Main.Last().P;
			const int32 Extra = FMath::CeilToInt(3600.0 / P.SampleSpacing);
			for (int32 i = 1; i <= Extra; ++i)
			{
				FPathSample Sm = L.Main.Last();
				Sm.P = From + FVector2D(0.0, P.SampleSpacing * i);
				Sm.Flags = PathFlags::Shore;
				L.Main.Add(Sm);
			}
			L.Route.Last().LastSample = L.Main.Num() - 1;
		}

		FinalizeSamples(L.Main);
		L.Main[0].Flags |= PathFlags::Start;
		L.Main.Last().Flags |= PathFlags::End;
		for (FPathSample& Sm : L.Main)
		{
			if (FVector2D::Distance(Sm.P, L.StartPoint) < P.StartClearingRadius) { Sm.Flags |= PathFlags::Start; }
		}
		return L.Main.Num() > 4;
	}

	/**
	 * Anchura muy variable por tramos: desfiladeros (3,5-5 m), pasos cerrados, tramos
	 * normales, anchos y explanadas (40-60 m), con transiciones de 8-25 m y bordes que
	 * respiran. Respeta la holgura del módulo y la de otras partes del camino: entre
	 * dos cauces queda siempre un muro (no se funden ni se ataja por ellos).
	 */
	inline void ComputeWidths(FLayout& L, FRng Rng)
	{
		using namespace PathDetail;
		const FGenParams& P = L.Params;
		const int32 NumS = L.Main.Num();
		if (NumS == 0) { return; }
		const uint32 WSeed = P.Seed ^ 0xA11CEu;

		// Secuencia de tramos a lo largo del camino; nunca dos seguidos del mismo tipo.
		struct FSection { double S0 = 0.0; double W = 0.0; double Len = 0.0; };
		TArray<FSection> Sections;
		{
			const double Total = L.Main.Last().S;
			double Cursor = 0.0;
			int32 Prev = INDEX_NONE;
			while (Cursor <= Total)
			{
				int32 Near = 0;
				MainPointAt(L.Main, Cursor, nullptr, &Near);
				double Wt[NumWidthKinds];
				WidthKindWeights(L.Main[Near].Biome, P.NarrowChance, Wt);
				if (Prev != INDEX_NONE) { Wt[Prev] = 0.0; }
				double Sum = 0.0;
				for (const double V : Wt) { Sum += V; }
				double Pick = Rng.Unit() * Sum;
				int32 Kind = NumWidthKinds - 1;
				for (int32 k = 0; k < NumWidthKinds; ++k)
				{
					if (Pick < Wt[k]) { Kind = k; break; }
					Pick -= Wt[k];
				}
				FSection Sec;
				Sec.S0 = Cursor;
				Sec.W = WidthOfKind(P, static_cast<EWidthKind>(Kind), Rng.Unit());
				Sec.Len = LengthOfKind(static_cast<EWidthKind>(Kind), Rng.Unit());
				Sections.Add(Sec);
				Cursor += Sec.Len;
				Prev = Kind;
			}
		}
		auto TransLen = [&Sections](int32 A, int32 B) { return FMath::Clamp(0.35 * FMath::Min(Sections[A].Len, Sections[B].Len), 800.0, 2500.0); };

		TArray<double> W;
		W.SetNum(NumS);
		int32 Sec = 0;
		for (int32 i = 0; i < NumS; ++i)
		{
			const FPathSample& Sm = L.Main[i];
			while (Sec + 1 < Sections.Num() && Sections[Sec + 1].S0 <= Sm.S) { ++Sec; }
			double Wi = Sections[Sec].W;
			if (Sec > 0)
			{
				const double T = TransLen(Sec - 1, Sec);
				Wi = LerpD(Sections[Sec - 1].W, Wi, SmoothStep(-0.5 * T, 0.5 * T, Sm.S - Sections[Sec].S0));
			}
			if (Sec + 1 < Sections.Num())
			{
				const double T = TransLen(Sec, Sec + 1);
				Wi = LerpD(Wi, Sections[Sec + 1].W, SmoothStep(-0.5 * T, 0.5 * T, Sm.S - Sections[Sec + 1].S0));
			}
			// Bordes que respiran: ±14 % a escala de ~20 m.
			W[i] = Wi * (1.0 + 0.14 * Fbm1(WSeed + 5u, Sm.S / 1800.0, 2));
		}
		W = SmoothScalars(W, 2);

		// Holgura con el borde del módulo y con otras partes del camino (muro de al menos 18 m).
		FSampleGrid Grid;
		Grid.Build(L.Main, L.WorldSize);
		for (int32 i = 0; i < NumS; ++i)
		{
			const FPathSample& Sm = L.Main[i];
			double Wi = W[i];
			const double Bd = L.BorderDistAt(Sm.P);
			Wi = FMath::Min(Wi, FMath::Max(P.PathWidthMin, 2.0 * (Bd - 1800.0)));
			double DSelf = 0.0;
			if (Grid.Nearest(Sm.P, 12000.0, DSelf, Sm.S, FMath::Max(9000.0, 2.0 * Wi)) != INDEX_NONE)
			{
				Wi = FMath::Min(Wi, FMath::Max(P.PathWidthMin, DSelf - 1800.0));
			}
			W[i] = FMath::Max(330.0, Wi);
		}
		// Cerca de portales: anchura del portal.
		for (const FRouteStep& Step : L.Route)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const int32 PortalIdx = Side == 0 ? Step.EntryPortal : Step.ExitPortal;
				if (PortalIdx == INDEX_NONE) { continue; }
				const FPortal& Portal = L.Portals[PortalIdx];
				for (int32 i = Step.FirstSample; i <= Step.LastSample && i < L.Main.Num(); ++i)
				{
					const double D = FVector2D::Distance(L.Main[i].P, Portal.Point);
					if (D < 4000.0) { W[i] = LerpD(Portal.Width, W[i], SmoothStep(1500.0, 4000.0, D)); }
				}
			}
		}
		TArray<double> Smoothed = SmoothScalars(W, 2);
		for (int32 i = 0; i < L.Main.Num(); ++i)
		{
			L.Main[i].Width = Smoothed[i];
			if (L.Main[i].Biome == ETNProcBiome::Mangrove) { L.Main[i].Width = Rng.Range(320.0, 420.0); }
			if ((L.Main[i].Flags & PathFlags::Start) != 0) { L.Main[i].Width = FMath::Max(L.Main[i].Width, P.StartClearingRadius * 1.2); }
			if ((L.Main[i].Flags & PathFlags::Shore) != 0) { L.Main[i].Width = FMath::Max(L.Main[i].Width, 3000.0); }
		}
	}

	/**
	 * Alturas del camino: nivel de cada módulo + ondulación suave con pendiente
	 * limitada; géiser/tobogán en saltos de nivel grandes; tablero o mesa a cota
	 * colosal en las pasadas altas de los cruces; torres en sus extremos.
	 */
	inline void ComputeZProfile(FLayout& L, FRng Rng)
	{
		using namespace PathDetail;
		const FGenParams& P = L.Params;
		const int32 NumS = L.Main.Num();
		if (NumS == 0) { return; }
		const uint32 ZSeed = P.Seed ^ 0x2E7Au;

		TArray<double> Z;
		Z.SetNum(NumS);
		for (int32 i = 0; i < NumS; ++i)
		{
			const FPathSample& Sm = L.Main[i];
			const bool bWet = IsWetBiome(Sm.Biome);
			Z[i] = L.Modules[Sm.Module].Level + (bWet ? 0.0 : 260.0 * Fbm1(ZSeed, Sm.S / 14000.0, 3));
		}

		// Segmentos continuos separados por cortes (cruces colosales y desniveles grandes).
		TArray<int32> Cuts; // índice de la muestra de portal donde hay corte
		TArray<uint8> CutIsUp;
		for (int32 k = 0; k + 1 < L.Route.Num(); ++k)
		{
			const FRouteStep& A = L.Route[k];
			const FRouteStep& B = L.Route[k + 1];
			const int32 PortalSample = A.LastSample;
			if (B.bHigh) { Cuts.Add(PortalSample); CutIsUp.Add(1); continue; }
			if (A.bHigh) { Cuts.Add(PortalSample); CutIsUp.Add(0); continue; }
			const double Delta = L.Modules[B.Module].Level - L.Modules[A.Module].Level;
			if (FMath::Abs(Delta) > P.SmoothTransitionMax)
			{
				Cuts.Add(PortalSample);
				CutIsUp.Add(Delta > 0.0 ? 1 : 0);
			}
		}

		// Suavizado + pendiente limitada por segmento (las pasadas altas se sobrescriben luego).
		{
			int32 SegStart = 0;
			TArray<int32> Bounds = Cuts;
			Bounds.Add(NumS - 1);
			for (const int32 SegEnd : Bounds)
			{
				if (SegEnd <= SegStart) { SegStart = SegEnd; continue; }
				TArray<double> Part;
				for (int32 i = SegStart; i <= SegEnd; ++i) { Part.Add(Z[i]); }
				Part = SmoothScalars(Part, 10);
				for (int32 i = SegStart; i <= SegEnd; ++i) { Z[i] = Part[i - SegStart]; }
				SlopeLimit(Z, L.Main, SegStart, SegEnd, P.MaxPathSlope);
				SegStart = SegEnd + 1;
			}
		}

		// Cota colosal de cada cruce.
		for (FCrossing& C : L.Crossings)
		{
			const FRouteStep& High = L.Route[C.HighStep];
			const FRouteStep& Low = L.Route[C.LowStep];
			double Ground = L.Modules[C.Module].Level;
			for (int32 i = Low.FirstSample; i <= Low.LastSample; ++i) { Ground = FMath::Max(Ground, Z[i]); }
			if (C.HighStep > 0) { Ground = FMath::Max(Ground, Z[L.Route[C.HighStep - 1].LastSample]); }
			if (C.HighStep + 1 < L.Route.Num()) { Ground = FMath::Max(Ground, Z[L.Route[C.HighStep + 1].FirstSample]); }
			C.TopZ = Ground + Rng.Range(P.ColossalHeightMin, P.ColossalHeightMax);
			for (int32 i = High.FirstSample; i <= High.LastSample; ++i)
			{
				Z[i] = C.TopZ;
				L.Main[i].Flags |= (C.Type == ETNProcCrossingType::Bridge) ? PathFlags::Elevated : PathFlags::Colossal;
				L.Main[i].Width = FMath::Clamp(L.Main[i].Width, 550.0, 900.0);
			}
			// Punto de cruce entre las dos pasadas.
			bool bFoundCross = false;
			for (int32 i = High.FirstSample; i < High.LastSample && !bFoundCross; ++i)
			{
				for (int32 j = Low.FirstSample; j < Low.LastSample; ++j)
				{
					FVector2D X;
					if (SegmentsIntersect(L.Main[i].P, L.Main[i + 1].P, L.Main[j].P, L.Main[j + 1].P, &X))
					{
						C.CrossPoint = X;
						bFoundCross = true;
						break;
					}
				}
			}
			if (!bFoundCross) { C.CrossPoint = L.Modules[C.Module].Centroid; }
		}

		// Extremos de las pasadas altas: torre al entrar (géiser al pie) y al salir (tobogán).
		const double TanSlide = FMath::Tan(FMath::DegreesToRadians(P.SlideAngleDeg));
		for (const FCrossing& C : L.Crossings)
		{
			const FRouteStep& High = L.Route[C.HighStep];
			const FVector2D InPortal = L.Main[High.FirstSample].P;
			const FVector2D OutPortal = L.Main[High.LastSample].P;

			// Pasada alta: plataforma de torre en ambos extremos.
			for (int32 i = High.FirstSample; i <= High.LastSample; ++i)
			{
				if (FVector2D::Distance(L.Main[i].P, InPortal) < P.TowerRadius || FVector2D::Distance(L.Main[i].P, OutPortal) < P.TowerRadius)
				{
					L.Main[i].Flags |= PathFlags::TowerTop;
				}
			}

			// Paso previo: la torre ocupa el final; el géiser queda al pie.
			if (C.HighStep > 0)
			{
				const FRouteStep& Prev = L.Route[C.HighStep - 1];
				int32 GeyserIdx = INDEX_NONE;
				for (int32 i = Prev.LastSample; i >= Prev.FirstSample; --i)
				{
					const double D = FVector2D::Distance(L.Main[i].P, InPortal);
					if (D < P.TowerRadius + 200.0)
					{
						L.Main[i].Flags |= PathFlags::UnderTower;
						Z[i] = C.TopZ;
						// Suelo amplio en la cima: el géiser aterriza en su centro, lejos del talud.
						L.Main[i].Width = FMath::Max(L.Main[i].Width, P.TowerRadius * 1.6);
					}
					else if (D >= P.TowerRadius + 700.0 && GeyserIdx == INDEX_NONE)
					{
						GeyserIdx = i;
					}
				}
				if (GeyserIdx != INDEX_NONE) { L.Main[GeyserIdx].Flags |= PathFlags::GeyserBase; }
			}

			// Paso siguiente: cima de torre y tobogán hasta el suelo.
			if (C.HighStep + 1 < L.Route.Num())
			{
				const FRouteStep& Next = L.Route[C.HighStep + 1];
				double SlideStartS = -1.0;
				for (int32 i = Next.FirstSample + 1; i <= Next.LastSample; ++i)
				{
					const double D = FVector2D::Distance(L.Main[i].P, OutPortal);
					if (SlideStartS < 0.0 && D < P.TowerRadius)
					{
						L.Main[i].Flags |= PathFlags::TowerTop;
						Z[i] = C.TopZ;
						L.Main[i].Width = FMath::Max(L.Main[i].Width, P.TowerRadius * 1.6);
						continue;
					}
					if (SlideStartS < 0.0) { SlideStartS = L.Main[i - 1].S; }
					const double Ramp = C.TopZ - TanSlide * (L.Main[i].S - SlideStartS);
					if (Ramp <= Z[i]) { break; }
					Z[i] = Ramp;
					L.Main[i].Flags |= PathFlags::Slide;
				}
			}
		}

		// Desniveles grandes entre módulos normales.
		for (int32 c = 0; c < Cuts.Num(); ++c)
		{
			const int32 CutIdx = Cuts[c];
			const int32 StepA = L.Main[CutIdx].Step;
			if (L.Route[StepA].bHigh || (StepA + 1 < L.Route.Num() && L.Route[StepA + 1].bHigh)) { continue; }
			if (CutIsUp[c])
			{
				// Géiser en el lado bajo, a ~8 m del escalón; aterriza pasado el borde.
				for (int32 i = CutIdx; i >= FMath::Max(0, CutIdx - 40); --i)
				{
					if (L.Main[CutIdx].S - L.Main[i].S >= 800.0) { L.Main[i].Flags |= PathFlags::GeyserBase; break; }
				}
				if (CutIdx + 1 < NumS) { L.Main[CutIdx + 1].Flags |= PathFlags::CliffUp; }
			}
			else
			{
				const double Top = Z[CutIdx];
				for (int32 i = CutIdx + 1; i < NumS; ++i)
				{
					const double Ramp = Top - TanSlide * (L.Main[i].S - L.Main[CutIdx].S);
					if (Ramp <= Z[i]) { break; }
					Z[i] = Ramp;
					L.Main[i].Flags |= PathFlags::Slide;
				}
			}
		}

		// Claro de salida plano y bajada final al mar.
		const double StartZ = Z[0];
		for (int32 i = 0; i < NumS; ++i)
		{
			if ((L.Main[i].Flags & PathFlags::Start) != 0) { Z[i] = StartZ; }
		}
		int32 FirstShore = NumS;
		for (int32 i = 0; i < NumS; ++i) { if ((L.Main[i].Flags & PathFlags::Shore) != 0) { FirstShore = i; break; } }
		for (int32 i = FirstShore; i < NumS; ++i)
		{
			const double T = static_cast<double>(i - FirstShore + 1) / FMath::Max(1, NumS - FirstShore);
			Z[i] = LerpD(Z[FMath::Max(0, FirstShore - 1)], -220.0, T);
		}
		// Rampa suave hacia la orilla antes de la bajada final (la playa llega a ras del agua).
		if (FirstShore > 0 && FirstShore < NumS)
		{
			const double BeachZ = 120.0;
			for (int32 i = FirstShore - 1; i >= 0; --i)
			{
				const double D = L.Main[FirstShore - 1].S - L.Main[i].S;
				if (D > 6000.0 || (L.Main[i].Flags & PathFlags::Special & ~PathFlags::Portal) != 0) { break; }
				Z[i] = LerpD(BeachZ, Z[i], SmoothStep(0.0, 6000.0, D));
			}
		}

		for (int32 i = 0; i < NumS; ++i) { L.Main[i].Z = Z[i]; }

		// Tramos sobre agua: alternan suelo firme (barras de arena / barro) con isletas o
		// pasarelas, salvo junto a torres, géiseres y toboganes, que necesitan suelo.
		double NextToggleS = -1.0;
		bool bWetStretch = false;
		for (int32 i = 0; i < NumS; ++i)
		{
			FPathSample& Sm = L.Main[i];
			if (!IsWetBiome(Sm.Biome)) { NextToggleS = -1.0; continue; }
			if (NextToggleS < 0.0)
			{
				bWetStretch = Rng.Chance(0.5);
				NextToggleS = Sm.S + (bWetStretch ? Rng.Range(4000.0, 11000.0) : Rng.Range(5000.0, 14000.0));
			}
			else if (Sm.S >= NextToggleS)
			{
				bWetStretch = !bWetStretch;
				NextToggleS = Sm.S + (bWetStretch ? Rng.Range(4000.0, 11000.0) : Rng.Range(5000.0, 14000.0));
			}
			if (!bWetStretch) { continue; }
			bool bNearSolid = false;
			for (int32 j = FMath::Max(0, i - 8); j <= FMath::Min(NumS - 1, i + 8); ++j)
			{
				const uint32 Solid = PathFlags::GeyserBase | PathFlags::Slide | PathFlags::UnderTower | PathFlags::TowerTop | PathFlags::CliffUp | PathFlags::Start;
				if ((L.Main[j].Flags & Solid) != 0) { bNearSolid = true; break; }
			}
			if (bNearSolid || (Sm.Flags & (PathFlags::Elevated | PathFlags::Colossal)) != 0) { continue; }
			Sm.Flags |= (Sm.Biome == ETNProcBiome::Water) ? PathFlags::Islet : PathFlags::Boardwalk;
		}

		// Tramo bajo de los cruces tipo cueva: marcado de túnel bajo la huella de la mesa.
		for (const FCrossing& C : L.Crossings)
		{
			if (C.Type != ETNProcCrossingType::Cave) { continue; }
			const FRouteStep& High = L.Route[C.HighStep];
			const FRouteStep& Low = L.Route[C.LowStep];
			const double Rise = C.TopZ - L.Modules[C.Module].Level;
			const double Footprint = 900.0 + Rise / 2.5 + 400.0;
			for (int32 j = Low.FirstSample; j <= Low.LastSample; ++j)
			{
				double MinD = 1e300;
				for (int32 i = High.FirstSample; i < High.LastSample; ++i)
				{
					double T = 0.0;
					MinD = FMath::Min(MinD, DistPointSegment(L.Main[j].P, L.Main[i].P, L.Main[i + 1].P, T));
				}
				if (MinD < Footprint) { L.Main[j].Flags |= PathFlags::Tunnel; L.Main[j].Width = FMath::Min(L.Main[j].Width, 900.0); }
			}
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Ramas
	// ─────────────────────────────────────────────────────────────────────────

	namespace PathDetail
	{
		inline bool IsCrossingModule(const FLayout& L, int32 Module)
		{
			for (const FCrossing& C : L.Crossings) { if (C.Module == Module) { return true; } }
			return false;
		}

		/** Forma de cada tipo de rama (cm). */
		struct FBranchShape
		{
			/** Longitud del tramo del principal que rodea. */
			double LenMin = 15000.0, LenMax = 45000.0;
			/** Separación lateral máxima respecto al principal. */
			double AmpMin = 4500.0, AmpMax = 11000.0;
			double WMin = 750.0, WMax = 1500.0;
			/** Ondulación de alturas (0 en la ruta alta, que lleva su propio perfil). */
			double Wave = 280.0;
		};

		inline FBranchShape BranchShapeOf(EBranchKind Kind)
		{
			FBranchShape S;
			switch (Kind)
			{
				case EBranchKind::Lane:
					S.LenMin = 12000.0; S.LenMax = 20000.0; S.AmpMin = 3800.0; S.AmpMax = 5200.0; S.WMin = 650.0; S.WMax = 850.0; break;
				case EBranchKind::Risky:
					S.LenMin = 9000.0; S.LenMax = 26000.0; S.AmpMin = 3500.0; S.AmpMax = 8000.0; S.WMin = 340.0; S.WMax = 520.0; break;
				case EBranchKind::High:
					S.LenMin = 18000.0; S.LenMax = 40000.0; S.AmpMin = 5000.0; S.AmpMax = 10000.0; S.WMin = 500.0; S.WMax = 900.0; S.Wave = 0.0; break;
				case EBranchKind::Bypass:
					S.LenMin = 5000.0; S.LenMax = 12000.0; S.AmpMin = 2600.0; S.AmpMax = 4200.0; S.WMin = 450.0; S.WMax = 900.0; S.Wave = 120.0; break;
				case EBranchKind::Scenic:
				default:
					break;
			}
			return S;
		}

		inline EBranchKind PickBranchKind(FRng& Rng)
		{
			const double U = Rng.Unit();
			if (U < 0.3) { return EBranchKind::Scenic; }
			if (U < 0.55) { return EBranchKind::Risky; }
			if (U < 0.75) { return EBranchKind::High; }
			return EBranchKind::Bypass;
		}
	}

	/**
	 * Bifurcaciones que se separan y vuelven a unirse más adelante (1..BranchMaxModules
	 * módulos), de varios tipos: alternativa tranquila y holgada, cornisa estrecha con
	 * más huecos, ruta alta (sube poco a poco y baja en tobogán) y rodeo corto; más los
	 * carriles del 2vs2.
	 */
	inline void BuildBranches(FLayout& L, FRng Rng)
	{
		using namespace PathDetail;
		const FGenParams& P = L.Params;
		L.Branches.Reset();
		if (L.Main.Num() < 20) { return; }

		FSampleGrid Grid;
		Grid.Build(L.Main, L.WorldSize);
		const double Total = L.MainLength();
		const uint32 BSeed = P.Seed ^ 0xB4A2Cu;
		const double TanSlide = FMath::Tan(FMath::DegreesToRadians(P.SlideAngleDeg));

		TArray<FVector2D> Towers;
		for (const FCrossing& C : L.Crossings)
		{
			Towers.Add(L.Main[L.Route[C.HighStep].FirstSample].P);
			Towers.Add(L.Main[L.Route[C.HighStep].LastSample].P);
		}

		const int32 Wanted = FMath::Max(0, P.NumLanes) + FMath::Max(0, P.NumBranches);
		for (int32 b = 0; b < Wanted; ++b)
		{
			const bool bLane = b < P.NumLanes;
			EBranchKind Kind = bLane ? EBranchKind::Lane : PickBranchKind(Rng);
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < 80 && !bPlaced; ++Attempt)
			{
				// Si un tipo no cabe en ningún sitio, a mitad de intentos se prueba otro.
				if (!bLane && Attempt == 40) { Kind = PickBranchKind(Rng); }
				const FBranchShape Shape = BranchShapeOf(Kind);
				const double SegLen = Rng.Range(Shape.LenMin, Shape.LenMax);
				const double S0 = Rng.Range(9000.0, FMath::Max(9001.0, Total - SegLen - 9000.0));
				int32 I0 = INDEX_NONE, I1 = INDEX_NONE;
				MainPointAt(L.Main, S0, nullptr, &I0);
				MainPointAt(L.Main, S0 + SegLen, nullptr, &I1);
				if (I0 == INDEX_NONE || I1 <= I0 + 8) { continue; }

				// Tramo principal elegible: sin estructuras, sin agua, pocos módulos.
				bool bEligible = true;
				TArray<int32> Modules;
				for (int32 i = I0; i <= I1 && bEligible; ++i)
				{
					const FPathSample& Sm = L.Main[i];
					if ((Sm.Flags & (PathFlags::Special & ~(PathFlags::Gap | PathFlags::Portal))) != 0) { bEligible = false; }
					if (IsWetBiome(Sm.Biome) || IsCrossingModule(L, Sm.Module)) { bEligible = false; }
					if ((Sm.Flags & PathFlags::Lane) != 0) { bEligible = false; }
					if (!Modules.Contains(Sm.Module)) { Modules.Add(Sm.Module); }
				}
				if (!bEligible || Modules.Num() > P.BranchMaxModules) { continue; }
				for (const FBranch& Other : L.Branches)
				{
					if (I0 <= Other.RejoinSample + 12 && I1 >= Other.ForkSample - 12) { bEligible = false; }
				}
				if (!bEligible) { continue; }

				const int32 Side = Rng.Chance(0.5) ? 1 : -1;
				const double Amp = Rng.Range(Shape.AmpMin, Shape.AmpMax);
				const double BranchW = Rng.Range(Shape.WMin, Shape.WMax);
				// Panza simétrica o cargada hacia la horquilla o hacia la unión.
				const double Skew = bLane ? 1.0 : Rng.Range(0.6, 1.6);

				TArray<FVector2D> Ctrl;
				const int32 NumCtrl = 36;
				for (int32 k = 0; k < NumCtrl; ++k)
				{
					const double T = static_cast<double>(k) / (NumCtrl - 1);
					FVector2D Dir;
					const FVector2D Base = MainPointAt(L.Main, L.Main[I0].S + T * (L.Main[I1].S - L.Main[I0].S), &Dir);
					const double Sin = FMath::Sin(Pi * FMath::Pow(T, Skew));
					const double Off = Amp * FMath::Pow(FMath::Max(0.0, Sin), 0.6) * (1.0 + 0.25 * Noise1(BSeed + static_cast<uint32>(b * 31 + Attempt), T * 4.0));
					Ctrl.Add(Base + LeftNormal(Dir) * (Off * Side));
				}
				Ctrl[0] = L.Main[I0].P;
				Ctrl.Last() = L.Main[I1].P;
				TArray<FVector2D> Pts = ResamplePolyline(ChaikinSmooth(Ctrl, 2), P.SampleSpacing);

				// Validación: sin pliegues (la curva desplazada se dobla en las curvas cerradas del
				// principal), separada del principal, dentro del mapa, lejos de estructuras y otras ramas.
				const double BranchLen = PolylineLength(Pts);
				const double EndZone = FMath::Min(5500.0, 0.3 * BranchLen);
				for (int32 k = 2; k < Pts.Num() && bEligible; ++k)
				{
					const FVector2D D0 = (Pts[k - 1] - Pts[k - 2]).GetSafeNormal();
					const FVector2D D1 = (Pts[k] - Pts[k - 1]).GetSafeNormal();
					if (FVector2D::DotProduct(D0, D1) < 0.64) { bEligible = false; }
				}
				for (int32 i = 0; i < Pts.Num() && bEligible; i += 2)
				{
					for (int32 j = i + 2; j < Pts.Num(); j += 2)
					{
						if ((j - i) * P.SampleSpacing < 6000.0) { continue; }
						if (FVector2D::DistSquared(Pts[i], Pts[j]) < 3000.0 * 3000.0) { bEligible = false; break; }
					}
				}
				double Acc = 0.0;
				for (int32 k = 0; k < Pts.Num() && bEligible; ++k)
				{
					if (k > 0) { Acc += FVector2D::Distance(Pts[k - 1], Pts[k]); }
					if (Acc < 2500.0 || BranchLen - Acc < 2500.0) { continue; }
					const FVector2D& Pt = Pts[k];
					if (Pt.X < P.MapEdgeClearance * 0.7 || Pt.X > L.WorldSize - P.MapEdgeClearance * 0.7 || Pt.Y < P.MapEdgeClearance * 0.7 || Pt.Y > L.CoastY(Pt.X) - 6000.0)
					{
						bEligible = false; break;
					}
					const int32 Mod = L.ModuleAt(Pt);
					if (Mod == INDEX_NONE || IsCrossingModule(L, Mod) || IsWetBiome(L.Modules[Mod].Biome)) { bEligible = false; break; }
					if (L.Modules[Mod].VisitCount == 0 && L.Modules[Mod].EmptyKind == ETNProcEmptyModuleMode::Elevated) { bEligible = false; break; }
					double DMain = 0.0;
					const int32 Near = Grid.Nearest(Pt, 20000.0, DMain);
					if (Near != INDEX_NONE)
					{
						// Permitido cerca del principal solo en su propio tramo de horquilla/unión.
						const bool bOwnSegment = Near >= I0 - 4 && Near <= I1 + 4;
						const double Need = L.Main[Near].Width * 0.5 + BranchW * 0.5 + 1600.0;
						if (DMain < Need && !(bOwnSegment && (Acc < EndZone || BranchLen - Acc < EndZone))) { bEligible = false; break; }
					}
					for (const FVector2D& T : Towers) { if (FVector2D::Distance(T, Pt) < P.TowerRadius + 3500.0) { bEligible = false; break; } }
					if (FVector2D::Distance(L.StartPoint, Pt) < P.StartClearingRadius + 3000.0) { bEligible = false; break; }
					for (const FBranch& Other : L.Branches)
					{
						for (const FPathSample& Os : Other.Samples)
						{
							if (FVector2D::DistSquared(Os.P, Pt) < 5500.0 * 5500.0) { bEligible = false; break; }
						}
						if (!bEligible) { break; }
					}
				}
				if (!bEligible) { continue; }

				FBranch Br;
				Br.ForkSample = I0;
				Br.RejoinSample = I1;
				Br.Side = Side;
				Br.Kind = Kind;
				for (int32 k = 0; k < Pts.Num(); ++k)
				{
					FPathSample Sm;
					Sm.P = Pts[k];
					Sm.Module = L.ModuleAt(Pts[k]);
					if (Sm.Module == INDEX_NONE) { Sm.Module = L.Main[I0].Module; }
					Sm.Biome = L.Modules[Sm.Module].Biome;
					Sm.Step = L.Main[I0].Step;
					Sm.Width = BranchW * (bLane ? 1.0 : (0.85 + 0.3 * (0.5 + 0.5 * Noise1(BSeed + 7u, static_cast<double>(k) / 20.0))));
					if (bLane) { Sm.Flags |= PathFlags::Lane; }
					Br.Samples.Add(Sm);
				}
				FinalizeSamples(Br.Samples);

				// Alturas: se une en ambos extremos a la cota del principal. La ruta alta sube con
				// pendiente suave desde que sale del cauce principal, sigue por lo alto y baja en
				// tobogán hasta antes de volver a tocarlo (dentro del cauce manda el suelo del
				// principal: subir o bajar ahí dejaría escalones); las demás ondulan.
				const double Z0 = L.Main[I0].Z;
				const double Z1 = L.Main[I1].Z;
				const double BLen = FMath::Max(1.0, Br.Samples.Last().S);
				const bool bHigh = Kind == EBranchKind::High;
				const double Peak = bHigh ? Rng.Range(1000.0, 1800.0) : 0.0;
				const double Climb = P.MaxPathSlope * 0.7;
				double SClear0 = BLen, SClear1 = 0.0;
				if (bHigh)
				{
					for (const FPathSample& Sm : Br.Samples)
					{
						double DMain = 0.0;
						const int32 Near = Grid.Nearest(Sm.P, 20000.0, DMain);
						const double Sep = Near == INDEX_NONE ? 1e9 : DMain - L.Main[Near].Width * 0.5;
						if (Sep > Sm.Width * 0.5 + 600.0) { SClear0 = FMath::Min(SClear0, Sm.S); SClear1 = FMath::Max(SClear1, Sm.S); }
					}
				}
				TArray<double> Z;
				for (const FPathSample& Sm : Br.Samples)
				{
					const double T = Sm.S / BLen;
					const double Base = LerpD(Z0, Z1, T);
					if (bHigh)
					{
						const double Up = Sm.S - SClear0;
						const double Down = SClear1 - Sm.S;
						Z.Add(Base + (Up > 0.0 && Down > 0.0 ? FMath::Min3(Peak, Climb * Up, TanSlide * Down) : 0.0));
					}
					else { Z.Add(Base + Shape.Wave * Fbm1(BSeed + 13u, Sm.S / 12000.0, 2) * FMath::Sin(Pi * T)); }
				}
				if (!bHigh) { Z = SmoothScalars(Z, 6); }
				Z[0] = Z0;
				Z.Last() = Z1;
				bool bSlopeOk = true;
				double MaxRise = 0.0;
				for (int32 k = 1; k < Z.Num(); ++k)
				{
					const double Ds = FMath::Max(1.0, Br.Samples[k].S - Br.Samples[k - 1].S);
					const double Slope = (Z[k] - Z[k - 1]) / Ds;
					// La ruta alta baja en tobogán: solo su subida tiene que ser una rampa andable.
					if (bHigh ? Slope > P.MaxPathSlope * 1.2 : FMath::Abs(Slope) > P.MaxPathSlope * 1.6) { bSlopeOk = false; break; }
					MaxRise = FMath::Max(MaxRise, Z[k] - LerpD(Z0, Z1, Br.Samples[k].S / BLen));
				}
				// Si no llega a subir de verdad no es ruta alta (el tramo es demasiado corto).
				if (!bSlopeOk || (bHigh && MaxRise < 700.0)) { continue; }
				for (int32 k = 0; k < Z.Num(); ++k)
				{
					Br.Samples[k].Z = Z[k];
					if (bHigh && k > 0 && (Z[k - 1] - Z[k]) / FMath::Max(1.0, Br.Samples[k].S - Br.Samples[k - 1].S) > P.MaxPathSlope * 1.5)
					{
						Br.Samples[k].Flags |= PathFlags::Slide;
					}
				}

				for (const FPathSample& Sm : Br.Samples) { L.Modules[Sm.Module].bHasBranch = true; }
				if (bLane)
				{
					for (int32 i = I0; i <= I1; ++i) { L.Main[i].Flags |= PathFlags::Lane; L.Main[i].Width = FMath::Min(L.Main[i].Width, 900.0); }
				}
				L.Branches.Add(Br);
				bPlaced = true;
			}
		}
	}
}
