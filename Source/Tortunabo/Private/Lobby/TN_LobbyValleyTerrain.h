#pragma once

#include "CoreMinimal.h"
#include "Algo/BinarySearch.h"
#include "Async/ParallelFor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "World/ProcMap/TN_ProcMapFlora.h"
#include "World/ProcMap/TN_ProcMapMath.h"
#include "../World/ProcMap/TN_ProcMapMeshKit.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"
#include "TN_CastleKit.h"

/**
 * Valle del lobby (ATN_LobbyValley), lógica pura: los doce sectores del reloj (un bioma por hora), las alturas del
 * terreno (suelo del valle, relieve de cada sector, sierra que lo cierra y cordillera lejana), los colores de sus caras
 * planas, los accidentes que pasan por encima de los sectores (volcanes, isletas, cabo, colinas) y la rejilla polar de
 * la malla con su altura interpolada (la misma que se ve: con ella se planta la vegetación y se posan los animales).
 *
 * Coordenadas locales del valle en cm, centrado en el castillo. Reloj como el del castillo: las 12 son +Y (la puerta
 * doble), las 3 son -X, las 6 son -Y y las 9 son +X.
 */
namespace TNLobbyValley
{
	constexpr int32 NumSectors = 12;
	/** Primer anillo del terreno: un poco por dentro del borde de la playa del castillo (3900) y por debajo de ella. */
	constexpr double InnerR = 3870.0;
	/** Borde exterior: detrás de la cordillera lejana (no se ve desde el valle). */
	constexpr double OuterR = 42000.0;
	/** Nivel del agua de la laguna y del manglar. */
	constexpr double WaterZ = -25.0;
	/** Radios de las casillas de la rejilla polar. */
	constexpr int32 Spokes = 240;
	/** Los animales viven entre estos radios: fuera del castillo y del valle cercano, pero a la vista desde la azotea. */
	constexpr double FaunaMinR = 6900.0;
	constexpr double FaunaMaxR = 12600.0;

	/** Cómo es el relieve de cada sector (el bioma decide la vegetación, la fauna y los colores de las formaciones). */
	enum class EValleyStyle : uint8
	{
		Lagoon,    ///< Laguna turquesa con isletas y cascada (las 12, delante de la puerta doble).
		Beach,     ///< Playa con palmeras y un cabo con faro.
		Dunes,     ///< Dunas con cactus y la tortuga colosal.
		Canyon,    ///< Cañón de mesas rojas con chimeneas de hadas.
		Volcano,   ///< Volcanes con lava que brilla y columna de humo.
		Cliffs,    ///< Acantilados en terrazas, agujas y un castillo en ruinas.
		Snow,      ///< Cumbres nevadas (detrás de la torre del homenaje).
		Forest,    ///< Bosque de pinos y abetos con un círculo de piedras.
		Village,   ///< Colinas con casitas, molino y depósito de agua.
		Farms,     ///< Campos de colores, pacas y otro molino.
		Jungle,    ///< Selva frondosa con pirámide y pilares kársticos.
		Mangrove,  ///< Manglar de agua somera con palafito.
		Count
	};

	struct FValleySector
	{
		EValleyStyle Style = EValleyStyle::Lagoon;
		ETNProcBiome Biome = ETNProcBiome::Water;
		/** Alto de la sierra de detrás del sector (cm, antes de los picos: estos lo multiplican entre 0,5 y 1,25). */
		double RidgeH = 5000.0;
		/** Cota de la nieve (cm). */
		double SnowZ = 7400.0;
		const TCHAR* Name = TEXT("");
	};

	/** Sector de una hora del reloj (0 = las 12). */
	inline const FValleySector& ValleySectorAt(int32 Index)
	{
		static const FValleySector Table[NumSectors] = {
			{ EValleyStyle::Lagoon, ETNProcBiome::Water, 5200.0, 7400.0, TEXT("Laguna") },
			{ EValleyStyle::Beach, ETNProcBiome::Beach, 3900.0, 7400.0, TEXT("Playa") },
			{ EValleyStyle::Dunes, ETNProcBiome::Desert, 4300.0, 99999.0, TEXT("Dunas") },
			{ EValleyStyle::Canyon, ETNProcBiome::Desert, 5000.0, 99999.0, TEXT("Cañón") },
			{ EValleyStyle::Volcano, ETNProcBiome::Volcanic, 5200.0, 99999.0, TEXT("Volcán") },
			{ EValleyStyle::Cliffs, ETNProcBiome::Rocky, 6800.0, 6300.0, TEXT("Acantilados") },
			{ EValleyStyle::Snow, ETNProcBiome::Rocky, 9000.0, 4300.0, TEXT("Cumbres nevadas") },
			{ EValleyStyle::Forest, ETNProcBiome::Rocky, 7200.0, 6200.0, TEXT("Bosque") },
			{ EValleyStyle::Village, ETNProcBiome::Human, 4600.0, 7400.0, TEXT("Pueblo") },
			{ EValleyStyle::Farms, ETNProcBiome::Human, 4200.0, 7400.0, TEXT("Granjas") },
			{ EValleyStyle::Jungle, ETNProcBiome::Jungle, 5600.0, 7400.0, TEXT("Selva") },
			{ EValleyStyle::Mangrove, ETNProcBiome::Mangrove, 4600.0, 7400.0, TEXT("Manglar") },
		};
		return Table[((Index % NumSectors) + NumSectors) % NumSectors];
	}

	/** Color sRGB 0xRRGGBB tal y como lo recibe el material de color de vértice (lineal, alfa 0). */
	inline FLinearColor ValleyHex(uint32 Hex)
	{
		return TNCastleKit::Col(Hex);
	}

	/** Punto a la hora Hour del reloj y a Dist del centro. */
	inline FVector2D ValleyClockPoint(double Hour, double Dist)
	{
		const double A = Hour / 12.0 * TNProcMap::TwoPi;
		return FVector2D(-Dist * FMath::Sin(A), Dist * FMath::Cos(A));
	}

	/** Hora del reloj (0..12) de un punto. */
	inline double ValleyHourOf(const FVector2D& P)
	{
		double A = FMath::Atan2(-P.X, P.Y);
		if (A < 0.0) { A += TNProcMap::TwoPi; }
		return A / TNProcMap::TwoPi * 12.0;
	}

	/** Mezcla de sectores en un punto: el suyo y, cerca de la frontera, el vecino (los pesos suman 1). */
	struct FValleyMix
	{
		int32 A = 0;
		int32 B = 0;
		double WA = 1.0;
		double WB = 0.0;
	};

	/** Transición entre sectores: 0,2 horas (6°) a cada lado de la frontera. */
	inline FValleyMix ValleyMixAtHour(double Hour)
	{
		constexpr double Band = 0.2;
		const double X = Hour + 0.5;
		const double Fl = FMath::FloorToDouble(X);
		const double F = X - Fl;
		const int32 I = ((static_cast<int32>(Fl) % NumSectors) + NumSectors) % NumSectors;
		auto Ease = [](double T)
		{
			const double C = FMath::Clamp(T, 0.0, 1.0);
			return C * C * (3.0 - 2.0 * C);
		};
		FValleyMix M;
		M.A = I;
		M.B = I;
		if (F < Band)
		{
			M.B = (I + NumSectors - 1) % NumSectors;
			M.WB = 0.5 * (1.0 - Ease(F / Band));
		}
		else if (F > 1.0 - Band)
		{
			M.B = (I + 1) % NumSectors;
			M.WB = 0.5 * (1.0 - Ease((1.0 - F) / Band));
		}
		M.WA = 1.0 - M.WB;
		return M;
	}

