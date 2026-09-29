#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "World/ProcMap/TN_ProcMapMath.h"
#include "World/ProcMap/TN_ProcMapLayout.h"

/**
 * Recorrido del tutorial de la primera partida (ATN_TutorialCourse, Docs/Tutorial.md): siempre el mismo, diseñado a mano
 * aquí (sin semilla que cambie: el ruido del relieve usa una fija), idéntico en todas las máquinas y en todas las partidas.
 *
 * Es un trozo del mundo del cooperativo suspendido entre nubes: dos islas alargadas (A, de la salida al cañón de la
 * catapulta; B, del cañón a la cascada) con un pasillo natural por el medio, taludes de 55-75° a los lados, una meseta con
 * vegetación hasta el borde y, por debajo, la roca colgando en cono. El pasillo cruza siete biomas del mapa procedural
 * (selva, roca, playa, desierto, laguna, manglar y pueblo) y en cada tramo hay una estación que enseña una cosa.
 *
 * Lógica pura (solo CoreMinimal y las cabeceras puras del mapa procedural): la usan la malla, la vegetación, la fauna, el
 * servidor (puntos de control, zona de caída, dónde va cada cosa) y el HUD (en qué estación está la tortuga).
 *
 * Espacio local del recorrido (cm): X a lo largo del pasillo, de la salida (X < 0) a la cascada (LipX); Y a la derecha
 * mirando hacia +X; Z arriba, con Z = 0 el suelo del pasillo junto a la cascada. El eje de las islas es Y = 0 y el pasillo
 * serpentea alrededor (CorridorCenter). ATN_TutorialCourse pone este espacio en el mundo con la cascada sobre el lobby.
 */
namespace TNTutorial
{
	/** Estaciones, en el orden del recorrido. */
	enum class EStation : uint8
	{
		Welcome,     ///< Moverse y mirar.
		Sprint,      ///< Correr y la energía.
		Jump,        ///< Saltar a un escalón.
		BellyDive,   ///< Panzazo (plancha) sobre la zanja.
		Pickup,      ///< Coger un objeto.
		Search,      ///< Rebuscar el montículo.
		Slots,       ///< La aleta y el caparazón: cambiar y soltar.
		Throw,       ///< Lanzar la bola al cangrejo.
		UseItem,     ///< Usar la barrita.
		Shell,       ///< Meterse en el caparazón y rodar.
		Trampoline,  ///< La medusa trampolín.
		Catapult,    ///< La catapulta sobre el cañón.
		Swim,        ///< Nadar la laguna.
		Carry,       ///< Coger y lanzar a Rodolfo.
		Escape,      ///< Liberarse de Berta.
		Emotes,      ///< Bailes y frases.
		Voice,       ///< Voz de proximidad.
		PauseMenu,   ///< Menú de pausa, ajustes y accesibilidad.
		Waterfall,   ///< La cascada al castillo.
		Count
	};

	constexpr int32 NumStations = static_cast<int32>(EStation::Count);
	constexpr int32 MaxTasks = 2;

	/** Tecla o botón que enseña una tarea (el HUD pone la del jugador, de teclado o de mando). */
	enum class EKey : uint8
	{
		None, Move, Look, Sprint, Jump, Interact, RotateInventory, DropItem, Shell, EmoteWheel, ChatWheel, Talk, Pause
	};

	/** Semilla fija del relieve, la vegetación y la fauna: el recorrido es siempre el mismo. */
	constexpr uint32 Seed = 0x7A7011A1u;

	/** Medidas del recorrido (cm, espacio local). */
	namespace Dims
	{
		/** Altura del suelo del pasillo junto a la cascada sobre el suelo del lobby donde se cae (180 m: por encima de las sierras del valle). */
		constexpr double Height = 18000.0;
		/** Separación de las filas de la malla a lo largo del pasillo. */
		constexpr double RowStep = 50.0;

