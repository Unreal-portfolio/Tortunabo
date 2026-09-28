#pragma once

#include "TN_BeachPropMeshes.h"

/**
 * Decorado militar de la playa (la tropa de Tortunavy; Docs/Modo_Carrera.md, «Decorado militar»): parapetos de sacos
 * terreros, cajas de munición, erizos antitanque, cascos, redes de camuflaje, bidones y soldaditos de juguete. Son
 * recetas de ATN_BeachDecor como las de TN_BeachPropMeshes.h, que incluye este archivo al final y le pasa los elementos
 * militares en BuildDecor (NumMilitaryVariants y BuildMilitaryDecor): low-poly de caras planas con color de vértice, a
 * TNBeach::Scale y dentro de la huella del contrato. La paleta es la de la tienda del General Galápago del cuartel
 * (verde oliva, caqui, azul marino y oro) y la escarapela de Tortunavy es una estrella dorada de cinco puntas sobre un
 * disco azul marino con un aro dorado.
 *
 * Alturas pensadas para la tortuga (1,4 m, salta 1,2 m): los escalones no pasan de 80 cm con SizeScale 1 y lo que sirve
 * para cubrirse ronda 1,1-1,2 m.
 */
namespace TNBeachProp
{
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Paleta y escarapela
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Colores del decorado militar. */
	struct FMilLook
	{
		FLinearColor Olive;
		FLinearColor OliveDark;
		FLinearColor OliveLight;
		FLinearColor Khaki;
		FLinearColor KhakiDark;
		FLinearColor Navy;
		FLinearColor Gold;
		FLinearColor Cream;
		FLinearColor Stencil;
		FLinearColor Rope;
		FLinearColor Steel;
		FLinearColor Brass;
		FLinearColor Copper;
		FLinearColor Sand;
		FLinearColor SandDark;
		FLinearColor PoleWood;
		FLinearColor Drift;
	};

	inline FMilLook MilLookOf()
	{
		FMilLook L;
		L.Olive = Hex(0x5E6B38, 0.05f);
		L.OliveDark = Hex(0x46512A, 0.05f);
		L.OliveLight = Hex(0x7A8650, 0.05f);
		L.Khaki = Hex(0xC8B98A);
		L.KhakiDark = Hex(0xA6966A);
		L.Navy = Hex(0x12305A, 0.1f);
		L.Gold = Hex(0xFFCB3D, 0.6f);
		L.Cream = Hex(0xF2E8CC);
		L.Stencil = Hex(0xE8D27A);
		L.Rope = Hex(0xD8C8A0);
		L.Steel = Hex(0x767C82, 0.6f);
		L.Brass = Hex(0xD9A441, 0.8f);
		L.Copper = Hex(0xB8703A, 0.7f);
		L.Sand = Hex(0xE6CC94);
		L.SandDark = Hex(0xCBA66A);
		L.PoleWood = Hex(0x8C7355);
		L.Drift = Hex(0xB9B2A6);
		return L;
	}