	/** Volcán: cono con cráter (se suma por encima de los sectores: no se corta en la frontera). */
	struct FValleyCone
	{
		double Hour;
		double Dist;
		double BaseR;
		double Height;
		double CraterR;
		double CraterDepth;
	};

	constexpr int32 NumValleyCones = 2;

	/** 0: el volcán grande de la sierra (las 4); 1: el pequeño del valle, entre el cañón y el volcán. */
	inline const FValleyCone& ValleyCone(int32 Index)
	{
		static const FValleyCone Cones[NumValleyCones] = {
			{ 4.05, 16800.0, 6200.0, 7400.0, 1050.0, 1000.0 },
			{ 3.72, 10200.0, 2400.0, 1500.0, 380.0, 260.0 },
		};
		return Cones[FMath::Clamp(Index, 0, NumValleyCones - 1)];
	}

	/** Cota del borde del cráter de un cono con el suelo a BaseZ. */
	inline double ValleyConeRimZ(const FValleyCone& Cn, double BaseZ)
	{
		return BaseZ + Cn.Height * FMath::Pow(1.0 - Cn.CraterR / Cn.BaseR, 1.5);
	}

	/** Montículo: isleta (Top es su cota absoluta), cabo o colina (Top es lo que sube sobre el suelo del valle). */
	struct FValleyMound
	{
		double Hour;
		double Dist;
		double Radius;
		double Top;
	};

	inline TArrayView<const FValleyMound> ValleyIslets()
	{
		static const FValleyMound List[] = {
			{ 11.78, 9600.0, 1400.0, 150.0 }, { 0.34, 8300.0, 950.0, 115.0 }, { 0.12, 11300.0, 760.0, 95.0 },
			{ 11.5, 7500.0, 650.0, 70.0 }, { 0.62, 10300.0, 620.0, 60.0 },
		};
		return MakeArrayView(List);
	}

	/** Cabo de la playa (con el faro) y colinas del pueblo y de las granjas (con los molinos). */
	inline TArrayView<const FValleyMound> ValleyHills()
	{
		static const FValleyMound List[] = {
			{ 1.33, 11800.0, 2300.0, 950.0 },
			{ 7.72, 9300.0, 2600.0, 620.0 }, { 8.22, 11600.0, 3100.0, 980.0 }, { 8.48, 8300.0, 2000.0, 420.0 },
			{ 9.35, 12200.0, 2600.0, 520.0 },
		};
		return MakeArrayView(List);
	}

	/** Colores del terreno de un estilo: suelo (dos tonos), roca (dos tonos) y los de la sierra. */
	struct FValleyPalette
	{
		FLinearColor Ground;
		FLinearColor Ground2;
		FLinearColor Rock;
		FLinearColor Rock2;
		FLinearColor MtGround;
		FLinearColor MtRock;
	};

	inline const FValleyPalette& ValleyPaletteOf(EValleyStyle Style)
	{
		static const FValleyPalette Table[static_cast<int32>(EValleyStyle::Count)] = {
			/* Laguna */      { ValleyHex(0xEFD9A0), ValleyHex(0x7BC45A), ValleyHex(0x8E8A7E), ValleyHex(0x7A766C), ValleyHex(0x4E8F3C), ValleyHex(0x857F74) },
			/* Playa */       { ValleyHex(0xF3DFA6), ValleyHex(0xA9B866), ValleyHex(0xB5A48A), ValleyHex(0x9E8E76), ValleyHex(0x9BAA5A), ValleyHex(0xA89A84) },
			/* Dunas */       { ValleyHex(0xF0C27A), ValleyHex(0xF7D79A), ValleyHex(0xC98A58), ValleyHex(0xB3764A), ValleyHex(0xD29560), ValleyHex(0xB9764A) },
			/* Cañón */       { ValleyHex(0xE2A870), ValleyHex(0xD6A265), ValleyHex(0xD9733F), ValleyHex(0xB9552E), ValleyHex(0xCF8E58), ValleyHex(0xC06A3C) },
			/* Volcán */      { ValleyHex(0x4A403A), ValleyHex(0x6A625A), ValleyHex(0x2A2424), ValleyHex(0x3A2E2A), ValleyHex(0x3A3230), ValleyHex(0x221E1E) },
			/* Acantilados */ { ValleyHex(0x8FA56A), ValleyHex(0xA3B478), ValleyHex(0x8E8E94), ValleyHex(0x76767E), ValleyHex(0x7F8F62), ValleyHex(0x85858C) },
			/* Nieve */       { ValleyHex(0x86B85C), ValleyHex(0x9CC46A), ValleyHex(0x7E818C), ValleyHex(0x6C6F7A), ValleyHex(0x6E9A50), ValleyHex(0x7A7D88) },
			/* Bosque */      { ValleyHex(0x4F8A3A), ValleyHex(0x62A046), ValleyHex(0x7A7A70), ValleyHex(0x66665E), ValleyHex(0x3F7432), ValleyHex(0x707068) },
			/* Pueblo */      { ValleyHex(0x8DCB5E), ValleyHex(0xA5D86E), ValleyHex(0x9A9486), ValleyHex(0x88826F), ValleyHex(0x6FA84C), ValleyHex(0x8C877A) },
			/* Granjas */     { ValleyHex(0x92C85C), ValleyHex(0xB5D46A), ValleyHex(0x9A9486), ValleyHex(0x88826F), ValleyHex(0x77AE50), ValleyHex(0x8C877A) },
			/* Selva */       { ValleyHex(0x3F8F34), ValleyHex(0x55A83E), ValleyHex(0x6E7D5C), ValleyHex(0x5A6848), ValleyHex(0x2F7A2C), ValleyHex(0x5E6B4E) },
			/* Manglar */     { ValleyHex(0x4F7F38), ValleyHex(0x6E6A40), ValleyHex(0x5E6048), ValleyHex(0x4C4E3A), ValleyHex(0x3F7432), ValleyHex(0x5E6048) },
		};
		return Table[FMath::Clamp(static_cast<int32>(Style), 0, static_cast<int32>(EValleyStyle::Count) - 1)];
	}

	/** Escalones de terraza: planos con un talud corto y empinado entre uno y otro (cañón, acantilados). */
	inline double ValleyTerrace(double V)
	{
		if (V <= 0.0) { return 0.0; }
		const double Fl = FMath::FloorToDouble(V);
		return Fl + TNProcMap::SmoothStep(0.7, 0.92, V - Fl);
	}

	/** Campana suave: 1 en el centro, 0 en el borde (Q = distancia / radio). */
	inline double ValleyBump(double Q)
	{
		if (Q >= 1.0) { return 0.0; }
		const double T = 1.0 - Q * Q;
		return T * T;
	}