		/** Isla A: de la punta de la salida al cañón de la catapulta. Isla B: del cañón a la cascada. */
		constexpr double PartA0 = -900.0;
		constexpr double PartA1 = 12500.0;
		constexpr double PartB0 = 13600.0;
		constexpr double PartB1 = 21500.0;
		/** El borde de la cascada: se cae por aquí al castillo. */
		constexpr double LipX = PartB1;

		/** Donde se aparece y el rincón cerrado de detrás (el fondo del pasillo sube hasta los taludes). */
		constexpr double SpawnX = 250.0;
		constexpr double BackWallLength = 350.0;

		/** Escalón de 90 cm (se salta: la tortuga sube 1,2 m). */
		constexpr double LedgeX = 2900.0;
		constexpr double LedgeH = 90.0;

		/** Zanja de 3 m y 1 m de hondo para el panzazo (se sale de un salto si se cae). */
		constexpr double TrenchX0 = 3900.0;
		constexpr double TrenchX1 = 4200.0;
		constexpr double TrenchDepth = 100.0;

		/** Playa de los objetos: la barrita, el montículo y el cangrejo de prácticas. */
		constexpr double BarX = 5150.0;
		constexpr double MoundX = 5900.0;
		constexpr double MoundRadius = 125.0;
		constexpr double DummyX = 7650.0;

		/** Cuesta del desierto para rodar en bola: baja 3,5 m en 12 m (16° de media). */
		constexpr double SlopeX0 = 8900.0;
		constexpr double SlopeX1 = 10100.0;
		constexpr double SlopeDrop = 350.0;

		/** La medusa al pie de la cuesta y la pared de 4,5 m de la terraza de la catapulta. */
		constexpr double JellyX = 10600.0;
		constexpr double TerraceX = 11000.0;
		constexpr double TerraceZ = 190.0;
		constexpr double CatapultX = 11650.0;

		/** Tronco de equilibrio sobre el cañón (para quien no quiera la catapulta), a un lado del pasillo. */
		constexpr double LogY = -410.0;
		constexpr double LogRadius = 42.0;

		/** Laguna: la entrada en rampa y la salida con un bordillo de 50 cm (salto del agua). */
		constexpr double PoolX0 = 15700.0;
		constexpr double PoolX1 = 16700.0;
		constexpr double PoolEntry = 260.0;
		constexpr double PoolWaterZ = -50.0;
		constexpr double PoolFloorZ = -280.0;

		/** Tortugas de práctica: Rodolfo (se deja coger) y Berta (te coge). */
		constexpr double RodolfoX = 17350.0;
		constexpr double BertaX = 18300.0;

		/** Arroyo que lleva a la cascada: sale de la plaza del pueblo y se ensancha hasta el borde. */
		constexpr double StreamX0 = 20300.0;
		constexpr double StreamHalfW = 130.0;
		constexpr double StreamLipHalfW = 280.0;
		constexpr double StreamDepth = 26.0;

		/** Por debajo de esto (Z local, 20 m bajo el suelo más bajo) se ha caído de las islas: vuelve a su punto de control. */
		constexpr double VoidZ = -2400.0;
	}

	inline double Smooth(double Edge0, double Edge1, double X) { return TNProcMap::SmoothStep(Edge0, Edge1, X); }

	/** 1 entre A y B (con transición de Blend a cada lado), 0 fuera. */
	inline double Band(double A, double B, double X, double Blend = 300.0)
	{
		return Smooth(A - Blend, A + Blend, X) * (1.0 - Smooth(B - Blend, B + Blend, X));
	}

	/** Isla en la que cae X: 0 = A, 1 = B, INDEX_NONE = el cañón o fuera. */
	inline int32 PartOf(double X)
	{
		if (X >= Dims::PartA0 && X <= Dims::PartA1) { return 0; }
		if (X >= Dims::PartB0 && X <= Dims::PartB1) { return 1; }
		return INDEX_NONE;
	}

	/** Estrechamiento de la punta de la salida (1 en el resto). */
	inline double TipTaper(double X)
	{
		return FMath::Max(0.25, Smooth(Dims::PartA0, Dims::PartA0 + 800.0, X));
	}