	/** Ejes de una cara de normal Normal: OutU hacia Up (proyectado en la cara) y OutS a su lado. */
	inline void MilFaceAxes(const FVector& Normal, const FVector& Up, FVector& OutN, FVector& OutU, FVector& OutS)
	{
		OutN = Normal.GetSafeNormal();
		OutU = (Up - OutN * FVector::DotProduct(Up, OutN)).GetSafeNormal();
		if (OutU.IsNearlyZero())
		{
			OutU = FVector::CrossProduct(OutN, FMath::Abs(OutN.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		}
		OutS = FVector::CrossProduct(OutN, OutU);
	}

	/** Estrella de cinco puntas plana (radio R, dentro 0,4 R) en Center, de cara a Normal y con una punta hacia Up. */
	inline void AddMilStar(FTNProcMeshBuffers& M, const FVector& Center, const FVector& Normal, const FVector& Up, double R, const FLinearColor& Color)
	{
		FVector Nn;
		FVector Uu;
		FVector Ss;
		MilFaceAxes(Normal, Up, Nn, Uu, Ss);
		for (int32 k = 0; k < 10; ++k)
		{
			const double A0 = UE_DOUBLE_TWO_PI * k / 10.0;
			const double A1 = UE_DOUBLE_TWO_PI * (k + 1) / 10.0;
			const double R0 = (k % 2) ? R * 0.4 : R;
			const double R1 = (k % 2) ? R : R * 0.4;
			M.AddTri(Center, Center + (Uu * FMath::Cos(A0) + Ss * FMath::Sin(A0)) * R0, Center + (Uu * FMath::Cos(A1) + Ss * FMath::Sin(A1)) * R1, Nn,
				(k % 2) ? Color : Shade(Color, 0.88f));
		}
	}

	/** Escarapela de Tortunavy de radio R pegada a una cara (Normal hacia fuera): disco azul marino, aro y estrella dorados. */
	inline void AddMilRoundel(FTNProcMeshBuffers& M, const FVector& Center, const FVector& Normal, const FVector& Up, double R, const FMilLook& Look)
	{
		FVector Nn;
		FVector Uu;
		FVector Ss;
		MilFaceAxes(Normal, Up, Nn, Uu, Ss);
		constexpr int32 DiscSeg = 12;
		const FVector Disc = Center + Nn * 1.5;
		for (int32 k = 0; k < DiscSeg; ++k)
		{
			const double A0 = UE_DOUBLE_TWO_PI * k / DiscSeg;
			const double A1 = UE_DOUBLE_TWO_PI * (k + 1) / DiscSeg;
			const FVector E0 = Uu * FMath::Cos(A0) + Ss * FMath::Sin(A0);
			const FVector E1 = Uu * FMath::Cos(A1) + Ss * FMath::Sin(A1);
			M.AddTri(Disc, Disc + E0 * (R * 0.86), Disc + E1 * (R * 0.86), Nn, Look.Navy);
			M.AddQuad(Disc + E0 * (R * 0.86), Disc + E1 * (R * 0.86), Disc + E1 * R, Disc + E0 * R, Nn, Look.Gold);
		}
		AddMilStar(M, Center + Nn * 3.0, Nn, Uu, R * 0.7, Look.Gold);
	}

	/** Mástil con la bandera de Tortunavy (azul marino con la estrella dorada por las dos caras): la tela ondea (parte animada). */
	inline void AddMilFlag(FParts& P, const FVector& Base, double PoleH, double FlagW, double FlagH, const FMilLook& Look)
	{
		const FVector Top = Base + FVector(0.0, 0.0, PoleH);
		AddTube(P.Body, { Base - FVector(0.0, 0.0, 40.0), Top }, 7.0, 6, Look.PoleWood);
		AddBlob(P.Body, FBeachFrame(Top + FVector(0.0, 0.0, 7.0), FQuat::Identity), 10.0, 10.0, 10.0, 6, 3, Look.Gold);
		P.ColCapsule(Base, Top - FVector(0.0, 0.0, 8.0), 8.0);
		const FVector FlagTop = Top - FVector(0.0, 0.0, 10.0);
		auto FlagAt = [&FlagTop, FlagW, FlagH](double U, double V)
		{
			const double Wave = 16.0 * FMath::Sin(UE_DOUBLE_PI * U * 1.5) * U;
			return FlagTop + FVector(FlagW * U, Wave, -FlagH * (1.0 - V) - FlagH * 0.12 * U);
		};
		AddSheet(P.Moving, 5, 2, FlagAt, 4.0, [&Look](int32 I, int32) { return (I % 2) ? Look.Navy : Shade(Look.Navy, 0.9f); });
		const FVector StarC = FlagAt(0.36, 0.5);
		for (const double Sy : { -1.0, 1.0 })
		{
			AddMilStar(P.Moving, StarC + FVector(0.0, Sy * 8.0, 0.0), FVector(0.0, Sy, 0.0), FVector::UpVector, FlagH * 0.3, Look.Gold);
		}
		P.Info.Anim = EAnim::Flutter;
		P.Info.AnimAxis = FVector::UpVector;
		P.Info.AnimAmp = 14.f;
		P.Info.AnimRate = 1.1f;
		P.AnimAround(FlagTop);
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Sacos terreros y parapetos
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Medidas de un saco (cm de juego con SizeScale 1): 4,3 × 2,4 × 1,2 cm reales, los de la tropa de Tortunavy. */
	namespace MilBag
	{
		constexpr double Len = 120.0;
		constexpr double Wid = 66.0;
		constexpr double H = 34.0;
		/** Lo que sube cada hilera (los sacos se aplastan unos sobre otros): 4 hileras = 1,2 m y la banqueta de 2 = 63 cm. */
		constexpr double RowStep = 28.5;
		/** Lo que se acorta cada hilera por cada extremo en escalera (se sube por los extremos andando). */
		constexpr double EndStep = 72.0;
	}

	/** Color de un saco: arpillera clara u oscura, lona verde o empapado de arena. */
	inline FLinearColor MilBagColor(uint32 BagSeed)
	{
		static const uint32 BagHex[6] = { 0xC8B98A, 0xB9A77A, 0xA8986A, 0xCDBE92, 0x8E8F5A, 0x7C8450 };
		return Hex(BagHex[FMath::Min(5, static_cast<int32>(Rnd(BagSeed, 1) * 6.0))]);
	}

	/** Saco terrero: almohada de seis lados (las puntas atadas en ±X) de Len × Wid × H con la base en Base, girada Rot. */
	inline void AddMilSandbag(FTNProcMeshBuffers& M, const FVector& Base, const FQuat& Rot, double Len, double Wid, double H, const FLinearColor& Color, uint32 BagSeed)
	{
		const double R = Len * 0.5;
		const TArray<FVector2D> Prof = { FVector2D(0.0, -4.0), FVector2D(R * 0.84, -2.0), FVector2D(R, H * 0.42), FVector2D(R * 0.82, H * 0.9), FVector2D(0.0, H) };
		AddRevolve(M, FBeachFrame(Base, Rot), Prof, 6, [&Color](int32 Ring, int32 Side) -> FLinearColor
		{
			if (Ring == 0) { return Shade(Color, 0.7f); }
			if (Ring == 3) { return Shade(Color, 1.07f); }
			return (Side == 1 || Side == 4) ? Color : Shade(Color, 0.88f);
		}, 0.07, BagSeed, Wid / (0.866 * Len));
	}

	/** Camino en planta de un parapeto: recto de A a B o arco (centro, radio y ángulos en grados, de Ang0 a Ang1). */
	struct FMilPath
	{
		bool bArc = false;
		FVector2D A = FVector2D::ZeroVector;
		FVector2D B = FVector2D::ZeroVector;
		FVector2D Center = FVector2D::ZeroVector;
		double Radius = 500.0;
		double Ang0 = 0.0;
		double Ang1 = 180.0;

		double Length() const
		{
			return bArc ? Radius * FMath::DegreesToRadians(FMath::Abs(Ang1 - Ang0)) : FVector2D::Distance(A, B);
		}

		/** Largo de una línea corrida Side cm a la izquierda del camino, por cada cm del camino (en recto, 1). */
		double LineFactor(double Side) const
		{
			if (!bArc) { return 1.0; }
			const double Sign = Ang1 >= Ang0 ? 1.0 : -1.0;
			return FMath::Max(0.1, (Radius - Sign * Side) / FMath::Max(1.0, Radius));
		}

		/** Punto a S cm del principio, corrido Side cm a la izquierda de la marcha, y el rumbo (grados) en OutYawDeg. */
		FVector2D At(double S, double Side, double& OutYawDeg) const
		{
			if (!bArc)
			{
				const FVector2D Dir = (B - A).GetSafeNormal();
				OutYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
				return A + Dir * S + FVector2D(-Dir.Y, Dir.X) * Side;
			}
			const double Sign = Ang1 >= Ang0 ? 1.0 : -1.0;
			const double AngDeg = Ang0 + Sign * FMath::RadiansToDegrees(S / FMath::Max(1.0, Radius));
			const double Ang = FMath::DegreesToRadians(AngDeg);
			OutYawDeg = AngDeg + Sign * 90.0;
			// La izquierda de la marcha es el centro si el arco gira a la izquierda.
			return Center + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Radius - Sign * Side);
		}
	};

	/** Un parapeto de sacos: hileras, banqueta de tiro, extremos en escalera y un tramo derrumbado. */
	struct FMilParapet
	{
		FMilPath Path;
		int32 Rows = 4;
		/** Hileras de la banqueta (0 = sin) y a qué lado del camino (+1 izquierda de la marcha, -1 derecha). */
		int32 StepRows = 2;
		double StepSide = 1.0;
		/** Tramo derrumbado en fracciones del largo (Gap1 <= Gap0: entero): ahí solo queda la hilera de abajo. */
		double Gap0 = 0.0;
		double Gap1 = 0.0;
		/** Los extremos bajan en escalera (se acorta cada hilera); en una esquina, el extremo que la toca no. */
		bool bStepStart = true;
		bool bStepEnd = true;
	};

	/** Hileras que hay a S cm del principio de una línea del parapeto (extremos en escalera y tramo derrumbado). */
	inline int32 MilRowsAt(const FMilParapet& W, int32 Rows, double S, double Len)
	{
		int32 Count = 0;
		for (int32 r = 0; r < Rows; ++r)
		{
			const double In0 = W.bStepStart ? r * MilBag::EndStep : 0.0;
			const double In1 = W.bStepEnd ? r * MilBag::EndStep : 0.0;
			if (S < In0 || S > Len - In1) { break; }
			if (r >= 1 && W.Gap1 > W.Gap0 && S > W.Gap0 * Len && S < W.Gap1 * Len) { break; }
			++Count;
		}
		return Count;
	}

	/**
	 * Parapeto de sacos (a soga, trabados de una hilera a otra) a lo largo de W.Path, con su banqueta y la colisión: una
	 * caja por tramo de igual alto (en los arcos, de 20° como mucho). Por fuera, 1,2 m (cubre a una tortuga agachada);
	 * por dentro, la banqueta de 63 cm hace de escalón; por los extremos se sube de hilera en hilera (28 cm).
	 */
	inline void AddMilParapet(FParts& P, const FMilParapet& W, uint32 WallSeed)
	{
		const double Len = W.Path.Length();
		if (Len < MilBag::Len) { return; }
		for (int32 Line = 0; Line < 2; ++Line)
		{
			const int32 Rows = Line == 0 ? W.Rows : W.StepRows;
			if (Rows <= 0) { continue; }
			const double Side = Line == 0 ? 0.0 : W.StepSide * MilBag::Wid * 0.92;
			const double Factor = W.Path.LineFactor(Side);
			for (int32 r = 0; r < Rows; ++r)
			{
				const double S0 = W.bStepStart ? r * MilBag::EndStep : 0.0;
				const double S1 = Len - (W.bStepEnd ? r * MilBag::EndStep : 0.0);
				if (S1 - S0 < MilBag::Len * 0.5) { break; }
				const int32 Count = FMath::Max(1, FMath::RoundToInt32((S1 - S0) * Factor / MilBag::Len));
				const double Pitch = (S1 - S0) / Count;
				for (int32 b = 0; b < Count; ++b)
				{
					const double S = S0 + Pitch * (b + 0.5);
					if (r >= 1 && W.Gap1 > W.Gap0 && S > W.Gap0 * Len && S < W.Gap1 * Len) { continue; }
					const uint32 BagSeed = HashMix(WallSeed + static_cast<uint32>(Line) * 7919u + static_cast<uint32>(r) * 131u, static_cast<uint32>(b));
					double Yaw = 0.0;
					const FVector2D At = W.Path.At(S, Side + RndIn(BagSeed, 2, -5.0, 5.0), Yaw);
					const FQuat Rot = YawQ(Yaw + RndIn(BagSeed, 3, -5.0, 5.0)) * FQuat(FVector(1.0, 0.0, 0.0), FMath::DegreesToRadians(RndIn(BagSeed, 4, -3.0, 3.0)));
					const double Grow = RndIn(BagSeed, 5, 0.94, 1.06);
					AddMilSandbag(P.Body, FVector(At.X, At.Y, r * MilBag::RowStep + RndIn(BagSeed, 6, -3.0, 2.0)), Rot, Pitch * Factor * 1.05 * Grow,
						MilBag::Wid * Grow, MilBag::H, MilBagColor(BagSeed), BagSeed);
				}
			}
			// Colisión por tramos de igual alto.
			const int32 Chunks = FMath::Max(1, FMath::CeilToInt32(Len / MilBag::EndStep));
			const double ChunkLen = Len / Chunks;
			int32 RunStart = 0;
			int32 RunRows = MilRowsAt(W, Rows, ChunkLen * 0.5, Len);
			for (int32 c = 1; c <= Chunks; ++c)
			{
				const int32 Here = c < Chunks ? MilRowsAt(W, Rows, ChunkLen * (c + 0.5), Len) : -1;
				const bool bTooBent = W.Path.bArc && FMath::RadiansToDegrees((c - RunStart) * ChunkLen / FMath::Max(1.0, W.Path.Radius)) > 20.0;
				if (Here == RunRows && !bTooBent) { continue; }
				if (RunRows > 0)
				{
					const double Sa = RunStart * ChunkLen;
					const double Sb = c * ChunkLen;
					double YawA = 0.0;
					double YawB = 0.0;
					double YawM = 0.0;
					const FVector2D Pa = W.Path.At(Sa, Side, YawA);
					const FVector2D Pb = W.Path.At(Sb, Side, YawB);
					const FVector2D Pm = W.Path.At((Sa + Sb) * 0.5, Side, YawM);
					const double Top = (RunRows - 1) * MilBag::RowStep + MilBag::H - 6.0;
					// Entre la cuerda y el arco (en recto coinciden).
					const FVector2D Cen = ((Pa + Pb) * 0.5 + Pm) * 0.5;
					P.ColBoxYaw(FVector(Cen.X, Cen.Y, (Top - 20.0) * 0.5), YawM, FVector(FVector2D::Distance(Pa, Pb) * 0.5 + 4.0, MilBag::Wid * 0.44, (Top + 20.0) * 0.5));
				}
				RunStart = c;
				RunRows = Here;
			}
		}
	}

	/** Sacos caídos de un derrumbe a los dos lados de Around (en planta, rumbo FacingYaw), unos sobre otros; con colisión. */
	inline void AddMilFallenBags(FParts& P, const FVector2D& Around, double FacingYaw, int32 Count, uint32 PileSeed, const FMilLook& Look)
	{
		for (int32 b = 0; b < Count; ++b)
		{
			const uint32 BagSeed = HashMix(PileSeed, 40u + static_cast<uint32>(b));
			const double Side = (b % 2) ? 1.0 : -1.0;
			const FVector2D Off(RndIn(BagSeed, 1, -95.0, 95.0), Side * RndIn(BagSeed, 2, 55.0, 150.0));
			const FVector2D At = Around + Off.GetRotated(FacingYaw);
			const double Z = b >= 4 ? MilBag::H * 0.7 : 0.0;
			const FQuat Rot = YawQ(RndIn(BagSeed, 3, 0.0, 360.0)) * FQuat(FVector(1.0, 0.0, 0.0), FMath::DegreesToRadians(RndIn(BagSeed, 4, -16.0, 16.0)));
			const FVector Base(At.X, At.Y, Z - 4.0);
			AddMilSandbag(P.Body, Base, Rot, MilBag::Len, MilBag::Wid, MilBag::H * 0.9, MilBagColor(BagSeed), BagSeed);
			P.ColBox(Base + Rot.RotateVector(FVector(0.0, 0.0, MilBag::H * 0.4)), Rot, FVector(MilBag::Len * 0.42, MilBag::Wid * 0.4, MilBag::H * 0.42));
		}
		// Un saco reventado y aplastado con su montoncito de arena.
		const FVector2D Burst = Around + FVector2D(RndIn(PileSeed, 90, -40.0, 40.0), 190.0).GetRotated(FacingYaw);
		AddMilSandbag(P.Body, FVector(Burst.X, Burst.Y, -2.0), YawQ(FacingYaw + 70.0), MilBag::Len * 1.05, MilBag::Wid * 1.2, MilBag::H * 0.45, Hex(0xB9A77A), PileSeed + 3u);
		AddRevolve(P.Body, FBeachFrame(FVector(Burst.X, Burst.Y, 0.0) + FVector(40.0, 0.0, 0.0), FQuat::Identity), CapProfile(80.0, 26.0, 3, 10.0), 8, Look.Sand, 0.18, PileSeed);
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Cajas, latas y cartuchos de munición
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Caja de munición grande (cm de juego con SizeScale 1): 5,4 × 3,1 × 2,7 cm reales, un escalón de 75 cm. */
	namespace MilCrate
	{
		constexpr double Len = 152.0;
		constexpr double Wid = 88.0;
		constexpr double H = 75.0;
		constexpr double Wall = 8.0;
	}

	/**
	 * Caja de munición de madera pintada de verde oliva (Len × Wid × H) con la base en Base y girada por Rot: tapa algo más
	 * ancha (o abierta, sin tapa), listones en las cabeceras, ranuras entre tablas, asas de cuerda, cierres, franja
	 * amarilla y la escarapela de Tortunavy en los costados largos (y en la tapa con bTopRoundel). Colisión: la caja (abierta,
	 * sus cuatro paredes y el fondo).
	 */
	inline void AddMilCrate(FParts& P, const FVector& Base, const FQuat& Rot, double Len, double Wid, double H, bool bLid, bool bTopRoundel, uint32 CrateSeed,
		const FMilLook& Look, bool bCollide = true)
	{
		const FBeachFrame F(Base, Rot);
		const double Hx = Len * 0.5;
		const double Hy = Wid * 0.5;
		const FLinearColor Paint = Blend(Look.Olive, Look.OliveLight, static_cast<float>(Rnd(CrateSeed, 1) * 0.6));
		const FLinearColor Dark = Look.OliveDark;
		auto AddPart = [&P, &F, &Rot, bCollide](const FVector& LocalC, const FVector& Half, const FLinearColor& Col, bool bSolid)
		{
			AddOBox(P.Body, F.P(LocalC), Rot, Half, Col);
			if (bSolid && bCollide) { P.ColBox(F.P(LocalC), Rot, Half); }
		};
		if (bLid)
		{
			AddPart(FVector(0.0, 0.0, (H - 6.0) * 0.5), FVector(Hx, Hy, (H - 6.0) * 0.5), Paint, true);
			AddPart(FVector(0.0, 0.0, H - 4.0), FVector(Hx + 3.0, Hy + 3.0, 4.0), Shade(Paint, 1.06f), true);
			// Cierres de metal en la cara de delante (+Y).
			for (const double Sx : { -0.3, 0.3 })
			{
				AddPart(FVector(Sx * Len, Hy + 2.0, H - 12.0), FVector(6.0, 2.0, 9.0), Look.Steel, false);
			}
		}
		else
		{
			const double T = MilCrate::Wall;
			AddPart(FVector(0.0, 0.0, 4.0), FVector(Hx, Hy, 4.0), Dark, true);
			AddPart(FVector(Hx - T * 0.5, 0.0, H * 0.5), FVector(T * 0.5, Hy, H * 0.5), Paint, true);
			AddPart(FVector(-Hx + T * 0.5, 0.0, H * 0.5), FVector(T * 0.5, Hy, H * 0.5), Paint, true);
			AddPart(FVector(0.0, Hy - T * 0.5, H * 0.5), FVector(Hx - T, T * 0.5, H * 0.5), Paint, true);
			AddPart(FVector(0.0, -Hy + T * 0.5, H * 0.5), FVector(Hx - T, T * 0.5, H * 0.5), Paint, true);
			// Madera sin pintar en el canto de las paredes.
			AddPart(FVector(0.0, 0.0, H + 0.8), FVector(Hx - 1.0, Hy - 1.0, 0.8), Hex(0xB08A58), false);
		}
		for (const double Sx : { -1.0, 1.0 })
		{
			// Listones de las cabeceras por las dos caras largas.
			for (const double Sy : { -1.0, 1.0 })
			{
				AddPart(FVector(Sx * (Hx - 9.0), Sy * (Hy + 1.5), H * 0.5 - 2.0), FVector(7.0, 2.5, H * 0.5 - 5.0), Dark, false);
			}
			// Asa de cuerda en la cabecera.
			const FVector HandleA = F.P(FVector(Sx * (Hx + 1.0), -16.0, H * 0.64));
			const FVector HandleB = F.P(FVector(Sx * (Hx + 1.0), 16.0, H * 0.64));
			const FVector Out = F.D(FVector(Sx * 9.0, 0.0, -8.0));
			AddTube(P.Body, { HandleA, HandleA + Out, HandleB + Out, HandleB }, 3.2, 4, Look.Rope, false);
		}
		for (const double Sy : { -1.0, 1.0 })
		{
			for (const double Kz : { 0.36, 0.7 })
			{
				AddPart(FVector(0.0, Sy * (Hy + 0.8), H * Kz), FVector(Hx - 18.0, 1.0, 1.6), Dark, false);
			}
			// Franja amarilla de plantilla y escarapela.
			AddPart(FVector(-Hx * 0.42, Sy * (Hy + 1.2), H * 0.52), FVector(Hx * 0.3, 1.0, H * 0.07), Look.Stencil, false);
			AddMilRoundel(P.Body, F.P(FVector(Hx * 0.36, Sy * (Hy + 1.0), H * 0.48)), F.D(FVector(0.0, Sy, 0.0)), F.D(FVector::UpVector), H * 0.27, Look);
		}
		if (bLid && bTopRoundel)
		{
			AddMilRoundel(P.Body, F.P(FVector(0.0, 0.0, H + 0.2)), F.D(FVector::UpVector), F.D(FVector(1.0, 0.0, 0.0)), Wid * 0.3, Look);
		}
	}

	/** Lata de munición de metal (90 × 42 × 57) con la base en Base: tapa, asa, cierre y franja amarilla. */
	inline void AddMilAmmoCan(FParts& P, const FVector& Base, const FQuat& Rot, const FMilLook& Look, bool bCollide = true)
	{
		const FBeachFrame F(Base, Rot);
		const FLinearColor Metal = Hex(0x4F5A32, 0.35f);
		AddOBox(P.Body, F.P(FVector(0.0, 0.0, 26.0)), Rot, FVector(45.0, 21.0, 26.0), Metal);
		AddOBox(P.Body, F.P(FVector(0.0, 0.0, 54.0)), Rot, FVector(47.0, 23.0, 3.0), Shade(Metal, 1.1f));
		AddOBox(P.Body, F.P(FVector(46.0, 0.0, 40.0)), Rot, FVector(2.5, 10.0, 9.0), Look.Steel);
		AddTube(P.Body, { F.P(FVector(-18.0, 0.0, 57.0)), F.P(FVector(-12.0, 0.0, 68.0)), F.P(FVector(12.0, 0.0, 68.0)), F.P(FVector(18.0, 0.0, 57.0)) }, 2.6, 4, Look.Steel, false);
		for (const double Sy : { -1.0, 1.0 })
		{
			AddOBox(P.Body, F.P(FVector(-10.0, Sy * 21.8, 30.0)), Rot, FVector(26.0, 0.8, 3.5), Look.Stencil);
		}
		if (bCollide) { P.ColBox(F.P(FVector(0.0, 0.0, 28.5)), Rot, FVector(46.0, 22.0, 28.5)); }
	}

	/** Cartucho de cañón de juguete: vaina de latón y proyectil de cobre, de Len de largo y radio R, de Base hacia Dir. */
	inline void AddMilShell(FTNProcMeshBuffers& M, const FVector& Base, const FVector& Dir, double Len, double R, const FMilLook& Look)
	{
		const TArray<FVector2D> Prof = { FVector2D(0.0, 0.0), FVector2D(R * 1.08, 0.0), FVector2D(R * 1.08, Len * 0.04), FVector2D(R, Len * 0.05), FVector2D(R, Len * 0.6),
			FVector2D(R * 0.86, Len * 0.66), FVector2D(R * 0.84, Len * 0.7), FVector2D(R * 0.62, Len * 0.86), FVector2D(0.0, Len) };
		AddRevolve(M, FBeachFrame(Base, AxisZTo(Dir)), Prof, 6, [&Look](int32 Ring, int32) -> FLinearColor { return Ring >= 6 ? Look.Copper : Look.Brass; });
	}

	/** Cartucho tumbado en la arena en (X, Y), apuntando hacia YawDeg. */
	inline void AddMilLyingShell(FTNProcMeshBuffers& M, double X, double Y, double YawDeg, double Len, double R, const FMilLook& Look)
	{
		const FVector Dir = YawQ(YawDeg).RotateVector(FVector(1.0, 0.0, 0.0));
		AddMilShell(M, FVector(X, Y, R) - Dir * (Len * 0.5), Dir, Len, R, Look);
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Soldaditos de juguete
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Pose de un soldadito de juguete. */
	enum class EMilPose : uint8
	{
		/** De pie, apuntando con el fusil. */
		Rifle,
		/** De pie, mirando con los prismáticos. */
		Binoculars,
		/** Rodilla en tierra con el bazuca al hombro. */
		Bazooka,
		/** Tumbado con el fusil delante. */
		Prone
	};

	/** Fusil de juguete de From (culata) a To (boca), con Up hacia arriba: cuerpo, cañón y cargador. */
	inline void AddMilRifle(FTNProcMeshBuffers& M, const FVector& From, const FVector& To, const FVector& Up, const FLinearColor& Color)
	{
		const FVector Dir = (To - From).GetSafeNormal();
		const double Len = FVector::Dist(From, To);
		const FQuat Q = FRotationMatrix::MakeFromXZ(Dir, Up).ToQuat();
		AddOBox(M, From + Dir * (Len * 0.36), Q, FVector(Len * 0.36, 2.6, 4.2), Color);
		AddTube(M, { From + Dir * (Len * 0.7), To }, 1.7, 4, Color);
		AddOBox(M, From + Dir * (Len * 0.42) - Q.GetAxisZ() * 7.0, Q, FVector(3.0, 2.0, 5.5), Color);
	}

	/**
	 * Soldadito de juguete verde de 5 cm (1,4 m, como la tortuga) sobre su peana, en el marco F (peana en el suelo, mira a
	 * su +X): el plástico de una pieza con las sombras de las hendiduras. Colisión (si bCollide): de pie y de rodillas, una
	 * cápsula del cuerpo; tumbado, una caja baja (40 cm) que se sube.
	 */
	inline void AddMilSoldier(FParts& P, const FBeachFrame& F, EMilPose Pose, const FLinearColor& Plastic, uint32 ManSeed, bool bCollide = true)
	{
		const FLinearColor Dark = Shade(Plastic, 0.8f);
		const FLinearColor Light = Shade(Plastic, 1.1f);
		auto Limb = [&P, &F, &Plastic](const FVector& A, const FVector& B, const FVector& C, double R)
		{
			AddTaperTube(P.Body, { F.P(A), F.P(B), F.P(C) }, R, R * 0.85, 6, Plastic, true);
		};
		auto Lump = [&P, &F](const FVector& C, double Rx, double Ry, double Rz, const FLinearColor& Col)
		{
			AddBlob(P.Body, FBeachFrame(F.P(C), F.Q), Rx, Ry, Rz, 8, 4, Col);
		};
		auto Helmet = [&P, &F, &Plastic, &Dark](const FVector& C, const FQuat& Tilt)
		{
			AddRevolve(P.Body, FBeachFrame(F.P(C), F.Q * Tilt), { FVector2D(14.5, -1.5), FVector2D(14.5, 1.0), FVector2D(12.0, 3.0), FVector2D(10.5, 9.5), FVector2D(6.0, 14.0),
				FVector2D(0.0, 15.0) }, 8, [&Plastic, &Dark](int32 Ring, int32) -> FLinearColor { return Ring <= 1 ? Dark : Plastic; });
		};
		const FVector Up = F.D(FVector::UpVector);
		switch (Pose)
		{
		case EMilPose::Prone:
		{
			AddPrismPoly(P.Body, F, Stadium(62.0, 24.0, 4), 0.0, 6.0, Light, Dark);
			Lump(FVector(6.0, 0.0, 22.0), 24.0, 16.0, 11.0, Plastic);
			Lump(FVector(-16.0, 0.0, 20.0), 6.0, 15.0, 10.0, Dark);
			AddOBox(P.Body, F.P(FVector(2.0, 0.0, 34.0)), F.Q, FVector(12.0, 11.0, 5.0), Dark);
			Limb(FVector(-18.0, 7.0, 18.0), FVector(-40.0, 11.0, 14.0), FVector(-60.0, 13.0, 12.0), 8.0);
			Limb(FVector(-18.0, -7.0, 18.0), FVector(-40.0, -11.0, 14.0), FVector(-60.0, -14.0, 12.0), 8.0);
			Lump(FVector(-64.0, 13.0, 12.0), 6.0, 7.0, 9.0, Dark);
			Lump(FVector(-64.0, -14.0, 12.0), 6.0, 7.0, 9.0, Dark);
			Lump(FVector(36.0, 0.0, 32.0), 10.0, 9.5, 10.0, Plastic);
			Helmet(FVector(35.0, 0.0, 35.0), FQuat(FVector(0.0, 1.0, 0.0), FMath::DegreesToRadians(18.0)));
			Limb(FVector(22.0, -15.0, 26.0), FVector(30.0, -22.0, 12.0), FVector(40.0, -8.0, 22.0), 5.5);
			Limb(FVector(22.0, 15.0, 26.0), FVector(44.0, 16.0, 12.0), FVector(58.0, -3.0, 22.0), 5.5);
			AddMilRifle(P.Body, F.P(FVector(26.0, -8.0, 24.0)), F.P(FVector(102.0, -4.0, 26.0)), Up, Plastic);
			if (bCollide) { P.ColBox(F.P(FVector(0.0, 0.0, 20.0)), F.Q, FVector(62.0, 24.0, 20.0)); }
			return;
		}
		case EMilPose::Bazooka:
		{
			// De rodillas: la parte de arriba del cuerpo va 26 cm más abajo que de pie.
			AddPrismPoly(P.Body, F, WobblyEllipse(34.0, 24.0, 8, 0.1, ManSeed), 0.0, 6.0, Light, Dark);
			Limb(FVector(0.0, 8.0, 46.0), FVector(20.0, 9.0, 44.0), FVector(22.0, 10.0, 8.0), 8.5);
			Limb(FVector(0.0, -8.0, 46.0), FVector(-2.0, -9.0, 12.0), FVector(-26.0, -10.0, 10.0), 8.5);
			Lump(FVector(26.0, 10.0, 9.0), 11.0, 7.0, 6.0, Dark);
			Lump(FVector(-30.0, -10.0, 10.0), 7.0, 7.0, 9.0, Dark);
			Lump(FVector(0.0, 0.0, 50.0), 11.0, 16.0, 6.0, Dark);
			Lump(FVector(0.0, 0.0, 69.0), 11.5, 16.5, 22.0, Plastic);
			AddOBox(P.Body, F.P(FVector(-13.0, 0.0, 72.0)), F.Q, FVector(6.0, 11.0, 12.0), Dark);
			Lump(FVector(3.0, 0.0, 95.0), 10.5, 10.0, 10.5, Plastic);
			Helmet(FVector(2.0, 0.0, 98.0), FQuat::Identity);
			// Bazuca al hombro derecho, con la boca abierta atrás, el mango y la mira.
			const FVector TubeA = F.P(FVector(-48.0, -15.0, 84.0));
			const FVector TubeB = F.P(FVector(62.0, -15.0, 90.0));
			AddTube(P.Body, { TubeA, TubeB }, 8.5, 8, Plastic);
			AddRevolve(P.Body, FBeachFrame(TubeA, AxisZTo(TubeA - TubeB)), { FVector2D(8.5, 0.0), FVector2D(12.5, 12.0), FVector2D(11.0, 12.0), FVector2D(7.0, 1.0) }, 8, Dark);
			AddOBox(P.Body, F.P(FVector(8.0, -15.0, 72.0)), F.Q, FVector(3.0, 3.0, 8.0), Dark);
			AddOBox(P.Body, F.P(FVector(22.0, -6.0, 93.0)), F.Q, FVector(5.0, 2.0, 5.0), Dark);
			Limb(FVector(0.0, -17.0, 82.0), FVector(0.0, -24.0, 68.0), FVector(8.0, -16.0, 76.0), 5.5);
			Limb(FVector(0.0, 17.0, 82.0), FVector(18.0, 14.0, 74.0), FVector(30.0, -8.0, 84.0), 5.5);
			if (bCollide) { P.ColCapsule(F.P(FVector(0.0, 0.0, 28.0)), F.P(FVector(0.0, 0.0, 80.0)), 26.0); }
			return;
		}
		default:
			break;
		}
		// De pie (fusil o prismáticos).
		const bool bRifle = Pose == EMilPose::Rifle;
		AddPrismPoly(P.Body, F, WobblyEllipse(32.0, 24.0, 8, 0.12, ManSeed), 0.0, 6.0, Light, Dark);
		const FVector FootL = bRifle ? FVector(12.0, 10.0, 8.0) : FVector(3.0, 9.0, 8.0);
		const FVector FootR = bRifle ? FVector(-12.0, -12.0, 8.0) : FVector(-3.0, -9.0, 8.0);
		Limb(FVector(0.0, 8.0, 72.0), (FVector(0.0, 8.0, 72.0) + FootL) * 0.5 + FVector(3.0, 1.0, 0.0), FootL, 8.5);
		Limb(FVector(0.0, -8.0, 72.0), (FVector(0.0, -8.0, 72.0) + FootR) * 0.5 + FVector(-1.0, -1.0, 0.0), FootR, 8.5);
		Lump(FootL + FVector(3.0, 0.0, 1.0), 11.0, 7.0, 6.0, Dark);
		Lump(FootR + FVector(3.0, 0.0, 1.0), 11.0, 7.0, 6.0, Dark);
		Lump(FVector(0.0, 0.0, 76.0), 11.0, 16.0, 6.0, Dark);
		Lump(FVector(0.0, 0.0, 95.0), 11.5, 16.5, 22.0, Plastic);
		AddOBox(P.Body, F.P(FVector(-13.0, 0.0, 98.0)), F.Q, FVector(6.0, 11.0, 12.0), Dark);
		if (bRifle)
		{
			Lump(FVector(3.0, 0.0, 121.0), 10.5, 10.0, 10.5, Plastic);
			Helmet(FVector(2.0, 0.0, 124.0), FQuat::Identity);
			Limb(FVector(0.0, -17.0, 108.0), FVector(4.0, -24.0, 94.0), FVector(16.0, -12.0, 104.0), 5.5);
			Limb(FVector(0.0, 17.0, 108.0), FVector(20.0, 16.0, 100.0), FVector(38.0, -4.0, 106.0), 5.5);
			AddMilRifle(P.Body, F.P(FVector(2.0, -12.0, 108.0)), F.P(FVector(80.0, -6.0, 111.0)), Up, Plastic);
		}
		else
		{
			// La cabeza algo levantada, los codos fuera y los prismáticos delante de los ojos.
			Lump(FVector(3.0, 0.0, 121.0), 10.5, 10.0, 10.5, Plastic);
			Helmet(FVector(1.0, 0.0, 124.5), FQuat(FVector(0.0, 1.0, 0.0), FMath::DegreesToRadians(-8.0)));
			Limb(FVector(0.0, -17.0, 108.0), FVector(12.0, -24.0, 100.0), FVector(16.0, -6.0, 119.0), 5.5);
			Limb(FVector(0.0, 17.0, 108.0), FVector(12.0, 24.0, 100.0), FVector(16.0, 6.0, 119.0), 5.5);
			for (const double Sy : { -1.0, 1.0 })
			{
				AddTube(P.Body, { F.P(FVector(13.0, Sy * 4.5, 122.0)), F.P(FVector(26.0, Sy * 4.5, 123.0)) }, 3.8, 6, Dark);
			}
			AddOBox(P.Body, F.P(FVector(18.0, 0.0, 123.0)), F.Q, FVector(4.0, 3.0, 2.0), Dark);
		}
		if (bCollide) { P.ColCapsule(F.P(FVector(0.0, 0.0, 26.0)), F.P(FVector(0.0, 0.0, 116.0)), 24.0); }
	}

	/** Soldadito volcado (tumbado de lado, con la peana de canto), como un juguete que alguien ha tirado. */
	inline void AddMilFallenSoldier(FParts& P, double X, double Y, double YawDeg, EMilPose Pose, const FLinearColor& Plastic, uint32 ManSeed)
	{
		const FBeachFrame F(FVector(X, Y, 22.0), YawQ(YawDeg) * FQuat(FVector(1.0, 0.0, 0.0), UE_DOUBLE_HALF_PI));
		AddMilSoldier(P, F, Pose, Plastic, ManSeed, false);
		P.ColCapsule(F.P(FVector(0.0, 0.0, 26.0)), F.P(FVector(0.0, 0.0, 116.0)), 22.0);
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Casco
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Casco de juguete de 23,6 × 20 cm y 9,5 de hondo (cm de juego con SizeScale 1). */
	namespace MilHelmet
	{
		/** Semieje de delante a atrás (sin el ala); el de lado a lado es Rx · Squash. */
		constexpr double Rx = 330.0;
		constexpr double Squash = 0.86;
		constexpr double Brim = 18.0;
		/** Alto del borde recto antes de la cúpula y hondo total. */
		constexpr double Rim = 22.0;
		constexpr double Depth = 265.0;
		constexpr double Thick = 12.0;
	}

	/** Punto de la cara de fuera de la cúpula del casco (marco del casco) a latitud LatDeg (0 = borde, 90 = cima) y acimut AzDeg. */
	inline FVector MilHelmetPoint(double LatDeg, double AzDeg)
	{
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double Az = FMath::DegreesToRadians(AzDeg);
		const double Rr = MilHelmet::Rx * FMath::Cos(Lat);
		return FVector(Rr * FMath::Cos(Az), Rr * FMath::Sin(Az) * MilHelmet::Squash, MilHelmet::Rim + (MilHelmet::Depth - MilHelmet::Rim) * FMath::Sin(Lat));
	}

	/** Normal hacia fuera de la cúpula en ese punto. */
	inline FVector MilHelmetNormal(double LatDeg, double AzDeg)
	{
		const FVector Pt = MilHelmetPoint(LatDeg, AzDeg);
		const double Ax = MilHelmet::Rx;
		const double Ay = MilHelmet::Rx * MilHelmet::Squash;
		const double Az = MilHelmet::Depth - MilHelmet::Rim;
		return FVector(Pt.X / (Ax * Ax), Pt.Y / (Ay * Ay), (Pt.Z - MilHelmet::Rim) / (Az * Az)).GetSafeNormal();
	}

	/**
	 * Casco militar de juguete hueco (con su grosor) en el marco F (borde en Z = 0, cúpula hacia +Z, frente hacia +X): ala,
	 * cúpula y, según Paint, verde oliva con la escarapela (0), caqui del desierto con la escarapela (1), verde oscuro con
	 * redecilla (2) o verde oliva con franja blanca y escarapela (3). Colisión (si bCollide): cajas finas tangentes a la
	 * cúpula por fuera, en las bandas de latitud BandFrom..BandTo (0: 0-30°, 1: 30-60°, 2: 60-90°) y ocho gajos: por
	 * dentro queda hueco (una tortuga cabe debajo o dentro si el casco está levantado o de lado).
	 */
	inline void AddMilHelmet(FParts& P, const FBeachFrame& F, int32 Paint, int32 BandFrom, int32 BandTo, bool bCollide, const FMilLook& Look)
	{
		using namespace MilHelmet;
		static const uint32 ShellHex[4] = { 0x5E6B38, 0xB9A676, 0x3F4A26, 0x5E6B38 };
		const FLinearColor ShellC = Hex(ShellHex[((Paint % 4) + 4) % 4], 0.08f);
		const FLinearColor BrimC = Shade(ShellC, 0.86f);
		const FLinearColor InnerC = Shade(ShellC, 0.55f);
		TArray<FVector2D> Prof;
		Prof.Add(FVector2D(Rx - Thick, 0.0));
		Prof.Add(FVector2D(Rx + Brim, 0.0));
		Prof.Add(FVector2D(Rx + Brim, 7.0));
		Prof.Add(FVector2D(Rx + 2.0, Rim));
		for (int32 i = 1; i <= 5; ++i)
		{
			const double Lat = UE_DOUBLE_HALF_PI * i / 6.0;
			Prof.Add(FVector2D(Rx * FMath::Cos(Lat), Rim + (Depth - Rim) * FMath::Sin(Lat)));
		}
		Prof.Add(FVector2D(0.0, Depth));
		Prof.Add(FVector2D(0.0, Depth - Thick));
		for (int32 i = 5; i >= 1; --i)
		{
			const double Lat = UE_DOUBLE_HALF_PI * i / 6.0;
			Prof.Add(FVector2D((Rx - Thick) * FMath::Cos(Lat), Rim + (Depth - Rim - Thick) * FMath::Sin(Lat)));
		}
		Prof.Add(FVector2D(Rx - Thick, Rim));
		Prof.Add(FVector2D(Rx - Thick, 0.0));
		// Tramos: 0-2 el ala, 3-8 la cúpula por fuera, del 10 en adelante por dentro.
		AddRevolve(P.Body, F, Prof, 12, [&ShellC, &BrimC, &InnerC](int32 Ring, int32 Side) -> FLinearColor
		{
			if (Ring <= 2) { return BrimC; }
			if (Ring <= 8) { return (Side % 3 == 0) ? Shade(ShellC, 0.94f) : ShellC; }
			return InnerC;
		}, 0.0, 0u, Squash);
		if (Paint == 2)
		{
			// Redecilla: meridianos y dos paralelos de cinta caqui un pelo por fuera.
			const FLinearColor NetC = Hex(0x8C8456);
			for (int32 m = 0; m < 12; ++m)
			{
				const double AzM = 30.0 * m + 15.0;
				for (int32 s = 0; s < 4; ++s)
				{
					const double La = 8.0 + 19.0 * s;
					const double Lb = La + 19.0;
					const FVector Pa = MilHelmetPoint(La, AzM);
					const FVector Pb = MilHelmetPoint(Lb, AzM);
					const FVector Na = MilHelmetNormal(La, AzM);
					const FVector Nb = MilHelmetNormal(Lb, AzM);
					const FVector Wd = FVector::CrossProduct(Na, (Pb - Pa).GetSafeNormal()).GetSafeNormal() * 6.0;
					P.Body.AddQuad(F.P(Pa + Na * 3.0 - Wd), F.P(Pa + Na * 3.0 + Wd), F.P(Pb + Nb * 3.0 + Wd), F.P(Pb + Nb * 3.0 - Wd), F.D(Na + Nb), NetC);
				}
			}
			for (const double LatP : { 30.0, 58.0 })
			{
				for (int32 s = 0; s < 24; ++s)
				{
					const FVector Pa = MilHelmetPoint(LatP, 15.0 * s);
					const FVector Pb = MilHelmetPoint(LatP, 15.0 * (s + 1));
					const FVector Na = MilHelmetNormal(LatP, 15.0 * s);
					const FVector Nb = MilHelmetNormal(LatP, 15.0 * (s + 1));
					const FVector Wd = FVector::CrossProduct((Pb - Pa).GetSafeNormal(), Na).GetSafeNormal() * 6.0;
					P.Body.AddQuad(F.P(Pa + Na * 3.0 - Wd), F.P(Pb + Nb * 3.0 - Wd), F.P(Pb + Nb * 3.0 + Wd), F.P(Pa + Na * 3.0 + Wd), F.D(Na + Nb), NetC);
				}
			}
		}
		if (Paint == 3)
		{
			// Franja blanca alrededor.
			const FLinearColor BandC = Hex(0xE8E4D8, 0.1f);
			for (int32 s = 0; s < 16; ++s)
			{
				const double A0 = 22.5 * s;
				const double A1 = 22.5 * (s + 1);
				P.Body.AddQuad(F.P(MilHelmetPoint(18.0, A0) + MilHelmetNormal(18.0, A0) * 2.5), F.P(MilHelmetPoint(18.0, A1) + MilHelmetNormal(18.0, A1) * 2.5),
					F.P(MilHelmetPoint(27.0, A1) + MilHelmetNormal(27.0, A1) * 2.5), F.P(MilHelmetPoint(27.0, A0) + MilHelmetNormal(27.0, A0) * 2.5),
					F.D(MilHelmetNormal(22.5, (A0 + A1) * 0.5)), BandC);
			}
		}
		if (Paint != 2)
		{
			// Escarapela en el frente (la curva de la cúpula se come 6 cm del borde: va 7 cm por fuera).
			const FVector Front = MilHelmetPoint(40.0, 0.0);
			const FVector FrontN = MilHelmetNormal(40.0, 0.0);
			AddMilRoundel(P.Body, F.P(Front + FrontN * 7.0), F.D(FrontN), F.D(FVector::UpVector), 62.0, Look);
		}
		if (!bCollide) { return; }
		const double BandLat[4] = { -4.0, 30.0, 60.0, 90.0 };
		for (int32 b = FMath::Max(0, BandFrom); b <= FMath::Min(2, BandTo); ++b)
		{
			for (int32 s = 0; s < 8; ++s)
			{
				const double AzM = 45.0 * (s + 0.5);
				const double MidLat = (BandLat[b] + BandLat[b + 1]) * 0.5;
				const FVector Lo = MilHelmetPoint(BandLat[b], AzM);
				const FVector Hi = MilHelmetPoint(BandLat[b + 1], AzM);
				const FVector S0 = MilHelmetPoint(MidLat, 45.0 * s);
				const FVector S1 = MilHelmetPoint(MidLat, 45.0 * (s + 1));
				const FVector AxX = (Hi - Lo).GetSafeNormal();
				FVector AxY = S1 - S0;
				AxY = (AxY - AxX * FVector::DotProduct(AxY, AxX)).GetSafeNormal();
				const FVector Mid = (Lo + Hi + S0 + S1) * 0.25;
				FVector AxZ = FVector::CrossProduct(AxX, AxY);
				if (FVector::DotProduct(AxZ, Mid - FVector(0.0, 0.0, Rim)) < 0.0)
				{
					AxY = -AxY;
					AxZ = -AxZ;
				}
				const FVector Half(FVector::Dist(Lo, Hi) * 0.5 + 6.0, FVector::Dist(S0, S1) * 0.5 + 8.0, Thick * 0.5 + 2.0);
				P.ColBox(F.P(Mid - AxZ * Half.Z), F.Q * FRotationMatrix::MakeFromXY(AxX, AxY).ToQuat(), Half);
			}
		}
	}

	/** Barboquejo de cinta caqui por Path (con la hebilla al final); Up fija el canto (cero: marco transportado). */
	inline void AddMilStrap(FTNProcMeshBuffers& M, const TArray<FVector>& Path, const FVector& Up, const FMilLook& Look)
	{
		AddSweep(M, Path, RectSection(2.0, 10.0), false, [](int32) { return 1.0; }, [&Look](int32 i, int32) { return (i % 3 == 2) ? Look.KhakiDark : Look.Khaki; }, true, Up);
		if (Path.Num() > 0)
		{
			AddYawBox(M, Path.Last() + FVector(0.0, 0.0, 2.0), 0.0, FVector(9.0, 13.0, 3.0), Look.Steel);
		}
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Red de camuflaje
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Manchas de camuflaje (verde oscuro, oliva, pardo y caqui) por ruido suave en planta. */
	inline FLinearColor MilCamoAt(double X, double Y, uint32 NetSeed)
	{
		static const uint32 CamoHex[4] = { 0x3F4F26, 0x5E6B38, 0x6B5433, 0xA89A68 };
		const double V = FMath::Sin(X * 0.0042 + Rnd(NetSeed, 1) * 6.28) + FMath::Sin(Y * 0.0051 + Rnd(NetSeed, 2) * 6.28)
			+ 0.8 * FMath::Sin((X + Y) * 0.0073 + Rnd(NetSeed, 3) * 6.28) + 0.6 * FMath::Sin((X - Y) * 0.011 + Rnd(NetSeed, 4) * 6.28);
		const int32 Index = V < -1.0 ? 0 : (V < 0.4 ? 1 : (V < 1.4 ? 2 : 3));
		return Hex(CamoHex[Index], 0.02f);
	}

	/** Paño de red de camuflaje sobre la superficie Pos(u, v): manchas, algún agujero y hojas de adorno por encima. */
	inline void AddMilNet(FTNProcMeshBuffers& M, int32 Nu, int32 Nv, TFunctionRef<FVector(double, double)> Pos, int32 Leaves, uint32 NetSeed)
	{
		AddSheet(M, Nu, Nv, Pos, 6.0, [&Pos, Nu, Nv, NetSeed](int32 I, int32 J)
		{
			const FVector C = Pos((I + 0.5) / Nu, (J + 0.5) / Nv);
			return MilCamoAt(C.X, C.Y, NetSeed);
		}, [NetSeed](int32 I, int32 J) { return Rnd(NetSeed, 5000 + I * 97 + J) > 0.09; });
		for (int32 l = 0; l < Leaves; ++l)
		{
			const FVector C = Pos(Rnd(NetSeed, 300 + l), Rnd(NetSeed, 700 + l)) + FVector(0.0, 0.0, 5.0);
			const double Ang = RndIn(NetSeed, 1100 + l, 0.0, UE_DOUBLE_TWO_PI);
			const double Sz = RndIn(NetSeed, 1500 + l, 35.0, 70.0);
			const FVector D(FMath::Cos(Ang), FMath::Sin(Ang), RndIn(NetSeed, 1900 + l, -0.3, 0.3));
			const FVector Across(-D.Y, D.X, 0.0);
			const FVector La = C - D * (Sz * 0.5);
			const FVector Lb = C + D * (Sz * 0.5);
			const FVector Lw = C + Across * (Sz * 0.3) + FVector(0.0, 0.0, Sz * 0.15);
			const FLinearColor Col = MilCamoAt(C.X * 3.0, C.Y * 3.0, NetSeed + 7u);
			M.AddTri(La, Lb, Lw, FVector::UpVector, Col);
			M.AddTri(La, Lb, Lw, -FVector::UpVector, Shade(Col, 0.8f));
		}
	}

	/** Palo de la red (de Foot a Top, con la atadura) y su cápsula. */
	inline void AddMilPole(FParts& P, const FVector& Foot, const FVector& Top, const FMilLook& Look)
	{
		AddTaperTube(P.Body, { Foot - FVector(0.0, 0.0, 50.0), Top }, 11.0, 8.5, 6, Look.PoleWood, true);
		AddBlob(P.Body, FBeachFrame(Top, FQuat::Identity), 14.0, 14.0, 9.0, 6, 2, Look.Rope);
		P.ColCapsule(Foot, Top - FVector(0.0, 0.0, 12.0), 12.0);
	}

	/** Viento de cuerda de From a una estaca clavada en Stake (sin colisión). */
	inline void AddMilGuy(FTNProcMeshBuffers& M, const FVector& From, const FVector& Stake, const FMilLook& Look)
	{
		AddTube(M, { From, Stake + FVector(0.0, 0.0, 14.0) }, 2.6, 3, Look.Rope, false);
		AddYawBox(M, Stake + FVector(0.0, 0.0, 10.0), 20.0, FVector(5.0, 5.0, 14.0), Look.PoleWood);
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Bidón
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/** Bidón de juguete de 7,7 × 2,9 × 10,7 cm (ancho en X, grueso en Y, alto en Z, en cm de juego con SizeScale 1). */
	namespace MilCan
	{
		constexpr double W = 215.0;
		constexpr double T = 80.0;
		constexpr double H = 300.0;
	}

	/**
	 * Bidón (de pie en su marco, con la base en Base y girado por Rot): cuerpo achaflanado, hombro, tres asas, boca con tapón
	 * de palanca (abierto con bOpen), cruz en relieve en las dos caras grandes y, con bRoundel, la escarapela en el cruce.
	 */
	inline void AddMilJerrycan(FParts& P, const FVector& Base, const FQuat& Rot, const FLinearColor& Paint, bool bRoundel, bool bOpen, const FMilLook& Look, bool bCollide = true)
	{
		const FBeachFrame F(Base, Rot);
		const double Hw = MilCan::W * 0.5;
		const double Ht = MilCan::T * 0.5;
		const double BodyTop = MilCan::H - 26.0;
		const double Ch = 16.0;
		const TArray<FVector2D> Outline = { FVector2D(Hw, Ht - Ch), FVector2D(Hw - Ch, Ht), FVector2D(-Hw + Ch, Ht), FVector2D(-Hw, Ht - Ch), FVector2D(-Hw, -Ht + Ch),
			FVector2D(-Hw + Ch, -Ht), FVector2D(Hw - Ch, -Ht), FVector2D(Hw, -Ht + Ch) };
		AddPrismPoly(P.Body, F, Outline, 0.0, BodyTop, Shade(Paint, 1.05f), Paint, true);
		TArray<FVector2D> Shoulder;
		for (const FVector2D& V : Outline) { Shoulder.Add(FVector2D(V.X * 0.9, V.Y * 0.78)); }
		AddPrismPoly(P.Body, F, Shoulder, BodyTop, BodyTop + 10.0, Shade(Paint, 1.08f), Shade(Paint, 0.92f));
		const double Deck = BodyTop + 10.0;
		for (const double HandleX : { -58.0, 0.0, 58.0 })
		{
			AddTube(P.Body, { F.P(FVector(HandleX, -Ht * 0.55, Deck - 4.0)), F.P(FVector(HandleX, -Ht * 0.45, Deck + 26.0)), F.P(FVector(HandleX, Ht * 0.45, Deck + 26.0)),
				F.P(FVector(HandleX, Ht * 0.55, Deck - 4.0)) }, 6.0, 4, Shade(Paint, 0.9f), false);
		}
		const FVector Spout(Hw - 34.0, 0.0, Deck);
		AddRevolve(P.Body, FBeachFrame(F.P(Spout - FVector(0.0, 0.0, 4.0)), F.Q), { FVector2D(19.0, 0.0), FVector2D(19.0, 22.0), FVector2D(15.0, 26.0), FVector2D(0.0, 26.0) }, 8, Look.Steel);
		if (bOpen)
		{
			// Tapón abierto hacia un lado y la boca oscura.
			AddRevolve(P.Body, FBeachFrame(F.P(Spout + FVector(0.0, 0.0, 22.5)), F.Q), { FVector2D(13.0, 0.0), FVector2D(0.0, 0.4) }, 8, Hex(0x1E1A16));
			AddOBox(P.Body, F.P(Spout + FVector(-4.0, 30.0, 34.0)), F.Q * FQuat(FVector(1.0, 0.0, 0.0), FMath::DegreesToRadians(-70.0)), FVector(20.0, 20.0, 5.0), Look.Steel);
		}
		else
		{
			AddOBox(P.Body, F.P(Spout + FVector(0.0, 0.0, 28.0)), F.Q, FVector(20.0, 20.0, 5.0), Look.Steel);
			AddOBox(P.Body, F.P(Spout + FVector(-26.0, 0.0, 30.0)), F.Q, FVector(14.0, 5.0, 3.0), Look.Steel);
		}
		for (const double Sy : { -1.0, 1.0 })
		{
			for (const double Diag : { -1.0, 1.0 })
			{
				const FVector DiagA(-Hw + 30.0, Sy * (Ht + 2.0), Diag > 0.0 ? 34.0 : BodyTop - 34.0);
				const FVector DiagB(Hw - 30.0, Sy * (Ht + 2.0), Diag > 0.0 ? BodyTop - 34.0 : 34.0);
				AddOBox(P.Body, F.P((DiagA + DiagB) * 0.5), F.Q * FRotationMatrix::MakeFromXZ(DiagB - DiagA, FVector(0.0, Sy, 0.0)).ToQuat(),
					FVector(FVector::Dist(DiagA, DiagB) * 0.5, 9.0, 3.0), Shade(Paint, 0.93f));
			}
			// Costura de las dos mitades por los cantos.
			AddOBox(P.Body, F.P(FVector(0.0, Sy * (Ht + 1.0), BodyTop - 4.0)), F.Q, FVector(Hw - Ch, 1.5, 3.0), Shade(Paint, 0.85f));
			if (bRoundel)
			{
				AddMilRoundel(P.Body, F.P(FVector(0.0, Sy * (Ht + 6.0), BodyTop * 0.5)), F.D(FVector(0.0, Sy, 0.0)), F.D(FVector::UpVector), 46.0, Look);
			}
		}
		if (bCollide) { P.ColBox(F.P(FVector(0.0, 0.0, Deck * 0.5)), F.Q, FVector(Hw, Ht, Deck * 0.5)); }
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Recetas por elemento
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	/**
	 * Parapeto de sacos terreros (huella 11 m): recto de 14,4 m (0), en media luna de 5,4 m de radio (1), recto a medio
	 * desmoronar (2), esquina en L con la bandera de Tortunavy (3), nido redondo de tres hileras con bandera, caja y un
	 * soldadito vigía (4) y media luna a medio desmoronar (5). Cuatro hileras (1,2 m) con banqueta de dos (63 cm) por un
	 * lado y los extremos en escalera; los derrumbes dejan una hilera y un montón de sacos caídos que se sube.
	 * Orientación (la de los alargados del reparto): a lo largo de X local, con la cara alta hacia -Y (lo que se defiende
	 * queda en +Y, donde van la banqueta y la bandera); el actor no se gira al azar, solo ±6°.
	 */
	inline void BuildMilSandbags(FParts& P, int32 Variant, uint32 Seed)
	{
		const FMilLook Look = MilLookOf();
		const int32 Kind = Variant % 6;
		const bool bRuined = Kind == 2 || Kind == 5;
		switch (Kind)
		{
		case 1:
		case 5:
		{
			// Media luna abombada hacia -Y.
			FMilParapet W;
			W.Path.bArc = true;
			W.Path.Center = FVector2D(0.0, 240.0);
			W.Path.Radius = 540.0;
			W.Path.Ang0 = 170.0;
			W.Path.Ang1 = 370.0;
			// La banqueta, por dentro (hacia el centro del arco).
			W.StepSide = 1.0;
			if (bRuined)
			{
				W.Gap0 = 0.56;
				W.Gap1 = 0.76;
			}
			AddMilParapet(P, W, Seed + 1u);
			if (bRuined)
			{
				double Yaw = 0.0;
				const FVector2D GapAt = W.Path.At((W.Gap0 + W.Gap1) * 0.5 * W.Path.Length(), 0.0, Yaw);
				AddMilFallenBags(P, GapAt, Yaw, 6, Seed + 5u, Look);
			}
			break;
		}
		case 3:
		{
			// Esquina: un tramo a lo largo de X en -Y y otro a lo largo de Y en +X, con la banqueta por dentro y la bandera en el rincón.
			FMilParapet W1;
			W1.Path.A = FVector2D(-560.0, -460.0);
			W1.Path.B = FVector2D(460.0, -460.0);
			W1.StepSide = 1.0;
			W1.bStepEnd = false;
			AddMilParapet(P, W1, Seed + 1u);
			FMilParapet W2;
			W2.Path.A = FVector2D(460.0, -520.0);
			W2.Path.B = FVector2D(460.0, 600.0);
			W2.StepSide = 1.0;
			W2.bStepStart = false;
			AddMilParapet(P, W2, Seed + 2u);
			AddMilFlag(P, FVector(300.0, -300.0, 0.0), 540.0, 170.0, 105.0, Look);
			break;
		}
		case 4:
		{
			// Nido redondo con la entrada por +Y, bandera en medio, una caja y un soldadito que asoma con los prismáticos hacia -Y.
			FMilParapet W;
			W.Path.bArc = true;
			W.Path.Center = FVector2D::ZeroVector;
			W.Path.Radius = 400.0;
			W.Path.Ang0 = 115.0;
			W.Path.Ang1 = 425.0;
			W.Rows = 3;
			W.StepRows = 0;
			AddMilParapet(P, W, Seed + 1u);
			AddMilFlag(P, FVector(-40.0, 30.0, 0.0), 520.0, 160.0, 100.0, Look);
			AddMilCrate(P, FVector(140.0, 120.0, 0.0), YawQ(24.0), 112.0, 66.0, 56.0, true, true, Seed + 3u, Look);
			AddMilSoldier(P, FBeachFrame(FVector(120.0, -190.0, 0.0), YawQ(-90.0 + RndIn(Seed, 9, -25.0, 25.0))), EMilPose::Binoculars, Hex(0x4E8A34, 0.3f), Seed + 4u);
			break;
		}
		default:
		{
			// Recto a lo largo de X, con la banqueta hacia +Y.
			FMilParapet W;
			W.Path.A = FVector2D(-720.0, 0.0);
			W.Path.B = FVector2D(720.0, 0.0);
			W.StepSide = 1.0;
			if (bRuined)
			{
				W.Gap0 = 0.38;
				W.Gap1 = 0.62;
			}
			AddMilParapet(P, W, Seed + 1u);
			if (bRuined)
			{
				AddMilFallenBags(P, FVector2D(0.0, 0.0), 0.0, 6, Seed + 5u, Look);
			}
			else
			{
				// Dos sacos sueltos en la arena junto a los extremos.
				for (int32 b = 0; b < 2; ++b)
				{
					const uint32 BagSeed = HashMix(Seed, 60u + static_cast<uint32>(b));
					const FVector Base((b == 0 ? 1.0 : -1.0) * RndIn(BagSeed, 2, 800.0, 880.0), RndIn(BagSeed, 1, -150.0, 170.0), -3.0);
					const FQuat Rot = YawQ(RndIn(BagSeed, 3, 0.0, 180.0));
					AddMilSandbag(P.Body, Base, Rot, MilBag::Len, MilBag::Wid, MilBag::H, MilBagColor(BagSeed), BagSeed);
					P.ColBox(Base + FVector(0.0, 0.0, MilBag::H * 0.4), Rot, FVector(MilBag::Len * 0.42, MilBag::Wid * 0.4, MilBag::H * 0.42));
				}
			}
			break;
		}
		}
		P.Info.SinkMax = 10.f;
		P.Info.TiltMax = 1.f;
		P.Info.bFreeYaw = false;
		P.Info.YawJitter = 6.f;
	}

	/**
	 * Cajas de munición (huella 7 m): cerrada con una lata de munición y cartuchos sueltos (0), tres pilas en escalera de
	 * 75, 150 y 225 cm (1), abierta y llena de cartuchos con la tapa apoyada como rampa de 28° hasta el canto (2) o volcada
	 * con los cartuchos desparramados y otra caja al lado con la lata encima (3).
	 */
	inline void BuildMilAmmoCrate(FParts& P, int32 Variant, uint32 Seed)
	{
		const FMilLook Look = MilLookOf();
		const int32 Kind = Variant % 4;
		switch (Kind)
		{
		case 1:
		{
			for (int32 Stack = 0; Stack < 3; ++Stack)
			{
				for (int32 Level = 0; Level <= Stack; ++Level)
				{
					const uint32 CrateSeed = HashMix(Seed, 10u + static_cast<uint32>(Stack * 4 + Level));
					const FVector Base(-165.0 + 165.0 * Stack + RndIn(CrateSeed, 2, -4.0, 4.0), RndIn(CrateSeed, 3, -5.0, 5.0), Level * MilCrate::H);
					AddMilCrate(P, Base, YawQ(RndIn(CrateSeed, 4, -3.0, 3.0)), MilCrate::Len, MilCrate::Wid, MilCrate::H, true, Level == Stack, CrateSeed, Look);
				}
			}
			AddMilAmmoCan(P, FVector(-180.0, 110.0, 0.0), YawQ(RndIn(Seed, 5, -30.0, 30.0)), Look);
			break;
		}
		case 2:
		{
			const double Len = 170.0;
			const double Wid = 96.0;
			const double H = 80.0;
			const FVector CrateAt(20.0, 0.0, 0.0);
			AddMilCrate(P, CrateAt, FQuat::Identity, Len, Wid, H, false, false, Seed + 1u, Look);
			// Dos capas de cartuchos a lo ancho: el fondo de dentro queda a ~45 cm.
			for (int32 Layer = 0; Layer < 2; ++Layer)
			{
				for (int32 k = 0; k < 6; ++k)
				{
					const double R = 11.0;
					const double X = CrateAt.X - 60.0 + 24.0 * k + Layer * 12.0;
					if (Layer == 1 && k == 5) { continue; }
					AddMilShell(P.Body, FVector(X, -38.0, 8.0 + R + Layer * 18.0), FVector(0.0, 1.0, 0.0), 76.0, R, Look);
				}
			}
			P.ColBox(FVector(CrateAt.X, 0.0, 22.0), FQuat::Identity, FVector(Len * 0.5 - 9.0, Wid * 0.5 - 9.0, 22.0));
			// La tapa, apoyada en el canto de -X y en la arena: una rampa.
			const FVector RimPt(CrateAt.X - Len * 0.5 - 2.0, 0.0, H);
			const double Run = FMath::Sqrt(FMath::Max(1.0, Len * Len - H * H));
			const FVector FootPt(RimPt.X - Run, 0.0, 0.0);
			const FVector Along = (RimPt - FootPt).GetSafeNormal();
			FVector Nrm = FVector::CrossProduct(Along, FVector(0.0, 1.0, 0.0));
			if (Nrm.Z < 0.0) { Nrm = -Nrm; }
			const FQuat LidQ = FRotationMatrix::MakeFromXZ(Along, Nrm).ToQuat();
			const FVector LidC = (RimPt + FootPt) * 0.5 - Nrm * 4.0;
			AddOBox(P.Body, LidC, LidQ, FVector(Len * 0.5, Wid * 0.5 + 3.0, 4.0), Shade(Look.Olive, 1.05f));
			AddMilRoundel(P.Body, LidC + Nrm * 4.0, Nrm, Along, Wid * 0.3, Look);
			P.ColBox(LidC - Nrm * 8.0, LidQ, FVector(Len * 0.5, Wid * 0.5, 12.0));
			AddMilLyingShell(P.Body, 150.0, 90.0, RndIn(Seed, 6, 0.0, 360.0), 76.0, 11.0, Look);
			AddMilLyingShell(P.Body, 170.0, -100.0, RndIn(Seed, 7, 0.0, 360.0), 76.0, 11.0, Look);
			break;
		}
		case 3:
		{
			// Volcada: de lado, con la boca hacia +Y y los cartuchos por la arena.
			const double Len = 160.0;
			const double Wid = 92.0;
			const double H = 78.0;
			AddMilCrate(P, FVector(-40.0, -150.0, Wid * 0.5), FQuat(FVector(1.0, 0.0, 0.0), FMath::DegreesToRadians(-90.0)), Len, Wid, H, false, false, Seed + 1u, Look);
			for (int32 k = 0; k < 8; ++k)
			{
				const double X = -110.0 + RndIn(Seed, 20 + k, 0.0, 150.0);
				const double Y = -50.0 + RndIn(Seed, 40 + k, 0.0, 230.0);
				AddMilLyingShell(P.Body, X, Y, RndIn(Seed, 60 + k, 0.0, 360.0), 76.0, 11.0, Look);
			}
			AddMilCrate(P, FVector(190.0, -80.0, 0.0), YawQ(28.0), MilCrate::Len, MilCrate::Wid, MilCrate::H, true, false, Seed + 2u, Look);
			AddMilAmmoCan(P, FVector(190.0, -80.0, MilCrate::H), YawQ(-10.0), Look);
			break;
		}
		default:
		{
			AddMilCrate(P, FVector(-30.0, -10.0, 0.0), YawQ(6.0), 160.0, 90.0, 76.0, true, true, Seed + 1u, Look);
			AddMilAmmoCan(P, FVector(100.0, 90.0, 0.0), YawQ(-24.0), Look);
			for (int32 k = 0; k < 3; ++k)
			{
				AddMilLyingShell(P.Body, RndIn(Seed, 20 + k, -160.0, 160.0), (k % 2 ? -1.0 : 1.0) * RndIn(Seed, 30 + k, 90.0, 160.0), RndIn(Seed, 40 + k, 0.0, 360.0), 70.0, 10.0, Look);
			}
			break;
		}
		}
		P.Info.SinkMax = 6.f;
		P.Info.TiltMax = 1.5f;
	}

	/** Erizo antitanque: tres barras perpendiculares entre sí cruzadas en el centro, apoyado en tres puntas (Style: 0 raíl, 1 raíl con algas, 2 madera). */
	inline void AddMilHedgehog(FParts& P, const FVector& Base, double YawDeg, double HalfLen, int32 Style, uint32 HogSeed, const FMilLook& Look)
	{
		static const uint32 RustHex[4] = { 0x7A4A2E, 0x8E5634, 0x6A4430, 0x9A6440 };
		const double Horiz = FMath::Sqrt(2.0 / 3.0);
		const double Vert = 1.0 / FMath::Sqrt(3.0);
		// Cada barra forma 35° con el suelo: con las tres puntas de abajo en la arena, el cruce queda a HalfLen / √3.
		const FVector C = Base + FVector(0.0, 0.0, HalfLen * Vert);
		for (int32 k = 0; k < 3; ++k)
		{
			const double Az = FMath::DegreesToRadians(YawDeg + 120.0 * k);
			const FVector E(Horiz * FMath::Cos(Az), Horiz * FMath::Sin(Az), Vert);
			const FVector Lo = C - E * HalfLen;
			const FVector Hi = C + E * HalfLen;
			if (Style == 2)
			{
				const FVector Bend = FVector::CrossProduct(E, FVector::UpVector).GetSafeNormal() * RndIn(HogSeed, 10 + k, -18.0, 18.0);
				AddBranch(P, { Lo, FMath::Lerp(Lo, Hi, 0.33) + Bend, FMath::Lerp(Lo, Hi, 0.66) + Bend * 0.5, Hi }, 27.0, 19.0, Hex(k == 1 ? 0xA7A49D : 0xB9B2A6), false);
				P.ColCapsule(Lo + E * 20.0, Hi - E * 15.0, 22.0);
			}
			else
			{
				// Raíl de doble T oxidado: alma y dos alas, algo girado sobre su eje.
				const FVector Side = FVector::CrossProduct(FVector::UpVector, E).GetSafeNormal();
				const FVector Up2 = FVector::CrossProduct(E, Side);
				const double Twist = FMath::DegreesToRadians(RndIn(HogSeed, 20 + k, -25.0, 25.0));
				const FVector Ny = Side * FMath::Cos(Twist) + Up2 * FMath::Sin(Twist);
				const FVector Nz = FVector::CrossProduct(E, Ny);
				const FQuat Q = FRotationMatrix::MakeFromXY(E, Ny).ToQuat();
				const FLinearColor Rust = Hex(RustHex[(k + static_cast<int32>(HogSeed % 4u)) % 4], 0.15f);
				AddOBox(P.Body, C, Q, FVector(HalfLen, 5.0, 20.0), Rust);
				for (const double Sz : { -1.0, 1.0 })
				{
					AddOBox(P.Body, C + Nz * (Sz * 20.0), Q, FVector(HalfLen, 19.0, 4.5), Shade(Rust, 0.88f));
				}
				// Churretes de óxido más oscuros.
				AddOBox(P.Body, C + E * (HalfLen * RndIn(HogSeed, 30 + k, -0.5, 0.5)) + Ny * 6.0, Q, FVector(HalfLen * 0.18, 1.5, 14.0), Hex(0x4E3222, 0.1f));
				P.ColCapsule(Lo + E * 22.0, Hi - E * 22.0, 22.0);
			}
			// Montoncito de arena en la punta de abajo.
			AddRevolve(P.Body, FBeachFrame(FVector(Lo.X, Lo.Y, Base.Z), FQuat::Identity), CapProfile(52.0, 18.0, 2, 8.0), 7, Look.Sand, 0.15, HogSeed + static_cast<uint32>(k));
		}
		if (Style == 2)
		{
			// Atado con cuerda en el cruce.
			for (int32 k = 0; k < 3; ++k)
			{
				const double Az = FMath::DegreesToRadians(YawDeg + 120.0 * k);
				AddLoop(P.Body, C, FVector(Horiz * FMath::Cos(Az), Horiz * FMath::Sin(Az), Vert), 30.0, 5.0, 8, Look.Rope);
			}
		}
		else
		{
			// Chapa de unión con los tornillos.
			AddBlob(P.Body, FBeachFrame(C, FQuat::Identity), 25.0, 25.0, 25.0, 6, 3, Hex(0x5A4636, 0.3f));
		}
		if (Style == 1)
		{
			// Algas colgando de las barras de arriba.
			for (int32 s = 0; s < 4; ++s)
			{
				const double Az = FMath::DegreesToRadians(YawDeg + 120.0 * (s % 3));
				const FVector E(Horiz * FMath::Cos(Az), Horiz * FMath::Sin(Az), Vert);
				const FVector From = C + E * (HalfLen * RndIn(HogSeed, 50 + s, 0.3, 0.8)) - FVector(0.0, 0.0, 22.0);
				const double Drop = RndIn(HogSeed, 60 + s, 70.0, 130.0);
				TArray<FVector> Strand;
				for (int32 i = 0; i <= 4; ++i)
				{
					const double T = i / 4.0;
					Strand.Add(From + FVector(12.0 * FMath::Sin(T * 5.0 + s), 10.0 * FMath::Cos(T * 4.0 + s), -Drop * T));
				}
				AddTaperTube(P.Body, Strand, 7.0, 2.5, 5, Hex(s % 2 ? 0x3E6B2A : 0x5B7F2E, 0.2f), true);
			}
		}
	}

	/**
	 * Erizo antitanque (huella 8 m): de raíles oxidados de 7,8 m (0; el cruce a 2,25 m: se pasa por debajo entre dos patas),
	 * de raíles con algas colgando (1), de madera a la deriva atada con cuerda (2) o una pareja más pequeña, uno de raíl y
	 * otro de madera (3). Colisión: una cápsula por barra.
	 */
	inline void BuildMilTankTrap(FParts& P, int32 Variant, uint32 Seed)
	{
		const FMilLook Look = MilLookOf();
		const int32 Kind = Variant % 4;
		switch (Kind)
		{
		case 1:  AddMilHedgehog(P, FVector::ZeroVector, RndIn(Seed, 1, 0.0, 120.0), 390.0, 1, Seed, Look); break;
		case 2:  AddMilHedgehog(P, FVector::ZeroVector, RndIn(Seed, 1, 0.0, 120.0), 380.0, 2, Seed, Look); break;
		case 3:
			AddMilHedgehog(P, FVector(0.0, -300.0, 0.0), RndIn(Seed, 1, 0.0, 120.0), 270.0, 0, Seed, Look);
			AddMilHedgehog(P, FVector(40.0, 320.0, 0.0), RndIn(Seed, 2, 0.0, 120.0), 260.0, 2, Seed + 7u, Look);
			break;
		default: AddMilHedgehog(P, FVector::ZeroVector, RndIn(Seed, 1, 0.0, 120.0), 390.0, 0, Seed, Look); break;
		}
		P.Info.SinkMin = 6.f;
		P.Info.SinkMax = 16.f;
		P.Info.TiltMax = 0.f;
	}

	/**
	 * Casco militar (huella 4,5 m; 6,6 × 5,7 m y 2,65 de hondo): boca abajo y medio enterrado, con la escarapela (0: asoma
	 * 1,7 m; desde ~50 cm la cúpula ya se anda, así que se sube de un salto), boca arriba como un cuenco con arena y un charco
	 * dentro (1: borde a 90 cm, fondo a 18), apoyado en un palo con una lata de munición escondida debajo (2: la boca se
	 * levanta 2 m por delante, se mete una debajo) y tumbado de lado como una cueva (3: 4,3 m de boca, suelo de arena).
	 */
	inline void BuildMilHelmet(FParts& P, int32 Variant, uint32 Seed)
	{
		const FMilLook Look = MilLookOf();
		const int32 Kind = Variant % 4;
		switch (Kind)
		{
		case 1:
		{
			AddMilHelmet(P, FBeachFrame(FVector(0.0, 0.0, 90.0), FQuat(FVector(1.0, 0.0, 0.0), UE_DOUBLE_PI)), 1, 0, 0, true, Look);
			AddPrismPoly(P.Body, FBeachFrame(), WobblyEllipse(300.0, 258.0, 14, 0.02, Seed), -30.0, 18.0, Look.Sand, Look.SandDark);
			AddPrismPoly(P.Body, FBeachFrame(), WobblyEllipse(170.0, 140.0, 12, 0.08, Seed + 1u), 17.0, 20.5, Hex(0x5B97A8, 0.9f), Hex(0x4A8494, 0.9f));
			AddPrismPoly(P.Body, FBeachFrame(FVector(-30.0, 20.0, 0.0), FQuat::Identity), WobblyEllipse(80.0, 55.0, 10, 0.1, Seed + 2u), 20.5, 21.5, Hex(0x8CC3CF, 0.95f),
				Hex(0x8CC3CF, 0.95f));
			for (int32 k = 0; k < 3; ++k)
			{
				const double Ang = RndIn(Seed, 10 + k, 0.0, UE_DOUBLE_TWO_PI);
				AddBlob(P.Body, FBeachFrame(FVector(FMath::Cos(Ang) * 220.0, FMath::Sin(Ang) * 180.0, 20.0), YawQ(Ang * 57.3)), 16.0, 12.0, 8.0, 6, 3, Hex(0x9A9690));
			}
			P.ColPrism(FVector::ZeroVector, 240.0, -30.0, 18.0, 22.5, 4);
			AddMilStrap(P.Body, { FVector(20.0, 262.0, 92.0), FVector(25.0, 300.0, 98.0), FVector(30.0, 325.0, 70.0), FVector(40.0, 338.0, 22.0), FVector(70.0, 350.0, 3.0),
				FVector(120.0, 372.0, 3.0) }, FVector::ZeroVector, Look);
			P.Info.SinkMax = 4.f;
			P.Info.TiltMax = 1.f;
			break;
		}
		case 2:
		{
			const double Beta = FMath::DegreesToRadians(-17.0);
			const double Lift = (MilHelmet::Rx + MilHelmet::Brim) * FMath::Sin(-Beta) - 2.0;
			AddMilHelmet(P, FBeachFrame(FVector(0.0, 0.0, Lift), FQuat(FVector(0.0, 1.0, 0.0), Beta)), 2, 0, 2, true, Look);
			AddBranch(P, { FVector(318.0, 30.0, -30.0), FVector(306.0, 20.0, 80.0), FVector(298.0, 6.0, 190.0) }, 13.0, 10.0, Look.Drift, true);
			AddMilAmmoCan(P, FVector(-60.0, 40.0, 0.0), YawQ(25.0), Look);
			AddMilStrap(P.Body, { FVector(318.0, 150.0, 188.0), FVector(335.0, 165.0, 120.0), FVector(352.0, 172.0, 30.0), FVector(372.0, 160.0, 3.0), FVector(420.0, 140.0, 3.0) },
				FVector::ZeroVector, Look);
			break;
		}
		case 3:
		{
			// De lado: la cúpula hacia +X, la boca (4,3 m de alto y 7 de ancho) hacia -X y el eje largo a lo ancho (Y).
			const FQuat Side = FQuat(FVector(0.0, 1.0, 0.0), UE_DOUBLE_HALF_PI) * YawQ(90.0);
			const FVector Origin(-130.0, 0.0, 130.0);
			AddMilHelmet(P, FBeachFrame(Origin, Side), 0, 0, 2, true, Look);
			// Suelo de arena por dentro a 12 cm: se estrecha hacia el fondo de la cúpula.
			TArray<FVector2D> Right;
			const double FloorLocal = 12.0 - Origin.Z;
			for (const double Dd : { 0.0, 50.0, 100.0, 145.0, 185.0, 215.0 })
			{
				const double SinL = FMath::Clamp((Dd - MilHelmet::Rim) / (MilHelmet::Depth - MilHelmet::Rim - MilHelmet::Thick), 0.0, 1.0);
				const double Shrink = FMath::Sqrt(FMath::Max(0.0, 1.0 - SinL * SinL));
				const double Ri = (MilHelmet::Rx - MilHelmet::Thick) * Shrink;
				const double Rj = (MilHelmet::Rx - MilHelmet::Thick) * MilHelmet::Squash * Shrink;
				const double Frac = FMath::Abs(FloorLocal) / FMath::Max(1.0, Rj);
				const double HalfW = Frac < 1.0 ? Ri * FMath::Sqrt(1.0 - Frac * Frac) - 10.0 : 0.0;
				Right.Add(FVector2D(Origin.X + Dd, FMath::Max(10.0, HalfW)));
			}
			TArray<FVector2D> Floor;
			for (const FVector2D& Pt : Right) { Floor.Add(FVector2D(Pt.X, -Pt.Y)); }
			for (int32 i = Right.Num() - 1; i >= 0; --i) { Floor.Add(Right[i]); }
			AddPrismPoly(P.Body, FBeachFrame(), Floor, -40.0, 12.0, Look.Sand, Look.SandDark);
			AddPrismPoly(P.Body, FBeachFrame(), { FVector2D(-128.0, -230.0), FVector2D(-175.0, -150.0), FVector2D(-200.0, 0.0), FVector2D(-175.0, 150.0), FVector2D(-128.0, 230.0) },
				-10.0, 7.0, Look.Sand, Look.SandDark);
			P.ColBox(FVector(-72.0, 0.0, -14.0), FQuat::Identity, FVector(62.0, 240.0, 26.0));
			P.ColBox(FVector(25.0, 0.0, -14.0), FQuat::Identity, FVector(38.0, 170.0, 26.0));
			// Arena amontonada a los lados.
			for (const double Sy : { -1.0, 1.0 })
			{
				AddBlob(P.Body, FBeachFrame(FVector(10.0, Sy * 318.0, -25.0), FQuat::Identity), 190.0, 80.0, 70.0, 8, 3, Look.Sand, 0.08, Seed);
			}
			AddMilStrap(P.Body, { FVector(-150.0, -120.0, 3.0), FVector(-215.0, -95.0, 3.0), FVector(-262.0, -30.0, 3.0), FVector(-245.0, 60.0, 3.0) }, FVector::UpVector, Look);
			break;
		}
		default:
		{
			AddMilHelmet(P, FBeachFrame(FVector(0.0, 0.0, -95.0), FQuat::Identity), 0, 0, 2, true, Look);
			// Arena removida alrededor del borde enterrado.
			AddRevolve(P.Body, FBeachFrame(), { FVector2D(395.0, -8.0), FVector2D(345.0, 10.0), FVector2D(300.0, 20.0) }, 12, Look.Sand, 0.06, Seed, MilHelmet::Squash);
			AddMilStrap(P.Body, { FVector(-60.0, 262.0, 4.0), FVector(-20.0, 330.0, 3.0), FVector(60.0, 372.0, 3.0), FVector(140.0, 352.0, 3.0), FVector(170.0, 300.0, 4.0) },
				FVector::UpVector, Look);
			P.Info.SinkMax = 6.f;
			P.Info.TiltMax = 2.f;
			break;
		}
		}
	}

	/**
	 * Red de camuflaje sobre palos (huella 15 m; 20 × 15,6 m): plana a 3,7 m sobre cuatro palos y uno en medio (4,5 m), con
	 * los lados colgando hasta 1,7 m y un faldón suelto que se mece (0); a dos aguas como un túnel abierto por ±X, con los
	 * lados de 26° que se suben andando hasta la cumbrera a 4,3 m (1); caída por -X, rampa de 16° hasta un techo a 3,8 m sobre
	 * dos palos, del que se salta (2); y la de 0 sobre un puesto de vigía (anillo de sacos, caja y soldadito) (3). Se pasa por
	 * debajo por cualquier lado abierto; los palos tienen colisión.
	 */
	inline void BuildMilCamoNet(FParts& P, int32 Variant, uint32 Seed)
	{
		const FMilLook Look = MilLookOf();
		const int32 Kind = Variant % 4;
		switch (Kind)
		{
		case 1:
		{
			auto NetZ = [](double X, double Y) -> double
			{
				const double Ay = FMath::Min(1.0, FMath::Abs(Y) / 860.0);
				const double Ax = FMath::Abs(X);
				double Z = 430.0 * (1.0 - Ay) - 22.0 * FMath::Sin(UE_DOUBLE_PI * Ay);
				Z -= Ax < 850.0 ? 18.0 * FMath::Cos(UE_DOUBLE_HALF_PI * X / 850.0) * (1.0 - Ay) : (Ax - 850.0) * 0.55 * (1.0 - Ay);
				return FMath::Max(2.0, Z);
			};
			auto NetPos = [&NetZ](double U, double V)
			{
				const double X = FMath::Lerp(-1000.0, 1000.0, U);
				const double Y = FMath::Lerp(-860.0, 860.0, V);
				return FVector(X, Y, NetZ(X, Y));
			};
			AddMilNet(P.Body, 16, 12, NetPos, 80, Seed);
			for (const double Sx : { -1.0, 1.0 })
			{
				const FVector Top(Sx * 850.0, 0.0, NetZ(Sx * 850.0, 0.0) - 5.0);
				AddMilPole(P, FVector(Sx * 850.0, 0.0, 0.0), Top, Look);
				AddMilGuy(P.Body, Top, FVector(Sx * 1180.0, 0.0, 0.0), Look);
				for (int32 k = 0; k < 5; ++k)
				{
					AddYawBox(P.Body, FVector(-800.0 + 400.0 * k, Sx * 870.0, 8.0), 15.0 * k, FVector(5.0, 5.0, 12.0), Look.PoleWood);
				}
			}
			// Los dos faldones: cajas inclinadas (se suben andando hasta la cumbrera).
			for (const double Sy : { -1.0, 1.0 })
			{
				const FVector RidgePt(0.0, 0.0, 420.0);
				const FVector FootPt(0.0, Sy * 860.0, 0.0);
				const FVector Slope = (FootPt - RidgePt).GetSafeNormal();
				FVector Nrm = FVector::CrossProduct(Slope, FVector(1.0, 0.0, 0.0));
				if (Nrm.Z < 0.0) { Nrm = -Nrm; }
				P.ColBox((RidgePt + FootPt) * 0.5 - Nrm * 10.0, FRotationMatrix::MakeFromXZ(Slope, Nrm).ToQuat(), FVector(FVector::Dist(RidgePt, FootPt) * 0.5, 1000.0, 10.0));
			}
			// Cortina que cuelga de la cumbrera por +X y se mece.
			const FVector Pivot(1000.0, 0.0, NetZ(1000.0, 0.0));
			AddSheet(P.Moving, 6, 3, [&NetZ](double U, double V)
			{
				const double Y = FMath::Lerp(-160.0, 160.0, U);
				return FVector(1004.0 + 6.0 * V, Y, NetZ(1000.0, Y) - 170.0 * V);
			}, 6.0, [Seed](int32 I, int32 J) { return MilCamoAt(I * 60.0, J * 90.0, Seed + 11u); });
			P.Info.Anim = EAnim::Sway;
			P.Info.AnimAxis = FVector(0.0, 1.0, 0.0);
			P.Info.AnimAmp = 8.f;
			P.Info.AnimRate = 0.3f;
			P.AnimAround(Pivot);
			break;
		}
		case 2:
		{
			auto NetZ = [](double X, double Y) -> double
			{
				double Z = X < 300.0 ? 380.0 * FMath::Clamp((X + 1000.0) / 1300.0, 0.0, 1.0) : 380.0 - FMath::Max(0.0, X - 850.0) * 0.9;
				const double Ay = FMath::Abs(Y);
				if (Ay > 600.0) { Z -= (Ay - 600.0) * 1.1 * FMath::Clamp((X + 400.0) / 700.0, 0.0, 1.0); }
				// Arrugas del paño caído.
				Z += 10.0 * FMath::Sin(Y * 0.011 + X * 0.004) * FMath::Clamp((300.0 - X) / 1300.0, 0.0, 1.0);
				return FMath::Max(3.0, Z);
			};
			auto NetPos = [&NetZ](double U, double V)
			{
				const double X = FMath::Lerp(-1000.0, 1000.0, U);
				const double Y = FMath::Lerp(-760.0, 760.0, V);
				return FVector(X, Y, NetZ(X, Y));
			};
			AddMilNet(P.Body, 16, 12, NetPos, 80, Seed);
			for (const double Sy : { -1.0, 1.0 })
			{
				const FVector Top(850.0, Sy * 600.0, NetZ(850.0, Sy * 600.0) - 5.0);
				AddMilPole(P, FVector(850.0, Sy * 600.0, 0.0), Top, Look);
				AddMilGuy(P.Body, Top, FVector(1150.0, Sy * 820.0, 0.0), Look);
				// Los palos de -X, caídos en la arena.
				const FVector FallA(-820.0, Sy * 560.0, 11.0);
				const FVector FallB(-380.0, Sy * 690.0, 12.0);
				AddTaperTube(P.Body, { FallA, FallB }, 11.0, 8.5, 6, Look.PoleWood, true);
				P.ColCapsule(FallA, FallB, 11.0);
			}
			// Rampa del paño caído y techo sobre los palos.
			const FVector RampLo(-1000.0, 0.0, 0.0);
			const FVector RampHi(300.0, 0.0, 380.0);
			const FVector Along = (RampHi - RampLo).GetSafeNormal();
			FVector Nrm = FVector::CrossProduct(FVector(0.0, 1.0, 0.0), Along);
			if (Nrm.Z < 0.0) { Nrm = -Nrm; }
			P.ColBox((RampLo + RampHi) * 0.5 - Nrm * 10.0, FRotationMatrix::MakeFromXZ(Along, Nrm).ToQuat(), FVector(FVector::Dist(RampLo, RampHi) * 0.5, 600.0, 10.0));
			P.ColBox(FVector(600.0, 0.0, 370.0), FQuat::Identity, FVector(300.0, 600.0, 10.0));
			// Faldón suelto por +X.
			const FVector Pivot(1000.0, 0.0, NetZ(1000.0, 0.0));
			AddSheet(P.Moving, 6, 3, [&NetZ](double U, double V)
			{
				const double Y = FMath::Lerp(-300.0, 300.0, U);
				return FVector(1004.0 + 8.0 * V, Y, NetZ(1000.0, Y) - 120.0 * V);
			}, 6.0, [Seed](int32 I, int32 J) { return MilCamoAt(I * 70.0, J * 90.0, Seed + 11u); });
			P.Info.Anim = EAnim::Sway;
			P.Info.AnimAxis = FVector(0.0, 1.0, 0.0);
			P.Info.AnimAmp = 9.f;
			P.Info.AnimRate = 0.32f;
			P.AnimAround(Pivot);
			break;
		}
		default:
		{
			auto NetZ = [](double X, double Y) -> double
			{
				const double Ax = FMath::Abs(X);
				const double Ay = FMath::Abs(Y);
				// Pico en el palo de en medio, comba entre los palos y los lados colgando.
				double Z = 370.0 + 80.0 * FMath::Max(0.0, 1.0 - FMath::Sqrt(FMath::Square(X / 850.0) + FMath::Square(Y / 600.0)));
				Z -= 35.0 * FMath::Sin(FMath::Clamp(Ax / 850.0, 0.0, 1.0) * UE_DOUBLE_PI) * FMath::Clamp(Ay / 600.0, 0.0, 1.0);
				if (Ax > 850.0) { Z -= (Ax - 850.0) * 0.9; }
				if (Ay > 600.0) { Z -= (Ay - 600.0) * 1.1; }
				return FMath::Max(20.0, Z);
			};
			auto NetPos = [&NetZ](double U, double V)
			{
				const double X = FMath::Lerp(-1000.0, 1000.0, U);
				const double Y = FMath::Lerp(-780.0, 780.0, V);
				return FVector(X, Y, NetZ(X, Y));
			};
			AddMilNet(P.Body, 16, 12, NetPos, 90, Seed);
			for (const double Sx : { -1.0, 1.0 })
			{
				for (const double Sy : { -1.0, 1.0 })
				{
					const FVector Top(Sx * 850.0, Sy * 600.0, NetZ(Sx * 850.0, Sy * 600.0) - 5.0);
					AddMilPole(P, FVector(Sx * 850.0, Sy * 600.0, 0.0), Top, Look);
					AddMilGuy(P.Body, Top, FVector(Sx * 1150.0, Sy * 850.0, 0.0), Look);
				}
			}
			AddMilPole(P, FVector::ZeroVector, FVector(0.0, 0.0, NetZ(0.0, 0.0) - 5.0), Look);
			if (Kind == 3)
			{
				// Puesto de vigía: anillo de sacos abierto por +X, una caja y un soldadito con los prismáticos.
				FMilParapet W;
				W.Path.bArc = true;
				W.Path.Center = FVector2D::ZeroVector;
				W.Path.Radius = 330.0;
				W.Path.Ang0 = 25.0;
				W.Path.Ang1 = 335.0;
				W.Rows = 3;
				W.StepRows = 0;
				AddMilParapet(P, W, Seed + 3u);
				AddMilCrate(P, FVector(-160.0, -110.0, 0.0), YawQ(30.0), 112.0, 66.0, 56.0, true, true, Seed + 4u, Look);
				AddMilSoldier(P, FBeachFrame(FVector(140.0, 130.0, 0.0), YawQ(RndIn(Seed, 9, -20.0, 20.0))), EMilPose::Binoculars, Hex(0x4E8A34, 0.3f), Seed + 5u);
			}
			// Faldón suelto que cuelga del lado +Y y se mece.
			const FVector Pivot(-100.0, 785.0, NetZ(-100.0, 780.0));
			AddSheet(P.Moving, 7, 3, [&NetZ](double U, double V)
			{
				const double X = FMath::Lerp(-450.0, 250.0, U);
				return FVector(X, 790.0 + 10.0 * V, NetZ(X, 780.0) - 110.0 * V - 14.0 * FMath::Sin(UE_DOUBLE_PI * U) * V);
			}, 6.0, [Seed](int32 I, int32 J) { return MilCamoAt(I * 60.0, J * 80.0, Seed + 11u); });
			P.Info.Anim = EAnim::Sway;
			P.Info.AnimAxis = FVector(1.0, 0.0, 0.0);
			P.Info.AnimAmp = 9.f;
			P.Info.AnimRate = 0.35f;
			P.AnimAround(Pivot);
			break;
		}
		}
		P.Info.SinkMax = 4.f;
		P.Info.TiltMax = 0.5f;
	}

	/**
	 * Bidón (huella 5 m; 2,15 × 0,8 × 3 m): de pie, verde oliva con la escarapela (0: 3 m, para cubrirse), tumbado, caqui
	 * del desierto (1: 80 cm, un escalón), dos tumbados en cruz, verde y rojo (2: 80 y 160 cm) o rojo de gasolina,
	 * tumbado con el tapón abierto y un charco tornasolado delante de la boca (3).
	 */
	inline void BuildMilJerrycan(FParts& P, int32 Variant, uint32 Seed)
	{
		const FMilLook Look = MilLookOf();
		const int32 Kind = Variant % 4;
		const FLinearColor Red = Hex(0xB8342A, 0.2f);
		const FLinearColor Desert = Hex(0xC2A46A, 0.08f);
		// Tumbado sobre una cara grande: el alto del bidón hacia -Y y el grueso hacia arriba.
		const FQuat Lying(FVector(1.0, 0.0, 0.0), UE_DOUBLE_HALF_PI);
		const double LieZ = MilCan::T * 0.5;
		switch (Kind)
		{
		case 1:
			AddMilJerrycan(P, FVector(0.0, MilCan::H * 0.5, LieZ), Lying, Desert, true, false, Look);
			break;
		case 2:
			AddMilJerrycan(P, FVector(0.0, MilCan::H * 0.5, LieZ), Lying, Look.Olive, true, false, Look);
			AddMilJerrycan(P, FVector(-MilCan::H * 0.5, 0.0, MilCan::T + LieZ), YawQ(90.0) * Lying, Red, false, false, Look);
			break;
		case 3:
		{
			AddMilJerrycan(P, FVector(0.0, MilCan::H * 0.5, LieZ), Lying, Red, false, true, Look);
			// Charco tornasolado delante de la boca (la boca queda en -Y).
			const FBeachFrame Puddle(FVector(60.0, -225.0, 0.0), YawQ(RndIn(Seed, 3, -20.0, 20.0)));
			AddPrismPoly(P.Body, Puddle, WobblyEllipse(150.0, 95.0, 12, 0.12, Seed), -2.0, 2.0, Hex(0x3C2E52, 0.9f), Hex(0x3C2E52, 0.9f));
			AddPrismPoly(P.Body, Puddle, WobblyEllipse(108.0, 68.0, 11, 0.14, Seed + 1u), 2.0, 3.0, Hex(0x2E8C8C, 0.9f), Hex(0x2E8C8C, 0.9f));
			AddPrismPoly(P.Body, Puddle, WobblyEllipse(58.0, 38.0, 9, 0.16, Seed + 2u), 3.0, 4.0, Hex(0xC9A13B, 0.9f), Hex(0xC9A13B, 0.9f));
			break;
		}
		default:
			AddMilJerrycan(P, FVector::ZeroVector, FQuat::Identity, Look.Olive, true, false, Look);
			break;
		}
		P.Info.SinkMin = 4.f;
		P.Info.SinkMax = 12.f;
		P.Info.TiltMax = 2.f;
	}

	/**
	 * Soldaditos de juguete verdes de 1,4 m (huella 6 m): solo con el fusil (0), con los prismáticos (1), con el bazuca (2)
	 * o tumbado (3); pareja fusil y prismáticos (4); trío con bazuca, tumbado y uno volcado (5); uno volcado y otro mirando
	 * (6); y patrulla en fila de tres (7).
	 */
	inline void BuildMilToySoldiers(FParts& P, int32 Variant, uint32 Seed)
	{
		static const uint32 GreenHex[4] = { 0x4E8A34, 0x5A9A3C, 0x467E2E, 0x6E9A56 };
		auto Green = [Seed](int32 Index) { return Hex(GreenHex[(Index + static_cast<int32>(Seed % 3u)) % 4], 0.3f); };
		auto Stand = [&P, Seed](double X, double Y, double YawDeg, EMilPose Pose, const FLinearColor& Col, int32 Salt)
		{
			AddMilSoldier(P, FBeachFrame(FVector(X, Y, 0.0), YawQ(YawDeg)), Pose, Col, Seed + static_cast<uint32>(Salt));
		};
		const int32 Kind = Variant % 8;
		switch (Kind)
		{
		case 1: Stand(0.0, 0.0, 0.0, EMilPose::Binoculars, Green(0), 1); break;
		case 2: Stand(0.0, 0.0, 0.0, EMilPose::Bazooka, Green(0), 1); break;
		case 3: Stand(0.0, 0.0, 0.0, EMilPose::Prone, Green(0), 1); break;
		case 4:
			Stand(-60.0, 110.0, 8.0, EMilPose::Rifle, Green(0), 1);
			Stand(70.0, -100.0, -12.0, EMilPose::Binoculars, Green(1), 2);
			break;
		case 5:
			Stand(-30.0, 160.0, 15.0, EMilPose::Bazooka, Green(0), 1);
			Stand(100.0, -60.0, -5.0, EMilPose::Prone, Green(1), 2);
			AddMilFallenSoldier(P, -170.0, -170.0, 60.0, EMilPose::Rifle, Green(2), Seed + 3u);
			break;
		case 6:
			AddMilFallenSoldier(P, -40.0, 60.0, -30.0, EMilPose::Rifle, Green(3), Seed + 1u);
			Stand(110.0, -90.0, 20.0, EMilPose::Binoculars, Green(0), 2);
			break;
		case 7:
			Stand(-10.0, -210.0, 0.0, EMilPose::Rifle, Green(0), 1);
			Stand(20.0, 0.0, 4.0, EMilPose::Rifle, Green(1), 2);
			Stand(-20.0, 210.0, -6.0, EMilPose::Binoculars, Green(2), 3);
			break;
		default: Stand(0.0, 0.0, 0.0, EMilPose::Rifle, Green(0), 1); break;
		}
		P.Info.SinkMax = 4.f;
		P.Info.TiltMax = 3.f;
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Reparto (declarados en TN_BeachPropMeshes.h)
	// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

	inline int32 NumMilitaryVariants(ETNBeachElement E)
	{
		switch (E)
		{
		case ETNBeachElement::Sandbags:    return 6;
		case ETNBeachElement::ToySoldiers: return 8;
		default:                           return 4;
		}
	}

	inline void BuildMilitaryDecor(FParts& P, ETNBeachElement E, int32 Variant, uint32 Seed)
	{
		switch (E)
		{
		case ETNBeachElement::Sandbags:       BuildMilSandbags(P, Variant, Seed); break;
		case ETNBeachElement::TankTrap:       BuildMilTankTrap(P, Variant, Seed); break;
		case ETNBeachElement::MilitaryHelmet: BuildMilHelmet(P, Variant, Seed); break;
		case ETNBeachElement::CamoNet:        BuildMilCamoNet(P, Variant, Seed); break;
		case ETNBeachElement::Jerrycan:       BuildMilJerrycan(P, Variant, Seed); break;
		case ETNBeachElement::ToySoldiers:    BuildMilToySoldiers(P, Variant, Seed); break;
		default:                              BuildMilAmmoCrate(P, Variant, Seed); break;
		}
	}
}