	/**
	 * Forma del valle a partir de la semilla: alturas y colores. Todo es determinista y sin estado (se puede llamar desde
	 * varios hilos).
	 */
	struct FValleyShape
	{
		uint32 S = 0;

		explicit FValleyShape(uint32 InSeed) : S(InSeed) {}

		/** Suelo del valle: casi llano junto al castillo (2-6 cm bajo su playa) y subiendo poco a poco. */
		double Floor(double R, const FVector2D& P) const
		{
			if (R < InnerR + 130.0)
			{
				return FMath::Lerp(2.0, 6.0, FMath::Clamp((R - InnerR) / 130.0, 0.0, 1.0));
			}
			const double Noise = 18.0 * TNProcMap::SmoothStep(4300.0, 6200.0, R) * TNProcMap::Fbm2(S + 1u, P.X / 2500.0, P.Y / 2500.0, 2);
			return 6.0 + 50.0 * TNProcMap::SmoothStep(4000.0, 9000.0, R) + Noise;
		}

		/** Sector (con su vecino en la frontera) de un punto; las fronteras ondulan para no ser radios rectos. */
		FValleyMix Mix(const FVector2D& P, double R) const
		{
			const double Wobble = 0.14 * TNProcMap::Noise2(S + 50u, R / 4500.0, 0.37) + 0.05 * TNProcMap::Noise2(S + 51u, P.X / 2600.0, P.Y / 2600.0);
			return ValleyMixAtHour(ValleyHourOf(P) + Wobble);
		}

		/** Sierra que cierra el valle: sube desde 120 m hasta la cresta (~205 m) y baja por detrás; picos crestados. */
		double Mountain(const FVector2D& P, double R, const FValleySector& Sec) const
		{
			if (R < 11500.0) { return 0.0; }
			const double A = FMath::Atan2(-P.X, P.Y);
			const double RidgeR = 20500.0 + 1400.0 * TNProcMap::Noise2(S + 2u, FMath::Cos(A) * 3.0, FMath::Sin(A) * 3.0);
			const double Rise = FMath::Pow(TNProcMap::SmoothStep(12000.0, RidgeR, R), 1.25);
			const double Fall = 1.0 - 0.72 * TNProcMap::SmoothStep(RidgeR, RidgeR + 6500.0, R);
			const double Peaks = 0.5 + 0.75 * TNProcMap::Ridged2(S + 3u, P.X / 7500.0, P.Y / 7500.0, 3);
			const double Crag = 1.0 + 0.12 * TNProcMap::Fbm2(S + 4u, P.X / 1500.0, P.Y / 1500.0, 2);
			return Sec.RidgeH * Rise * Fall * Peaks * Crag;
		}

		/** Cordillera lejana (a ~350 m): asoma por encima de la sierra y tapa todo el horizonte. */
		double Far(const FVector2D& P, double R) const
		{
			if (R < 23000.0) { return 0.0; }
			const double Peaks = 6500.0 + 6000.0 * TNProcMap::Ridged2(S + 5u, P.X / 9000.0, P.Y / 9000.0, 3);
			const double Q = (R - 35000.0) / 5200.0;
			return TNProcMap::SmoothStep(23000.0, 26000.0, R) * (Peaks * FMath::Exp(-Q * Q) + 900.0 * TNProcMap::SmoothStep(23000.0, 30000.0, R));
		}

		/** Relieve propio de cada estilo (se suma al suelo; lo que va bajo el agua lo lleva a su cota). */
		double StyleTerm(EValleyStyle Style, const FVector2D& P, double R, double FloorZ) const
		{
			const double Zone = TNProcMap::SmoothStep(5200.0, 7600.0, R);
			switch (Style)
			{
				case EValleyStyle::Lagoon:
				{
					// Cubeta de la laguna (~2 m de fondo) y, al fondo, el cantil de la cascada.
					const double Basin = TNProcMap::SmoothStep(4600.0, 6600.0, R) * (1.0 - TNProcMap::SmoothStep(13000.0, 14600.0, R));
					const double Bed = WaterZ - 205.0 + 55.0 * TNProcMap::Fbm2(S + 20u, P.X / 1800.0, P.Y / 1800.0, 2);
					const double Cliff = 2300.0 * TNProcMap::SmoothStep(14000.0, 15200.0, R) * (0.85 + 0.15 * TNProcMap::Fbm2(S + 21u, P.X / 2500.0, P.Y / 2500.0, 2));
					return Basin * (Bed - FloorZ) + Cliff;
				}
				case EValleyStyle::Beach:
					return Zone * (35.0 + 55.0 * TNProcMap::Fbm2(S + 22u, P.X / 2200.0, P.Y / 2200.0, 2))
						+ 420.0 * TNProcMap::SmoothStep(10500.0, 14500.0, R) * (0.6 + 0.4 * TNProcMap::Fbm2(S + 23u, P.X / 3000.0, P.Y / 3000.0, 2));
				case EValleyStyle::Dunes:
				{
					// Crestas alargadas perpendiculares al viento.
					const double Wa = 0.6;
					const double U = (P.X * FMath::Cos(Wa) + P.Y * FMath::Sin(Wa)) / 1900.0;
					const double V = (-P.X * FMath::Sin(Wa) + P.Y * FMath::Cos(Wa)) / 5200.0;
					const double Dune = FMath::Pow(TNProcMap::Ridged2(S + 24u, U, V, 2), 1.5);
					return Zone * (110.0 + 480.0 * Dune + 200.0 * TNProcMap::Fbm2(S + 25u, P.X / 5000.0, P.Y / 5000.0, 2));
				}
				case EValleyStyle::Canyon:
				{
					const double N = 0.5 + 0.5 * TNProcMap::Fbm2(S + 26u, P.X / 5200.0, P.Y / 5200.0, 3);
					const double Level = N * 2.2 + TNProcMap::SmoothStep(6500.0, 15000.0, R) * 1.7 - 0.6;
					return Zone * (720.0 * ValleyTerrace(Level) + 40.0 * TNProcMap::Fbm2(S + 27u, P.X / 900.0, P.Y / 900.0, 2));
				}
				case EValleyStyle::Volcano:
					return Zone * (70.0 + 130.0 * (0.5 + 0.5 * TNProcMap::Fbm2(S + 28u, P.X / 1600.0, P.Y / 1600.0, 3)));
				case EValleyStyle::Cliffs:
				{
					const double N = 0.5 + 0.5 * TNProcMap::Fbm2(S + 29u, P.X / 3800.0, P.Y / 3800.0, 3);
					const double Level = N * 1.8 + TNProcMap::SmoothStep(6500.0, 14500.0, R) * 2.6 - 0.7;
					return Zone * (820.0 * ValleyTerrace(Level) + 80.0 * TNProcMap::Fbm2(S + 30u, P.X / 900.0, P.Y / 900.0, 2));
				}
				case EValleyStyle::Snow:
					return Zone * TNProcMap::SmoothStep(6000.0, 12000.0, R) * (250.0 + 500.0 * (0.5 + 0.5 * TNProcMap::Fbm2(S + 31u, P.X / 3500.0, P.Y / 3500.0, 3)));
				case EValleyStyle::Forest:
					return Zone * (180.0 + 650.0 * (0.5 + 0.5 * TNProcMap::Fbm2(S + 32u, P.X / 3000.0, P.Y / 3000.0, 3)));
				case EValleyStyle::Village:
					return Zone * (100.0 + 140.0 * TNProcMap::Fbm2(S + 33u, P.X / 3000.0, P.Y / 3000.0, 2));
				case EValleyStyle::Farms:
					return Zone * (70.0 + 380.0 * TNProcMap::SmoothStep(7000.0, 15000.0, R) + 50.0 * TNProcMap::Fbm2(S + 34u, P.X / 4000.0, P.Y / 4000.0, 2));
				case EValleyStyle::Jungle:
					return Zone * (180.0 + 650.0 * (0.5 + 0.5 * TNProcMap::Fbm2(S + 35u, P.X / 2600.0, P.Y / 2600.0, 3)));
				case EValleyStyle::Mangrove:
				{
					// Llanura de fango a ras de agua: la mitad asoma y la otra mitad queda somera.
					const double Mask = TNProcMap::SmoothStep(4800.0, 6600.0, R) * (1.0 - TNProcMap::SmoothStep(12500.0, 14200.0, R));
					const double Flat = WaterZ - 45.0 + 100.0 * TNProcMap::Fbm2(S + 36u, P.X / 1300.0, P.Y / 1300.0, 2);
					return Mask * (Flat - FloorZ);
				}
				default:
					return 0.0;
			}
		}