	/** Recto donde hace falta (medusa, terraza, catapulta y cañón; laguna; cascada); serpentea en el resto. */
	inline double MeanderMask(double X)
	{
		double M = Smooth(-400.0, 700.0, X);
		M *= 1.0 - Smooth(9800.0, 10300.0, X) * (1.0 - Smooth(14500.0, 15000.0, X));
		M *= 1.0 - Smooth(15200.0, 15600.0, X) * (1.0 - Smooth(16900.0, 17300.0, X));
		M *= 1.0 - Smooth(20200.0, 20700.0, X);
		return M;
	}

	/** Y del centro del pasillo. */
	inline double CorridorCenter(double X)
	{
		const double A = 200.0 * MeanderMask(X);
		return A * (0.6 * FMath::Sin(TNProcMap::TwoPi * X / 5200.0 + 0.5) + 0.4 * FMath::Sin(TNProcMap::TwoPi * X / 2600.0 + 2.1));
	}

	/** Semiancho del pasillo (suelo pisable entre los taludes). */
	inline double CorridorHalfWidth(double X)
	{
		struct FKnot { double X; double W; };
		static const FKnot Knots[] = {
			{ -900.0, 380.0 }, { 2400.0, 380.0 }, { 2700.0, 320.0 }, { 4600.0, 320.0 }, { 5000.0, 620.0 }, { 8500.0, 620.0 },
			{ 8800.0, 420.0 }, { 10200.0, 420.0 }, { 10400.0, 520.0 }, { 12500.0, 520.0 }, { 13600.0, 900.0 }, { 15300.0, 900.0 },
			{ 15600.0, 560.0 }, { 16900.0, 560.0 }, { 17100.0, 480.0 }, { 18600.0, 480.0 }, { 18900.0, 700.0 }, { 20500.0, 700.0 },
			{ 20900.0, 460.0 }, { 21600.0, 460.0 } };
		constexpr int32 Num = UE_ARRAY_COUNT(Knots);
		double W = Knots[Num - 1].W;
		if (X <= Knots[0].X)
		{
			W = Knots[0].W;
		}
		else
		{
			for (int32 i = 0; i + 1 < Num; ++i)
			{
				if (X <= Knots[i + 1].X)
				{
					W = TNProcMap::LerpD(Knots[i].W, Knots[i + 1].W, Smooth(Knots[i].X, Knots[i + 1].X, X));
					break;
				}
			}
		}
		return W * TipTaper(X);
	}

	/** Alto de los taludes sobre el suelo: más altos en la roca, más bajos en la playa y el pueblo. */
	inline double BankHeight(double X)
	{
		double H = 380.0 + 90.0 * TNProcMap::Noise1(Seed + 11u, X / 1700.0);
		H += 110.0 * Band(2500.0, 4700.0, X);
		H -= 90.0 * Band(4800.0, 8700.0, X);
		H -= 60.0 * Band(18700.0, 21600.0, X);
		return FMath::Max(260.0, H);
	}

	/** Ancho en planta de los taludes (con el alto, 55-75° de pendiente). */
	inline double BankWidth(double X)
	{
		return (240.0 + 50.0 * TNProcMap::Noise1(Seed + 13u, X / 1300.0)) * TipTaper(X);
	}

	/** Forma del talud: muy empinado abajo (no se sube de un salto) y redondeado arriba, donde empieza la meseta. */
	inline double BankProfile(double T)
	{
		return 1.0 - FMath::Pow(1.0 - FMath::Clamp(T, 0.0, 1.0), 2.2);
	}

	/** Cota del terreno del tramo (el suelo natural): escalón, cuesta, hondonada de la medusa y terraza de la catapulta. */
	inline double GroundZ(double X)
	{
		if (X >= Dims::PartB0 - 1.0) { return 0.0; }
		if (X < Dims::LedgeX) { return 0.0; }
		if (X < Dims::SlopeX0) { return Dims::LedgeH; }
		if (X < Dims::SlopeX1) { return Dims::LedgeH - Dims::SlopeDrop * Smooth(Dims::SlopeX0, Dims::SlopeX1, X); }
		if (X < Dims::TerraceX) { return Dims::LedgeH - Dims::SlopeDrop; }
		return Dims::TerraceZ;
	}

