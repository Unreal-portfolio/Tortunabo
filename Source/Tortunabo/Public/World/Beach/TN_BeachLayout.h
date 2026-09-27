#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachTypes.h"
#include "World/ProcMap/TN_ProcMapMath.h"

/**
 * Playa del modo carrera (ATN_BeachRaceGenerator, Docs/Modo_Carrera.md), lógica pura: el terreno fijo (perfil hacia el
 * mar, dunas, bancos de la selva, repisa y acantilado de roca, fondo del mar), la salida, las zonas de meta y de
 * zambullida, y el reparto de cada ronda con su semilla (bandas, castillo con salas, pasos de quads, zonas de gaviotas,
 * pasarelas guía y el asiento de cada elemento en la arena). Sin mundo ni actores: lo usan el generador (el servidor para
 * colocar y cada cliente, con la semilla replicada, para los asientos) y los tests (Private/Tests/TN_BeachLayoutTest.cpp).
 *
 * Espacio local del generador, en cm: X a lo largo del recorrido (línea de salida en X = 0, borde del acantilado en
 * X ≈ Length, el mar más allá), Y a lo ancho (la playa jugable en |Y| <= HalfWidth) y Z arriba, con el agua en Z = 0.
 *
 * Orientación de los elementos (Yaw, grados sobre Z): con Yaw 0 el eje X local del elemento mira al mar. Los alargados
 * (Extent > 0) tienen el largo por su eje X local, centrado en su origen, y la huella es su semigrosor por el eje Y
 * local: el alambre y el paso de quads van a ~90° (cruzan la playa), la pasarela y el caminito de palos a ~0° (hacia el
 * mar). El castillo con salas, la puerta de conchas, el cubo roto y la pala van a ~0°: se entra por -X y se sale por +X.
 * Todos se colocan con el origen a la cota del suelo en su centro, y el suelo bajo su huella queda liso (FStamp): plano
 * a esa cota en los redondos y, en los alargados, siguiendo la cuesta de la playa sin dunas.
 */
namespace TNBeachLayout
{
	// ─────────────────────────────────────────────────────────────────────────
	// Medidas fijas
	// ─────────────────────────────────────────────────────────────────────────

	constexpr double Length = TNBeach::CourseLength;
	constexpr double HalfWidth = TNBeach::CourseWidth * 0.5;
	constexpr double WaterZ = 0.0;
	/** Cota del borde del acantilado sobre el agua (la roca asoma RockRise más). */
	constexpr double CliffTopZ = WaterZ + TNBeach::CliffHeight;
	/** Desnivel de la arena de la salida al borde: 36 m en 1200 m, más al principio (siempre se ve el mar). */
	constexpr double BeachDrop = 3600.0;
	/** El borde del acantilado ondula ±2,5 m a lo ancho (siempre igual). */
	constexpr double EdgeWobble = 250.0;
	/** Repisa de roca: empieza enterrada 24 m antes del borde y asoma RockRise sobre la arena desde unos 16 m antes. */
	constexpr double RockStart = 2400.0;
	constexpr double RockRise = 50.0;
	/** Fondo al pie del acantilado: 11 m de agua (siempre se cae en agua honda). */
	constexpr double SeabedFootZ = WaterZ - 1100.0;
	/** Zona de zambullida: de 7,5 m antes del filo a 40 m sobre el vacío. */
	constexpr double JumpZoneBefore = 750.0;
	constexpr double JumpZoneBeyond = 4000.0;
	/** Agua de meta: hasta 300 m mar adentro y 250 m a cada lado de la playa. */
	constexpr double FinishWaterReach = 30000.0;
	constexpr double FinishWaterSide = 25000.0;
	/** Semilla fija del terreno (el terreno es el mismo en todas las rondas y partidas). */
	constexpr uint32 TerrainSeed = 0xB3AC4u;

	// ── Salida: en el linde de la selva, entre las raíces de un árbol colosal y bajo hojas enormes ──
	constexpr int32 NumStartSpots = 4;
	constexpr double StartSpotX = -800.0;
	constexpr double StartSpotSpacing = 1000.0;
	constexpr double StartRowSpacing = 900.0;
	constexpr double TrunkX = -5200.0;
	constexpr double TrunkRadius = 1500.0;
	/** Muros invisibles: detrás de la salida (delante del tronco) y a los lados (8 m fuera de la playa jugable). */
	constexpr double BackWallX = -3200.0;
	constexpr double SideWallY = HalfWidth + 800.0;

	// ── Reparto ──
	/** Nada a menos de 40 m de la salida (las huellas empiezan 58 m por delante de las tortugas). */
	constexpr double ItemsStartX = 5000.0;
	/** Nada a menos de 30 m del borde del acantilado. */
	constexpr double ItemsEndX = Length - 3000.0;
	constexpr double SideMargin = 500.0;
	/** Paso libre a lo ancho que siempre queda (obstáculos inflados MinPassage / 2 y un camino de un lado a otro). */
	constexpr double MinPassage = 1200.0;
	constexpr double BandLength = 5000.0;
	/** Separación mínima entre huellas (salvo las alas del castillo, que se tocan). */
	constexpr double ItemPad = 150.0;
	/** Casilla de la rejilla con la que se comprueba el paso. */
	constexpr double PassCell = 200.0;
	/** Borde del asiento de cada elemento en la arena: de la huella a la arena natural, entre 2,5 y 16 m (40 % de la huella; 80 % en los alargados). */
	constexpr double StampBlendMin = 250.0;
	constexpr double StampBlendMax = 1600.0;

	// ─────────────────────────────────────────────────────────────────────────
	// Terreno fijo
	// ─────────────────────────────────────────────────────────────────────────

	/** X del filo del acantilado a lo ancho de la playa (ondula poco: el borde se lee claro). */
	inline double EdgeX(double Y)
	{
		return Length + EdgeWobble * (0.6 * FMath::Sin(Y / 5200.0 + 0.7) + 0.4 * FMath::Sin(Y / 1900.0 + 2.1));
	}

	/** Cota de la arena a lo largo del recorrido, sin dunas: cae más al principio y sigue subiendo por detrás de la salida. */
	inline double ProfileZ(double X)
	{
		const double T = FMath::Clamp(X / Length, 0.0, 1.0);
		double Z = CliffTopZ + BeachDrop * FMath::Pow(1.0 - T, 1.5);
		if (X < 0.0) { Z += BeachDrop * 1.5 / Length * (-X); }
		return Z;
	}