		/** Cota de un cono en P (-1e9 fuera de su base); bOutCrater, dentro del cráter (manda sobre todo lo demás). */
		double ConeHeight(const FValleyCone& Cn, const FVector2D& P, double BaseZ, bool& bOutCrater) const
		{
			bOutCrater = false;
			const FVector2D C = ValleyClockPoint(Cn.Hour, Cn.Dist);
			const double D = FVector2D::Distance(P, C);
			if (D >= Cn.BaseR) { return -1.0e9; }
			if (D < Cn.CraterR)
			{
				bOutCrater = true;
				const double Q = D / Cn.CraterR;
				return ValleyConeRimZ(Cn, BaseZ) - Cn.CraterDepth * (1.0 - Q * Q);
			}
			// Barrancos radiales en las laderas (no en el borde del cráter).
			const double Ang = FMath::Atan2(P.Y - C.Y, P.X - C.X);
			const double Gully = 1.0 - 0.07 * FMath::Abs(FMath::Sin(Ang * 7.0 + 0.4)) * TNProcMap::SmoothStep(Cn.CraterR, Cn.CraterR * 1.8, D);
			return BaseZ + Cn.Height * FMath::Pow(1.0 - D / Cn.BaseR, 1.5) * Gully;
		}

		/** Altura del terreno en P. */
		double Height(const FVector2D& P) const
		{
			const double R = P.Size();
			const double F = Floor(R, P);
			const FValleyMix M = Mix(P, R);
			const FValleySector& SA = ValleySectorAt(M.A);
			double H = F + Far(P, R) + M.WA * (StyleTerm(SA.Style, P, R, F) + Mountain(P, R, SA));
			if (M.WB > 0.0)
			{
				const FValleySector& SB = ValleySectorAt(M.B);
				H += M.WB * (StyleTerm(SB.Style, P, R, F) + Mountain(P, R, SB));
			}
			for (const FValleyMound& Isl : ValleyIslets())
			{
				const double Q = FVector2D::Distance(P, ValleyClockPoint(Isl.Hour, Isl.Dist)) / Isl.Radius;
				if (Q < 1.0)
				{
					const double Low = WaterZ - 220.0;
					H = FMath::Max(H, Low + (Isl.Top - Low) * TNProcMap::SmoothStep(1.0, 0.3, Q));
				}
			}
			for (const FValleyMound& Hill : ValleyHills())
			{
				const double Q = FVector2D::Distance(P, ValleyClockPoint(Hill.Hour, Hill.Dist)) / Hill.Radius;
				if (Q < 1.0)
				{
					const double Lump = 0.92 + 0.08 * TNProcMap::Noise2(S + 37u, P.X / 900.0, P.Y / 900.0);
					H = FMath::Max(H, F + Hill.Top * ValleyBump(Q) * Lump);
				}
			}
			for (int32 c = 0; c < NumValleyCones; ++c)
			{
				bool bCrater = false;
				const double Cz = ConeHeight(ValleyCone(c), P, F, bCrater);
				H = bCrater ? Cz : FMath::Max(H, Cz);
			}
			return H;
		}

		/** Campos de las granjas: parcelas de colores en una cuadrícula algo sesgada, con setos entre ellas. */
		FLinearColor FieldColor(const FVector2D& P) const
		{
			const FVector2D Dr = ValleyClockPoint(9.0, 1.0);
			const FVector2D Da(-Dr.Y, Dr.X);
			const double A = FVector2D::DotProduct(P, Dr) / 1500.0;
			const double B = (FVector2D::DotProduct(P, Da) + 0.12 * FVector2D::DotProduct(P, Dr)) / 1150.0;
			const int32 Ia = FMath::FloorToInt32(A);
			const int32 Ib = FMath::FloorToInt32(B);
			const double Fa = A - Ia;
			const double Fb = B - Ib;
			static const FLinearColor Hedge = ValleyHex(0x5E8C3A);
			if (FMath::Min(FMath::Min(Fa, 1.0 - Fa), FMath::Min(Fb, 1.0 - Fb)) < 0.06) { return Hedge; }
			static const FLinearColor Fields[6] = { ValleyHex(0xE8C95A), ValleyHex(0x8CC850), ValleyHex(0x6FAE44), ValleyHex(0xA0714A), ValleyHex(0xA68ED6), ValleyHex(0xF0CF3A) };
			return Fields[TNProcMap::HashCell(S + 60u, Ia, Ib) % 6u];
		}