	/** Semiancho del arroyo que lleva a la cascada (0 antes de que empiece). */
	inline double StreamHalfWidth(double X)
	{
		if (X < Dims::StreamX0) { return 0.0; }
		return TNProcMap::LerpD(Dims::StreamHalfW, Dims::StreamLipHalfW, Smooth(Dims::LipX - 900.0, Dims::LipX, X));
	}

	/** Suelo del pasillo a D del centro (con el rincón de la salida, la zanja, la laguna y el arroyo). */
	inline double FloorZ(double X, double D)
	{
		const double G = GroundZ(X);
		const double Ad = FMath::Abs(D);
		double Z = G + 4.0 * TNProcMap::Noise2(Seed + 3u, X / 320.0, D / 320.0);
		// Rincón de la salida: el fondo del pasillo sube en curva hasta lo alto de los taludes.
		if (X < 0.0)
		{
			Z = FMath::Max(Z, G + BankHeight(X) * BankProfile(-X / Dims::BackWallLength));
		}
		// Zanja del panzazo, de talud a talud.
		if (X >= Dims::TrenchX0 && X <= Dims::TrenchX1)
		{
			Z = G - Dims::TrenchDepth;
		}
		// Laguna: rampa de entrada y bordillo a plomo en la salida.
		if (X > Dims::PoolX0 && X < Dims::PoolX1)
		{
			Z = TNProcMap::LerpD(G, Dims::PoolFloorZ, Smooth(Dims::PoolX0, Dims::PoolX0 + Dims::PoolEntry, X));
		}
		// Arroyo por el medio, más ancho junto al borde.
		const double Hw = StreamHalfWidth(X);
		if (Hw > 0.0)
		{
			Z -= Dims::StreamDepth * Smooth(Hw, Hw - 45.0, Ad);
		}
		return Z;
	}

	/** Ancho de la meseta a cada lado (entre lo alto del talud y el borde de la isla). */
	inline double PlateauRight(double X) { return (620.0 + 320.0 * TNProcMap::Noise1(Seed + 23u, X / 1900.0)) * TipTaper(X); }
	inline double PlateauLeft(double X) { return (620.0 + 320.0 * TNProcMap::Noise1(Seed + 29u, X / 1700.0)) * TipTaper(X); }

	/** Distancia del eje de la isla (Y = 0) a su borde derecho e izquierdo. */
	inline double RimRight(double X) { return CorridorCenter(X) + CorridorHalfWidth(X) + BankWidth(X) + PlateauRight(X); }
	inline double RimLeft(double X) { return -CorridorCenter(X) + CorridorHalfWidth(X) + BankWidth(X) + PlateauLeft(X); }

	/** Meseta junto al pasillo (fuera de los taludes): lomas suaves y el borde que se redondea hacia abajo. */
	inline double PlateauZ(double X, double Y, double D, double W, double BW, double BH, double G)
	{
		const double Out = FMath::Abs(D) - (W + BW);
		const double RimDist = D > 0.0 ? RimRight(X) - Y : Y + RimLeft(X);
		double Z = G + BH;
		Z += 70.0 * TNProcMap::Fbm2(Seed + 17u, X / 1300.0, Y / 1300.0, 3) * Smooth(0.0, 350.0, Out);
		Z += 35.0 * Smooth(100.0, 700.0, Out);
		Z -= 90.0 * Smooth(260.0, 0.0, RimDist);
		return Z;
	}