	/**
	 * Dunas suaves: crestas a lo ancho (el viento viene del mar) cada ~52 m, deformadas y con lomas sueltas; 1,7 m en el
	 * centro y 3,5 m junto a la selva. Nada en los primeros 30 m (la salida) y se allanan antes de la roca.
	 */
	inline double DuneZ(double X, double Y)
	{
		const double Env = TNProcMap::SmoothStep(3000.0, 16000.0, X) * (1.0 - TNProcMap::SmoothStep(Length - 9000.0, Length - RockStart, X));
		if (Env <= 0.0) { return 0.0; }
		const double SideT = TNProcMap::SmoothStep(HalfWidth - 6000.0, HalfWidth, FMath::Abs(Y));
		const double Amp = (170.0 + 180.0 * SideT) * Env;
		const double Wx = X + 2600.0 * TNProcMap::Noise2(TerrainSeed, X / 14000.0, Y / 14000.0);
		const double Wy = Y + 2600.0 * TNProcMap::Noise2(TerrainSeed + 1u, X / 14000.0 + 5.3, Y / 14000.0 - 2.1);
		const double Phase = Wx / 5200.0 + 0.35 * TNProcMap::Noise2(TerrainSeed + 2u, Wx / 26000.0, Wy / 9000.0);
		const double Ridge = 0.5 + 0.5 * FMath::Sin(TNProcMap::TwoPi * Phase);
		const double Crest = 0.55 + 0.45 * TNProcMap::Noise2(TerrainSeed + 3u, Wx / 18000.0, Wy / 7000.0);
		const double Lumps = TNProcMap::Fbm2(TerrainSeed + 4u, Wx / 9000.0, Wy / 9000.0, 3);
		return Amp * (1.2 * Ridge * Crest - 0.35 + 0.45 * Lumps);
	}

	/** Lo que sube el suelo hacia la selva: bancos a los lados (38 m y colinas detrás) y la ladera de detrás de la salida. */
	inline double BankZ(double X, double Y)
	{
		const double Out = FMath::Abs(Y) - HalfWidth;
		double Z = 0.0;
		if (Out > 0.0)
		{
			Z += 3800.0 * TNProcMap::SmoothStep(300.0, 11000.0, Out) + 4500.0 * TNProcMap::SmoothStep(9000.0, 42000.0, Out);
			Z += 1600.0 * TNProcMap::SmoothStep(6000.0, 20000.0, Out) * TNProcMap::Fbm2(TerrainSeed + 7u, X / 22000.0, Y / 22000.0, 3);
		}
		if (X < -1800.0)
		{
			Z += 4200.0 * TNProcMap::SmoothStep(-1800.0, -12000.0, X) + 3000.0 * TNProcMap::SmoothStep(-9000.0, -24000.0, X);
		}
		return Z;
	}

	/** Arena natural (sin los asientos de la ronda). */
	inline double SandZ(double X, double Y)
	{
		return ProfileZ(X) + DuneZ(X, Y) + BankZ(X, Y);
	}

	/** Lo que la repisa de roca queda sobre la arena: enterrada al empezar (24 m antes del borde) y RockRise desde ~16 m. */
	inline double RockOffset(double X)
	{
		return TNProcMap::LerpD(-90.0, RockRise, TNProcMap::SmoothStep(Length - RockStart, Length - 1600.0, X));
	}

	/** Suelo firme hasta el filo (arena o repisa de roca), sin los asientos de la ronda. */
	inline double GroundZ(double X, double Y)
	{
		const double Sand = SandZ(X, Y);
		return X < Length - RockStart ? Sand : Sand + FMath::Max(0.0, RockOffset(X));
	}

	/** Fondo del mar: 11 m de agua al pie del acantilado, más hondo mar adentro. */
	inline double SeabedZ(double X, double Y)
	{
		const double D = FMath::Max(0.0, X - Length);
		return SeabedFootZ - 700.0 * TNProcMap::SmoothStep(0.0, 25000.0, D) - 2400.0 * TNProcMap::SmoothStep(15000.0, 90000.0, D)
			+ 120.0 * TNProcMap::Noise2(TerrainSeed + 9u, X / 6000.0, Y / 6000.0);
	}

	/** Suelo o fondo del mar, según a qué lado del filo quede el punto. */
	inline double SurfaceZ(double X, double Y)
	{
		return X > EdgeX(Y) ? SeabedZ(X, Y) : GroundZ(X, Y);
	}

	// ── Asientos de la ronda: el suelo liso bajo cada elemento ──

	/**
	 * Asiento de un elemento en la arena (el «sello» de la ronda en el terreno fijo): dentro de su huella, el suelo queda
	 * liso; en el borde (Blend), vuelve a la arena natural. Los redondos quedan a nivel, a la cota natural de su centro;
	 * los alargados siguen la cuesta de la playa sin dunas, pasando por la cota natural de su centro. Los hoyos (la
	 * plataforma) los traen los elementos en su malla: el terreno no se cava.
	 */
	struct FStamp
	{
		FVector2D A = FVector2D::ZeroVector;
		FVector2D B = FVector2D::ZeroVector;
		double Radius = 0.0;
		double Blend = StampBlendMin;
		/** A nivel (LevelZ) o siguiendo la cuesta sin dunas más Offset. */
		bool bLevel = true;
		double LevelZ = 0.0;
		double Offset = 0.0;
		/** Arena algo más oscura y apisonada (las rodadas de los quads). */
		float Tint = 0.f;
	};

	/** Cota de la arena sin dunas (la cuesta de la playa y los bancos de la selva). */
	inline double BaseZ(double X, double Y)
	{
		return ProfileZ(X) + BankZ(X, Y);
	}

	/**
	 * Arena con los asientos de la ronda en (X, Y), a partir de la arena natural Natural: dentro de una huella, su cota;
	 * en los bordes, la mezcla ponderada con la natural. OutTint, si se pide, lo apisonado.
	 */
	inline double StampedZ(const TArray<FStamp>& Stamps, double X, double Y, double Natural, float* OutTint = nullptr)
	{
		const FVector2D P(X, Y);
		double WeightSum = 0.0;
		double Accum = 0.0;
		float Tint = 0.f;
		for (const FStamp& Stamp : Stamps)
		{
			double T = 0.0;
			const double Dist = TNProcMap::DistPointSegment(P, Stamp.A, Stamp.B, T);
			if (Dist >= Stamp.Radius + Stamp.Blend) { continue; }
			const FVector2D Q = Stamp.A + (Stamp.B - Stamp.A) * T;
			const double Target = Stamp.bLevel ? Stamp.LevelZ : BaseZ(Q.X, Q.Y) + Stamp.Offset;
			const double W = 1.0 - TNProcMap::SmoothStep(Stamp.Radius, Stamp.Radius + Stamp.Blend, Dist);
			if (W >= 1.0)
			{
				if (OutTint) { *OutTint = Stamp.Tint; }
				return Target;
			}
			WeightSum += W;
			Accum += W * (Target - Natural);
			Tint = FMath::Max(Tint, Stamp.Tint * static_cast<float>(W));
		}
		if (OutTint) { *OutTint = Tint; }
		return WeightSum > 0.0 ? Natural + Accum / FMath::Max(1.0, WeightSum) : Natural;
	}