		/** Color de una cara del terreno en un sector (sin el agua, la playa del castillo, los conos ni la bruma). */
		FLinearColor StyleColor(const FValleySector& Sec, const FVector2D& P, double R, double Z, const FVector& N) const
		{
			const FValleyPalette& Pal = ValleyPaletteOf(Sec.Style);
			const double Up = N.Z;
			const float Steep = static_cast<float>(TNProcMap::SmoothStep(0.86, 0.6, Up));
			const float Sheer = static_cast<float>(TNProcMap::SmoothStep(0.6, 0.3, Up));
			const float Mt = static_cast<float>(TNProcMap::SmoothStep(12500.0, 17500.0, R));
			const double Patch = TNProcMap::Noise2(S + 41u, P.X / 1700.0, P.Y / 1700.0);
			FLinearColor Ground = TNProcMesh::TNProcLerpColor(Pal.Ground, Pal.Ground2, static_cast<float>(TNProcMap::SmoothStep(-0.2, 0.5, Patch)));
			FLinearColor Rock = TNProcMesh::TNProcLerpColor(Pal.Rock, Pal.Rock2, static_cast<float>(0.5 + 0.5 * FMath::Sin(Z / 190.0 + Patch)));
			FLinearColor MtRock = Pal.MtRock;
			switch (Sec.Style)
			{
				case EValleyStyle::Lagoon:
					// Arena en la orilla y hierba en cuanto sube.
					Ground = TNProcMesh::TNProcLerpColor(Pal.Ground, Pal.Ground2, static_cast<float>(TNProcMap::SmoothStep(WaterZ + 20.0, WaterZ + 90.0, Z)));
					break;
				case EValleyStyle::Beach:
				{
					static const FLinearColor Wet = ValleyHex(0xDCC48C);
					Ground = TNProcMesh::TNProcLerpColor(Pal.Ground, Wet, static_cast<float>(TNProcMap::SmoothStep(WaterZ + 45.0, WaterZ + 8.0, Z)));
					Ground = TNProcMesh::TNProcLerpColor(Ground, Pal.Ground2, static_cast<float>(TNProcMap::SmoothStep(260.0, 520.0, Z)));
					break;
				}
				case EValleyStyle::Dunes:
				{
					// Barlovento claro y sotavento en sombra.
					static const FLinearColor Lee = ValleyHex(0xE3AE66);
					const double Wd = N.X * FMath::Cos(0.6) + N.Y * FMath::Sin(0.6);
					Ground = TNProcMesh::TNProcLerpColor(Lee, Pal.Ground2, static_cast<float>(0.5 + 0.5 * FMath::Clamp(Wd * 4.0, -1.0, 1.0)));
					break;
				}
				case EValleyStyle::Canyon:
				{
					// Estratos por altura en los taludes; techos de las mesas con matas de salvia.
					static const FLinearColor Strata[4] = { ValleyHex(0xD9733F), ValleyHex(0xE8955A), ValleyHex(0xF0C48A), ValleyHex(0xB9552E) };
					static const FLinearColor Sage = ValleyHex(0xA7A36A);
					const double Bz = (Z + 70.0 * TNProcMap::Noise2(S + 43u, P.X / 1500.0, P.Y / 1500.0)) / 240.0;
					Rock = Strata[((FMath::FloorToInt32(Bz) % 4) + 4) % 4];
					MtRock = Rock;
					Ground = Z < 260.0 ? Pal.Ground : TNProcMesh::TNProcLerpColor(Pal.Ground2, Sage, static_cast<float>(TNProcMap::SmoothStep(0.2, 0.6, Patch)));
					break;
				}
				case EValleyStyle::Farms:
					if (R > 6200.0 && R < 15800.0) { Ground = FieldColor(P); }
					break;
				case EValleyStyle::Mangrove:
					Ground = TNProcMesh::TNProcLerpColor(Pal.Ground2, Pal.Ground, static_cast<float>(TNProcMap::SmoothStep(WaterZ + 10.0, WaterZ + 70.0, Z)));
					break;
				default:
					break;
			}
			FLinearColor Col = TNProcMesh::TNProcLerpColor(Ground, Rock, Steep);
			Col = TNProcMesh::TNProcLerpColor(Col, TNProcMesh::TNProcLerpColor(Pal.MtGround, MtRock, Steep), Mt);
			Col = Col * FMath::Lerp(1.f, 0.8f, Sheer);
			// Nieve en lo alto, solo en las caras que miran arriba.
			static const FLinearColor SnowC = ValleyHex(0xF5F8FF);
			const double SnowN = 260.0 * TNProcMap::Noise2(S + 42u, P.X / 2100.0, P.Y / 2100.0);
			const float Snow = static_cast<float>(TNProcMap::SmoothStep(Sec.SnowZ - 220.0, Sec.SnowZ + 220.0, Z + SnowN) * TNProcMap::SmoothStep(0.4, 0.62, Up));
			return TNProcMesh::TNProcLerpColor(Col, SnowC, Snow);
		}

		/** Color de una cara plana (centro C, normal N): sectores, fondo del agua, playa del castillo, conos y bruma. */
		FLinearColor Color(const FVector& C, const FVector& N) const
		{
			const FVector2D P(C.X, C.Y);
			const double R = P.Size();
			const FValleyMix M = Mix(P, R);
			FLinearColor Col;
			if (C.Z < WaterZ - 4.0)
			{
				// Fondo: arena clara en la orilla y verde agua en lo hondo (fango en el manglar).
				const bool bMud = ValleySectorAt(M.WA >= 0.5 ? M.A : M.B).Style == EValleyStyle::Mangrove;
				static const FLinearColor Sand = ValleyHex(0xE2D5A2);
				static const FLinearColor Deep = ValleyHex(0x6FA48C);
				static const FLinearColor Mud = ValleyHex(0x7A7248);
				static const FLinearColor MudDeep = ValleyHex(0x4E5634);
				Col = TNProcMesh::TNProcLerpColor(bMud ? Mud : Sand, bMud ? MudDeep : Deep, static_cast<float>(TNProcMap::SmoothStep(WaterZ - 20.0, WaterZ - 210.0, C.Z)));
			}
			else
			{
				Col = StyleColor(ValleySectorAt(M.A), P, R, C.Z, N) * static_cast<float>(M.WA);
				if (M.WB > 0.0) { Col += StyleColor(ValleySectorAt(M.B), P, R, C.Z, N) * static_cast<float>(M.WB); }
				// Junto al castillo, la misma arena que su playa.
				static const FLinearColor CastleSand = ValleyHex(0xF2DCA8);
				Col = TNProcMesh::TNProcLerpColor(Col, CastleSand, static_cast<float>(1.0 - TNProcMap::SmoothStep(4300.0, 6400.0, R)));
			}
			// Conos volcánicos: roca oscura, rojiza junto al cráter.
			for (int32 c = 0; c < NumValleyCones; ++c)
			{
				const FValleyCone& Cn = ValleyCone(c);
				const double D = FVector2D::Distance(P, ValleyClockPoint(Cn.Hour, Cn.Dist));
				if (D >= Cn.BaseR) { continue; }
				static const FLinearColor Ash = ValleyHex(0x3A302C);
				static const FLinearColor Scorch = ValleyHex(0x5A3A2E);
				static const FLinearColor Pit = ValleyHex(0x2A2020);
				const FLinearColor ConeC = D < Cn.CraterR * 1.02 ? Pit : TNProcMesh::TNProcLerpColor(Ash, Scorch, static_cast<float>(TNProcMap::SmoothStep(Cn.CraterR * 2.2, Cn.CraterR, D)));
				Col = TNProcMesh::TNProcLerpColor(Col, ConeC, static_cast<float>(TNProcMap::SmoothStep(Cn.BaseR, Cn.BaseR * 0.8, D)));
			}
			// Cordillera lejana: azulada por la distancia y con nieve en las cumbres.
			if (R > 24500.0)
			{
				static const FLinearColor FarRock = ValleyHex(0x8494B4);
				static const FLinearColor FarSnow = ValleyHex(0xE4EAF6);
				const double Sn = 400.0 * TNProcMap::Noise2(S + 44u, P.X / 5000.0, P.Y / 5000.0);
				const FLinearColor FarC = (C.Z + Sn > 9800.0 && N.Z > 0.42) ? FarSnow : FarRock;
				Col = TNProcMesh::TNProcLerpColor(Col, FarC, static_cast<float>(0.7 * TNProcMap::SmoothStep(24500.0, 33000.0, R)));
			}
			return Col;
		}
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Vegetación por estilo
	// ─────────────────────────────────────────────────────────────────────────