	/** Cota de la superficie de las islas en (X, Y) local: pasillo, taludes y meseta. */
	inline double SurfaceZ(double X, double Y)
	{
		const double D = Y - CorridorCenter(X);
		const double Ad = FMath::Abs(D);
		const double W = CorridorHalfWidth(X);
		if (Ad <= W)
		{
			return FloorZ(X, D);
		}
		const double BW = BankWidth(X);
		const double BH = BankHeight(X);
		const double G = GroundZ(X);
		if (Ad <= W + BW)
		{
			double Z = G + BH * BankProfile((Ad - W) / FMath::Max(1.0, BW));
			if (X < 0.0)
			{
				Z = FMath::Max(Z, FloorZ(X, W));
			}
			return Z;
		}
		return PlateauZ(X, Y, D, W, BW, BH, G);
	}

	/** Suelo del pasillo en su centro (para colocar cosas y los puntos de control). */
	inline double CorridorFloorAt(double X, double D = 0.0) { return FloorZ(X, D); }

	/** Hondo de la roca que cuelga bajo la isla (más en el medio de cada isla, poco en la punta). */
	inline double KeelDepth(double X)
	{
		double Bulge = 0.0;
		if (X <= Dims::PartA1 + 1.0)
		{
			Bulge = FMath::Sin(TNProcMap::Pi * FMath::Clamp((X - Dims::PartA0) / (Dims::PartA1 - Dims::PartA0), 0.0, 1.0));
		}
		else
		{
			Bulge = FMath::Sin(TNProcMap::Pi * FMath::Clamp((X - Dims::PartB0) / (Dims::PartB1 - Dims::PartB0), 0.0, 1.0));
		}
		const double D = 1400.0 + 500.0 * TNProcMap::Noise1(Seed + 31u, X / 2300.0) + 900.0 * Bulge;
		return D * FMath::Max(0.2, Smooth(Dims::PartA0, Dims::PartA0 + 1500.0, X));
	}

	/** Pesos de bioma a lo largo del pasillo (suman 1): selva, roca, playa, desierto, laguna, manglar y pueblo. */
	inline void BiomeWeights(double X, double OutW[TNProcMap::NumBiomes])
	{
		using TNProcMap::BiomeIndex;
		for (int32 b = 0; b < TNProcMap::NumBiomes; ++b) { OutW[b] = 0.0; }
		constexpr double Blend = 350.0;
		OutW[BiomeIndex(ETNProcBiome::Jungle)] = 1.0 - Smooth(2500.0 - Blend, 2500.0 + Blend, X);
		OutW[BiomeIndex(ETNProcBiome::Rocky)] = Band(2500.0, 4700.0, X, Blend);
		OutW[BiomeIndex(ETNProcBiome::Beach)] = Band(4700.0, 8700.0, X, Blend);
		OutW[BiomeIndex(ETNProcBiome::Desert)] = Band(8700.0, 13000.0, X, Blend);
		OutW[BiomeIndex(ETNProcBiome::Water)] = Band(13000.0, 16900.0, X, Blend);
		OutW[BiomeIndex(ETNProcBiome::Mangrove)] = Band(16900.0, 18700.0, X, Blend);
		OutW[BiomeIndex(ETNProcBiome::Human)] = Smooth(18700.0 - Blend, 18700.0 + Blend, X);
		double Sum = 0.0;
		for (int32 b = 0; b < TNProcMap::NumBiomes; ++b) { Sum += OutW[b]; }
		if (Sum <= 1e-6)
		{
			OutW[BiomeIndex(ETNProcBiome::Jungle)] = 1.0;
			return;
		}
		for (int32 b = 0; b < TNProcMap::NumBiomes; ++b) { OutW[b] /= Sum; }
	}

	/** Bioma dominante en X (para la fauna y los carteles). */
	inline ETNProcBiome DominantBiome(double X)
	{
		double W[TNProcMap::NumBiomes];
		BiomeWeights(X, W);
		int32 Best = 0;
		for (int32 b = 1; b < TNProcMap::NumBiomes; ++b) { if (W[b] > W[Best]) { Best = b; } }
		return TNProcMap::BiomeFromIndex(Best);
	}

