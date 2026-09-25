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

	/** Anchura variable: holgada en claros, estrecha en pasos; respeta la holgura del módulo. */
	inline void ComputeWidths(FLayout& L, FRng Rng)
	{
		const FGenParams& P = L.Params;
		const uint32 WSeed = P.Seed ^ 0xA11CEu;
		TArray<double> W;
		W.SetNum(L.Main.Num());
		for (int32 i = 0; i < L.Main.Num(); ++i)
		{
			const FPathSample& Sm = L.Main[i];
			const double Base = 0.5 + 0.5 * Fbm1(WSeed, Sm.S / 15000.0, 3);
			double Wi = LerpD(P.PathWidthMin * 1.6, P.PathWidthMax, FMath::Pow(Base, 1.5));
			const double Narrow = Noise1(WSeed + 99u, Sm.S / 9000.0);
			const double NarrowStart = 1.0 - 2.0 * P.NarrowChance;
			if (Narrow > NarrowStart)
			{
				const double T = SmoothStep(NarrowStart, NarrowStart + 0.18, Narrow);
				Wi = LerpD(Wi, P.PathWidthMin, T);
			}
			const double Bd = L.BorderDistAt(Sm.P);
			Wi = FMath::Min(Wi, FMath::Max(P.PathWidthMin, 2.0 * (Bd - 1800.0)));
			W[i] = FMath::Max(P.PathWidthMin, Wi);
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
		TArray<double> Smoothed = SmoothScalars(W, 4);
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

			/** Muestra más cercana dentro de Radius (INDEX_NONE si ninguna). */
			int32 Nearest(const FVector2D& P, double Radius, double& OutDist) const
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
							const double D = FVector2D::Distance(P, (*Samples)[Idx].P);
							if (D < OutDist) { OutDist = D; Best = Idx; }
						}
					}
				}
				return OutDist <= Radius ? Best : INDEX_NONE;
			}
		};

		inline bool IsCrossingModule(const FLayout& L, int32 Module)
		{
			for (const FCrossing& C : L.Crossings) { if (C.Module == Module) { return true; } }
			return false;
		}
	}

	/** Bifurcaciones que se separan y vuelven a unirse más adelante (1..BranchMaxModules módulos). */
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
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < 60 && !bPlaced; ++Attempt)
			{
				const double SegLen = bLane ? Rng.Range(12000.0, 20000.0) : Rng.Range(15000.0, 50000.0);
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
				const double Amp = bLane ? Rng.Range(3800.0, 5200.0) : Rng.Range(4500.0, 11000.0);
				const double BranchW = bLane ? Rng.Range(650.0, 850.0) : Rng.Range(450.0, 1100.0);

				TArray<FVector2D> Ctrl;
				const int32 NumCtrl = 36;
				for (int32 k = 0; k < NumCtrl; ++k)
				{
					const double T = static_cast<double>(k) / (NumCtrl - 1);
					FVector2D Dir;
					const FVector2D Base = MainPointAt(L.Main, L.Main[I0].S + T * (L.Main[I1].S - L.Main[I0].S), &Dir);
					const double Sin = FMath::Sin(Pi * T);
					const double Off = Amp * FMath::Pow(FMath::Max(0.0, Sin), 0.6) * (1.0 + 0.25 * Noise1(BSeed + static_cast<uint32>(b * 31 + Attempt), T * 4.0));
					Ctrl.Add(Base + LeftNormal(Dir) * (Off * Side));
				}
				Ctrl[0] = L.Main[I0].P;
				Ctrl.Last() = L.Main[I1].P;
				TArray<FVector2D> Pts = ResamplePolyline(ChaikinSmooth(Ctrl, 2), P.SampleSpacing);

				// Validación: separada del principal, dentro del mapa, lejos de estructuras y otras ramas.
				const double BranchLen = PolylineLength(Pts);
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
						if (DMain < Need && !(bOwnSegment && (Acc < 5500.0 || BranchLen - Acc < 5500.0))) { bEligible = false; break; }
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
				Br.Kind = bLane ? EBranchKind::Lane : (Rng.Chance(0.5) ? EBranchKind::Risky : EBranchKind::Scenic);
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

				// Alturas: se une en ambos extremos a la cota del principal, ondulando en medio.
				const double Z0 = L.Main[I0].Z;
				const double Z1 = L.Main[I1].Z;
				const double BLen = Br.Samples.Last().S;
				TArray<double> Z;
				for (const FPathSample& Sm : Br.Samples)
				{
					const double T = BLen > 0.0 ? Sm.S / BLen : 0.0;
					Z.Add(LerpD(Z0, Z1, T) + 280.0 * Fbm1(BSeed + 13u, Sm.S / 12000.0, 2) * FMath::Sin(Pi * T));
				}
				Z = SmoothScalars(Z, 6);
				Z[0] = Z0;
				Z.Last() = Z1;
				bool bSlopeOk = true;
				for (int32 k = 1; k < Z.Num(); ++k)
				{
					const double Ds = FMath::Max(1.0, Br.Samples[k].S - Br.Samples[k - 1].S);
					if (FMath::Abs(Z[k] - Z[k - 1]) / Ds > P.MaxPathSlope * 1.6) { bSlopeOk = false; break; }
				}
				if (!bSlopeOk) { continue; }
				for (int32 k = 0; k < Z.Num(); ++k) { Br.Samples[k].Z = Z[k]; }

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