	/** Una especie de la vegetación de un sector (o un objeto suelto, con Shape == Prop). */
	struct FValleyFloraPick
	{
		TNProcMap::EFloraShape Shape = TNProcMap::EFloraShape::Bush;
		TNProcMap::EPropKind Prop = TNProcMap::EPropKind::Crate;
		/** Ejemplares por cada 100 m² donde puede crecer. */
		float Density = 0.5f;
		float ScaleMin = 0.8f;
		float ScaleMax = 1.2f;
		uint8 Zones = TNProcMap::FloraZone::Land;
		float SlopeMax = 35.f;
		/** También en la sierra (árboles y peñascos); si no, solo en el valle. */
		bool bMountain = false;
	};

	/** Árbol (o planta alta): no sube por encima del límite del bosque. */
	inline bool ValleyIsTree(TNProcMap::EFloraShape Shape)
	{
		using ES = TNProcMap::EFloraShape;
		switch (Shape)
		{
			case ES::Rock: case ES::Stones: case ES::Prop: case ES::Bush: case ES::AshBush: case ES::DryBush: case ES::Barrel: case ES::Hedge:
			case ES::Fern: case ES::Grass: case ES::Flowers: case ES::Reeds: case ES::Creeper: case ES::BananaPlant: case ES::FanPalm: case ES::SeaGrape:
			case ES::Umbrella:
				return false;
			default:
				return true;
		}
	}

	/** Vegetación de cada estilo: las especies de su bioma que se leen desde lejos (árboles, arbustos grandes, rocas). */
	inline void ValleyFloraPicksFor(EValleyStyle Style, TArray<FValleyFloraPick>& Out)
	{
		Out.Reset();
		using ES = TNProcMap::EFloraShape;
		constexpr uint8 L = TNProcMap::FloraZone::Land;
		constexpr uint8 Sh = TNProcMap::FloraZone::Shore;
		constexpr uint8 W = TNProcMap::FloraZone::Shallows;
		constexpr uint8 D = TNProcMap::FloraZone::Deep;
		auto Add = [&Out](ES InShape, float InDensity, float InMin, float InMax, uint8 InZones, float InSlope, bool bInMountain)
		{
			FValleyFloraPick& Pick = Out.AddDefaulted_GetRef();
			Pick.Shape = InShape;
			Pick.Density = InDensity;
			Pick.ScaleMin = InMin;
			Pick.ScaleMax = InMax;
			Pick.Zones = InZones;
			Pick.SlopeMax = InSlope;
			Pick.bMountain = bInMountain;
		};
		switch (Style)
		{
			case EValleyStyle::Lagoon:
				Add(ES::Willow, 0.35f, 0.8f, 1.2f, L, 30.f, true);
				Add(ES::BroadTree, 0.3f, 0.7f, 1.2f, L, 35.f, true);
				Add(ES::Birch, 0.3f, 0.8f, 1.2f, L, 35.f, true);
				Add(ES::Palm, 0.45f, 0.8f, 1.25f, L, 28.f, false);
				Add(ES::Reeds, 2.2f, 1.2f, 2.0f, Sh | W, 30.f, false);
				Add(ES::Cypress, 0.25f, 0.8f, 1.2f, Sh | W, 30.f, false);
				Add(ES::Bush, 0.5f, 0.9f, 1.6f, L, 40.f, false);
				Add(ES::Rock, 0.18f, 0.6f, 2.2f, L | Sh, 70.f, true);
				break;
			case EValleyStyle::Beach:
				Add(ES::Palm, 1.1f, 0.8f, 1.3f, L, 28.f, false);
				Add(ES::Casuarina, 0.3f, 0.8f, 1.3f, L, 35.f, true);
				Add(ES::SeaGrape, 0.35f, 1.0f, 1.6f, L, 35.f, false);
				Add(ES::FanPalm, 0.5f, 1.4f, 2.4f, L, 35.f, false);
				Add(ES::Pandanus, 0.2f, 0.9f, 1.3f, L, 30.f, false);
				Add(ES::Rock, 0.3f, 0.6f, 2.4f, L | Sh, 70.f, true);
				Add(ES::Bush, 0.15f, 0.9f, 1.5f, L, 40.f, false);
				break;
			case EValleyStyle::Dunes:
				Add(ES::Saguaro, 0.35f, 0.8f, 1.4f, L, 30.f, true);
				Add(ES::JoshuaTree, 0.12f, 0.8f, 1.3f, L, 30.f, false);
				Add(ES::Barrel, 0.35f, 1.2f, 2.0f, L, 35.f, false);
				Add(ES::DryBush, 0.4f, 1.2f, 2.0f, L, 40.f, false);
				Add(ES::DeadTree, 0.05f, 0.8f, 1.2f, L, 35.f, false);
				Add(ES::Rock, 0.18f, 0.6f, 2.4f, L, 70.f, true);
				break;
			case EValleyStyle::Canyon:
				Add(ES::Saguaro, 0.25f, 0.8f, 1.4f, L, 32.f, true);
				Add(ES::Acacia, 0.25f, 0.9f, 1.4f, L, 25.f, false);
				Add(ES::DryBush, 0.4f, 1.2f, 2.0f, L, 40.f, false);
				Add(ES::Barrel, 0.2f, 1.2f, 2.0f, L, 35.f, false);
				Add(ES::JoshuaTree, 0.08f, 0.8f, 1.3f, L, 30.f, false);
				Add(ES::Rock, 0.45f, 0.6f, 2.6f, L, 75.f, true);
				break;
			case EValleyStyle::Volcano:
				Add(ES::CharredTree, 0.45f, 0.8f, 1.3f, L, 40.f, true);
				Add(ES::DeadTree, 0.15f, 0.8f, 1.2f, L, 40.f, false);
				Add(ES::AshBush, 0.35f, 1.2f, 2.0f, L, 45.f, false);
				Add(ES::Rock, 0.8f, 0.6f, 2.8f, L, 80.f, true);
				Add(ES::Pine, 0.06f, 0.7f, 1.1f, L, 35.f, false);
				break;
			case EValleyStyle::Cliffs:
				Add(ES::Pine, 0.6f, 0.8f, 1.4f, L, 40.f, true);
				Add(ES::Fir, 0.3f, 0.8f, 1.4f, L, 40.f, true);
				Add(ES::Birch, 0.12f, 0.8f, 1.2f, L, 35.f, false);
				Add(ES::Bush, 0.35f, 1.0f, 1.8f, L, 50.f, false);
				Add(ES::Rock, 0.9f, 0.6f, 2.8f, L, 80.f, true);
				break;
			case EValleyStyle::Snow:
				Add(ES::Fir, 1.2f, 0.8f, 1.5f, L, 42.f, true);
				Add(ES::Pine, 0.5f, 0.8f, 1.3f, L, 40.f, true);
				Add(ES::Rock, 0.4f, 0.6f, 2.6f, L, 80.f, true);
				Add(ES::Bush, 0.3f, 1.0f, 1.6f, L, 45.f, false);
				break;
			case EValleyStyle::Forest:
				Add(ES::Pine, 1.5f, 0.8f, 1.5f, L, 40.f, true);
				Add(ES::Fir, 0.9f, 0.8f, 1.5f, L, 40.f, true);
				Add(ES::Birch, 0.6f, 0.8f, 1.3f, L, 35.f, true);
				Add(ES::Bush, 0.5f, 1.0f, 1.8f, L, 45.f, false);
				Add(ES::Fern, 0.4f, 1.5f, 2.4f, L, 40.f, false);
				Add(ES::Rock, 0.2f, 0.6f, 2.2f, L, 75.f, true);
				break;
			case EValleyStyle::Village:
				Add(ES::Ornamental, 0.45f, 0.8f, 1.3f, L, 25.f, false);
				Add(ES::BroadTree, 0.3f, 0.7f, 1.2f, L, 30.f, true);
				Add(ES::Birch, 0.2f, 0.8f, 1.2f, L, 30.f, true);
				Add(ES::Hedge, 0.3f, 1.0f, 1.5f, L, 20.f, false);
				Add(ES::Bush, 0.35f, 1.0f, 1.6f, L, 35.f, false);
				break;
			case EValleyStyle::Farms:
				Add(ES::Ornamental, 0.3f, 0.8f, 1.2f, L, 22.f, false);
				Add(ES::BroadTree, 0.18f, 0.7f, 1.2f, L, 30.f, true);
				Add(ES::Hedge, 0.2f, 1.0f, 1.5f, L, 20.f, false);
				Add(ES::Bush, 0.2f, 1.0f, 1.6f, L, 35.f, false);
				Add(ES::Prop, 0.3f, 1.6f, 2.2f, L, 18.f, false);
				Out.Last().Prop = TNProcMap::EPropKind::HayBale;
				break;
			case EValleyStyle::Jungle:
				Add(ES::BroadTree, 2.2f, 0.7f, 1.4f, L, 45.f, true);
				Add(ES::Ceiba, 0.16f, 0.9f, 1.3f, L, 30.f, true);
				Add(ES::TreeFern, 0.5f, 0.9f, 1.5f, L, 45.f, false);
				Add(ES::Bamboo, 0.35f, 0.9f, 1.4f, L, 40.f, false);
				Add(ES::Palm, 0.4f, 0.8f, 1.3f, L, 35.f, true);
				Add(ES::BananaPlant, 0.7f, 1.2f, 2.0f, L, 40.f, false);
				Add(ES::Rock, 0.1f, 0.6f, 2.0f, L, 75.f, true);
				break;
			case EValleyStyle::Mangrove:
			default:
				Add(ES::MangroveTree, 2.0f, 0.8f, 1.5f, L | Sh | W | D, 40.f, true);
				Add(ES::Cypress, 0.55f, 0.8f, 1.3f, L | Sh | W, 35.f, true);
				Add(ES::Pandanus, 0.3f, 0.9f, 1.3f, L | Sh, 35.f, false);
				Add(ES::Reeds, 1.4f, 1.2f, 2.0f, Sh | W, 30.f, false);
				Add(ES::Palm, 0.12f, 0.8f, 1.2f, L, 30.f, false);
				Add(ES::TreeFern, 0.2f, 0.9f, 1.4f, L, 45.f, false);
				break;
		}
	}