	/** Una estación: su tramo (X local), cuántas tareas tiene y la tecla de cada una. */
	struct FStationDef
	{
		EStation Id = EStation::Welcome;
		double X0 = 0.0;
		double X1 = 0.0;
		int32 NumTasks = 1;
		EKey Keys[MaxTasks] = { EKey::None, EKey::None };
		/** Tramo en el que se aparece al caerse (X del punto de control). */
		double CheckpointX = 0.0;
	};

	inline const FStationDef& Station(int32 Index)
	{
		static const FStationDef Table[NumStations] = {
			{ EStation::Welcome,      -400.0,  1000.0, 2, { EKey::Move, EKey::Look }, Dims::SpawnX },
			{ EStation::Sprint,       1000.0,  2500.0, 1, { EKey::Sprint, EKey::None }, 1100.0 },
			{ EStation::Jump,         2500.0,  3300.0, 1, { EKey::Jump, EKey::None }, 2600.0 },
			{ EStation::BellyDive,    3300.0,  4700.0, 1, { EKey::Jump, EKey::None }, 3400.0 },
			{ EStation::Pickup,       4700.0,  5500.0, 1, { EKey::Interact, EKey::None }, 4800.0 },
			{ EStation::Search,       5500.0,  6300.0, 1, { EKey::Interact, EKey::None }, 5550.0 },
			{ EStation::Slots,        6300.0,  6900.0, 2, { EKey::RotateInventory, EKey::DropItem }, 6350.0 },
			{ EStation::Throw,        6900.0,  7900.0, 1, { EKey::Interact, EKey::None }, 6950.0 },
			{ EStation::UseItem,      7900.0,  8700.0, 1, { EKey::Interact, EKey::None }, 7950.0 },
			{ EStation::Shell,        8700.0, 10300.0, 2, { EKey::Shell, EKey::Shell }, 8750.0 },
			{ EStation::Trampoline,  10300.0, 11000.0, 1, { EKey::None, EKey::None }, 10350.0 },
			{ EStation::Catapult,    11000.0, 14000.0, 1, { EKey::None, EKey::None }, 11200.0 },
			{ EStation::Swim,        14000.0, 16900.0, 2, { EKey::None, EKey::Jump }, 14050.0 },
			{ EStation::Carry,       16900.0, 17800.0, 2, { EKey::Interact, EKey::Interact }, 16950.0 },
			{ EStation::Escape,      17800.0, 18700.0, 2, { EKey::Shell, EKey::Move }, 17850.0 },
			{ EStation::Emotes,      18700.0, 19500.0, 2, { EKey::EmoteWheel, EKey::ChatWheel }, 18750.0 },
			{ EStation::Voice,       19500.0, 20200.0, 1, { EKey::Talk, EKey::None }, 19550.0 },
			{ EStation::PauseMenu,   20200.0, 20900.0, 1, { EKey::Pause, EKey::None }, 20250.0 },
			{ EStation::Waterfall,   20900.0, 21600.0, 1, { EKey::None, EKey::None }, 20950.0 },
		};
		return Table[FMath::Clamp(Index, 0, NumStations - 1)];
	}

	/** Estación del tramo en el que está X (la primera si está antes, la última si está después). */
	inline int32 StationAtX(double X)
	{
		for (int32 i = 0; i < NumStations; ++i)
		{
			if (X < Station(i).X1)
			{
				return i;
			}
		}
		return NumStations - 1;
	}

	/**
	 * Puntos de control (X local) en orden: se vuelve al último que se haya pisado al caerse de las islas. Uno justo pasado
	 * el cañón, para que quien ya lo cruzó no tenga que volver a la catapulta.
	 */
	inline int32 NumCheckpoints() { return NumStations + 1; }

	inline double CheckpointX(int32 Index)
	{
		// Las estaciones, más uno en la pradera de la isla B nada más cruzar el cañón.
		static const double Extra = Dims::PartB0 + 150.0;
		if (Index < 0) { return Station(0).CheckpointX; }
		if (Index <= static_cast<int32>(EStation::Catapult)) { return Station(Index).CheckpointX; }
		if (Index == static_cast<int32>(EStation::Catapult) + 1) { return Extra; }
		return Station(FMath::Min(Index - 1, NumStations - 1)).CheckpointX;
	}