	// ── Salida, meta y zambullida ──

	/** Sitio de salida Index (local, en el suelo, mirando al mar): 4 en fila a 10 m y más filas detrás si hacen falta. */
	inline FVector StartSpot(int32 Index)
	{
		const int32 Slot = FMath::Max(0, Index);
		const int32 Col = Slot % NumStartSpots;
		const int32 Row = (Slot / NumStartSpots) % 3;
		const double Y = (Col - 0.5 * (NumStartSpots - 1)) * StartSpotSpacing;
		const double X = StartSpotX - Row * StartRowSpacing;
		return FVector(X, Y, GroundZ(X, Y));
	}

	/** Agua de meta (P local, los pies de la tortuga): más allá del filo y a ras del agua o por debajo. */
	inline bool IsFinishWaterLocal(const FVector& P)
	{
		return P.X > EdgeX(P.Y) + 50.0 && P.X < Length + FinishWaterReach && FMath::Abs(P.Y) < HalfWidth + FinishWaterSide && P.Z <= WaterZ + 30.0;
	}

	/** Franja de la zambullida (P local): los últimos 7,5 m de la repisa y el vacío sobre el agua hasta 40 m más allá. */
	inline bool IsCliffJumpZoneLocal(const FVector& P)
	{
		const double D = P.X - EdgeX(P.Y);
		return FMath::Abs(P.Y) <= HalfWidth + 3000.0 && D >= -JumpZoneBefore && D <= JumpZoneBeyond && P.Z > WaterZ + 30.0
			&& P.Z < CliffTopZ + RockRise + 4000.0;
	}