	/** Zona de agua de una cota (la misma división que la vegetación del mapa procedural). */
	inline uint8 ValleyZoneOf(double Z)
	{
		if (Z >= WaterZ + 40.0) { return TNProcMap::FloraZone::Land; }
		if (Z >= WaterZ - 40.0) { return TNProcMap::FloraZone::Shore; }
		if (Z >= WaterZ - 180.0) { return TNProcMap::FloraZone::Shallows; }
		if (Z >= WaterZ - 450.0) { return TNProcMap::FloraZone::Deep; }
		return 0;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Materiales (los del mapa procedural; sin ellos, el de color de vértice del castillo)
	// ─────────────────────────────────────────────────────────────────────────

	inline UMaterialInterface* ValleyLoadMaterial(const TCHAR* Path, UMaterialInterface* Fallback)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, Path);
		return Mat ? Mat : Fallback;
	}

	inline UMaterialInterface* ValleyTerrainMaterial()
	{
		return ValleyLoadMaterial(TEXT("/Game/ProcMap/Materials/M_ProcTerrain.M_ProcTerrain"), TNCastleKit::VertexColorMaterial());
	}

	inline UMaterialInterface* ValleyFoliageMaterial()
	{
		return ValleyLoadMaterial(TEXT("/Game/ProcMap/Materials/M_ProcFoliage.M_ProcFoliage"), TNCastleKit::VertexColorMaterial());
	}

	inline UMaterialInterface* ValleyGlowMaterial()
	{
		return ValleyLoadMaterial(TEXT("/Game/ProcMap/Materials/M_ProcGlow.M_ProcGlow"), TNCastleKit::VertexColorMaterial());
	}

	inline UMaterialInterface* ValleyBirdMaterial()
	{
		return ValleyLoadMaterial(TEXT("/Game/ProcMap/Materials/M_ProcBird.M_ProcBird"), ValleyFoliageMaterial());
	}

	inline UMaterialInterface* ValleyWaterMaterial()
	{
		return ValleyLoadMaterial(TEXT("/Game/ProcMap/Materials/MI_ProcSeaAnim.MI_ProcSeaAnim"),
			ValleyLoadMaterial(TEXT("/Game/ProcMap/Materials/MI_ProcSea.MI_ProcSea"), TNCastleKit::VertexColorMaterial()));
	}

	inline UMaterialInterface* ValleyCascadeMaterial()
	{
		return ValleyLoadMaterial(TEXT("/Game/ProcMap/Materials/M_ProcCascade.M_ProcCascade"), ValleyWaterMaterial());
	}
}

/**
 * Rejilla polar del terreno del valle: anillos cada vez más separados (1,3 m junto al castillo, 15 m en la cordillera
 * lejana) por 240 radios. Guarda las alturas de sus vértices; HeightAt interpola en el mismo triángulo que dibuja la
 * malla (diagonal alterna por casilla), así lo que se planta o se posa encima no flota ni se entierra.
 */
struct FTNLobbyValleyGrid
{
	TArray<double> Radii;
	TArray<double> Heights;
	TArray<double> SinA;
	TArray<double> CosA;
	int32 NumSpokes = TNLobbyValley::Spokes;