	/** Punto de control más avanzado que queda en X o por detrás, en la misma isla. */
	inline int32 CheckpointAtOrBefore(double X)
	{
		int32 Best = 0;
		const int32 Part = PartOf(X);
		for (int32 i = 0; i < NumCheckpoints(); ++i)
		{
			const double Cx = CheckpointX(i);
			if (Cx <= X + 1.0 && (Part == INDEX_NONE || PartOf(Cx) == Part))
			{
				Best = i;
			}
		}
		return Best;
	}

	/** Pie (suelo) de un punto de control, en el centro del pasillo. */
	inline FVector CheckpointFeet(int32 Index)
	{
		const double X = CheckpointX(Index);
		const double C = CorridorCenter(X);
		return FVector(X, C, CorridorFloorAt(X, 0.0));
	}

	/** Sitio de aparición N (hasta ocho, en dos filas a los lados del centro del pasillo). */
	inline FVector SpawnFeet(int32 Index)
	{
		const int32 Slot = ((Index % 8) + 8) % 8;
		const double Lateral = ((Slot % 4) - 1.5) * 160.0;
		const double X = Dims::SpawnX - (Slot / 4) * 180.0;
		const double C = CorridorCenter(X);
		return FVector(X, C + Lateral, CorridorFloorAt(X, Lateral));
	}

	// ── Dónde va cada cosa (pies en el suelo, espacio local) ────────────────────

	inline FVector BarFeet() { const double X = Dims::BarX; const double D = 140.0; return FVector(X, CorridorCenter(X) + D, FloorZ(X, D)); }

	inline FVector MoundFeet()
	{
		const double X = Dims::MoundX;
		const double D = CorridorHalfWidth(X) - 230.0;
		return FVector(X, CorridorCenter(X) + D, FloorZ(X, D));
	}

	inline FVector DummyFeet()
	{
		const double X = Dims::DummyX;
		const double D = -(CorridorHalfWidth(X) - 260.0);
		return FVector(X, CorridorCenter(X) + D, FloorZ(X, D));
	}

	inline FVector JellyFeet() { const double X = Dims::JellyX; return FVector(X, CorridorCenter(X), GroundZ(X)); }

	inline FVector CatapultFeet() { return FVector(Dims::CatapultX, CorridorCenter(Dims::CatapultX), Dims::TerraceZ); }

	inline FVector RodolfoFeet() { const double X = Dims::RodolfoX; const double D = 170.0; return FVector(X, CorridorCenter(X) + D, FloorZ(X, D)); }

	inline FVector BertaFeet() { const double X = Dims::BertaX; const double D = -170.0; return FVector(X, CorridorCenter(X) + D, FloorZ(X, D)); }

	/** Extremos del tronco de equilibrio sobre el cañón (su eje, a la altura de la madera). */
	inline FVector LogStart() { return FVector(Dims::PartA1 - 160.0, Dims::LogY, Dims::TerraceZ + Dims::LogRadius * 0.4); }
	inline FVector LogEnd() { return FVector(Dims::PartB0 + 160.0, Dims::LogY, GroundZ(Dims::PartB0) + Dims::LogRadius * 0.4); }

	/** Cartel de la estación N: en el borde del pasillo, al empezar su tramo, a un lado o al otro. */
	inline FVector SignFeet(int32 Index)
	{
		const FStationDef& S = Station(Index);
		// La primera, delante de donde se aparece (su tramo empieza en el rincón de la salida).
		const double X = Index == 0 ? Dims::SpawnX + 450.0 : S.X0 + 150.0;
		const double Side = (Index % 2 == 0) ? 1.0 : -1.0;
		const double D = Side * (CorridorHalfWidth(X) - 70.0);
		return FVector(X, CorridorCenter(X) + D, FloorZ(X, D));
	}
}