	/** Progreso 0..1 de la salida (X = 0) al filo. */
	inline double CourseProgress(const FVector& P)
	{
		return FMath::Clamp(P.X / EdgeX(P.Y), 0.0, 1.0);
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Reglas del reparto por elemento
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Cómo se reparte cada elemento. Sale de su categoría y su huella (TNBeach::CategoryOf, TNBeach::FootprintRadius),
	 * así lo que se añada al contrato entra solo, con ajustes para los conocidos.
	 */
	struct FElementRule
	{
		/** Peso relativo dentro de su categoría. */
		double Weight = 1.0;
		/** Tramo del recorrido (0 = salida, 1 = borde) donde puede salir. */
		double MinT = 0.0;
		double MaxT = 1.0;
		/** > 0: más hacia el mar (peso x (1 + SeaBias (2t - 1)), sin bajar de 0); < 0: más cerca de la salida. */
		double SeaBias = 0.0;
		/** > 0: prefiere los lados (junto a la selva); < 0: el centro. */
		double SideBias = 0.0;
		int32 MaxPerRound = 1000;
		double SizeMin = 0.8;
		double SizeMax = 1.2;
		/** Alargados: largo (el Extent, cm). 0 = redondo. */
		double ExtentMin = 0.0;
		double ExtentMax = 0.0;
		/** Giro (grados) y su margen: 180 = cualquiera. */
		double BaseYaw = 0.0;
		double YawJitter = 180.0;
		/** Cierra el paso (cuenta para dejar siempre el hueco libre a lo ancho). */
		bool bBlocking = true;
		/** Va por encima (gaviotas): no ocupa suelo; solo no se pisa con otros como él. */
		bool bOverlay = false;
		/** Lo coloca su propia pasada (castillo, quads, gaviotas), no el relleno de las bandas. */
		bool bSpecial = false;
		/** Pequeño: a veces sale en corrillo de 2-3 iguales. */
		bool bClusters = false;
	};

	inline FElementRule RuleOf(ETNBeachElement E)
	{
		FElementRule R;
		const double Foot = TNBeach::FootprintRadius(E);
		switch (TNBeach::CategoryOf(E))
		{
			case ETNBeachCategory::Decor:
				// Pequeño, frecuente y en corrillos; grande, escaso (y lo enorme, uno por ronda).
				R.Weight = Foot < 500.0 ? 1.1 : (Foot < 1500.0 ? 1.0 : (Foot < 3000.0 ? 0.45 : 0.2));
				R.bClusters = Foot < 500.0;
				R.MaxPerRound = Foot < 3000.0 ? 1000 : 1;
				R.SizeMin = Foot < 1500.0 ? 0.75 : 0.9;
				R.SizeMax = Foot < 1500.0 ? 1.3 : 1.1;
				break;
			case ETNBeachCategory::Trap:
				R.MinT = 0.08;
				R.SeaBias = 0.3;
				R.bBlocking = false;
				break;
			case ETNBeachCategory::Enemy:
			default:
				// Amenazas que se esquivan, no muros: no cierran el paso.
				R.MinT = 0.15;
				R.SeaBias = 0.5;
				R.MaxPerRound = 6;
				R.SizeMin = 0.9;
				R.SizeMax = 1.15;
				R.bBlocking = false;
				break;
		}
		switch (E)
		{
			case ETNBeachElement::Coconut:
				R.Weight = 1.5;
				R.SideBias = 0.8;
				break;
			case ETNBeachElement::StrandedJellyfish:
				R.SeaBias = 1.0;
				break;
			case ETNBeachElement::RedBra:
				R.Weight = 0.5;
				R.MaxPerRound = 1;
				R.MinT = 0.2;
				R.MaxT = 0.8;
				break;
			case ETNBeachElement::Clam:
			case ETNBeachElement::DecorShell:
			case ETNBeachElement::Starfish:
				R.SeaBias = 0.9;
				break;
			case ETNBeachElement::ShipSailWreck:
				R.Weight = 0.8;
				R.MaxPerRound = 1;
				R.MinT = 0.3;
				break;
			case ETNBeachElement::MossyLog:
				R.SideBias = 0.5;
				break;
			case ETNBeachElement::SixPackRings:
			case ETNBeachElement::PlasticCup:
			case ETNBeachElement::Bottle:
			case ETNBeachElement::Lollipop:
			case ETNBeachElement::WatermelonRind:
			case ETNBeachElement::Straw:
				R.MinT = 0.05;
				R.MaxT = 0.9;
				R.SideBias = -0.3;
				break;
			case ETNBeachElement::PlantedUmbrella:
			case ETNBeachElement::BeachChair:
				R.Weight = 1.0;
				R.MinT = 0.08;
				R.MaxT = 0.85;
				R.SideBias = -0.2;
				break;
			case ETNBeachElement::SandCastleHuge:
				R.Weight = 0.35;
				R.MaxPerRound = 2;
				break;
			case ETNBeachElement::Driftwood:
				R.SeaBias = 0.6;
				break;
			case ETNBeachElement::Boardwalk:
				R.Weight = 0.35;
				R.MaxPerRound = 4;
				R.ExtentMin = 2500.0;
				R.ExtentMax = 6000.0;
				R.YawJitter = 35.0;
				R.SizeMin = 0.9;
				R.SizeMax = 1.1;
				R.bBlocking = false;
				R.bClusters = false;
				break;
			case ETNBeachElement::WoodenPostPath:
				R.Weight = 0.4;
				R.MaxPerRound = 5;
				R.ExtentMin = 3000.0;
				R.ExtentMax = 8000.0;
				R.YawJitter = 30.0;
				R.SizeMin = 0.9;
				R.SizeMax = 1.1;
				R.bBlocking = false;
				R.bClusters = false;
				break;
			case ETNBeachElement::BarbedWire:
				R.SeaBias = 0.6;
				R.ExtentMin = 2200.0;
				R.ExtentMax = 6000.0;
				R.BaseYaw = 90.0;
				R.YawJitter = 25.0;
				R.SizeMin = 0.9;
				R.SizeMax = 1.1;
				R.bBlocking = true;
				break;
			case ETNBeachElement::Seaweed:
				R.Weight = 1.1;
				R.SeaBias = 1.2;
				break;
			case ETNBeachElement::WobblyPlatform:
				R.Weight = 0.8;
				R.MinT = 0.15;
				R.MaxPerRound = 6;
				R.YawJitter = 20.0;
				R.SizeMin = 0.85;
				R.SizeMax = 1.2;
				break;
			case ETNBeachElement::BrokenBucket:
				R.Weight = 0.8;
				R.YawJitter = 30.0;
				break;
			case ETNBeachElement::SpadeRamp:
				R.Weight = 0.7;
				R.YawJitter = 20.0;
				break;
			case ETNBeachElement::ShellGate:
				R.Weight = 0.6;
				R.YawJitter = 15.0;
				break;
			case ETNBeachElement::SandDungeon:
				R.bSpecial = true;
				R.bBlocking = true;
				R.MaxPerRound = 1;
				R.YawJitter = 8.0;
				R.SizeMin = 0.95;
				R.SizeMax = 1.08;
				break;
			case ETNBeachElement::GiantCrab:
				R.Weight = 1.2;
				R.MinT = 0.3;
				R.SeaBias = 1.5;
				R.MaxPerRound = 5;
				break;
			case ETNBeachElement::SeaUrchin:
				R.Weight = 1.4;
				R.MinT = 0.45;
				R.SeaBias = 1.8;
				R.MaxPerRound = 8;
				break;
			case ETNBeachElement::Lizard:
				R.Weight = 0.8;
				R.MinT = 0.1;
				R.MaxT = 0.85;
				R.SeaBias = -0.3;
				R.SideBias = 0.9;
				R.MaxPerRound = 4;
				break;
			case ETNBeachElement::QuadLane:
				R.bSpecial = true;
				R.BaseYaw = 90.0;
				R.YawJitter = 0.0;
				R.SizeMin = 0.9;
				R.SizeMax = 1.1;
				break;
			case ETNBeachElement::GullZone:
				R.bSpecial = true;
				R.bOverlay = true;
				R.SizeMin = 0.9;
				R.SizeMax = 1.3;
				break;
			default:
				break;
		}
		return R;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Reparto de una ronda
	// ─────────────────────────────────────────────────────────────────────────

	/** Para qué se ha puesto cada cosa (resumen y pruebas). */
	enum class EItemRole : uint8
	{
		Fill,
		Dungeon,
		DungeonWing,
		QuadLane,
		GullZone,
		GuidePath
	};

	/** Un elemento colocado: una cápsula en planta (un disco si HalfLength = 0). */
	struct FItem
	{
		ETNBeachElement Element = ETNBeachElement::Coconut;
		FVector2D Pos = FVector2D::ZeroVector;
		/** Grados; el eje X local del elemento (su largo, si es alargado). */
		double Yaw = 0.0;
		/** Huella (cm, ya por SizeScale): radio del disco o semigrosor de la cápsula. */
		double Radius = 0.0;
		/** Medio largo (Extent / 2) a lo largo de su eje X local; 0 = disco. */
		double HalfLength = 0.0;
		FTNBeachElementSpec Spec;
		bool bBlocking = true;
		bool bOverlay = false;
		EItemRole Role = EItemRole::Fill;

		FVector2D Axis() const
		{
			const double A = FMath::DegreesToRadians(Yaw);
			return FVector2D(FMath::Cos(A), FMath::Sin(A));
		}
		FVector2D EndA() const { return Pos - Axis() * HalfLength; }
		FVector2D EndB() const { return Pos + Axis() * HalfLength; }
		double Area() const { return PI * Radius * Radius + 4.0 * Radius * HalfLength; }
	};

	/** Distancia entre dos segmentos en planta (0 si se cruzan). */
	inline double SegmentDistance(const FVector2D& A0, const FVector2D& A1, const FVector2D& B0, const FVector2D& B1)
	{
		if (TNProcMap::SegmentsIntersect(A0, A1, B0, B1)) { return 0.0; }
		double T = 0.0;
		double D = TNProcMap::DistPointSegment(A0, B0, B1, T);
		D = FMath::Min(D, TNProcMap::DistPointSegment(A1, B0, B1, T));
		D = FMath::Min(D, TNProcMap::DistPointSegment(B0, A0, A1, T));
		D = FMath::Min(D, TNProcMap::DistPointSegment(B1, A0, A1, T));
		return D;
	}

	/** Holgura entre dos huellas (negativa si se solapan). */
	inline double Clearance(const FItem& A, const FItem& B)
	{
		return SegmentDistance(A.EndA(), A.EndB(), B.EndA(), B.EndB()) - A.Radius - B.Radius;
	}

	/** Asiento de un elemento: su huella, a la cota natural de su centro (donde se coloca su origen). */
	inline FStamp MakeStamp(const FItem& Item)
	{
		FStamp Stamp;
		Stamp.A = Item.EndA();
		Stamp.B = Item.EndB();
		Stamp.Radius = Item.Radius;
		// Los alargados cortan más dunas a lo largo (el paso de quads cruza la playa entera): borde más ancho.
		Stamp.Blend = FMath::Clamp((Item.HalfLength > 0.0 ? 0.8 : 0.4) * Item.Radius, StampBlendMin, StampBlendMax);
		Stamp.bLevel = Item.HalfLength <= 0.0;
		const double Natural = SandZ(Item.Pos.X, Item.Pos.Y);
		Stamp.LevelZ = Natural;
		Stamp.Offset = Natural - BaseZ(Item.Pos.X, Item.Pos.Y);
		Stamp.Tint = Item.Element == ETNBeachElement::QuadLane ? 0.14f : 0.f;
		return Stamp;
	}

	/**
	 * Rejilla del paso: cada casilla cuenta los obstáculos (inflados MinPassage / 2) que la tapan y los lados de la playa
	 * también la tapan. Si hay un camino de casillas libres de la salida al borde, en cada corte a lo ancho queda al menos
	 * un hueco de MinPassage.
	 */
	struct FPassGrid
	{
		double X0 = 0.0;
		double Y0 = 0.0;
		int32 NX = 0;
		int32 NY = 0;
		TArray<uint16> Count;

		void Init()
		{
			X0 = ItemsStartX - 2000.0;
			Y0 = -HalfWidth;
			NX = FMath::CeilToInt32((ItemsEndX + 2000.0 - X0) / PassCell);
			NY = FMath::CeilToInt32(2.0 * HalfWidth / PassCell);
			Count.Init(0, NX * NY);
		}

		FVector2D CellCenter(int32 IX, int32 IY) const
		{
			return FVector2D(X0 + (IX + 0.5) * PassCell, Y0 + (IY + 0.5) * PassCell);
		}

		bool IsFree(int32 IX, int32 IY) const
		{
			const double CY = Y0 + (IY + 0.5) * PassCell;
			return FMath::Abs(CY) <= HalfWidth - MinPassage * 0.5 && Count[IY * NX + IX] == 0;
		}

		/** Suma (Delta = 1) o quita (-1) un obstáculo. */
		void Stamp(const FItem& Item, int32 Delta)
		{
			if (!Item.bBlocking || Item.bOverlay || NX <= 0) { return; }
			const double Reach = Item.Radius + MinPassage * 0.5;
			const FVector2D A = Item.EndA();
			const FVector2D B = Item.EndB();
			const int32 IX0 = FMath::Max(0, FMath::FloorToInt32((FMath::Min(A.X, B.X) - Reach - X0) / PassCell));
			const int32 IX1 = FMath::Min(NX - 1, FMath::CeilToInt32((FMath::Max(A.X, B.X) + Reach - X0) / PassCell));
			const int32 IY0 = FMath::Max(0, FMath::FloorToInt32((FMath::Min(A.Y, B.Y) - Reach - Y0) / PassCell));
			const int32 IY1 = FMath::Min(NY - 1, FMath::CeilToInt32((FMath::Max(A.Y, B.Y) + Reach - Y0) / PassCell));
			for (int32 IY = IY0; IY <= IY1; ++IY)
			{
				for (int32 IX = IX0; IX <= IX1; ++IX)
				{
					double T = 0.0;
					if (TNProcMap::DistPointSegment(CellCenter(IX, IY), A, B, T) > Reach) { continue; }
					uint16& C = Count[IY * NX + IX];
					C = static_cast<uint16>(FMath::Clamp(static_cast<int32>(C) + Delta, 0, 65535));
				}
			}
		}

		/** Hay un camino de casillas libres (vecinas en cruz) de la primera columna a la última. */
		bool IsConnected() const
		{
			if (NX <= 0 || NY <= 0) { return false; }
			TArray<uint8> Seen;
			Seen.Init(0, NX * NY);
			TArray<int32> Queue;
			Queue.Reserve(NX * 4);
			for (int32 IY = 0; IY < NY; ++IY)
			{
				if (IsFree(0, IY))
				{
					Seen[IY * NX] = 1;
					Queue.Add(IY * NX);
				}
			}
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 Cell = Queue[Head];
				const int32 CX = Cell % NX;
				const int32 CY = Cell / NX;
				if (CX == NX - 1) { return true; }
				const int32 Nx[4] = { CX + 1, CX - 1, CX, CX };
				const int32 Ny[4] = { CY, CY, CY + 1, CY - 1 };
				for (int32 k = 0; k < 4; ++k)
				{
					if (Nx[k] < 0 || Nx[k] >= NX || Ny[k] < 0 || Ny[k] >= NY) { continue; }
					const int32 Next = Ny[k] * NX + Nx[k];
					if (Seen[Next] || !IsFree(Nx[k], Ny[k])) { continue; }
					Seen[Next] = 1;
					Queue.Add(Next);
				}
			}
			return false;
		}
	};

	/** El reparto de una ronda. */
	struct FRoundLayout
	{
		int32 Seed = 0;
		TArray<FItem> Items;
		/** Asiento en la arena de cada elemento que está en el suelo (todos menos las gaviotas). */
		TArray<FStamp> Stamps;
		int32 NumDecor = 0;
		int32 NumTraps = 0;
		int32 NumEnemies = 0;
		int32 NumQuadLanes = 0;
		int32 NumGullZones = 0;
		int32 NumGuidePaths = 0;
		/** Centro del castillo con salas (0 si no hay). */
		FVector2D DungeonPos = FVector2D::ZeroVector;
		/** Lado (+1 / -1) por el que se rodea el castillo. */
		int32 DungeonGapSide = 0;
		bool bPassageOk = false;

		/** Resumen para el registro («[Playa] ronda N: ...»). */
		FString Summary() const
		{
			return FString::Printf(TEXT("semilla %d · %d elementos (%d decorado, %d trampas, %d enemigos) · %d pasos de quads · %d zonas de gaviotas · %d asientos · %d tramos de pasarela guía · castillo con salas a %.0f m (se rodea por %s) · paso libre de %.0f m %s"),
				Seed, Items.Num(), NumDecor, NumTraps, NumEnemies, NumQuadLanes, NumGullZones, Stamps.Num(), NumGuidePaths, DungeonPos.X / 100.0,
				DungeonGapSide > 0 ? TEXT("la derecha (+Y)") : TEXT("la izquierda (-Y)"), MinPassage / 100.0, bPassageOk ? TEXT("OK") : TEXT("ROTO"));
		}
	};

	/** Densidad de las bandas: fracción del área cubierta por huellas, del 5 % en la salida al 19 % junto al mar. */
	inline double BandCoverage(double T)
	{
		return 0.05 + 0.14 * FMath::Clamp(T, 0.0, 1.0);
	}

	inline double ProgressOfX(double X)
	{
		return FMath::Clamp((X - ItemsStartX) / (ItemsEndX - ItemsStartX), 0.0, 1.0);
	}

	inline double XOfProgress(double T)
	{
		return ItemsStartX + FMath::Clamp(T, 0.0, 1.0) * (ItemsEndX - ItemsStartX);
	}

	/** Dentro de la playa repartible (salvo el paso de quads, que la cruza entera). */
	inline bool InBounds(const FItem& Item)
	{
		for (const FVector2D& P : { Item.EndA(), Item.EndB() })
		{
			if (P.X - Item.Radius < ItemsStartX || P.X + Item.Radius > ItemsEndX) { return false; }
			if (Item.Element != ETNBeachElement::QuadLane && FMath::Abs(P.Y) + Item.Radius > HalfWidth - SideMargin) { return false; }
		}
		return true;
	}

	/** No pisa nada de lo ya puesto (las gaviotas solo miran a otras gaviotas y las demás no las miran a ellas). */
	inline bool Fits(const TArray<FItem>& Items, const FItem& New, double Pad)
	{
		for (const FItem& Old : Items)
		{
			if (Old.bOverlay != New.bOverlay) { continue; }
			if (Clearance(Old, New) < Pad) { return false; }
		}
		return true;
	}

	/** Especificación de un elemento con su tamaño (y su largo, si es alargado). */
	inline FItem MakeItem(TNProcMap::FRng& Rng, ETNBeachElement E, const FElementRule& Rule, double SizeScale, double Extent)
	{
		FItem Item;
		Item.Element = E;
		Item.Radius = TNBeach::FootprintRadius(E) * SizeScale;
		Item.HalfLength = Extent * 0.5;
		Item.bBlocking = Rule.bBlocking;
		Item.bOverlay = Rule.bOverlay;
		Item.Yaw = Rule.YawJitter >= 180.0 ? Rng.Range(0.0, 360.0) : Rule.BaseYaw + Rng.Range(-Rule.YawJitter, Rule.YawJitter);
		Item.Spec.Element = E;
		Item.Spec.Seed = Rng.RangeInt(1, 0x7FFFFFFF);
		Item.Spec.SizeScale = static_cast<float>(SizeScale);
		Item.Spec.Extent = static_cast<float>(Extent);
		return Item;
	}

	/** Elige un elemento de la categoría Cat para el progreso T (Count si no hay ninguno posible). */
	inline ETNBeachElement PickElement(TNProcMap::FRng& Rng, ETNBeachCategory Cat, double T, const TArray<int32>& Counts)
	{
		const int32 Num = static_cast<int32>(ETNBeachElement::Count);
		TArray<double> W;
		W.Init(0.0, Num);
		double Total = 0.0;
		for (int32 i = 0; i < Num; ++i)
		{
			const ETNBeachElement E = static_cast<ETNBeachElement>(i);
			if (TNBeach::CategoryOf(E) != Cat) { continue; }
			const FElementRule Rule = RuleOf(E);
			if (Rule.bSpecial || Rule.Weight <= 0.0 || T < Rule.MinT || T > Rule.MaxT || Counts[i] >= Rule.MaxPerRound) { continue; }
			W[i] = Rule.Weight * FMath::Max(0.0, 1.0 + Rule.SeaBias * (2.0 * T - 1.0));
			Total += W[i];
		}
		if (Total <= 0.0) { return ETNBeachElement::Count; }
		double U = Rng.Unit() * Total;
		for (int32 i = 0; i < Num; ++i)
		{
			if (W[i] <= 0.0) { continue; }
			U -= W[i];
			if (U <= 0.0) { return static_cast<ETNBeachElement>(i); }
		}
		for (int32 i = Num - 1; i >= 0; --i)
		{
			if (W[i] > 0.0) { return static_cast<ETNBeachElement>(i); }
		}
		return ETNBeachElement::Count;
	}

	/** Y de un elemento según su preferencia de lado (Usable: hasta dónde puede ir su centro). */
	inline double PickY(TNProcMap::FRng& Rng, double SideBias, double Usable)
	{
		if (Usable <= 0.0) { return 0.0; }
		if (SideBias > 0.0 && Rng.Chance(SideBias))
		{
			return (Rng.Chance(0.5) ? 1.0 : -1.0) * Rng.Range(0.55, 1.0) * Usable;
		}
		if (SideBias < 0.0 && Rng.Chance(-SideBias))
		{
			return Rng.Range(-0.55, 0.55) * Usable;
		}
		return Rng.Range(-Usable, Usable);
	}

	/** Estado del reparto mientras se construye. */
	struct FBuilder
	{
		FRoundLayout& Out;
		TNProcMap::FRng Rng;
		FPassGrid Grid;
		TArray<int32> Counts;

		FBuilder(FRoundLayout& InOut, int32 InSeed)
			: Out(InOut)
			, Rng(static_cast<uint64>(static_cast<uint32>(InSeed)) * 0x9E3779B1ull + 0xBEAC4ull)
		{
			Grid.Init();
			Counts.Init(0, static_cast<int32>(ETNBeachElement::Count));
		}

		bool TryAdd(const FItem& Item, double Pad)
		{
			if (!InBounds(Item) || !Fits(Out.Items, Item, Pad)) { return false; }
			Out.Items.Add(Item);
			Grid.Stamp(Item, 1);
			++Counts[static_cast<int32>(Item.Element)];
			return true;
		}

		void PopLast()
		{
			if (Out.Items.Num() == 0) { return; }
			const FItem& Last = Out.Items.Last();
			Grid.Stamp(Last, -1);
			--Counts[static_cast<int32>(Last.Element)];
			Out.Items.Pop();
		}

		/** Quita lo añadido desde From (lo último primero) hasta que vuelva a haber paso. */
		void RestorePassage(int32 From)
		{
			while (Out.Items.Num() > From && !Grid.IsConnected()) { PopLast(); }
		}

		/**
		 * Decorado que sirve de muro en las alas del castillo y cabe en MaxRadius: redondo y que cierra el paso; de 7 a 26 m
		 * de huella mientras quepa (los grandes, más a menudo) y más pequeño para cerrar el último hueco.
		 */
		ETNBeachElement PickWallBlocker(double MaxRadius)
		{
			const int32 Num = static_cast<int32>(ETNBeachElement::Count);
			const double MinFoot = MaxRadius >= 1500.0 ? 700.0 : 250.0;
			TArray<ETNBeachElement> Options;
			TArray<double> Weights;
			double Total = 0.0;
			for (int32 i = 0; i < Num; ++i)
			{
				const ETNBeachElement E = static_cast<ETNBeachElement>(i);
				if (TNBeach::CategoryOf(E) != ETNBeachCategory::Decor) { continue; }
				const FElementRule Rule = RuleOf(E);
				const double Foot = TNBeach::FootprintRadius(E);
				if (!Rule.bBlocking || Rule.ExtentMax > 0.0 || Foot < MinFoot || Foot > 2600.0 || Foot * 0.6 > MaxRadius || Counts[i] >= Rule.MaxPerRound)
				{
					continue;
				}
				Options.Add(E);
				Weights.Add(Foot);
				Total += Foot;
			}
			if (Options.Num() == 0) { return ETNBeachElement::Count; }
			double U = Rng.Unit() * Total;
			for (int32 i = 0; i < Options.Num(); ++i)
			{
				U -= Weights[i];
				if (U <= 0.0) { return Options[i]; }
			}
			return Options.Last();
		}

		/**
		 * Castillo con salas hacia la mitad del recorrido y dos alas de decorado y alambre que cierran la playa en embudo
		 * hacia su entrada: o se atraviesa o se rodea por el único hueco (18 m, con algas) junto a la selva de un lado.
		 */
		void PlaceDungeon()
		{
			const FElementRule Rule = RuleOf(ETNBeachElement::SandDungeon);
			const double S = Rng.Range(Rule.SizeMin, Rule.SizeMax);
			FItem Castle = MakeItem(Rng, ETNBeachElement::SandDungeon, Rule, S, 0.0);
			Castle.Role = EItemRole::Dungeon;
			Castle.Pos = FVector2D(XOfProgress(Rng.Range(0.42, 0.58)), Rng.Range(-1.0, 1.0) * HalfWidth * 0.28);
			if (!TryAdd(Castle, ItemPad)) { return; }
			Out.DungeonPos = Castle.Pos;
			const int32 GapSide = Rng.Chance(0.5) ? 1 : -1;
			Out.DungeonGapSide = GapSide;
			const int32 WallStart = Out.Items.Num();
			static constexpr double Sweep = 0.18;
			const double GapWidth = MinPassage + 600.0;
			for (const int32 Side : { -1, 1 })
			{
				const double EdgeY = Side * (HalfWidth - SideMargin);
				const double StopY = Side == GapSide ? EdgeY - Side * GapWidth : EdgeY;
				const double UEnd = FMath::Abs(StopY - Castle.Pos.Y);
				auto LineAt = [&Castle, Side](double U)
				{
					return FVector2D(Castle.Pos.X - Sweep * U, Castle.Pos.Y + Side * U);
				};
				const FVector2D LineDir = FVector2D(-Sweep, static_cast<double>(Side)).GetSafeNormal();
				const double StretchU = 1.0 / FMath::Abs(LineDir.Y);
				const FVector2D Across(LineDir.Y, -LineDir.X);
				const double WireR = TNBeach::FootprintRadius(ETNBeachElement::BarbedWire);
				double Cursor = Castle.Radius + 40.0;
				bool bWire = false;
				int32 Guard = 0;
				// Un muro de piezas que se tocan (alambre entre dos piezas) hasta el final del ala; lo que sobra al final se cierra
				// con una pieza más pequeña.
				while (Cursor < UEnd - 150.0 && ++Guard < 40)
				{
					const double Left = UEnd - Cursor;
					if (bWire && Left * StretchU >= 2.0 * WireR + 800.0)
					{
						const FElementRule WireRule = RuleOf(ETNBeachElement::BarbedWire);
						const double Len = FMath::Min(Rng.Range(1600.0, 3000.0), Left * StretchU - 2.0 * WireR);
						FItem Wire = MakeItem(Rng, ETNBeachElement::BarbedWire, WireRule, 1.0, Len);
						Wire.Role = EItemRole::DungeonWing;
						Wire.Yaw = FMath::RadiansToDegrees(FMath::Atan2(LineDir.Y, LineDir.X));
						Wire.Pos = LineAt(Cursor + (WireR + Len * 0.5) / StretchU);
						TryAdd(Wire, 0.0);
						Cursor += (Len + 2.0 * WireR) / StretchU + 20.0;
					}
					else
					{
						// El desorden de través mueve la pieza hasta 30 cm a lo ancho: que no se salga de la playa.
						const double MaxR = 0.5 * Left - 30.0;
						const ETNBeachElement E = MaxR > 0.0 ? PickWallBlocker(MaxR) : ETNBeachElement::Count;
						if (E == ETNBeachElement::Count) { break; }
						const FElementRule BlockRule = RuleOf(E);
						const double FootR = TNBeach::FootprintRadius(E);
						const double Sb = FMath::Min(Rng.Range(0.85, 1.15), MaxR / FootR);
						const double Rb = FootR * Sb;
						FItem Block = MakeItem(Rng, E, BlockRule, Sb, 0.0);
						Block.Role = EItemRole::DungeonWing;
						// Desorden solo de través: a lo largo del ala las piezas se tocan sin pisarse.
						Block.Pos = LineAt(Cursor + Rb) + Across * Rng.Range(-150.0, 150.0);
						TryAdd(Block, 0.0);
						Cursor += 2.0 * Rb + 40.0;
					}
					bWire = !bWire;
				}
				if (Side == GapSide)
				{
					// Algas en el hueco: rodear también cuesta.
					for (int32 k = 0; k < 2; ++k)
					{
						const FElementRule WeedRule = RuleOf(ETNBeachElement::Seaweed);
						FItem Weed = MakeItem(Rng, ETNBeachElement::Seaweed, WeedRule, Rng.Range(0.8, 1.0), 0.0);
						Weed.Role = EItemRole::DungeonWing;
						const double GapCenterY = EdgeY - Side * GapWidth * 0.5;
						Weed.Pos = FVector2D(LineAt(FMath::Abs(GapCenterY - Castle.Pos.Y)).X + (k == 0 ? -1.0 : 1.0) * Rng.Range(500.0, 1500.0), GapCenterY);
						TryAdd(Weed, ItemPad);
					}
				}
			}
			RestorePassage(WallStart);
		}

		/** Pasos de quads: 2 o 3 franjas que cruzan la playa entera, separadas 170 m como poco. */
		void PlaceQuadLanes()
		{
			const FElementRule Rule = RuleOf(ETNBeachElement::QuadLane);
			const int32 Wanted = Rng.Chance(0.5) ? 3 : 2;
			int32 Placed = 0;
			for (int32 Try = 0; Try < 60 && Placed < Wanted; ++Try)
			{
				FItem Lane = MakeItem(Rng, ETNBeachElement::QuadLane, Rule, Rng.Range(Rule.SizeMin, Rule.SizeMax), TNBeach::CourseWidth);
				Lane.Role = EItemRole::QuadLane;
				Lane.Yaw = 90.0;
				Lane.Pos = FVector2D(XOfProgress(Rng.Range(0.15, 0.92)), 0.0);
				bool bFar = true;
				for (const FItem& Other : Out.Items)
				{
					if (Other.Role == EItemRole::QuadLane && FMath::Abs(Other.Pos.X - Lane.Pos.X) < 17000.0) { bFar = false; }
				}
				if (bFar && TryAdd(Lane, ItemPad + 800.0)) { ++Placed; }
			}
		}

		/** Zonas de gaviotas y pelícanos: 2-4 por encima de la segunda mitad larga del recorrido. */
		void PlaceGullZones()
		{
			const FElementRule Rule = RuleOf(ETNBeachElement::GullZone);
			const int32 Wanted = Rng.RangeInt(2, 4);
			int32 Placed = 0;
			for (int32 Try = 0; Try < 60 && Placed < Wanted; ++Try)
			{
				FItem Zone = MakeItem(Rng, ETNBeachElement::GullZone, Rule, Rng.Range(Rule.SizeMin, Rule.SizeMax), 0.0);
				Zone.Role = EItemRole::GullZone;
				Zone.Pos = FVector2D(XOfProgress(Rng.Range(0.22, 0.97)), Rng.Range(-0.55, 0.55) * HalfWidth);
				if (TryAdd(Zone, 2000.0)) { ++Placed; }
			}
		}

		/** A veces, una hilera de pasarelas y caminitos de palos hacia el mar (guía visual), rodeando lo que haya. */
		void PlaceGuidePath()
		{
			if (!Rng.Chance(0.6)) { return; }
			FVector2D Cursor(XOfProgress(Rng.Range(0.03, 0.45)), Rng.Range(-0.5, 0.5) * HalfWidth);
			double Heading = Rng.Range(-10.0, 10.0);
			const double Total = Rng.Range(15000.0, 35000.0);
			double Done = 0.0;
			int32 Guard = 0;
			while (Done < Total && Cursor.X < ItemsEndX - 3000.0 && ++Guard < 40)
			{
				const bool bBoard = Rng.Chance(0.55);
				const ETNBeachElement E = bBoard ? ETNBeachElement::Boardwalk : ETNBeachElement::WoodenPostPath;
				const FElementRule Rule = RuleOf(E);
				const double Len = bBoard ? Rng.Range(3000.0, 6000.0) : Rng.Range(3500.0, 7000.0);
				FItem Seg = MakeItem(Rng, E, Rule, Rng.Range(Rule.SizeMin, Rule.SizeMax), Len);
				Seg.Role = EItemRole::GuidePath;
				Seg.Yaw = Heading + Rng.Range(-8.0, 8.0);
				Seg.Pos = Cursor + Seg.Axis() * (Len * 0.5 + Seg.Radius);
				const double Step = Len + 2.0 * Seg.Radius + Rng.Range(400.0, 900.0);
				if (TryAdd(Seg, ItemPad))
				{
					++Out.NumGuidePaths;
				}
				Cursor += Seg.Axis() * Step;
				Done += Step;
				// Se tuerce poco y vuelve hacia el centro si se acerca a la selva.
				Heading = FMath::Clamp(Heading + Rng.Range(-9.0, 9.0) - 0.0006 * Cursor.Y, -22.0, 22.0);
			}
		}

		/** Relleno por bandas de 50 m, más denso hacia el mar, sin cerrar nunca el paso. */
		void FillBands()
		{
			for (double BX = ItemsStartX; BX < ItemsEndX - 1.0; BX += BandLength)
			{
				const double BX1 = FMath::Min(BX + BandLength, ItemsEndX);
				const double T = ProgressOfX(0.5 * (BX + BX1));
				const double Target = BandCoverage(T) * (BX1 - BX) * 2.0 * (HalfWidth - SideMargin);
				double Area = 0.0;
				for (const FItem& Item : Out.Items)
				{
					if (Item.bOverlay || Item.Role == EItemRole::QuadLane || Item.Role == EItemRole::GuidePath) { continue; }
					if (Item.Pos.X >= BX && Item.Pos.X < BX1) { Area += Item.Area(); }
				}
				const int32 BandStart = Out.Items.Num();
				int32 Added = 0;
				for (int32 Attempt = 0; Attempt < 70 && Area < Target && Added < 16; ++Attempt)
				{
					const double WD = 0.62 - 0.22 * T;
					const double WT = 0.22 + 0.10 * T;
					const double WE = 0.16 + 0.12 * T;
					const double U = Rng.Unit() * (WD + WT + WE);
					const ETNBeachCategory Cat = U < WD ? ETNBeachCategory::Decor : (U < WD + WT ? ETNBeachCategory::Trap : ETNBeachCategory::Enemy);
					const ETNBeachElement E = PickElement(Rng, Cat, T, Counts);
					if (E == ETNBeachElement::Count) { continue; }
					const FElementRule Rule = RuleOf(E);
					const double S = Rng.Range(Rule.SizeMin, Rule.SizeMax);
					const double Extent = Rule.ExtentMax > 0.0 ? Rng.Range(Rule.ExtentMin, Rule.ExtentMax) : 0.0;
					FItem Item = MakeItem(Rng, E, Rule, S, Extent);
					const double Reach = Item.Radius + FMath::Abs(Item.Axis().Y) * Item.HalfLength;
					Item.Pos = FVector2D(Rng.Range(BX, BX1), PickY(Rng, Rule.SideBias, HalfWidth - SideMargin - Reach));
					if (!TryAdd(Item, ItemPad)) { continue; }
					Area += Item.Area();
					++Added;
					if (Rule.bClusters && Rng.Chance(0.35))
					{
						// Corrillo: 1-2 más iguales alrededor (cocos bajo las palmeras, conchas, vasos...).
						const int32 More = Rng.RangeInt(1, 2);
						for (int32 m = 0; m < More && Counts[static_cast<int32>(E)] < Rule.MaxPerRound; ++m)
						{
							FItem Mate = MakeItem(Rng, E, Rule, Rng.Range(Rule.SizeMin, Rule.SizeMax), 0.0);
							const double Ang = Rng.Range(0.0, TNProcMap::TwoPi);
							Mate.Pos = Item.Pos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Item.Radius + Mate.Radius + Rng.Range(150.0, 700.0));
							if (TryAdd(Mate, ItemPad * 0.5))
							{
								Area += Mate.Area();
								++Added;
							}
						}
					}
				}
				RestorePassage(BandStart);
			}
		}

		void Finish()
		{
			for (const FItem& Item : Out.Items)
			{
				switch (TNBeach::CategoryOf(Item.Element))
				{
					case ETNBeachCategory::Decor: ++Out.NumDecor; break;
					case ETNBeachCategory::Trap: ++Out.NumTraps; break;
					default: ++Out.NumEnemies; break;
				}
				if (Item.Element == ETNBeachElement::QuadLane) { ++Out.NumQuadLanes; }
				if (Item.Element == ETNBeachElement::GullZone) { ++Out.NumGullZones; }
				if (!Item.bOverlay) { Out.Stamps.Add(MakeStamp(Item)); }
			}
			Out.bPassageOk = Grid.IsConnected();
		}
	};

	/**
	 * Reparto de la ronda con Seed (determinista: el mismo en el servidor y en cada cliente). Orden: castillo con sus
	 * alas, pasos de quads, zonas de gaviotas, pasarela guía (a veces) y el relleno por bandas; tras cada paso se quita lo
	 * último puesto si cierra el paso libre.
	 */
	inline void GenerateRound(int32 Seed, FRoundLayout& Out)
	{
		Out = FRoundLayout();
		Out.Seed = Seed;
		FBuilder Builder(Out, Seed);
		Builder.PlaceDungeon();
		Builder.PlaceQuadLanes();
		Builder.PlaceGullZones();
		Builder.PlaceGuidePath();
		Builder.FillBands();
		Builder.Finish();
	}
}