	void Build(const TNLobbyValley::FValleyShape& Shape)
	{
		Radii.Reset();
		double R = TNLobbyValley::InnerR;
		while (R < TNLobbyValley::OuterR - 600.0)
		{
			Radii.Add(R);
			R += FMath::Clamp(R * 0.034, 110.0, 1500.0);
		}
		Radii.Add(TNLobbyValley::OuterR);
		SinA.SetNumUninitialized(NumSpokes);
		CosA.SetNumUninitialized(NumSpokes);
		for (int32 k = 0; k < NumSpokes; ++k)
		{
			const double A = TNProcMap::TwoPi * k / NumSpokes;
			SinA[k] = FMath::Sin(A);
			CosA[k] = FMath::Cos(A);
		}
		Heights.SetNumUninitialized(Radii.Num() * NumSpokes);
		ParallelFor(Radii.Num(), [this, &Shape](int32 Ring)
		{
			for (int32 k = 0; k < NumSpokes; ++k)
			{
				Heights[Ring * NumSpokes + k] = Shape.Height(Point(Ring, k));
			}
		});
	}

	bool IsValid() const { return Radii.Num() >= 2 && Heights.Num() == Radii.Num() * NumSpokes; }

	FVector2D Point(int32 Ring, int32 Spoke) const
	{
		const int32 K = ((Spoke % NumSpokes) + NumSpokes) % NumSpokes;
		return FVector2D(-Radii[Ring] * SinA[K], Radii[Ring] * CosA[K]);
	}

	double Height(int32 Ring, int32 Spoke) const
	{
		const int32 K = ((Spoke % NumSpokes) + NumSpokes) % NumSpokes;
		return Heights[Ring * NumSpokes + K];
	}

	/** Altura de la malla en P (la del triángulo que lo contiene); fuera de la rejilla, la del anillo más cercano. */
	double HeightAt(const FVector2D& P) const
	{
		if (!IsValid()) { return 0.0; }
		const double R = P.Size();
		const int32 Upper = Algo::UpperBound(Radii, R);
		const int32 I = FMath::Clamp(Upper - 1, 0, Radii.Num() - 2);
		const double T = FMath::Clamp((R - Radii[I]) / FMath::Max(1.0, Radii[I + 1] - Radii[I]), 0.0, 1.0);
		double A = FMath::Atan2(-P.X, P.Y);
		if (A < 0.0) { A += TNProcMap::TwoPi; }
		const double Sf = A / TNProcMap::TwoPi * NumSpokes;
		const int32 K = FMath::Clamp(FMath::FloorToInt32(Sf), 0, NumSpokes - 1);
		const double Sv = FMath::Clamp(Sf - K, 0.0, 1.0);
		const double Z00 = Height(I, K);
		const double Z01 = Height(I, K + 1);
		const double Z10 = Height(I + 1, K);
		const double Z11 = Height(I + 1, K + 1);
		if (((I + K) & 1) == 0)
		{
			// Diagonal de (I, K) a (I + 1, K + 1).
			return Sv >= T ? Z00 + Sv * (Z01 - Z00) + T * (Z11 - Z01) : Z00 + T * (Z10 - Z00) + Sv * (Z11 - Z10);
		}
		// Diagonal de (I, K + 1) a (I + 1, K).
		return Sv + T <= 1.0 ? Z00 + Sv * (Z01 - Z00) + T * (Z10 - Z00) : Z11 + (1.0 - Sv) * (Z10 - Z11) + (1.0 - T) * (Z01 - Z11);
	}

	/** Normal del terreno en P (diferencias centradas de 60 cm). */
	FVector NormalAt(const FVector2D& P) const
	{
		constexpr double E = 60.0;
		const double Dx = (HeightAt(P + FVector2D(E, 0.0)) - HeightAt(P - FVector2D(E, 0.0))) / (2.0 * E);
		const double Dy = (HeightAt(P + FVector2D(0.0, E)) - HeightAt(P - FVector2D(0.0, E))) / (2.0 * E);
		return FVector(-Dx, -Dy, 1.0).GetSafeNormal();
	}

	/**
	 * Malla de caras planas del terreno: dos triángulos por casilla (diagonal alterna, como HeightAt), cada uno con el
	 * color de su centro y un tono propio. Se rellena en paralelo por anillos, con los mismos convenios que
	 * FTNProcMeshBuffers::AddTri (cara hacia arriba, UV planas).
	 */
	void EmitMesh(TNProcMesh::FTNProcMeshBuffers& Out, const TNLobbyValley::FValleyShape& Shape) const
	{
		if (!IsValid()) { return; }
		const int32 NumRings = Radii.Num();
		const int32 NumV = (NumRings - 1) * NumSpokes * 6;
		Out.Verts.SetNumUninitialized(NumV);
		Out.Normals.SetNumUninitialized(NumV);
		Out.UVs.SetNumUninitialized(NumV);
		Out.Colors.SetNumUninitialized(NumV);
		Out.Tris.SetNumUninitialized(NumV);
		auto Emit = [&Out, &Shape](int32 Base, const FVector& A, const FVector& B, const FVector& C, float Tone)
		{
			FVector N = FVector::CrossProduct(B - A, C - A);
			const bool bFlip = N.Z < 0.0;
			N = N.GetSafeNormal();
			if (bFlip) { N = -N; }
			if (N.IsNearlyZero()) { N = FVector::UpVector; }
			const FVector P1 = bFlip ? C : B;
			const FVector P2 = bFlip ? B : C;
			FLinearColor Col = Shape.Color((A + B + C) / 3.0, N) * Tone;
			Col.A = 0.f;
			const FVector Pts[3] = { A, P1, P2 };
			for (int32 v = 0; v < 3; ++v)
			{
				Out.Verts[Base + v] = Pts[v];
				Out.Normals[Base + v] = N;
				Out.UVs[Base + v] = FVector2D(Pts[v].X, Pts[v].Y) / 400.0;
				Out.Colors[Base + v] = Col;
			}
			Out.Tris[Base] = Base;
			Out.Tris[Base + 1] = Base + 2;
			Out.Tris[Base + 2] = Base + 1;
		};
		ParallelFor(NumRings - 1, [this, &Emit](int32 Ring)
		{
			for (int32 k = 0; k < NumSpokes; ++k)
			{
				const FVector C00(Point(Ring, k), Height(Ring, k));
				const FVector C01(Point(Ring, k + 1), Height(Ring, k + 1));
				const FVector C10(Point(Ring + 1, k), Height(Ring + 1, k));
				const FVector C11(Point(Ring + 1, k + 1), Height(Ring + 1, k + 1));
				const int32 Base = (Ring * NumSpokes + k) * 6;
				const float ToneA = 0.95f + 0.1f * static_cast<float>(0.5 + 0.5 * TNProcMesh::TNProcHashNoise(Ring * 2, k, 0x7A11u));
				const float ToneB = 0.95f + 0.1f * static_cast<float>(0.5 + 0.5 * TNProcMesh::TNProcHashNoise(Ring * 2 + 1, k, 0x7A11u));
				if (((Ring + k) & 1) == 0)
				{
					Emit(Base, C00, C01, C11, ToneA);
					Emit(Base + 3, C00, C11, C10, ToneB);
				}
				else
				{
					Emit(Base, C00, C01, C10, ToneA);
					Emit(Base + 3, C01, C11, C10, ToneB);
				}
			}
		});
	}
};
