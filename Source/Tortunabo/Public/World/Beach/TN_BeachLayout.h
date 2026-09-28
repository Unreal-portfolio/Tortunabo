#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachTypes.h"
#include "World/ProcMap/TN_ProcMapMath.h"

/**
 * Playa del modo carrera (ATN_BeachRaceGenerator, Docs/Modo_Carrera.md), lógica pura: el terreno fijo (perfil hacia el
 * mar, relieve con corredores, dunas con cresta, pozas de agua y trincheras, bancos de la selva, repisa y acantilado de
 * roca, fondo del mar), la salida, las zonas de meta y de zambullida, y el reparto de cada ronda con su semilla (castillos
 * con salas, filas que obligan a zigzaguear, pasos de quads, zonas de gaviotas, puestos militares, rincones, lanzadores,
 * relleno por bandas, los puntos interesantes para el botín y el asiento de cada elemento en la arena). Sin mundo ni
 * actores: lo usan el generador (el servidor para colocar y cada cliente, con la semilla replicada, para los asientos) y
 * los tests (Private/Tests/TN_BeachLayoutTest.cpp).
 *
 * Espacio local del generador, en cm: X a lo largo del recorrido (línea de salida en X = 0, borde del acantilado en
 * X ≈ Length, el mar más allá), Y a lo ancho (la playa jugable en |Y| <= HalfWidth) y Z arriba, con el agua en Z = 0.
 *
 * Orientación de los elementos (Yaw, grados sobre Z): con Yaw 0 el eje X local del elemento mira al mar. Los alargados
 * (Extent > 0) tienen el largo por su eje X local, centrado en su origen, y la huella es su semigrosor por el eje Y
 * local: el alambre y el paso de quads van a ~90° (cruzan la playa), la pasarela y el caminito de palos a ~0° (hacia el
 * mar). El castillo con salas, la puerta de conchas, el cubo roto, la pala, las catapultas y los trampolines van a ~0°:
 * se entra (o se lanza) hacia +X. Todos se colocan con el origen a la cota de su asiento (PlacementZ): el suelo bajo su
 * huella queda liso (FStamp) y a nivel, salvo el paso de quads, que no allana (solo oscurece sus rodadas). Los enemigos
 * no tienen asiento: buscan el suelo ellos solos.
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

	// ── Salida: una fila de cuatro huevos en el linde de la selva, entre las raíces de un árbol colosal ──
	constexpr int32 NumStartSpots = 4;
	constexpr double StartSpotX = -800.0;
	constexpr double StartSpotSpacing = 1000.0;
	constexpr double StartRowSpacing = 900.0;
	constexpr double TrunkX = -5200.0;
	constexpr double TrunkRadius = 1500.0;
	/** Muros invisibles: detrás de la salida (delante del tronco) y a los lados (8 m fuera de la playa jugable). */
	constexpr double BackWallX = -3200.0;
	constexpr double SideWallY = HalfWidth + 800.0;
	/** Al romperse los huevos, cada tortuga sale lanzada hacia el mar a esta velocidad (cm/s): ~15 m por el aire. */
	constexpr double EggLaunchForward = 1000.0;
	constexpr double EggLaunchUp = 650.0;
	/** Se lanza a quien esté en la salida al abrirse (por delante del muro de detrás y hasta aquí, a lo largo). */
	constexpr double EggLaunchReachX = 1500.0;

	// ── Reparto ──
	/** Nada a menos de 60 m por delante de la línea de salida (68 m de los huevos). */
	constexpr double ItemsStartX = 6000.0;
	/** Nada a menos de 30 m del borde del acantilado. */
	constexpr double ItemsEndX = Length - 3000.0;
	/** Los enemigos (su huella entera: por donde patrullan) quedan a 5 m de la selva como poco. */
	constexpr double SideMargin = 500.0;
	/**
	 * Hasta dónde llega la huella del decorado y las trampas: a 4 m de los muros, también sobre el pie de los bancos (junto
	 * a la selva tampoco queda un pasillo libre).
	 */
	constexpr double SideReach = HalfWidth + 400.0;
	/** Paso libre a lo ancho que siempre queda (obstáculos inflados MinPassage / 2 y un camino de un lado a otro). */
	constexpr double MinPassage = 800.0;
	constexpr double BandLength = 5000.0;
	/** Separación mínima entre huellas (salvo las piezas de las filas y las alas del castillo, que se tocan). */
	constexpr double ItemPad = 150.0;
	/** Casilla de la rejilla con la que se comprueba el paso. */
	constexpr double PassCell = 200.0;
	/** Borde del asiento de cada elemento en la arena: de la huella a la arena natural, entre 2,5 y 16 m (40 % de la huella; 80 % en los alargados). */
	constexpr double StampBlendMin = 250.0;
	constexpr double StampBlendMax = 1600.0;
	/** Ninguna línea recta hacia el mar libre más de esto (cm): donde la haya, se pone algo en medio. */
	constexpr double MaxStraightRun = 7000.0;
	/** Arcos de salto que se dejan libres por delante de cada lanzador (cm desde su borde) y su semiancho. */
	constexpr double CatapultArc = 3000.0;
	constexpr double TrampolineArc = 2200.0;
	constexpr double JumpArcHalfWidth = 500.0;
	/** Catapultas por ronda como poco (si no caben sueltas, pasan a serlo trampolines de delante de un obstáculo). */
	constexpr int32 MinCatapults = 6;

	// ── Relieve fijo ──
	/** Corredores: caminos naturales más bajos entre las dunas (dos que se separan y se juntan y un tercero en medio). */
	constexpr int32 NumCorridors = 3;
	/**
	 * Trincheras (zigzag, en la zona del 28-31 % del recorrido): canal de 3,2 m entre dos caballones de arena con
	 * tablones por dentro; el suelo del canal, 60 cm por debajo de la arena y los caballones, 45 cm por encima (~1,05 m
	 * desde dentro: se sale de un salto). El terreno se cava TrenchDig hasta TrenchDigFlat del eje y vuelve a la arena
	 * natural en TrenchDigReach; los caballones y los sacos terreros son mallas del generador.
	 */
	constexpr double TrenchHalfChannel = 160.0;
	constexpr double TrenchBermTop = 260.0;
	constexpr double TrenchBermFoot = 560.0;
	constexpr double TrenchBermHeight = 45.0;
	constexpr double TrenchDig = 60.0;
	constexpr double TrenchDigFlat = 210.0;
	constexpr double TrenchDigReach = 660.0;
	/** Cornisa de arena en lo alto de algunas crestas: un labio de ~95 cm que se salta (la tortuga salta 1,2 m). */
	constexpr double LipHeight = 95.0;
	/** Hay cornisa donde la cresta tiene al menos esta fracción de su altura (fuera de los collados y de las puntas). */
	constexpr double LipMinCrest = 0.72;
	constexpr double LipMaxU = 0.85;
	/** Parte baja (dedo) de la cara de sotavento de una cresta: suaviza el pie. */
	constexpr double RidgeSlipToe = 0.25;

	// ─────────────────────────────────────────────────────────────────────────
	// Terreno fijo: perfil, bancos y relieve
	// ─────────────────────────────────────────────────────────────────────────

	/** X del filo del acantilado a lo ancho de la playa (ondula poco: el borde se lee claro). */
	inline double EdgeX(double Y)
	{
		return Length + EdgeWobble * (0.6 * FMath::Sin(Y / 5200.0 + 0.7) + 0.4 * FMath::Sin(Y / 1900.0 + 2.1));
	}

	/** Cota de la arena a lo largo del recorrido, sin relieve: cae más al principio y sigue subiendo por detrás de la salida. */
	inline double ProfileZ(double X)
	{
		const double T = FMath::Clamp(X / Length, 0.0, 1.0);
		double Z = CliffTopZ + BeachDrop * FMath::Pow(1.0 - T, 1.5);
		if (X < 0.0) { Z += BeachDrop * 1.5 / Length * (-X); }
		return Z;
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

	/** Cota sin relieve (la cuesta de la playa y los bancos de la selva). */
	inline double BaseZ(double X, double Y)
	{
		return ProfileZ(X) + BankZ(X, Y);
	}

	/** Envolvente del relieve: llano en los primeros 35 m (la salida), crece hasta ~170 m y se allana antes de la roca. */
	inline double ReliefEnvelope(double X)
	{
		return TNProcMap::SmoothStep(3500.0, 17000.0, X) * (1.0 - TNProcMap::SmoothStep(Length - 9000.0, Length - RockStart, X));
	}

	// ── Corredores ──

	/** Eje del par de corredores (Y): serpentea de un lado a otro de la playa. */
	inline double CorridorAxisY(double X)
	{
		const double S = X / Length;
		return 2600.0 * FMath::Sin(TNProcMap::TwoPi * (0.8 * S + 0.1)) + 1400.0 * TNProcMap::Noise1(TerrainSeed + 31u, X / 26000.0);
	}

	/** Cuánto se separan los dos corredores (0 juntos, 1 separados del todo): se separan dos veces y se juntan en medio. */
	inline double CorridorSplit(double X)
	{
		const double S = X / Length;
		return TNProcMap::SmoothStep(0.08, 0.18, S) * (1.0 - TNProcMap::SmoothStep(0.33, 0.43, S))
			+ TNProcMap::SmoothStep(0.52, 0.62, S) * (1.0 - TNProcMap::SmoothStep(0.84, 0.93, S));
	}

	/** Peso del tercer corredor (el del centro, entre los otros dos en la segunda separación). */
	inline double ThirdCorridorWeight(double X)
	{
		const double S = X / Length;
		return TNProcMap::SmoothStep(0.6, 0.66, S) * (1.0 - TNProcMap::SmoothStep(0.79, 0.85, S));
	}

	/** Un corredor a la altura X: su centro (Y), su semiancho de fondo y su peso (0 = aquí no hay). */
	struct FCorridorSample
	{
		double Y = 0.0;
		double HalfW = 1700.0;
		double Weight = 0.0;
	};

	/** Corredor K (0 el de -Y, 1 el de +Y, 2 el del centro) a la altura X. */
	inline FCorridorSample CorridorAt(int32 K, double X)
	{
		FCorridorSample C;
		const double Axis = CorridorAxisY(X);
		const double Split = CorridorSplit(X);
		const double Wobble = 900.0 * TNProcMap::Noise1(TerrainSeed + 40u + static_cast<uint32>(K), X / 9000.0);
		C.HalfW = 1700.0 + 500.0 * TNProcMap::Noise1(TerrainSeed + 50u + static_cast<uint32>(K), X / 14000.0);
		if (K < 2)
		{
			const double Side = K == 0 ? -1.0 : 1.0;
			const double Spread = 5400.0 + 700.0 * ThirdCorridorWeight(X);
			C.Y = Axis + Side * Split * Spread + Wobble * Split;
			C.Weight = 1.0;
		}
		else
		{
			C.Y = Axis + 0.5 * Wobble;
			C.Weight = ThirdCorridorWeight(X);
			C.HalfW *= 0.75;
		}
		// Dentro de la playa jugable, a 20 m de la selva como poco.
		const double Lim = HalfWidth - 2000.0 - C.HalfW;
		C.Y = FMath::Clamp(C.Y, -Lim, Lim);
		return C;
	}

	/** Cuánto está (X, Y) dentro de un corredor: 1 en el fondo, 0 fuera (con sus orillas suaves). */
	inline double CorridorMask(double X, double Y)
	{
		double M = 0.0;
		for (int32 K = 0; K < NumCorridors; ++K)
		{
			const FCorridorSample C = CorridorAt(K, X);
			if (C.Weight <= 0.0) { continue; }
			const double D = FMath::Abs(Y - C.Y);
			M = FMath::Max(M, C.Weight * (1.0 - TNProcMap::SmoothStep(C.HalfW * 0.5, C.HalfW * 1.3, D)));
		}
		return M;
	}

	/** Lo que se hunde el fondo de los corredores respecto a las dunas (1-2,4 m). */
	inline double CorridorDepth(double X)
	{
		return 170.0 + 70.0 * TNProcMap::Noise1(TerrainSeed + 60u, X / 17000.0);
	}

	// ── Dunas y ondulación ──

	/**
	 * Campo de dunas: crestas a lo ancho (el viento viene del mar) cada ~52 m, deformadas, con lomas sueltas y crestadas en
	 * diagonal que rompen las filas; de 2,8 m en el centro a 4,8 m junto a la selva.
	 */
	inline double DuneFieldZ(double X, double Y)
	{
		const double SideT = TNProcMap::SmoothStep(HalfWidth - 6000.0, HalfWidth, FMath::Abs(Y));
		const double Amp = 280.0 + 200.0 * SideT;
		const double Wx = X + 2600.0 * TNProcMap::Noise2(TerrainSeed, X / 14000.0, Y / 14000.0);
		const double Wy = Y + 2600.0 * TNProcMap::Noise2(TerrainSeed + 1u, X / 14000.0 + 5.3, Y / 14000.0 - 2.1);
		const double Phase = Wx / 5200.0 + 0.35 * TNProcMap::Noise2(TerrainSeed + 2u, Wx / 26000.0, Wy / 9000.0);
		const double Ridge = 0.5 + 0.5 * FMath::Sin(TNProcMap::TwoPi * Phase);
		const double Crest = 0.55 + 0.45 * TNProcMap::Noise2(TerrainSeed + 3u, Wx / 18000.0, Wy / 7000.0);
		const double Lumps = TNProcMap::Fbm2(TerrainSeed + 4u, Wx / 9000.0, Wy / 9000.0, 3);
		const double Knobs = TNProcMap::Ridged2(TerrainSeed + 5u, Wx / 7000.0 + 0.4 * Wy / 7000.0, Wy / 5200.0, 2);
		return Amp * (1.2 * Ridge * Crest - 0.45 + 0.45 * Lumps + 0.7 * (Knobs - 0.45));
	}

	/** Ondulación irregular de varias escalas (130 m, 43 m y 16 m): el suelo nunca es plano del todo. */
	inline double UndulationZ(double X, double Y)
	{
		return 170.0 * TNProcMap::Fbm2(TerrainSeed + 70u, X / 13000.0, Y / 13000.0, 2)
			+ 70.0 * TNProcMap::Noise2(TerrainSeed + 71u, X / 4300.0, Y / 4300.0)
			+ 25.0 * TNProcMap::Noise2(TerrainSeed + 72u, X / 1600.0, Y / 1600.0);
	}

	// ── Dunas con cresta ──

	/**
	 * Duna con cresta marcada (a lomo): cara de barlovento suave (~14-18°) hacia Windward y cara de sotavento (la de la
	 * avalancha, ~30°) al otro lado; la cresta se dobla en media luna hacia sotavento (Bend) y baja en las puntas y en sus
	 * collados. Las que cruzan la playa miran con la cara empinada hacia la salida: o se rodean, o se pasa por un collado,
	 * o se sube (y, con cornisa, se salta el labio de arriba).
	 */
	struct FRidge
	{
		FVector2D Center = FVector2D::ZeroVector;
		/** A lo largo de la cresta (unitario). */
		FVector2D Along = FVector2D(0.0, 1.0);
		/** Hacia la cara suave (unitario, perpendicular a Along). */
		FVector2D Windward = FVector2D(1.0, 0.0);
		double HalfLength = 4000.0;
		double Height = 350.0;
		/** Cuánto se doblan las puntas hacia sotavento (cm, en la punta). */
		double Bend = 0.0;
		/** Collados: posición a lo largo (-1..1, en medias longitudes); < -1,5 = sin collado. */
		double ColU[2] = { -9.0, -9.0 };
		double ColWidth = 900.0;
		/** Con cornisa en lo alto (fuera de los collados y de las puntas). */
		bool bLip = false;
		/** Separa dos corredores a lo largo del recorrido (no cruza la playa). */
		bool bDivider = false;
	};

	/** Altura de la cresta en U (a lo largo, cm desde el centro): baja en las puntas y en los collados. */
	inline double RidgeCrestHeight(const FRidge& Ridge, double U)
	{
		const double Un = FMath::Abs(U) / Ridge.HalfLength;
		double H = Ridge.Height * (1.0 - TNProcMap::SmoothStep(0.55, 1.15, Un));
		for (const double Col : Ridge.ColU)
		{
			if (Col < -1.5) { continue; }
			H *= 1.0 - 0.72 * (1.0 - TNProcMap::SmoothStep(0.0, Ridge.ColWidth, FMath::Abs(U - Col * Ridge.HalfLength)));
		}
		return H;
	}

	/** Desplazamiento de la cresta hacia barlovento (negativo: hacia sotavento) en U: media luna. */
	inline double RidgeBendAt(const FRidge& Ridge, double U)
	{
		const double Un = U / Ridge.HalfLength;
		return -Ridge.Bend * Un * Un;
	}

	/** Punto de la cresta en U (planta). */
	inline FVector2D RidgeCrestPoint(const FRidge& Ridge, double U)
	{
		return Ridge.Center + Ridge.Along * U + Ridge.Windward * RidgeBendAt(Ridge, U);
	}

	/** Semiancho de cada cara de la cresta con altura H: barlovento (4 H) y sotavento (~30°). */
	inline double RidgeWindwardWidth(double H) { return FMath::Max(400.0, H * 4.0); }
	inline double RidgeSlipWidth(double H) { return FMath::Max(150.0, H / 0.52); }

	/** Lo que levanta la duna con cresta el suelo en P. */
	inline double RidgeZ(const FRidge& Ridge, const FVector2D& P)
	{
		const FVector2D D = P - Ridge.Center;
		const double U = FVector2D::DotProduct(D, Ridge.Along);
		if (FMath::Abs(U) > Ridge.HalfLength * 1.2) { return 0.0; }
		const double H = RidgeCrestHeight(Ridge, U);
		if (H <= 1.0) { return 0.0; }
		const double V = FVector2D::DotProduct(D, Ridge.Windward) - RidgeBendAt(Ridge, U);
		if (V >= 0.0)
		{
			// Barlovento: pie redondeado y cresta viva (pendiente máxima 4/3 de la media).
			const double S = TNProcMap::Saturate(1.0 - V / RidgeWindwardWidth(H));
			return H * S * S * (2.0 - S);
		}
		// Sotavento: recta a ~30° con el pie suavizado.
		const double S = TNProcMap::Saturate(1.0 + V / RidgeSlipWidth(H));
		const double Toe = RidgeSlipToe;
		const double F = S >= Toe ? S - 0.5 * Toe : S * S / (2.0 * Toe);
		return H * F / (1.0 - 0.5 * Toe);
	}

	/** Dunas con cresta (siempre las mismas): se colocan a partir de los corredores. */
	inline TArray<FRidge> BuildRidges()
	{
		enum class EKind : uint8 { Cross, Divider, Flank };
		struct FSpec
		{
			double S;
			EKind Kind;
			int32 Corridor;
			double Height;
			double HalfLen;
			bool bLip;
		};
		// Cross: cruza la playa sobre el corredor Corridor con un collado en él. Divider: separa dos corredores a lo largo
		// (el 0 separa 0 y 1, o 0 y el central; el 1, el central y 1). Flank: media luna entre el corredor de fuera y la
		// selva, del lado con más sitio.
		static const FSpec Specs[] = {
			{ 0.125, EKind::Cross, 1, 300.0, 5200.0, true },
			{ 0.205, EKind::Divider, 0, 380.0, 4200.0, false },
			{ 0.235, EKind::Flank, 0, 330.0, 3400.0, true },
			{ 0.365, EKind::Cross, 0, 420.0, 5600.0, true },
			{ 0.64, EKind::Cross, 0, 400.0, 5000.0, true },
			{ 0.705, EKind::Divider, 0, 420.0, 3800.0, false },
			{ 0.77, EKind::Divider, 1, 420.0, 3600.0, false },
			{ 0.715, EKind::Flank, 1, 380.0, 3600.0, true },
			{ 0.835, EKind::Cross, 1, 380.0, 5000.0, true },
			{ 0.895, EKind::Flank, 0, 260.0, 3000.0, false },
		};
		TNProcMap::FRng Rng(static_cast<uint64>(TerrainSeed) * 131ull + 11ull);
		TArray<FRidge> Result;
		for (const FSpec& Spec : Specs)
		{
			FRidge Ridge;
			const double X = Spec.S * Length;
			Ridge.Height = Spec.Height * Rng.Range(0.9, 1.1);
			Ridge.HalfLength = Spec.HalfLen * Rng.Range(0.9, 1.1);
			Ridge.ColWidth = Rng.Range(700.0, 1100.0);
			Ridge.bLip = Spec.bLip;
			const double Ang = FMath::DegreesToRadians(Rng.Range(-22.0, 22.0));
			if (Spec.Kind == EKind::Divider)
			{
				const bool bThird = ThirdCorridorWeight(X) > 0.5;
				const FCorridorSample A = CorridorAt(bThird ? (Spec.Corridor == 0 ? 0 : 2) : 0, X);
				const FCorridorSample B = CorridorAt(bThird ? (Spec.Corridor == 0 ? 2 : 1) : 1, X);
				const double Room = FMath::Abs(B.Y - A.Y) - 1.3 * (A.HalfW + B.HalfW);
				Ridge.Height = FMath::Min(Ridge.Height, FMath::Max(0.0, Room / 6.5));
				Ridge.Center = FVector2D(X, 0.5 * (A.Y + B.Y));
				const double Tilt = FMath::DegreesToRadians(Rng.Range(-9.0, 9.0));
				Ridge.Along = FVector2D(FMath::Cos(Tilt), FMath::Sin(Tilt));
				const double Side = Rng.Chance(0.5) ? 1.0 : -1.0;
				Ridge.Windward = FVector2D(-Ridge.Along.Y, Ridge.Along.X) * Side;
				Ridge.bDivider = true;
			}
			else
			{
				Ridge.Along = FVector2D(FMath::Sin(Ang), FMath::Cos(Ang));
				Ridge.Windward = FVector2D(FMath::Cos(Ang), -FMath::Sin(Ang));
				Ridge.Bend = Rng.Range(0.12, 0.25) * Ridge.HalfLength;
				if (Spec.Kind == EKind::Cross)
				{
					const FCorridorSample C = CorridorAt(Spec.Corridor, X);
					Ridge.Center = FVector2D(X, C.Y + Rng.Range(-0.3, 0.3) * Ridge.HalfLength);
					const double ColAt = (C.Y - Ridge.Center.Y) * Ridge.Along.Y;
					Ridge.ColU[0] = ColAt / Ridge.HalfLength;
					if (Rng.Chance(0.45))
					{
						const double Other = Rng.Range(-0.7, 0.7);
						if (FMath::Abs(Other - Ridge.ColU[0]) > 0.5) { Ridge.ColU[1] = Other; }
					}
				}
				else
				{
					const FCorridorSample Left = CorridorAt(0, X);
					const FCorridorSample Right = CorridorAt(1, X);
					const double RoomL = (Left.Y - 1.3 * Left.HalfW) - (-HalfWidth + 1500.0);
					const double RoomR = (HalfWidth - 1500.0) - (Right.Y + 1.3 * Right.HalfW);
					const bool bRight = Spec.Corridor == 0 ? RoomR > RoomL : RoomR >= RoomL * 0.8;
					const double Room = bRight ? RoomR : RoomL;
					Ridge.HalfLength = FMath::Min(Ridge.HalfLength, 0.42 * Room);
					Ridge.Center = FVector2D(X, bRight ? Right.Y + 1.3 * Right.HalfW + 0.5 * Room : Left.Y - 1.3 * Left.HalfW - 0.5 * Room);
				}
			}
			// Dentro de la playa.
			const double Lim = HalfWidth - 1200.0;
			if (FMath::Abs(Ridge.Center.Y) + Ridge.HalfLength * FMath::Abs(Ridge.Along.Y) + 400.0 > Lim)
			{
				Ridge.HalfLength = FMath::Max(1000.0, (Lim - FMath::Abs(Ridge.Center.Y) - 400.0) / FMath::Max(0.2, FMath::Abs(Ridge.Along.Y)));
			}
			Result.Add(Ridge);
		}
		return Result;
	}

	inline const TArray<FRidge>& Ridges()
	{
		static const TArray<FRidge> Table = BuildRidges();
		return Table;
	}

	/** Suma de lo que levantan todas las dunas con cresta en P. */
	inline double RidgesZ(const FVector2D& P)
	{
		double Sum = 0.0;
		for (const FRidge& Ridge : Ridges()) { Sum += RidgeZ(Ridge, P); }
		return Sum;
	}

	/**
	 * Cuánto manda la duna con cresta en P: 1 sobre sus caras (por su altura a lo largo), 0 a 12 m de su pie. Ahí se
	 * amansan las dunas de alrededor, con una transición ancha (si cambiara con la altura de la cresta, la pendiente se
	 * dispararía en el pie).
	 */
	inline double RidgeInfluence(const FRidge& Ridge, const FVector2D& P)
	{
		const FVector2D D = P - Ridge.Center;
		const double U = FVector2D::DotProduct(D, Ridge.Along);
		if (FMath::Abs(U) > Ridge.HalfLength * 1.2) { return 0.0; }
		const double H = RidgeCrestHeight(Ridge, U);
		if (H <= 1.0) { return 0.0; }
		const double V = FVector2D::DotProduct(D, Ridge.Windward) - RidgeBendAt(Ridge, U);
		const double Beyond = V >= 0.0 ? V - RidgeWindwardWidth(H) : -V - RidgeSlipWidth(H);
		return (H / Ridge.Height) * (1.0 - TNProcMap::SmoothStep(0.0, 1200.0, Beyond));
	}

	/** Relieve natural de la playa (dunas, corredores, ondulación y crestas), sin pozas ni trincheras. */
	inline double ReliefZ(double X, double Y)
	{
		const double Env = ReliefEnvelope(X);
		if (Env <= 0.0) { return 0.0; }
		const FVector2D P(X, Y);
		double Crests = 0.0;
		double Influence = 0.0;
		for (const FRidge& Ridge : Ridges())
		{
			Crests += RidgeZ(Ridge, P);
			Influence = FMath::Max(Influence, RidgeInfluence(Ridge, P));
		}
		const double Cm = CorridorMask(X, Y);
		// Sobre las crestas y a su pie, las dunas de alrededor se amansan (así ninguna cara pasa de ~40°).
		const double Dunes = DuneFieldZ(X, Y) * (1.0 - 0.8 * Cm) * (1.0 - 0.8 * Influence);
		return Env * (Dunes - CorridorDepth(X) * Cm + UndulationZ(X, Y) + Crests);
	}

	/** Arena natural sin pozas ni trincheras. */
	inline double NaturalZ(double X, double Y)
	{
		return BaseZ(X, Y) + ReliefZ(X, Y);
	}

	// ── Trincheras ──

	/** Una línea de trinchera en zigzag (eje del canal). */
	struct FTrench
	{
		TArray<FVector2D> Points;
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
	};

	/** Dos líneas de trinchera casi a lo ancho (la de delante abierta por +Y y la de detrás por -Y): se cruzan o se rodean. */
	inline TArray<FTrench> BuildTrenches()
	{
		struct FSpec
		{
			double S;
			double Y0;
			double Y1;
			double Zig;
			double Step;
		};
		static const FSpec Specs[] = { { 0.305, -10800.0, 9600.0, 480.0, 1300.0 }, { 0.28, -8200.0, 11200.0, 420.0, 1200.0 } };
		TNProcMap::FRng Rng(static_cast<uint64>(TerrainSeed) * 977ull + 5ull);
		TArray<FTrench> Result;
		for (const FSpec& Spec : Specs)
		{
			FTrench Trench;
			const double X0 = Spec.S * Length;
			double Y = Spec.Y0;
			int32 K = 0;
			for (int32 Guard = 0; Guard < 64; ++Guard)
			{
				Trench.Points.Add(FVector2D(X0 + (K % 2 ? Spec.Zig : -Spec.Zig) + Rng.Range(-80.0, 80.0), Y));
				if (Y >= Spec.Y1) { break; }
				Y = FMath::Min(Spec.Y1, Y + Spec.Step * Rng.Range(0.85, 1.15));
				++K;
			}
			Trench.Min = Trench.Points[0];
			Trench.Max = Trench.Points[0];
			for (const FVector2D& P : Trench.Points)
			{
				Trench.Min = FVector2D(FMath::Min(Trench.Min.X, P.X), FMath::Min(Trench.Min.Y, P.Y));
				Trench.Max = FVector2D(FMath::Max(Trench.Max.X, P.X), FMath::Max(Trench.Max.Y, P.Y));
			}
			Result.Add(Trench);
		}
		return Result;
	}

	inline const TArray<FTrench>& Trenches()
	{
		static const TArray<FTrench> Table = BuildTrenches();
		return Table;
	}

	/** Distancia en planta de P al eje de la trinchera más cercana (más de Beyond si está lejos de todas: no se mide). */
	inline double TrenchDistance(const FVector2D& P, double Beyond = 1000000.0)
	{
		double Best = Beyond;
		for (const FTrench& Trench : Trenches())
		{
			if (P.X < Trench.Min.X - Beyond || P.X > Trench.Max.X + Beyond || P.Y < Trench.Min.Y - Beyond || P.Y > Trench.Max.Y + Beyond) { continue; }
			for (int32 i = 0; i + 1 < Trench.Points.Num(); ++i)
			{
				double T = 0.0;
				Best = FMath::Min(Best, TNProcMap::DistPointSegment(P, Trench.Points[i], Trench.Points[i + 1], T));
			}
		}
		return Best;
	}

	/** Distancia en planta de un segmento (un elemento) a los ejes de las trincheras. */
	inline double TrenchSegmentDistance(const FVector2D& A, const FVector2D& B)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FTrench& Trench : Trenches())
		{
			for (int32 i = 0; i + 1 < Trench.Points.Num(); ++i)
			{
				const FVector2D& P0 = Trench.Points[i];
				const FVector2D& P1 = Trench.Points[i + 1];
				if (TNProcMap::SegmentsIntersect(A, B, P0, P1)) { return 0.0; }
				double T = 0.0;
				double D = TNProcMap::DistPointSegment(A, P0, P1, T);
				D = FMath::Min(D, TNProcMap::DistPointSegment(B, P0, P1, T));
				D = FMath::Min(D, TNProcMap::DistPointSegment(P0, A, B, T));
				D = FMath::Min(D, TNProcMap::DistPointSegment(P1, A, B, T));
				Best = FMath::Min(Best, D);
			}
		}
		return Best;
	}

	/** Lo que se cava la arena en (X, Y) para el canal de las trincheras. */
	inline double TrenchCarve(double X, double Y)
	{
		const double D = TrenchDistance(FVector2D(X, Y), TrenchDigReach + 100.0);
		if (D >= TrenchDigReach) { return 0.0; }
		return TrenchDig * (1.0 - TNProcMap::SmoothStep(TrenchDigFlat, TrenchDigReach, D));
	}

	// ── Pozas ──

	/**
	 * Poza de agua de verdad en una hondonada: elipse ondulada con su nivel de agua (por debajo de toda su orilla), fondo
	 * que baja hasta Depth (se nada en el centro) y orillas que se suben andando (~24°). Unas cortan un corredor (o se nada
	 * o se toma otro camino) y otras se rodean; las de marea, hacia el mar, llevan rocas.
	 */
	struct FPool
	{
		FVector2D Center = FVector2D::ZeroVector;
		double Rx = 1000.0;
		double Ry = 1000.0;
		/** Giro de la elipse (radianes). */
		double Angle = 0.0;
		double Depth = 180.0;
		double Water = 0.0;
		double Phase1 = 0.0;
		double Phase2 = 0.0;
		/** Corredor que corta (-1 = ninguno). */
		int32 Corridor = -1;
		/** Poza de marea (con rocas en la orilla). */
		bool bTide = false;

		/** Radio máximo de la orilla (cm). */
		double OuterR() const { return FMath::Max(Rx, Ry) * 1.2; }
	};

	/** Radio normalizado de P en la poza: 1 en la orilla, menos dentro. */
	inline double PoolU(const FPool& Pool, const FVector2D& P)
	{
		const FVector2D D = P - Pool.Center;
		const double C = FMath::Cos(Pool.Angle);
		const double S = FMath::Sin(Pool.Angle);
		const double Lx = (D.X * C + D.Y * S) / Pool.Rx;
		const double Ly = (-D.X * S + D.Y * C) / Pool.Ry;
		const double Rn = FMath::Sqrt(Lx * Lx + Ly * Ly);
		const double Theta = FMath::Atan2(Ly, Lx);
		const double Wob = 1.0 + 0.12 * FMath::Sin(2.0 * Theta + Pool.Phase1) + 0.07 * FMath::Sin(3.0 * Theta + Pool.Phase2);
		return Rn / Wob;
	}

	/** Punto de la poza en el ángulo Theta (del espacio normalizado de la elipse) y el radio normalizado U. */
	inline FVector2D PoolPoint(const FPool& Pool, double Theta, double U)
	{
		const double Wob = 1.0 + 0.12 * FMath::Sin(2.0 * Theta + Pool.Phase1) + 0.07 * FMath::Sin(3.0 * Theta + Pool.Phase2);
		const double Lx = FMath::Cos(Theta) * Wob * U * Pool.Rx;
		const double Ly = FMath::Sin(Theta) * Wob * U * Pool.Ry;
		const double C = FMath::Cos(Pool.Angle);
		const double S = FMath::Sin(Pool.Angle);
		return Pool.Center + FVector2D(Lx * C - Ly * S, Lx * S + Ly * C);
	}

	/** Fondo de la poza a radio normalizado U (por encima del agua fuera de la orilla: la orilla sube a ~24°). */
	inline double PoolBedZ(const FPool& Pool, double U)
	{
		if (U <= 1.0) { return Pool.Water - Pool.Depth * (1.0 - TNProcMap::SmoothStep(0.2, 1.0, U)); }
		return Pool.Water + (U - 1.0) * FMath::Min(Pool.Rx, Pool.Ry) * 0.45;
	}

	/** Pozas de la playa (siempre las mismas). */
	inline TArray<FPool> BuildPools()
	{
		struct FSpec
		{
			double S;
			int32 Corridor;
			double Rx;
			double Ry;
			bool bTide;
		};
		// Corridor >= 0: la poza corta ese corredor. -1: fuera de los corredores, del lado con más sitio (se rodea).
		static const FSpec Specs[] = {
			{ 0.155, 0, 1100.0, 1800.0, false },
			{ 0.19, -1, 800.0, 1000.0, false },
			{ 0.335, 1, 1100.0, 1800.0, false },
			{ 0.4, -1, 900.0, 700.0, false },
			{ 0.605, 1, 1000.0, 1600.0, false },
			{ 0.68, 2, 900.0, 1500.0, false },
			{ 0.745, -1, 1400.0, 1100.0, true },
			{ 0.8, 0, 1200.0, 2000.0, true },
			{ 0.862, -1, 1000.0, 1300.0, true },
			{ 0.9, 1, 1300.0, 2100.0, true },
		};
		TNProcMap::FRng Rng(static_cast<uint64>(TerrainSeed) * 613ull + 3ull);
		TArray<FPool> Result;
		for (const FSpec& Spec : Specs)
		{
			FPool Pool;
			const double X = Spec.S * Length;
			Pool.Rx = Spec.Rx * Rng.Range(0.9, 1.1);
			Pool.Ry = Spec.Ry * Rng.Range(0.9, 1.1);
			Pool.Angle = FMath::DegreesToRadians(Rng.Range(-15.0, 15.0));
			Pool.Phase1 = Rng.Range(0.0, TNProcMap::TwoPi);
			Pool.Phase2 = Rng.Range(0.0, TNProcMap::TwoPi);
			Pool.bTide = Spec.bTide;
			Pool.Corridor = Spec.Corridor;
			// Honda en el centro (se nada) y con el fondo a menos de ~26°.
			Pool.Depth = FMath::Min(240.0, 0.2 * FMath::Min(Pool.Rx, Pool.Ry));
			if (Spec.Corridor >= 0)
			{
				Pool.Center = FVector2D(X, CorridorAt(Spec.Corridor, X).Y);
			}
			else
			{
				const FCorridorSample Left = CorridorAt(0, X);
				const FCorridorSample Right = CorridorAt(1, X);
				const double RoomL = (Left.Y - 1.3 * Left.HalfW) - (-HalfWidth + 1500.0);
				const double RoomR = (HalfWidth - 1500.0) - (Right.Y + 1.3 * Right.HalfW);
				Pool.Center = FVector2D(X, RoomR >= RoomL ? Right.Y + 1.3 * Right.HalfW + 0.5 * RoomR : Left.Y - 1.3 * Left.HalfW - 0.5 * RoomL);
			}
			// Nivel del agua: 30 cm por debajo de lo más bajo de su orilla (tres anillos), así nunca se sale.
			double Lowest = TNumericLimits<double>::Max();
			for (const double Ring : { 1.0, 1.2, 1.45 })
			{
				for (int32 k = 0; k < 32; ++k)
				{
					const FVector2D Q = PoolPoint(Pool, TNProcMap::TwoPi * k / 32.0, Ring);
					Lowest = FMath::Min(Lowest, NaturalZ(Q.X, Q.Y));
				}
			}
			Pool.Water = Lowest - 30.0;
			Result.Add(Pool);
		}
		return Result;
	}

	inline const TArray<FPool>& Pools()
	{
		static const TArray<FPool> Table = BuildPools();
		return Table;
	}

	/** Índice de la poza cuya agua cubre P (dentro de su orilla), o INDEX_NONE. */
	inline int32 PoolAt(const FVector2D& P, double UMax = 1.0)
	{
		const TArray<FPool>& All = Pools();
		for (int32 i = 0; i < All.Num(); ++i)
		{
			const FPool& Pool = All[i];
			const double R = Pool.OuterR() * UMax;
			if (FMath::Abs(P.X - Pool.Center.X) > R || FMath::Abs(P.Y - Pool.Center.Y) > R) { continue; }
			if (PoolU(Pool, P) <= UMax) { return i; }
		}
		return INDEX_NONE;
	}

	/**
	 * Cajas (espacio local) del agua nadable de una poza: rebanadas de 2 m a lo largo de X que cubren donde el fondo queda
	 * 40 cm o más por debajo del agua, del agua hasta 3 m por debajo del fondo (se nada con el centro de la cápsula dentro).
	 */
	inline void PoolSwimBoxes(const FPool& Pool, TArray<FBox>& OutBoxes)
	{
		const double R = Pool.OuterR();
		for (double X = Pool.Center.X - R; X <= Pool.Center.X + R; X += 200.0)
		{
			double YMin = TNumericLimits<double>::Max();
			double YMax = -TNumericLimits<double>::Max();
			double Low = Pool.Water;
			for (double Y = Pool.Center.Y - R; Y <= Pool.Center.Y + R; Y += 50.0)
			{
				const double Bed = FMath::Min(NaturalZ(X, Y), PoolBedZ(Pool, PoolU(Pool, FVector2D(X, Y))));
				if (Bed > Pool.Water - 40.0) { continue; }
				YMin = FMath::Min(YMin, Y);
				YMax = FMath::Max(YMax, Y);
				Low = FMath::Min(Low, Bed);
			}
			if (YMax < YMin) { continue; }
			OutBoxes.Add(FBox(FVector(X - 110.0, YMin - 40.0, Low - 300.0), FVector(X + 110.0, YMax + 40.0, Pool.Water)));
		}
	}

	// ── Arena, roca y mar ──

	/** Arena natural del terreno fijo (con pozas y trincheras; sin los asientos de la ronda). */
	inline double SandZ(double X, double Y)
	{
		double Z = NaturalZ(X, Y) - TrenchCarve(X, Y);
		const FVector2D P(X, Y);
		for (const FPool& Pool : Pools())
		{
			const double Reach = 3.5 * FMath::Max(Pool.Rx, Pool.Ry);
			if (FMath::Abs(X - Pool.Center.X) > Reach || FMath::Abs(Y - Pool.Center.Y) > Reach) { continue; }
			Z = FMath::Min(Z, PoolBedZ(Pool, PoolU(Pool, P)));
		}
		return Z;
	}

	/** Relieve de la arena respecto a la cuesta sin relieve (color de las crestas y los fondos). */
	inline double DuneZ(double X, double Y)
	{
		return SandZ(X, Y) - BaseZ(X, Y);
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

	/** Cornisa en U de la duna con cresta (fuera de los collados y de las puntas). */
	inline bool IsLipAt(const FRidge& Ridge, double U)
	{
		return Ridge.bLip && FMath::Abs(U) <= LipMaxU * Ridge.HalfLength && RidgeCrestHeight(Ridge, U) >= LipMinCrest * Ridge.Height;
	}

	/** Puntos de las cornisas cada 4 m (para que nada se asiente encima). */
	inline const TArray<FVector2D>& LipSamples()
	{
		static const TArray<FVector2D> Table = []()
		{
			TArray<FVector2D> Points;
			for (const FRidge& Ridge : Ridges())
			{
				if (!Ridge.bLip) { continue; }
				for (double U = -Ridge.HalfLength; U <= Ridge.HalfLength; U += 400.0)
				{
					if (IsLipAt(Ridge, U)) { Points.Add(RidgeCrestPoint(Ridge, U)); }
				}
			}
			return Points;
		}();
		return Table;
	}

	// ── Asientos de la ronda: el suelo liso bajo cada elemento ──

	/**
	 * Asiento de un elemento en la arena (el «sello» de la ronda en el terreno fijo): dentro de su huella, el suelo queda
	 * liso y a nivel, a la cota natural de su centro; en el borde (Blend), vuelve a la arena natural. Los pasos de quads no
	 * allanan (bTintOnly): solo oscurecen sus rodadas. Los hoyos (la plataforma) los traen los elementos en su malla: el
	 * terreno no se cava.
	 */
	struct FStamp
	{
		FVector2D A = FVector2D::ZeroVector;
		FVector2D B = FVector2D::ZeroVector;
		double Radius = 0.0;
		double Blend = StampBlendMin;
		double LevelZ = 0.0;
		/** No cambia la altura: solo el tinte (las rodadas). */
		bool bTintOnly = false;
		/** Arena algo más oscura y apisonada (las rodadas de los quads). */
		float Tint = 0.f;
	};

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
			const double W = 1.0 - TNProcMap::SmoothStep(Stamp.Radius, Stamp.Radius + Stamp.Blend, Dist);
			Tint = FMath::Max(Tint, Stamp.Tint * static_cast<float>(W));
			if (Stamp.bTintOnly) { continue; }
			if (W >= 1.0)
			{
				if (OutTint) { *OutTint = FMath::Max(Tint, Stamp.Tint); }
				return Stamp.LevelZ;
			}
			WeightSum += W;
			Accum += W * (Stamp.LevelZ - Natural);
		}
		if (OutTint) { *OutTint = Tint; }
		return WeightSum > 0.0 ? Natural + Accum / FMath::Max(1.0, WeightSum) : Natural;
	}

	// ── Salida, meta y zambullida ──

	/** Sitio de salida Index (local, en el suelo, mirando al mar): 4 huevos en fila a 10 m y más filas detrás si hacen falta. */
	inline FVector StartSpot(int32 Index)
	{
		const int32 Slot = FMath::Max(0, Index);
		const int32 Col = Slot % NumStartSpots;
		const int32 Row = (Slot / NumStartSpots) % 3;
		const double Y = (Col - 0.5 * (NumStartSpots - 1)) * StartSpotSpacing;
		const double X = StartSpotX - Row * StartRowSpacing;
		return FVector(X, Y, GroundZ(X, Y));
	}

	/** El sitio P es bueno para salir: arena seca, fuera de pozas, trincheras y cornisas, y casi llano (menos de 12°). */
	inline bool IsDryFlatSpot(const FVector2D& P)
	{
		if (PoolAt(P, 1.4) != INDEX_NONE || TrenchDistance(P, TrenchBermFoot + 500.0) < TrenchBermFoot + 400.0) { return false; }
		for (const FVector2D& Lip : LipSamples())
		{
			if (FVector2D::DistSquared(Lip, P) < FMath::Square(600.0)) { return false; }
		}
		constexpr double H = 100.0;
		const double Dx = (SandZ(P.X + H, P.Y) - SandZ(P.X - H, P.Y)) / (2.0 * H);
		const double Dy = (SandZ(P.X, P.Y + H) - SandZ(P.X, P.Y - H)) / (2.0 * H);
		return Dx * Dx + Dy * Dy < FMath::Square(0.2126);
	}

	/**
	 * Línea del sprint de desempate (siempre la misma): lo más cerca de la mitad del recorrido donde los 12 sitios (4 en
	 * fila y dos filas detrás, como la salida) caen en arena seca y casi llana.
	 */
	inline double SprintLineX()
	{
		static const double LineX = []()
		{
			for (int32 Step = 0; Step <= 60; ++Step)
			{
				const double X = 0.5 * Length + (Step % 2 == 0 ? 1.0 : -1.0) * 500.0 * ((Step + 1) / 2);
				bool bGood = true;
				for (int32 Slot = 0; Slot < NumStartSpots * 3 && bGood; ++Slot)
				{
					const double Y = (Slot % NumStartSpots - 0.5 * (NumStartSpots - 1)) * StartSpotSpacing;
					bGood = IsDryFlatSpot(FVector2D(X - (Slot / NumStartSpots) * StartRowSpacing, Y));
				}
				if (bGood) { return X; }
			}
			return 0.5 * Length;
		}();
		return LineX;
	}

	/** Sitio Index del sprint de desempate (local, en la arena natural): como StartSpot, pero en SprintLineX. */
	inline FVector SprintSpot(int32 Index)
	{
		const int32 Slot = FMath::Max(0, Index);
		const int32 Col = Slot % NumStartSpots;
		const int32 Row = (Slot / NumStartSpots) % 3;
		const double Y = (Col - 0.5 * (NumStartSpots - 1)) * StartSpotSpacing;
		const double X = SprintLineX() - Row * StartRowSpacing;
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
		/**
		 * Fracción de la huella que ocupa de verdad (lo que no pisan otros): 1 en lo quieto; en los enemigos, su cuerpo y
		 * su sitio (la huella entera es por donde patrullan).
		 */
		double CoreFraction = 1.0;
		/** Cierra el paso (cuenta para dejar siempre el hueco libre a lo ancho). */
		bool bBlocking = true;
		/** Va por encima (gaviotas): no ocupa suelo; solo no se pisa con otros como él. */
		bool bOverlay = false;
		/** Lo coloca su propia pasada (castillos con salas, quads, gaviotas, piezas militares grandes), no el relleno. */
		bool bSpecial = false;
		/** Lanza a la tortuga hacia delante (catapulta, trampolín): se le deja libre el arco de salto. */
		bool bLauncher = false;
		/** A veces sale en corrillo: probabilidad y cuántos más como mucho. */
		double ClusterChance = 0.0;
		int32 ClusterMax = 2;
	};

	inline FElementRule RuleOf(ETNBeachElement E)
	{
		FElementRule R;
		const double Foot = TNBeach::FootprintRadius(E);
		switch (TNBeach::CategoryOf(E))
		{
			case ETNBeachCategory::Decor:
				// Pequeño, frecuente y en corrillos; grande, menos (y lo enorme, casi nunca).
				R.Weight = Foot < 500.0 ? 1.1 : (Foot < 1500.0 ? 1.0 : (Foot < 3000.0 ? 0.55 : 0.25));
				R.ClusterChance = Foot < 500.0 ? 0.4 : 0.0;
				R.MaxPerRound = Foot < 3000.0 ? 1000 : 2;
				R.SizeMin = Foot < 1500.0 ? 0.75 : 0.9;
				R.SizeMax = Foot < 1500.0 ? 1.3 : 1.1;
				break;
			case ETNBeachCategory::Trap:
				R.SeaBias = 0.25;
				R.bBlocking = false;
				break;
			case ETNBeachCategory::Enemy:
			default:
				// Amenazas que se esquivan, no muros: no cierran el paso.
				R.MinT = 0.02;
				R.SeaBias = 0.4;
				R.MaxPerRound = 12;
				R.SizeMin = 0.9;
				R.SizeMax = 1.15;
				R.CoreFraction = 0.4;
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
				R.MaxT = 0.9;
				R.SideBias = -0.3;
				break;
			case ETNBeachElement::PlantedUmbrella:
			case ETNBeachElement::BeachChair:
				R.Weight = 1.0;
				R.MaxT = 0.85;
				R.SideBias = -0.2;
				break;
			case ETNBeachElement::SandCastleSmall:
				R.Weight = 1.5;
				break;
			case ETNBeachElement::SandCastleHuge:
				// Además de los de su pasada (PlaceCastles), alguno más en el relleno.
				R.Weight = 0.6;
				R.MaxPerRound = 10;
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
				R.ClusterChance = 0.0;
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
				R.ClusterChance = 0.0;
				break;
			// Decorado militar: lo grande va en sus grupos (parapetos, puestos, filas, trincheras); lo pequeño, también suelto.
			case ETNBeachElement::Sandbags:
			case ETNBeachElement::TankTrap:
			case ETNBeachElement::CamoNet:
				R.bSpecial = true;
				R.SizeMin = 0.9;
				R.SizeMax = 1.1;
				break;
			case ETNBeachElement::AmmoCrate:
			case ETNBeachElement::Jerrycan:
			case ETNBeachElement::MilitaryHelmet:
				R.Weight = 0.3;
				R.ClusterChance = 0.3;
				break;
			case ETNBeachElement::ToySoldiers:
				R.Weight = 0.35;
				R.ClusterChance = 0.45;
				break;
			case ETNBeachElement::BarbedWire:
				R.Weight = 0.9;
				R.SeaBias = 0.4;
				R.ExtentMin = 2200.0;
				R.ExtentMax = 6000.0;
				R.BaseYaw = 90.0;
				R.YawJitter = 25.0;
				R.SizeMin = 0.9;
				R.SizeMax = 1.1;
				R.bBlocking = true;
				break;
			case ETNBeachElement::Seaweed:
				// Algas a montones, en campos de 2-5 manchas.
				R.Weight = 2.4;
				R.SeaBias = 0.8;
				R.MaxPerRound = 80;
				R.ClusterChance = 0.55;
				R.ClusterMax = 4;
				break;
			case ETNBeachElement::WobblyPlatform:
				R.Weight = 0.8;
				R.MinT = 0.08;
				R.MaxPerRound = 10;
				R.YawJitter = 20.0;
				R.SizeMin = 0.85;
				R.SizeMax = 1.2;
				break;
			case ETNBeachElement::BrokenBucket:
				R.Weight = 0.8;
				R.YawJitter = 30.0;
				break;
			case ETNBeachElement::SpadeRamp:
				R.Weight = 0.8;
				R.YawJitter = 20.0;
				break;
			case ETNBeachElement::ShellGate:
				R.Weight = 0.6;
				R.YawJitter = 15.0;
				break;
			case ETNBeachElement::SandDungeon:
				R.bSpecial = true;
				R.bBlocking = true;
				R.MaxPerRound = 3;
				R.YawJitter = 8.0;
				R.SizeMin = 0.95;
				R.SizeMax = 1.08;
				break;
			case ETNBeachElement::ClamTrap:
				R.Weight = 1.2;
				R.SeaBias = 0.6;
				R.MaxPerRound = 16;
				R.ClusterChance = 0.2;
				R.ClusterMax = 1;
				break;
			case ETNBeachElement::MovingPlatform:
				R.Weight = 1.3;
				R.MaxPerRound = 16;
				R.YawJitter = 25.0;
				break;
			case ETNBeachElement::Catapult:
				R.Weight = 1.4;
				R.MaxPerRound = 18;
				R.YawJitter = 20.0;
				R.bLauncher = true;
				break;
			case ETNBeachElement::Mine:
				R.Weight = 0.35;
				R.MaxPerRound = 45;
				R.ClusterChance = 0.3;
				R.ClusterMax = 2;
				break;
			case ETNBeachElement::Trampoline:
				R.Weight = 1.4;
				R.MaxPerRound = 34;
				R.SeaBias = 0.1;
				R.YawJitter = 15.0;
				R.bLauncher = true;
				break;
			case ETNBeachElement::GiantCrab:
				// Cangrejos a menudo y desde el principio, a veces en grupos de 2-3.
				R.Weight = 2.0;
				R.MinT = 0.02;
				R.SeaBias = 0.6;
				R.MaxPerRound = 32;
				R.CoreFraction = 0.35;
				R.ClusterChance = 0.4;
				R.ClusterMax = 2;
				break;
			case ETNBeachElement::SeaUrchin:
				R.Weight = 1.0;
				R.MinT = 0.08;
				R.SeaBias = 1.0;
				R.MaxPerRound = 22;
				R.CoreFraction = 0.45;
				break;
			case ETNBeachElement::Lizard:
				R.Weight = 0.8;
				R.MinT = 0.02;
				R.MaxT = 0.88;
				R.SeaBias = -0.3;
				R.SideBias = 0.9;
				R.MaxPerRound = 14;
				R.CoreFraction = 0.4;
				break;
			case ETNBeachElement::QuadLane:
				R.bSpecial = true;
				R.BaseYaw = 90.0;
				R.YawJitter = 0.0;
				R.SizeMin = 0.9;
				R.SizeMax = 1.1;
				R.CoreFraction = 1.0;
				break;
			case ETNBeachElement::GullZone:
				R.bSpecial = true;
				R.bOverlay = true;
				R.SizeMin = 0.8;
				R.SizeMax = 1.3;
				R.CoreFraction = 1.0;
				break;
			default:
				break;
		}
		return R;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Reparto de una ronda
	// ─────────────────────────────────────────────────────────────────────────

	/** Para qué se ha puesto cada cosa (resumen, huellas y pruebas). */
	enum class EItemRole : uint8
	{
		Fill,
		Dungeon,
		DungeonWing,
		QuadLane,
		GullZone,
		GuidePath,
		/** Pieza de una fila que cierra casi toda la playa (el hueco cambia de lado de una fila a otra). */
		Row,
		/** Parapetos, puestos, filas de erizos, campos de minas y lo de las trincheras. */
		Military,
		/** Castillo enorme de su pasada. */
		Castle,
		/** Trampolín o catapulta de su pasada (delante de algo: un castillo, una cresta, una fila, una poza). */
		Launcher,
		/** Pared de un rincón escondido a un lado. */
		Nook,
		/** Puesto para que ninguna línea recta hacia el mar quede libre. */
		Plug
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
		/** Lo que ocupa de verdad (cm): la huella, salvo en los enemigos (su cuerpo y su sitio). Con esto no se pisan. */
		double Core = 0.0;
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
		/** Área de la huella entera. */
		double Area() const { return PI * Radius * Radius + 4.0 * Radius * HalfLength; }
		/** Área de lo que ocupa de verdad (la ocupación de las bandas). */
		double CoreArea() const { return PI * Core * Core + 4.0 * Core * HalfLength; }
	};

	/** Si el elemento deja su asiento en la arena (todo lo del suelo salvo los enemigos, que buscan el suelo solos). */
	inline bool HasSeat(const FItem& Item)
	{
		return !Item.bOverlay && TNBeach::CategoryOf(Item.Element) != ETNBeachCategory::Enemy;
	}

	/** Borde del asiento de un elemento (40 % de la huella; 80 % en los alargados). */
	inline double SeatBlend(const FItem& Item)
	{
		return FMath::Clamp((Item.HalfLength > 0.0 ? 0.8 : 0.4) * Item.Radius, StampBlendMin, StampBlendMax);
	}

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

	/** Holgura entre lo que ocupan dos elementos (negativa si se pisan). */
	inline double Clearance(const FItem& A, const FItem& B)
	{
		return SegmentDistance(A.EndA(), A.EndB(), B.EndA(), B.EndB()) - A.Core - B.Core;
	}

	/** Asiento de un elemento: su huella, a nivel a la cota natural de su centro (el paso de quads solo tiñe). */
	inline FStamp MakeStamp(const FItem& Item)
	{
		FStamp Stamp;
		Stamp.A = Item.EndA();
		Stamp.B = Item.EndB();
		Stamp.Radius = Item.Radius;
		Stamp.Blend = SeatBlend(Item);
		Stamp.LevelZ = SandZ(Item.Pos.X, Item.Pos.Y);
		Stamp.bTintOnly = Item.Element == ETNBeachElement::QuadLane;
		Stamp.Tint = Item.Element == ETNBeachElement::QuadLane ? 0.14f : 0.f;
		return Stamp;
	}

	/** Cota a la que se coloca el origen del elemento: la de su asiento (la arena natural de su centro). */
	inline double PlacementZ(const FItem& Item)
	{
		return SandZ(Item.Pos.X, Item.Pos.Y);
	}

	/**
	 * El asiento no deja paredes: la arena natural bajo la huella no se aparta de la cota del asiento más de 0,4 veces su
	 * borde (así el borde del asiento queda por debajo de ~31°). Lo que caería sobre una cresta o una vaguada honda, o a lo
	 * largo de una cuesta fuerte (las pasarelas son planas), ahí no cabe.
	 */
	inline bool SeatIsGentle(const FItem& Item)
	{
		if (!HasSeat(Item) || Item.Element == ETNBeachElement::QuadLane) { return true; }
		const double Level = SandZ(Item.Pos.X, Item.Pos.Y);
		const double Limit = 0.4 * SeatBlend(Item);
		const FVector2D A = Item.EndA();
		const FVector2D B = Item.EndB();
		const FVector2D Ax = Item.Axis();
		const FVector2D Sd(-Ax.Y, Ax.X);
		const int32 Steps = Item.HalfLength > 0.0 ? FMath::Clamp(FMath::CeilToInt32(2.0 * Item.HalfLength / FMath::Max(300.0, Item.Radius)), 1, 12) : 0;
		for (int32 s = 0; s <= Steps; ++s)
		{
			const FVector2D C = Steps > 0 ? A + (B - A) * (static_cast<double>(s) / Steps) : Item.Pos;
			for (int32 k = 0; k < 8; ++k)
			{
				const double Ang = TNProcMap::TwoPi * k / 8.0;
				const FVector2D P = C + (Ax * FMath::Cos(Ang) + Sd * FMath::Sin(Ang)) * Item.Radius;
				if (FMath::Abs(SandZ(P.X, P.Y) - Level) > Limit) { return false; }
			}
		}
		return true;
	}

	/**
	 * El terreno fijo deja poner el elemento: ni su asiento ni lo que ocupa tocan una poza, una trinchera ni una cornisa,
	 * y lo grande (14 m de huella o más) no se asienta sobre una cresta (la cortaría). Los pasos de quads pasan por
	 * encima de todo (no allanan) y las gaviotas van por el aire.
	 */
	inline bool TerrainAllows(const FItem& Item)
	{
		if (Item.bOverlay || Item.Element == ETNBeachElement::QuadLane) { return true; }
		const bool bSeat = HasSeat(Item);
		const double Reach = bSeat ? Item.Radius + SeatBlend(Item) : Item.Core;
		const FVector2D A = Item.EndA();
		const FVector2D B = Item.EndB();
		const FVector2D BoxMin(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		const FVector2D BoxMax(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		auto Near = [&BoxMin, &BoxMax](const FVector2D& Lo, const FVector2D& Hi, double Margin)
		{
			return Hi.X + Margin >= BoxMin.X && Lo.X - Margin <= BoxMax.X && Hi.Y + Margin >= BoxMin.Y && Lo.Y - Margin <= BoxMax.Y;
		};
		for (const FPool& Pool : Pools())
		{
			const double Keep = Reach + Pool.OuterR() * 1.3;
			if (!Near(Pool.Center, Pool.Center, Keep)) { continue; }
			double T = 0.0;
			const double Dist = TNProcMap::DistPointSegment(Pool.Center, A, B, T);
			if (Dist >= Keep) { continue; }
			// De cerca, con la forma de verdad: ni el asiento ni lo que ocupa llegan a la orilla (1,3 veces su radio).
			const FVector2D Closest = A + (B - A) * T;
			const FVector2D Toward = Dist > 1.0 ? (Pool.Center - Closest) / Dist : FVector2D(1.0, 0.0);
			if (PoolU(Pool, Closest) < 1.3 || PoolU(Pool, Closest + Toward * FMath::Min(Reach, Dist)) < 1.3) { return false; }
			for (int32 k = 0; k < 12; ++k)
			{
				const double Ang = TNProcMap::TwoPi * k / 12.0;
				if (PoolU(Pool, Closest + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Reach) < 1.3) { return false; }
			}
		}
		const double TrenchKeep = Reach + TrenchBermFoot + 200.0;
		for (const FTrench& Trench : Trenches())
		{
			if (Near(Trench.Min, Trench.Max, TrenchKeep) && TrenchSegmentDistance(A, B) < TrenchKeep) { return false; }
		}
		if (!bSeat) { return true; }
		for (const FVector2D& Lip : LipSamples())
		{
			if (!Near(Lip, Lip, Reach + 150.0)) { continue; }
			double T = 0.0;
			if (TNProcMap::DistPointSegment(Lip, A, B, T) < Reach + 150.0) { return false; }
		}
		if (Item.Radius >= 1400.0 && Item.HalfLength <= 0.0)
		{
			double Crest = RidgesZ(Item.Pos);
			for (int32 k = 0; k < 8; ++k)
			{
				const double Ang = TNProcMap::TwoPi * k / 8.0;
				Crest = FMath::Max(Crest, RidgesZ(Item.Pos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Item.Radius * 0.7)));
			}
			if (Crest > 180.0) { return false; }
		}
		return true;
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
		/** Filas pegadas a la selva (siempre tapadas). */
		TArray<uint8> SideRow;

		void Init()
		{
			X0 = ItemsStartX - 2000.0;
			Y0 = -HalfWidth;
			NX = FMath::CeilToInt32((ItemsEndX + 2000.0 - X0) / PassCell);
			NY = FMath::CeilToInt32(2.0 * HalfWidth / PassCell);
			Count.Init(0, NX * NY);
			SideRow.Init(0, NY);
			for (int32 IY = 0; IY < NY; ++IY)
			{
				const double CY = Y0 + (IY + 0.5) * PassCell;
				SideRow[IY] = FMath::Abs(CY) > HalfWidth - MinPassage * 0.5 ? 1 : 0;
			}
		}

		FVector2D CellCenter(int32 IX, int32 IY) const
		{
			return FVector2D(X0 + (IX + 0.5) * PassCell, Y0 + (IY + 0.5) * PassCell);
		}

		bool IsFree(int32 IX, int32 IY) const
		{
			return SideRow[IY] == 0 && Count[IY * NX + IX] == 0;
		}

		int32 ColumnOf(double X) const
		{
			return FMath::Clamp(FMath::FloorToInt32((X - X0) / PassCell), 0, FMath::Max(0, NX - 1));
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

		/** Hay un camino de casillas libres (vecinas en cruz) de la columna IXA a la IXB sin salirse de ellas. */
		bool IsConnectedRange(int32 IXA, int32 IXB) const
		{
			if (NX <= 0 || NY <= 0) { return false; }
			const int32 CA = FMath::Clamp(FMath::Min(IXA, IXB), 0, NX - 1);
			const int32 CB = FMath::Clamp(FMath::Max(IXA, IXB), 0, NX - 1);
			const int32 W = CB - CA + 1;
			TArray<uint8> Seen;
			Seen.Init(0, W * NY);
			TArray<int32> Queue;
			Queue.Reserve(W * 8);
			for (int32 IY = 0; IY < NY; ++IY)
			{
				if (IsFree(CA, IY))
				{
					Seen[IY * W] = 1;
					Queue.Add(IY * W);
				}
			}
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 Cell = Queue[Head];
				const int32 LX = Cell % W;
				const int32 LY = Cell / W;
				if (LX == W - 1) { return true; }
				const int32 Nx[4] = { LX + 1, LX - 1, LX, LX };
				const int32 Ny[4] = { LY, LY, LY + 1, LY - 1 };
				for (int32 k = 0; k < 4; ++k)
				{
					if (Nx[k] < 0 || Nx[k] >= W || Ny[k] < 0 || Ny[k] >= NY) { continue; }
					const int32 Next = Ny[k] * W + Nx[k];
					if (Seen[Next] || !IsFree(CA + Nx[k], Ny[k])) { continue; }
					Seen[Next] = 1;
					Queue.Add(Next);
				}
			}
			return false;
		}

		/** Hay un camino de casillas libres de la primera columna a la última. */
		bool IsConnected() const
		{
			return IsConnectedRange(0, NX - 1);
		}
	};

	/** Qué tiene de interesante un punto de la ronda (para el botín y las pruebas). */
	enum class EInterestKind : uint8
	{
		/** Arco de salto de una catapulta o un trampolín (de Pos a To): se deja libre de elementos. */
		JumpArc,
		/** Cima: lo alto de una cresta, de un castillo enorme o de uno con salas (Height: lo que sobresale del suelo). */
		Summit,
		/** Atajo: hueco estrecho de una fila, lanzador delante de un obstáculo o poza que corta un corredor (de Pos a To). */
		Shortcut,
		/** Rincón escondido a un lado de la playa, medio cerrado por decorado (Pos es su hueco). */
		Nook,
		/** Tramo de trinchera (Pos, dentro del canal). */
		Trench,
		/** Camino alternativo: el fondo de un corredor donde se separa de los otros. */
		Detour
	};

	/** Punto interesante de la ronda (espacio local del generador; Z, la de la arena natural en Pos). */
	struct FInterestPoint
	{
		EInterestKind Kind = EInterestKind::Summit;
		FVector2D Pos = FVector2D::ZeroVector;
		/** Fin del tramo (arcos y atajos); igual a Pos si es un punto. */
		FVector2D To = FVector2D::ZeroVector;
		double Z = 0.0;
		/** Lo que sobresale del suelo lo que hay en Pos (las cimas de los castillos); 0 si es el suelo. */
		double Height = 0.0;
		/** El elemento que lo crea (Count si es del terreno fijo). */
		ETNBeachElement Source = ETNBeachElement::Count;
	};

	/** El reparto de una ronda. */
	struct FRoundLayout
	{
		int32 Seed = 0;
		TArray<FItem> Items;
		/** Asiento en la arena de cada elemento que lo lleva (HasSeat). */
		TArray<FStamp> Stamps;
		/** Puntos interesantes (arcos de salto, cimas, atajos, rincones, trincheras y caminos alternativos). */
		TArray<FInterestPoint> Interest;
		int32 NumDecor = 0;
		int32 NumTraps = 0;
		int32 NumEnemies = 0;
		int32 NumCrabs = 0;
		int32 NumUrchins = 0;
		int32 NumLizards = 0;
		int32 NumQuadLanes = 0;
		int32 NumGullZones = 0;
		int32 NumGuidePaths = 0;
		int32 NumDungeons = 0;
		int32 NumCastles = 0;
		int32 NumRows = 0;
		int32 NumMilitary = 0;
		int32 NumLaunchers = 0;
		int32 NumNooks = 0;
		int32 NumPlugs = 0;
		/** Ocupación de cada banda de 50 m desde ItemsStartX (fracción del área jugable cubierta por lo que ocupa cada elemento). */
		TArray<float> BandCover;
		double CoverMean = 0.0;
		double CoverFirstThird = 0.0;
		/** Centro del castillo con salas principal (0 si no hay). */
		FVector2D DungeonPos = FVector2D::ZeroVector;
		/** Lado (+1 / -1) por el que se rodea el castillo principal. */
		int32 DungeonGapSide = 0;
		bool bPassageOk = false;

		/** Resumen para el registro («[Playa] ronda N: ...»). */
		FString Summary() const
		{
			FString Bands;
			for (const float Cover : BandCover)
			{
				Bands += FString::Printf(TEXT("%s%.0f"), Bands.IsEmpty() ? TEXT("") : TEXT(" "), Cover * 100.0f);
			}
			return FString::Printf(TEXT("semilla %d · %d elementos (%d decorado, %d trampas, %d enemigos: %d cangrejos, %d erizos, %d lagartos, %d pasos de quads, %d zonas de gaviotas) · %d castillos con salas y %d enormes · %d filas · %d piezas militares · %d lanzadores · %d rincones · %d tapones · %d asientos · %d puntos interesantes · %d tramos de pasarela guía · castillo principal a %.0f m (se rodea por %s) · paso libre de %.0f m %s · ocupación por banda de 50 m desde %.0f m (%%): %s · media %.0f %%, primer tercio %.0f %%"),
				Seed, Items.Num(), NumDecor, NumTraps, NumEnemies, NumCrabs, NumUrchins, NumLizards, NumQuadLanes, NumGullZones, NumDungeons, NumCastles, NumRows,
				NumMilitary, NumLaunchers, NumNooks, NumPlugs, Stamps.Num(), Interest.Num(), NumGuidePaths, DungeonPos.X / 100.0,
				DungeonGapSide > 0 ? TEXT("la derecha (+Y)") : TEXT("la izquierda (-Y)"), MinPassage / 100.0, bPassageOk ? TEXT("OK") : TEXT("ROTO"),
				ItemsStartX / 100.0, *Bands, CoverMean * 100.0, CoverFirstThird * 100.0);
		}
	};

	/** Densidad de las bandas: fracción del área ocupada, del 26 % en la salida al 46 % junto al mar. */
	inline double BandCoverage(double T)
	{
		return 0.26 + 0.2 * FMath::Clamp(T, 0.0, 1.0);
	}

	inline double ProgressOfX(double X)
	{
		return FMath::Clamp((X - ItemsStartX) / (ItemsEndX - ItemsStartX), 0.0, 1.0);
	}

	inline double XOfProgress(double T)
	{
		return ItemsStartX + FMath::Clamp(T, 0.0, 1.0) * (ItemsEndX - ItemsStartX);
	}

	/** Número de bandas de BandLength entre ItemsStartX e ItemsEndX. */
	inline int32 NumBands()
	{
		return FMath::CeilToInt32((ItemsEndX - ItemsStartX) / BandLength);
	}

	/** Hasta dónde puede llegar a lo ancho la huella de E: el decorado y las trampas, hasta SideReach; los enemigos, a 5 m de la selva. */
	inline double SideLimit(ETNBeachElement E)
	{
		return TNBeach::CategoryOf(E) == ETNBeachCategory::Enemy ? HalfWidth - SideMargin : SideReach;
	}

	/** Dentro de la playa repartible (salvo el paso de quads, que la cruza entera). Con la huella entera: los enemigos patrullan dentro. */
	inline bool InBounds(const FItem& Item)
	{
		for (const FVector2D& P : { Item.EndA(), Item.EndB() })
		{
			if (P.X - Item.Radius < ItemsStartX || P.X + Item.Radius > ItemsEndX) { return false; }
			if (Item.Element != ETNBeachElement::QuadLane && FMath::Abs(P.Y) + Item.Radius > SideLimit(Item.Element)) { return false; }
		}
		return true;
	}

	/** Especificación de un elemento con su tamaño (y su largo, si es alargado). */
	inline FItem MakeItem(TNProcMap::FRng& Rng, ETNBeachElement E, const FElementRule& Rule, double SizeScale, double Extent)
	{
		FItem Item;
		Item.Element = E;
		Item.Radius = TNBeach::FootprintRadius(E) * SizeScale;
		Item.Core = Item.Radius * Rule.CoreFraction;
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
			// Una llamada al generador por sentencia: el orden de evaluación de una expresión no está fijado.
			const double Sign = Rng.Chance(0.5) ? 1.0 : -1.0;
			return Sign * Rng.Range(0.55, 1.0) * Usable;
		}
		if (SideBias < 0.0 && Rng.Chance(-SideBias))
		{
			return Rng.Range(-0.55, 0.55) * Usable;
		}
		return Rng.Range(-Usable, Usable);
	}

	/**
	 * Marca (con Mark(A, B, Radio)) lo del terreno fijo que corta una línea recta hacia el mar: las pozas con su orilla, las
	 * trincheras y lo alto de las crestas (más de 1,5 m). Lo usan los tapones de las líneas rectas y sus pruebas.
	 */
	template <typename FMarkFn>
	void MarkTerrainObstacles(FMarkFn&& Mark)
	{
		for (const FPool& Pool : Pools()) { Mark(Pool.Center, Pool.Center, Pool.OuterR()); }
		for (const FTrench& Trench : Trenches())
		{
			for (int32 k = 0; k + 1 < Trench.Points.Num(); ++k) { Mark(Trench.Points[k], Trench.Points[k + 1], TrenchBermTop); }
		}
		for (const FRidge& Ridge : Ridges())
		{
			for (double U = -Ridge.HalfLength; U <= Ridge.HalfLength; U += 400.0)
			{
				if (RidgeCrestHeight(Ridge, U) < 150.0) { continue; }
				const FVector2D Crest = RidgeCrestPoint(Ridge, U);
				Mark(Crest, Crest, 300.0);
			}
		}
		for (const FVector2D& Lip : LipSamples()) { Mark(Lip, Lip, 250.0); }
	}

	/** Estado del reparto mientras se construye. */
	struct FBuilder
	{
		FRoundLayout& Out;
		TNProcMap::FRng Rng;
		FPassGrid Grid;
		TArray<int32> Counts;
		/** 1 = sigue puesto; 0 = quitado (se compacta al final). */
		TArray<uint8> Alive;
		/** Arcos de salto y huecos de los rincones: nada se pone encima. */
		TArray<FItem> Reserved;
		/** Cubos de BucketSize con los índices de lo que toca cada uno (para no mirar todos los elementos). */
		TArray<TArray<int32>> Buckets;
		TArray<int32> VisitMark;
		int32 VisitStamp = 0;
		double BucketX0 = 0.0;
		double BucketY0 = 0.0;
		int32 BucketsX = 0;
		int32 BucketsY = 0;
		static constexpr double BucketSize = 2500.0;
		/** Ventana (cm a cada lado) en la que se comprueba el paso al poner algo que cierra. */
		static constexpr double PassWindow = 6000.0;

		FBuilder(FRoundLayout& InOut, int32 InSeed)
			: Out(InOut)
			, Rng(static_cast<uint64>(static_cast<uint32>(InSeed)) * 0x9E3779B1ull + 0xBEAC4ull)
		{
			Grid.Init();
			Counts.Init(0, static_cast<int32>(ETNBeachElement::Count));
			BucketX0 = ItemsStartX - 5000.0;
			BucketY0 = -HalfWidth - 5000.0;
			BucketsX = FMath::CeilToInt32((ItemsEndX + 5000.0 - BucketX0) / BucketSize);
			BucketsY = FMath::CeilToInt32((2.0 * HalfWidth + 10000.0) / BucketSize);
			Buckets.SetNum(BucketsX * BucketsY);
		}

		/** Cubos que toca la caja de la cápsula de Item inflada Extra. */
		void BucketRange(const FItem& Item, double Extra, int32& BX0, int32& BX1, int32& BY0, int32& BY1) const
		{
			const FVector2D A = Item.EndA();
			const FVector2D B = Item.EndB();
			const double R = Item.Core + Extra;
			BX0 = FMath::Clamp(FMath::FloorToInt32((FMath::Min(A.X, B.X) - R - BucketX0) / BucketSize), 0, BucketsX - 1);
			BX1 = FMath::Clamp(FMath::FloorToInt32((FMath::Max(A.X, B.X) + R - BucketX0) / BucketSize), 0, BucketsX - 1);
			BY0 = FMath::Clamp(FMath::FloorToInt32((FMath::Min(A.Y, B.Y) - R - BucketY0) / BucketSize), 0, BucketsY - 1);
			BY1 = FMath::Clamp(FMath::FloorToInt32((FMath::Max(A.Y, B.Y) + R - BucketY0) / BucketSize), 0, BucketsY - 1);
		}

		/** No pisa nada de lo ya puesto ni lo reservado (las gaviotas solo miran a otras gaviotas y las demás no las miran a ellas). */
		bool Fits(const FItem& New, double Pad)
		{
			int32 BX0 = 0, BX1 = 0, BY0 = 0, BY1 = 0;
			BucketRange(New, Pad, BX0, BX1, BY0, BY1);
			++VisitStamp;
			for (int32 BY = BY0; BY <= BY1; ++BY)
			{
				for (int32 BX = BX0; BX <= BX1; ++BX)
				{
					for (const int32 Index : Buckets[BY * BucketsX + BX])
					{
						if (VisitMark[Index] == VisitStamp) { continue; }
						VisitMark[Index] = VisitStamp;
						if (!Alive[Index]) { continue; }
						const FItem& Old = Out.Items[Index];
						if (Old.bOverlay != New.bOverlay) { continue; }
						if (Clearance(Old, New) < Pad) { return false; }
					}
				}
			}
			if (!New.bOverlay)
			{
				for (const FItem& Zone : Reserved)
				{
					if (Clearance(Zone, New) < 0.0) { return false; }
				}
			}
			return true;
		}

		/**
		 * Pone el elemento si cabe: dentro de la playa, sin pisar nada, donde el terreno lo deja (con un asiento sin
		 * paredes) y, si cierra el paso, sin cortar el paso libre en la ventana de alrededor.
		 */
		bool TryAdd(const FItem& Item, double Pad)
		{
			if (!InBounds(Item) || !TerrainAllows(Item) || !Fits(Item, Pad) || !SeatIsGentle(Item)) { return false; }
			const int32 Index = Out.Items.Add(Item);
			Alive.Add(static_cast<uint8>(1));
			VisitMark.Add(0);
			int32 BX0 = 0, BX1 = 0, BY0 = 0, BY1 = 0;
			BucketRange(Item, 0.0, BX0, BX1, BY0, BY1);
			for (int32 BY = BY0; BY <= BY1; ++BY)
			{
				for (int32 BX = BX0; BX <= BX1; ++BX) { Buckets[BY * BucketsX + BX].Add(Index); }
			}
			Grid.Stamp(Item, 1);
			++Counts[static_cast<int32>(Item.Element)];
			if (Item.bBlocking && !Item.bOverlay)
			{
				const double XMin = FMath::Min(Item.EndA().X, Item.EndB().X) - Item.Radius;
				const double XMax = FMath::Max(Item.EndA().X, Item.EndB().X) + Item.Radius;
				if (!Grid.IsConnectedRange(Grid.ColumnOf(XMin - PassWindow), Grid.ColumnOf(XMax + PassWindow)))
				{
					Remove(Index);
					return false;
				}
			}
			return true;
		}

		void Remove(int32 Index)
		{
			if (!Alive.IsValidIndex(Index) || !Alive[Index]) { return; }
			Alive[Index] = 0;
			const FItem& Item = Out.Items[Index];
			Grid.Stamp(Item, -1);
			--Counts[static_cast<int32>(Item.Element)];
		}

		/** Quita lo que cierra el paso añadido desde From (lo último primero) hasta que vuelva a haber paso. */
		void RestorePassage(int32 From)
		{
			if (Grid.IsConnected()) { return; }
			for (int32 i = Out.Items.Num() - 1; i >= From; --i)
			{
				const FItem& Item = Out.Items[i];
				if (!Alive[i] || !Item.bBlocking || Item.bOverlay) { continue; }
				Remove(i);
				if (Grid.IsConnected()) { return; }
			}
		}

		/** Zona del arco de salto de un lanzador: de su borde hasta CatapultArc / TrampolineArc por delante, de 10 m de ancho. */
		static FItem JumpArcZone(const FItem& Launcher)
		{
			const double Reach = Launcher.Element == ETNBeachElement::Catapult ? CatapultArc : TrampolineArc;
			FItem Zone;
			Zone.Element = Launcher.Element;
			Zone.Yaw = Launcher.Yaw;
			Zone.Radius = JumpArcHalfWidth;
			Zone.Core = JumpArcHalfWidth;
			Zone.HalfLength = 0.5 * Reach;
			Zone.Pos = Launcher.Pos + Launcher.Axis() * (Launcher.Radius + 100.0 + 0.5 * Reach);
			Zone.bBlocking = false;
			return Zone;
		}

		/** Deja libre el arco de salto de un lanzador (desde su borde hacia delante) y lo apunta como punto interesante. */
		void ReserveJumpArc(const FItem& Launcher, EItemRole Why)
		{
			const double Reach = Launcher.Element == ETNBeachElement::Catapult ? CatapultArc : TrampolineArc;
			const FVector2D Dir = Launcher.Axis();
			Reserved.Add(JumpArcZone(Launcher));
			FInterestPoint Arc;
			Arc.Kind = EInterestKind::JumpArc;
			Arc.Pos = Launcher.Pos;
			Arc.To = Launcher.Pos + Dir * (Launcher.Radius + 100.0 + Reach);
			Arc.Source = Launcher.Element;
			Out.Interest.Add(Arc);
			if (Why == EItemRole::Launcher)
			{
				FInterestPoint Cut = Arc;
				Cut.Kind = EInterestKind::Shortcut;
				Out.Interest.Add(Cut);
			}
		}

		/** TryAdd y, si es un lanzador, con su arco de salto libre de todo (si no lo está, no se pone). */
		bool TryAddWithArc(const FItem& Item, double Pad)
		{
			const bool bLauncher = RuleOf(Item.Element).bLauncher;
			if (bLauncher)
			{
				const FItem Zone = JumpArcZone(Item);
				if (!InBounds(Zone) || !Fits(Zone, 0.0)) { return false; }
			}
			if (!TryAdd(Item, Pad)) { return false; }
			if (bLauncher) { ReserveJumpArc(Item, Item.Role); }
			return true;
		}

		/** Un elemento cualquiera de la regla de E con tamaño al azar, en Pos (sin ponerlo). */
		FItem Make(ETNBeachElement E, const FVector2D& Pos, EItemRole Role, double SizeScale = -1.0)
		{
			const FElementRule Rule = RuleOf(E);
			const double S = SizeScale > 0.0 ? SizeScale : Rng.Range(Rule.SizeMin, Rule.SizeMax);
			const double Extent = Rule.ExtentMax > 0.0 ? Rng.Range(Rule.ExtentMin, Rule.ExtentMax) : 0.0;
			FItem Item = MakeItem(Rng, E, Rule, S, Extent);
			Item.Pos = Pos;
			Item.Role = Role;
			return Item;
		}

		/** Trampolín (o catapulta) justo delante (hacia la salida) de Target, apuntando a él: salta encima o por encima. */
		bool PlaceLauncherBefore(const FVector2D& Target, double TargetReach, ETNBeachElement E, double Along = 0.0)
		{
			FItem Item = Make(E, FVector2D::ZeroVector, EItemRole::Launcher, 1.0);
			Item.Yaw = Rng.Range(-8.0, 8.0);
			const FVector2D Dir = Item.Axis();
			const FVector2D Side(-Dir.Y, Dir.X);
			for (int32 Try = 0; Try < 4; ++Try)
			{
				const double Back = TargetReach + Item.Radius + Rng.Range(150.0, 700.0);
				const double Lateral = Along + Rng.Range(-400.0, 400.0);
				Item.Pos = Target - Dir * Back + Side * Lateral;
				if (!TryAdd(Item, ItemPad)) { continue; }
				FInterestPoint Cut;
				Cut.Kind = EInterestKind::Shortcut;
				Cut.Pos = Item.Pos;
				Cut.To = Target + Dir * TargetReach;
				Cut.Source = E;
				Out.Interest.Add(Cut);
				FInterestPoint Arc = Cut;
				Arc.Kind = EInterestKind::JumpArc;
				Out.Interest.Add(Arc);
				return true;
			}
			return false;
		}

		/**
		 * Decorado que sirve de muro (en las alas del castillo, las filas y los rincones) y cabe en MaxRadius: redondo y
		 * que cierra el paso; de MinFoot a 26 m de huella mientras quepa (los grandes, más a menudo). Theme limita a una
		 * lista (vacía = cualquiera).
		 */
		ETNBeachElement PickWallBlocker(double MaxRadius, const TArray<ETNBeachElement>& Theme)
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
				if (Theme.Num() > 0 && !Theme.Contains(E)) { continue; }
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
		 * Muro de piezas que se tocan de From a To (con alambre entre pieza y pieza si bWire): lo que sobra al final se
		 * cierra con una pieza más pequeña. LeakChance: a veces deja entre dos piezas un hueco estrecho (2,5-4,5 m) que no
		 * cuenta como paso (se pasa apurando) y se apunta como atajo. Devuelve cuántas piezas ha puesto.
		 */
		int32 BuildWall(const FVector2D& From, const FVector2D& To, EItemRole Role, const TArray<ETNBeachElement>& Theme, bool bWire, double LeakChance)
		{
			const FVector2D Delta = To - From;
			const double Len = Delta.Size();
			if (Len < 200.0) { return 0; }
			const FVector2D Dir = Delta / Len;
			const FVector2D Across(-Dir.Y, Dir.X);
			const double WireR = TNBeach::FootprintRadius(ETNBeachElement::BarbedWire);
			double Cursor = 0.0;
			bool bWireTurn = false;
			int32 Placed = 0;
			for (int32 Guard = 0; Guard < 48 && Cursor < Len - 150.0; ++Guard)
			{
				const double Left = Len - Cursor;
				if (bWire && bWireTurn && Left >= 2.0 * WireR + 800.0)
				{
					const FElementRule WireRule = RuleOf(ETNBeachElement::BarbedWire);
					const double WireLen = FMath::Min(Rng.Range(1600.0, 3000.0), Left - 2.0 * WireR);
					FItem Wire = MakeItem(Rng, ETNBeachElement::BarbedWire, WireRule, 1.0, WireLen);
					Wire.Role = Role;
					Wire.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
					Wire.Pos = From + Dir * (Cursor + WireR + WireLen * 0.5);
					if (TryAdd(Wire, 0.0)) { ++Placed; }
					Cursor += WireLen + 2.0 * WireR + 20.0;
				}
				else
				{
					// El desorden de través mueve la pieza hasta 30 cm a lo ancho: que no se salga de la playa.
					const double MaxR = 0.5 * Left - 30.0;
					const ETNBeachElement E = MaxR > 0.0 ? PickWallBlocker(MaxR, Theme) : ETNBeachElement::Count;
					if (E == ETNBeachElement::Count) { break; }
					const FElementRule BlockRule = RuleOf(E);
					const double FootR = TNBeach::FootprintRadius(E);
					const double Sb = FMath::Min(Rng.Range(0.85, 1.15), MaxR / FootR);
					double Rb = FootR * Sb;
					// Si no cabe (una duna, una poza, un paso de quads...), la misma pieza más pequeña en su sitio: así el muro no
					// se queda con agujeros. Si ni así, hueco del tamaño de la primera.
					for (const double Shrink : { 1.0, 0.8, 0.6 })
					{
						const double S = FMath::Max(0.6, Sb * Shrink);
						FItem Block = MakeItem(Rng, E, BlockRule, S, 0.0);
						Block.Role = Role;
						const double Rs = FootR * S;
						// Desorden solo de través: a lo largo las piezas se tocan sin pisarse.
						Block.Pos = From + Dir * (Cursor + Rs) + Across * Rng.Range(-150.0, 150.0);
						if (TryAdd(Block, 0.0))
						{
							++Placed;
							Rb = Rs;
							break;
						}
					}
					Cursor += 2.0 * Rb + 40.0;
					if (LeakChance > 0.0 && Cursor < Len - 1500.0 && Rng.Chance(LeakChance))
					{
						const double Leak = Rng.Range(250.0, 450.0);
						FInterestPoint Cut;
						Cut.Kind = EInterestKind::Shortcut;
						Cut.Pos = From + Dir * (Cursor + 0.5 * Leak) - FVector2D(600.0, 0.0);
						Cut.To = Cut.Pos + FVector2D(1200.0, 0.0);
						Out.Interest.Add(Cut);
						Cursor += Leak;
					}
				}
				bWireTurn = !bWireTurn;
			}
			return Placed;
		}

		// ── Castillos con salas ──

		/**
		 * Castillo con salas hacia la mitad del recorrido y dos alas de decorado y alambre que cierran la playa en embudo
		 * hacia su entrada: o se atraviesa o se rodea por el único hueco (14 m, con algas) junto a la selva de un lado.
		 */
		void PlaceMainDungeon()
		{
			const FElementRule Rule = RuleOf(ETNBeachElement::SandDungeon);
			FItem Castle;
			bool bPlaced = false;
			for (int32 Try = 0; Try < 24 && !bPlaced; ++Try)
			{
				Castle = MakeItem(Rng, ETNBeachElement::SandDungeon, Rule, Rng.Range(Rule.SizeMin, Rule.SizeMax), 0.0);
				Castle.Role = EItemRole::Dungeon;
				const double CastleX = XOfProgress(Rng.Range(0.42, 0.58));
				Castle.Pos = FVector2D(CastleX, Rng.Range(-1.0, 1.0) * HalfWidth * 0.28);
				bPlaced = TryAdd(Castle, ItemPad);
			}
			if (!bPlaced) { return; }
			Out.DungeonPos = Castle.Pos;
			const int32 GapSide = Rng.Chance(0.5) ? 1 : -1;
			Out.DungeonGapSide = GapSide;
			const int32 WallStart = Out.Items.Num();
			static constexpr double Sweep = 0.18;
			const double GapWidth = MinPassage + 600.0;
			static const TArray<ETNBeachElement> AnyTheme;
			for (const int32 Side : { -1, 1 })
			{
				// Hasta la selva (a 6 m de los muros): por el lado cerrado no queda un pasillo junto a los árboles.
				const double EdgeY = Side * (SideReach - 200.0);
				const double StopY = Side == GapSide ? EdgeY - Side * GapWidth : EdgeY;
				const double UEnd = FMath::Abs(StopY - Castle.Pos.Y);
				const FVector2D LineDir = FVector2D(-Sweep, static_cast<double>(Side)).GetSafeNormal();
				const double StretchU = 1.0 / FMath::Abs(LineDir.Y);
				const FVector2D From = Castle.Pos + LineDir * ((Castle.Radius + 40.0) * StretchU);
				const FVector2D To = Castle.Pos + LineDir * (UEnd * StretchU);
				BuildWall(From, To, EItemRole::DungeonWing, AnyTheme, true, 0.0);
				if (Side == GapSide)
				{
					// Algas en el hueco: rodear también cuesta.
					for (int32 k = 0; k < 2; ++k)
					{
						FItem Weed = Make(ETNBeachElement::Seaweed, FVector2D::ZeroVector, EItemRole::DungeonWing, Rng.Range(0.8, 1.0));
						const double GapCenterY = EdgeY - Side * GapWidth * 0.5;
						const double LineX = Castle.Pos.X - Sweep * FMath::Abs(GapCenterY - Castle.Pos.Y);
						Weed.Pos = FVector2D(LineX + (k == 0 ? -1.0 : 1.0) * Rng.Range(500.0, 1500.0), GapCenterY);
						TryAdd(Weed, ItemPad);
					}
				}
			}
			RestorePassage(WallStart);
			AddSummit(Castle, 1150.0 * Castle.Spec.SizeScale);
		}

		/** Otro castillo con salas (sin alas) entre T0 y T1. */
		bool PlaceExtraDungeon(double T0, double T1)
		{
			const FElementRule Rule = RuleOf(ETNBeachElement::SandDungeon);
			for (int32 Try = 0; Try < 40; ++Try)
			{
				FItem Castle = MakeItem(Rng, ETNBeachElement::SandDungeon, Rule, Rng.Range(Rule.SizeMin, Rule.SizeMax), 0.0);
				Castle.Role = EItemRole::Dungeon;
				const double CastleX = XOfProgress(Rng.Range(T0, T1));
				Castle.Pos = FVector2D(CastleX, Rng.Range(-1.0, 1.0) * HalfWidth * 0.55);
				if (TryAdd(Castle, ItemPad))
				{
					AddSummit(Castle, 1150.0 * Castle.Spec.SizeScale);
					return true;
				}
			}
			return false;
		}

		/** El principal con sus alas y uno o dos más por las otras mitades (al menos uno). */
		void PlaceDungeons()
		{
			PlaceMainDungeon();
			const bool bEarly = Rng.Chance(0.6) && PlaceExtraDungeon(0.12, 0.36);
			if (!bEarly || Rng.Chance(0.6)) { PlaceExtraDungeon(0.62, 0.9); }
		}

		void AddSummit(const FItem& Item, double Height)
		{
			FInterestPoint Top;
			Top.Kind = EInterestKind::Summit;
			Top.Pos = Item.Pos;
			Top.To = Item.Pos;
			Top.Height = Height;
			Top.Source = Item.Element;
			Out.Interest.Add(Top);
		}

		// ── Quads y gaviotas ──

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
				for (int32 i = 0; i < Out.Items.Num(); ++i)
				{
					if (Alive[i] && Out.Items[i].Role == EItemRole::QuadLane && FMath::Abs(Out.Items[i].Pos.X - Lane.Pos.X) < 17000.0) { bFar = false; }
				}
				if (bFar && TryAdd(Lane, ItemPad + 800.0)) { ++Placed; }
			}
		}

		/**
		 * Zonas de gaviotas y pelícanos: 4-6 repartidas a lo largo (una por tramo) y a lo ancho (lados alternos), con 150 m
		 * como poco entre centros y cada una con su tamaño (su círculo) distinto.
		 */
		void PlaceGullZones()
		{
			const FElementRule Rule = RuleOf(ETNBeachElement::GullZone);
			const int32 Wanted = Rng.RangeInt(4, 6);
			TArray<double> Sizes;
			for (int32 k = 0; k < Wanted; ++k)
			{
				Sizes.Add(FMath::Lerp(Rule.SizeMin, Rule.SizeMax, (k + Rng.Range(0.2, 0.8)) / Wanted));
			}
			Rng.Shuffle(Sizes);
			const double Side0 = Rng.Chance(0.5) ? 1.0 : -1.0;
			static const double Lateral[3] = { 0.5, 0.0, -0.5 };
			for (int32 k = 0; k < Wanted; ++k)
			{
				const double T0 = 0.1 + 0.87 * k / Wanted;
				const double T1 = 0.1 + 0.87 * (k + 1) / Wanted;
				for (int32 Try = 0; Try < 20; ++Try)
				{
					FItem Zone = MakeItem(Rng, ETNBeachElement::GullZone, Rule, Sizes[k], 0.0);
					Zone.Role = EItemRole::GullZone;
					const double Lane = Side0 * Lateral[k % 3] + Rng.Range(-0.18, 0.18);
					Zone.Pos = FVector2D(XOfProgress(Rng.Range(T0 + 0.15 * (T1 - T0), T1 - 0.15 * (T1 - T0))), Lane * HalfWidth);
					bool bFar = true;
					for (int32 i = 0; i < Out.Items.Num(); ++i)
					{
						if (Alive[i] && Out.Items[i].Role == EItemRole::GullZone && FVector2D::Distance(Out.Items[i].Pos, Zone.Pos) < 15000.0) { bFar = false; }
					}
					if (bFar && TryAdd(Zone, 2000.0)) { break; }
				}
			}
		}

		// ── Filas que obligan a zigzaguear ──

		/**
		 * Filas de obstáculos que cierran casi toda la playa con un hueco que cambia de lado de una a otra (izquierda,
		 * derecha, embudo al centro): obligan a zigzaguear. Cada fila es de un tema (militar con alambre, restos de la
		 * marea o trastos de playa), a veces deja un hueco estrecho para apurar y, a veces, una catapulta o un trampolín
		 * delante para saltarla.
		 */
		void PlaceBarrierRows()
		{
			static const double RowT[] = { 0.1, 0.18, 0.37, 0.64, 0.72, 0.8, 0.885 };
			static const TArray<ETNBeachElement> Military = { ETNBeachElement::Sandbags, ETNBeachElement::TankTrap, ETNBeachElement::AmmoCrate };
			static const TArray<ETNBeachElement> Wrack = { ETNBeachElement::Driftwood, ETNBeachElement::MossyLog, ETNBeachElement::Rock,
				ETNBeachElement::RockCluster, ETNBeachElement::OldPlanks, ETNBeachElement::FishingNet, ETNBeachElement::Buoy };
			static const TArray<ETNBeachElement> Picnic = { ETNBeachElement::BeachChair, ETNBeachElement::PlantedUmbrella, ETNBeachElement::SandCastleSmall,
				ETNBeachElement::SandCastleHuge, ETNBeachElement::Buoy, ETNBeachElement::BeachBall, ETNBeachElement::ToyBucket };
			const int32 Pattern = Rng.RangeInt(0, 2);
			for (int32 r = 0; r < static_cast<int32>(UE_ARRAY_COUNT(RowT)); ++r)
			{
				if (!Rng.Chance(0.9)) { continue; }
				const double X = XOfProgress(RowT[r] + Rng.Range(-0.02, 0.02));
				const int32 Kind = (Pattern + r) % 3;
				const int32 ThemeIndex = Rng.RangeInt(0, 2);
				const TArray<ETNBeachElement>& Theme = ThemeIndex == 0 ? Military : (ThemeIndex == 1 ? Wrack : Picnic);
				const bool bWire = ThemeIndex == 0;
				const double Gap = Rng.Range(1400.0, 2200.0);
				// Hasta la selva (a 6 m de los muros): por el lado cerrado no queda un pasillo junto a los árboles.
				const double Edge = SideReach - 200.0;
				const int32 RowStart = Out.Items.Num();
				int32 Placed = 0;
				FVector2D GapCenter;
				double Tilt = 0.0;
				double Bow = 0.0;
				double FunnelY = 0.0;
				double FunnelSweep = 0.0;
				if (Kind < 2)
				{
					// Hueco junto a la selva de un lado (-Y o +Y), algo en diagonal y combada.
					const double GapSide = Kind == 0 ? -1.0 : 1.0;
					const double YFrom = -GapSide * Edge;
					const double YTo = GapSide * (Edge - Gap);
					Tilt = Rng.Range(-0.12, 0.12);
					Bow = Rng.Range(-1200.0, 1200.0);
					constexpr int32 Pieces = 4;
					for (int32 k = 0; k < Pieces; ++k)
					{
						const double Ya = FMath::Lerp(YFrom, YTo, static_cast<double>(k) / Pieces);
						const double Yb = FMath::Lerp(YFrom, YTo, static_cast<double>(k + 1) / Pieces);
						auto LineX = [X, Tilt, Bow, Edge](double Y) { return X + Tilt * Y + Bow * (1.0 - FMath::Square(Y / Edge)); };
						Placed += BuildWall(FVector2D(LineX(Ya), Ya), FVector2D(LineX(Yb), Yb), EItemRole::Row, Theme, bWire, 0.12);
					}
					GapCenter = FVector2D(X + Tilt * GapSide * (Edge - 0.5 * Gap), GapSide * (Edge - 0.5 * Gap));
				}
				else
				{
					// Embudo: dos alas desde la selva que bajan hacia la salida y dejan el hueco en medio.
					FunnelY = Rng.Range(-0.3, 0.3) * HalfWidth;
					FunnelSweep = Rng.Range(0.18, 0.3);
					for (const double Side : { -1.0, 1.0 })
					{
						const FVector2D Inner(X, FunnelY + Side * 0.5 * Gap);
						const double Span = Side * Edge - Inner.Y;
						const FVector2D Outer(X - FunnelSweep * FMath::Abs(Span), Side * Edge);
						Placed += BuildWall(Inner, Outer, EItemRole::Row, Theme, bWire, 0.1);
					}
					GapCenter = FVector2D(X, FunnelY);
				}
				RestorePassage(RowStart);
				if (Placed == 0) { continue; }
				++Out.NumRows;
				// Algas en el hueco: el paso bueno tampoco es gratis.
				const double WeedX = Rng.Range(900.0, 1600.0);
				const double WeedY = Rng.Range(-300.0, 300.0);
				FItem Weed = Make(ETNBeachElement::Seaweed, GapCenter + FVector2D(WeedX, WeedY), EItemRole::Row);
				TryAdd(Weed, ItemPad);
				// A veces, un lanzador delante de la fila, lejos del hueco: o se salta o se va hasta el hueco.
				if (Rng.Chance(0.6))
				{
					const double Y = FMath::Clamp(-GapCenter.Y * Rng.Range(0.3, 0.8), -HalfWidth * 0.6, HalfWidth * 0.6);
					// Delante de la fila de verdad (inclinada y combada, o en embudo), no de su X de partida.
					const double RowX = Kind < 2 ? X + Tilt * Y + Bow * (1.0 - FMath::Square(Y / Edge))
						: X - FunnelSweep * FMath::Max(0.0, FMath::Abs(Y - FunnelY) - 0.5 * Gap);
					PlaceLauncherBefore(FVector2D(RowX, Y), 900.0, Rng.Chance(0.5) ? ETNBeachElement::Catapult : ETNBeachElement::Trampoline);
				}
			}
		}

		// ── Militar ──

		/** Puesto: red de camuflaje con parapeto de sacos hacia el mar y cajas, bidones, cascos y soldaditos alrededor. */
		bool PlaceOutpost(const FVector2D& Center)
		{
			FItem Net = Make(ETNBeachElement::CamoNet, Center, EItemRole::Military);
			if (!TryAdd(Net, ItemPad)) { return false; }
			// Sacos en arco por delante (hacia el mar).
			const int32 Bags = Rng.RangeInt(2, 3);
			for (int32 b = 0; b < Bags; ++b)
			{
				FItem Bag = Make(ETNBeachElement::Sandbags, FVector2D::ZeroVector, EItemRole::Military);
				const double Ang = FMath::DegreesToRadians(-50.0 + 100.0 * (b + 0.5) / Bags + Rng.Range(-8.0, 8.0));
				Bag.Pos = Center + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Net.Radius + Bag.Radius + 100.0);
				Bag.Yaw = FMath::RadiansToDegrees(Ang) + 90.0;
				TryAdd(Bag, 0.0);
			}
			static const ETNBeachElement Gear[] = { ETNBeachElement::AmmoCrate, ETNBeachElement::AmmoCrate, ETNBeachElement::Jerrycan, ETNBeachElement::Jerrycan,
				ETNBeachElement::MilitaryHelmet, ETNBeachElement::ToySoldiers, ETNBeachElement::ToySoldiers };
			const int32 NumGear = Rng.RangeInt(3, 6);
			for (int32 g = 0; g < NumGear; ++g)
			{
				const ETNBeachElement E = Gear[Rng.RangeInt(0, static_cast<int32>(UE_ARRAY_COUNT(Gear)) - 1)];
				FItem Thing = Make(E, FVector2D::ZeroVector, EItemRole::Military);
				const double Ang = FMath::DegreesToRadians(Rng.Range(80.0, 280.0));
				Thing.Pos = Center + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Net.Radius + Thing.Radius + Rng.Range(150.0, 500.0));
				TryAdd(Thing, 50.0);
			}
			return true;
		}

		/** Fila de erizos antitanque de From a To con huecos de 2,5-5 m (se pasa apurando). */
		int32 PlaceTankTrapLine(const FVector2D& From, const FVector2D& To)
		{
			const FVector2D Delta = To - From;
			const double Len = Delta.Size();
			if (Len < 500.0) { return 0; }
			const FVector2D Dir = Delta / Len;
			int32 Placed = 0;
			double Cursor = 0.0;
			for (int32 Guard = 0; Guard < 24 && Cursor < Len; ++Guard)
			{
				FItem Trap = Make(ETNBeachElement::TankTrap, FVector2D::ZeroVector, EItemRole::Military);
				Trap.Pos = From + Dir * (Cursor + Trap.Radius) + FVector2D(Rng.Range(-200.0, 200.0), 0.0);
				if (TryAdd(Trap, 0.0)) { ++Placed; }
				Cursor += 2.0 * Trap.Radius + Rng.Range(250.0, 500.0);
			}
			return Placed;
		}

		/** Campo de minas: 5-9 minas repartidas en una mancha de ~25 x 40 m alrededor de Center. */
		int32 PlaceMinefield(const FVector2D& Center)
		{
			const int32 Wanted = Rng.RangeInt(5, 9);
			int32 Placed = 0;
			for (int32 Try = 0; Try < Wanted * 4 && Placed < Wanted; ++Try)
			{
				const double MineX = Rng.Range(-1250.0, 1250.0);
				const double MineY = Rng.Range(-2000.0, 2000.0);
				FItem Mine = Make(ETNBeachElement::Mine, Center + FVector2D(MineX, MineY), EItemRole::Military);
				if (TryAdd(Mine, 200.0)) { ++Placed; }
			}
			return Placed;
		}

		/** La tropa de las trincheras fijas: sacos en las puntas, erizos y minas por delante y un puesto por detrás. */
		void PlaceTrenchGarrison()
		{
			const TArray<FTrench>& All = Trenches();
			if (All.Num() == 0) { return; }
			for (const FTrench& Trench : All)
			{
				for (const FVector2D& End : { Trench.Points[0], Trench.Points.Last() })
				{
					const double Beyond = End.Y > 0.0 ? 1.0 : -1.0;
					FItem Bag = Make(ETNBeachElement::Sandbags, End + FVector2D(Rng.Range(-300.0, 300.0), Beyond * 1800.0), EItemRole::Military);
					Bag.Yaw = Rng.Range(-20.0, 20.0);
					TryAdd(Bag, ItemPad);
				}
			}
			// Por delante de la línea del mar: erizos y, a veces, minas.
			const FTrench& Front = All[0];
			const double FrontX = Front.Max.X;
			const double LineX0 = FrontX + Rng.Range(1800.0, 2600.0);
			const double LineX1 = FrontX + Rng.Range(1800.0, 2600.0);
			PlaceTankTrapLine(FVector2D(LineX0, Front.Min.Y + 1500.0), FVector2D(LineX1, Front.Max.Y - 1500.0));
			if (Rng.Chance(0.6))
			{
				const double FieldX = FrontX + Rng.Range(3800.0, 5000.0);
				PlaceMinefield(FVector2D(FieldX, Rng.Range(-0.5, 0.5) * HalfWidth));
			}
			// Por detrás de la línea de la salida: el puesto.
			const FTrench& Back = All.Num() > 1 ? All[1] : All[0];
			for (int32 Try = 0; Try < 10; ++Try)
			{
				const double PostX = Back.Min.X - Rng.Range(2600.0, 3600.0);
				if (PlaceOutpost(FVector2D(PostX, Rng.Range(-0.6, 0.6) * HalfWidth))) { break; }
			}
		}

		/** Puestos, filas de erizos y campos de minas por toda la playa (algunos, en los corredores: el camino natural). */
		void PlaceMilitaryPosts()
		{
			const int32 Posts = Rng.RangeInt(3, 4);
			for (int32 p = 0; p < Posts; ++p)
			{
				for (int32 Try = 0; Try < 12; ++Try)
				{
					const double PostX = XOfProgress(Rng.Range(0.1, 0.92));
					if (PlaceOutpost(FVector2D(PostX, Rng.Range(-0.75, 0.75) * HalfWidth))) { break; }
				}
			}
			const int32 Lines = Rng.RangeInt(1, 2);
			for (int32 l = 0; l < Lines; ++l)
			{
				const double X = XOfProgress(Rng.Range(0.15, 0.9));
				const double Y0 = Rng.Range(-0.8, 0.0) * HalfWidth;
				const double Span = Rng.Range(6000.0, 11000.0);
				const double Ang = FMath::DegreesToRadians(90.0 + Rng.Range(-35.0, 35.0));
				const FVector2D From(X, Y0);
				PlaceTankTrapLine(From, From + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Span);
			}
			const int32 Fields = Rng.RangeInt(2, 3);
			for (int32 f = 0; f < Fields; ++f)
			{
				const double X = XOfProgress(Rng.Range(0.12, 0.9));
				const bool bInCorridor = Rng.Chance(0.5);
				const FCorridorSample C = CorridorAt(Rng.RangeInt(0, 1), X);
				PlaceMinefield(FVector2D(X, bInCorridor ? C.Y : Rng.Range(-0.7, 0.7) * HalfWidth));
			}
		}

		// ── Rincones, castillos y lanzadores ──

		/** Rincones escondidos junto a la selva: una herradura de decorado grande con el hueco hacia el centro de la playa o hacia el mar. */
		void PlaceNooks()
		{
			static const TArray<ETNBeachElement> Walls = { ETNBeachElement::Rock, ETNBeachElement::RockCluster, ETNBeachElement::MossyLog,
				ETNBeachElement::Driftwood, ETNBeachElement::OldPlanks, ETNBeachElement::FishingNet, ETNBeachElement::SandCastleSmall,
				ETNBeachElement::Sandbags };
			const int32 Wanted = Rng.RangeInt(3, 5);
			int32 Made = 0;
			for (int32 Try = 0; Try < Wanted * 30 && Made < Wanted; ++Try)
			{
				const double Side = Rng.Chance(0.5) ? 1.0 : -1.0;
				const double Hollow = Rng.Range(600.0, 900.0);
				const FVector2D Center(XOfProgress(Rng.Range(0.08, 0.9)), Side * (HalfWidth - SideMargin - Hollow - 1900.0));
				FItem Hole;
				Hole.Element = ETNBeachElement::Coconut;
				Hole.Pos = Center;
				Hole.Radius = Hollow;
				Hole.Core = Hollow;
				Hole.bBlocking = false;
				if (!InBounds(Hole) || !TerrainAllows(Hole) || !Fits(Hole, ItemPad)) { continue; }
				// Hueco hacia el centro de la playa (-Side) o hacia el mar (+X).
				const double OpenAng = Rng.Chance(0.5) ? (Side > 0.0 ? -HALF_PI : HALF_PI) : 0.0;
				const int32 NookStart = Out.Items.Num();
				int32 Pieces = 0;
				double Ang = OpenAng + FMath::DegreesToRadians(55.0);
				const double EndAng = OpenAng + TNProcMap::TwoPi - FMath::DegreesToRadians(55.0);
				for (int32 Guard = 0; Guard < 16 && Ang < EndAng; ++Guard)
				{
					const ETNBeachElement E = PickWallBlocker(650.0, Walls);
					if (E == ETNBeachElement::Count) { break; }
					// Piezas de 6,5 m de huella como mucho: la herradura cabe entre el hueco y la selva y se asienta en las dunas
					// de los lados.
					const double FootR = TNBeach::FootprintRadius(E);
					FItem Piece = Make(E, FVector2D::ZeroVector, EItemRole::Nook, FMath::Min(Rng.Range(0.8, 1.0), 650.0 / FootR));
					const double Dist = Hollow + Piece.Radius + 50.0;
					Piece.Pos = Center + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Dist;
					if (TryAdd(Piece, 0.0)) { ++Pieces; }
					Ang += 2.0 * FMath::Asin(FMath::Min(1.0, (Piece.Radius + 30.0) / Dist));
				}
				if (Pieces < 3)
				{
					for (int32 i = Out.Items.Num() - 1; i >= NookStart; --i) { Remove(i); }
					continue;
				}
				Reserved.Add(Hole);
				FInterestPoint Nook;
				Nook.Kind = EInterestKind::Nook;
				Nook.Pos = Center;
				Nook.To = Center;
				Out.Interest.Add(Nook);
				++Made;
			}
			Out.NumNooks = Made;
		}

		/**
		 * Castillos de arena enormes por todo el recorrido (5-8), la mitad con un trampolín delante para subirse. Uno por
		 * tramo, con sus intentos (un tramo de dunas no se come los de los demás), y los que falten, donde quepan.
		 */
		void PlaceCastles()
		{
			const int32 Wanted = Rng.RangeInt(5, 8);
			int32 Made = 0;
			auto TryCastle = [this, &Made](double Progress)
			{
				const double CastleY = Rng.Range(-0.7, 0.7) * HalfWidth;
				FItem Castle = Make(ETNBeachElement::SandCastleHuge, FVector2D(XOfProgress(Progress), CastleY), EItemRole::Castle);
				if (!TryAdd(Castle, ItemPad)) { return false; }
				++Made;
				AddSummit(Castle, 880.0 * Castle.Spec.SizeScale);
				if (Rng.Chance(0.55)) { PlaceLauncherBefore(Castle.Pos, Castle.Radius, ETNBeachElement::Trampoline); }
				return true;
			};
			for (int32 Slot = 0; Slot < Wanted; ++Slot)
			{
				for (int32 Try = 0; Try < 10; ++Try)
				{
					const double SlotT = (Slot + Rng.Range(0.1, 0.9)) / Wanted;
					if (TryCastle(SlotT)) { break; }
				}
			}
			for (int32 Try = 0; Try < 10 * Wanted && Made < Wanted; ++Try)
			{
				const double AnyT = Rng.Range(0.02, 0.98);
				TryCastle(AnyT);
			}
		}

		/**
		 * Trampolines delante de lo alto (castillos con salas, crestas con cornisa, pozas) para saltarlo, y alguna catapulta
		 * suelta: siempre apuntando al mar y con su arco libre.
		 */
		void PlaceLaunchers()
		{
			for (int32 i = 0; i < Out.Items.Num(); ++i)
			{
				if (!Alive[i] || Out.Items[i].Role != EItemRole::Dungeon) { continue; }
				const FItem Castle = Out.Items[i];
				const int32 Wanted = Rng.RangeInt(1, 2);
				for (int32 k = 0; k < Wanted; ++k)
				{
					PlaceLauncherBefore(Castle.Pos, Castle.Radius, ETNBeachElement::Trampoline, Rng.Range(-0.5, 0.5) * Castle.Radius);
				}
			}
			// Al pie de la cara empinada de las crestas con cornisa: un bote y se pasa por encima.
			for (const FRidge& Ridge : Ridges())
			{
				if (!Ridge.bLip || Ridge.Windward.X < 0.3) { continue; }
				const int32 Wanted = Rng.RangeInt(1, 2);
				for (int32 k = 0; k < Wanted; ++k)
				{
					const double U = Rng.Range(-0.6, 0.6) * Ridge.HalfLength;
					if (!IsLipAt(Ridge, U)) { continue; }
					const double Slip = RidgeSlipWidth(RidgeCrestHeight(Ridge, U));
					const FVector2D Foot = RidgeCrestPoint(Ridge, U) - Ridge.Windward * Slip;
					PlaceLauncherBefore(Foot, 200.0, ETNBeachElement::Trampoline);
				}
			}
			// Delante de las pozas que cortan un corredor: saltarla en vez de nadar.
			for (const FPool& Pool : Pools())
			{
				if (Pool.Corridor < 0 || !Rng.Chance(0.6)) { continue; }
				PlaceLauncherBefore(Pool.Center, Pool.OuterR() + 1200.0, Rng.Chance(0.5) ? ETNBeachElement::Trampoline : ETNBeachElement::Catapult);
			}
		}

		/**
		 * Las trampas que tienen que verse mucho (catapultas, trampolines, plataformas móviles, conchas que atrapan,
		 * plataformas sobre hoyos, palas, cubos y puertas), repartidas a lo largo por tramos antes del relleno: grandes como
		 * son, con la playa ya llena no cabrían. Las catapultas, primero: su arco de salto libre es el más largo. Cada tramo
		 * tiene sus intentos (uno difícil no se come los de los demás) y lo que no cabe en el suyo se busca luego por toda
		 * la playa.
		 */
		void PlaceFeaturedTraps()
		{
			struct FQuota
			{
				ETNBeachElement Element;
				int32 Min;
				int32 Max;
			};
			static const FQuota Quotas[] = {
				{ ETNBeachElement::Catapult, 6, 9 },
				{ ETNBeachElement::Trampoline, 6, 9 },
				{ ETNBeachElement::MovingPlatform, 9, 13 },
				{ ETNBeachElement::ClamTrap, 7, 10 },
				{ ETNBeachElement::WobblyPlatform, 5, 7 },
				{ ETNBeachElement::SpadeRamp, 4, 6 },
				{ ETNBeachElement::BrokenBucket, 4, 6 },
				{ ETNBeachElement::ShellGate, 3, 5 },
			};
			for (const FQuota& Quota : Quotas)
			{
				const FElementRule Rule = RuleOf(Quota.Element);
				const int32 Wanted = Rng.RangeInt(Quota.Min, Quota.Max);
				auto TryAt = [this, &Quota, &Rule](double Progress)
				{
					FItem Item = Make(Quota.Element, FVector2D::ZeroVector, EItemRole::Fill);
					const double Reach = Item.Radius + FMath::Abs(Item.Axis().Y) * Item.HalfLength;
					const double TrapX = XOfProgress(Progress);
					Item.Pos = FVector2D(TrapX, PickY(Rng, Rule.SideBias, SideLimit(Quota.Element) - Reach));
					return TryAddWithArc(Item, ItemPad);
				};
				int32 Made = 0;
				for (int32 Slot = 0; Slot < Wanted; ++Slot)
				{
					for (int32 Try = 0; Try < 8; ++Try)
					{
						const double SlotT = FMath::Max(Rule.MinT, (Slot + Rng.Range(0.05, 0.95)) / Wanted);
						if (TryAt(SlotT))
						{
							++Made;
							break;
						}
					}
				}
				for (int32 Try = 0; Try < 12 * Wanted && Made < Wanted; ++Try)
				{
					const double AnyT = Rng.Range(Rule.MinT, Rule.MaxT);
					if (TryAt(AnyT)) { ++Made; }
				}
			}
		}

		/**
		 * Catapultas garantizadas: si con todo el reparto hay menos de Minimum, los trampolines de delante de un obstáculo
		 * (castillos, crestas, pozas y filas) pasan a ser catapultas, en el mismo sitio y un poco más atrás (son más grandes):
		 * saltan lo mismo, más lejos.
		 */
		void EnsureCatapults(int32 Minimum)
		{
			const int32 CatapultIndex = static_cast<int32>(ETNBeachElement::Catapult);
			const int32 NumBefore = Out.Items.Num();
			for (int32 i = 0; i < NumBefore && Counts[CatapultIndex] < Minimum; ++i)
			{
				if (!Alive[i] || Out.Items[i].Element != ETNBeachElement::Trampoline || Out.Items[i].Role != EItemRole::Launcher) { continue; }
				const FItem Tramp = Out.Items[i];
				FItem Cat = Make(ETNBeachElement::Catapult, FVector2D::ZeroVector, EItemRole::Launcher, 1.0);
				Cat.Yaw = Tramp.Yaw;
				const double Back = Cat.Radius - Tramp.Radius + 50.0;
				Cat.Pos = Tramp.Pos - Cat.Axis() * Back;
				Remove(i);
				if (TryAdd(Cat, ItemPad))
				{
					for (FInterestPoint& Point : Out.Interest)
					{
						if (Point.Source == ETNBeachElement::Trampoline && Point.Pos.Equals(Tramp.Pos, 0.01))
						{
							Point.Pos = Cat.Pos;
							Point.Source = ETNBeachElement::Catapult;
						}
					}
				}
				else
				{
					// No cabe: vuelve el trampolín (cabía antes y no cierra el paso).
					TryAdd(Tramp, ItemPad);
				}
			}
		}

		/** A veces, una hilera de pasarelas y caminitos de palos hacia el mar (guía visual), rodeando lo que haya. */
		void PlaceGuidePath()
		{
			if (!Rng.Chance(0.6)) { return; }
			const double StartX = XOfProgress(Rng.Range(0.03, 0.45));
			FVector2D Cursor(StartX, Rng.Range(-0.5, 0.5) * HalfWidth);
			double Heading = Rng.Range(-10.0, 10.0);
			const double Total = Rng.Range(15000.0, 35000.0);
			double Done = 0.0;
			for (int32 Guard = 0; Guard < 40 && Done < Total && Cursor.X < ItemsEndX - 3000.0; ++Guard)
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
				if (TryAdd(Seg, ItemPad)) { ++Out.NumGuidePaths; }
				Cursor += Seg.Axis() * Step;
				Done += Step;
				// Se tuerce poco y vuelve hacia el centro si se acerca a la selva.
				Heading = FMath::Clamp(Heading + Rng.Range(-9.0, 9.0) - 0.0006 * Cursor.Y, -22.0, 22.0);
			}
		}

		// ── Relleno ──

		/** Ocupación (área de lo que ocupa) de lo que tiene su centro en [BX0, BX1), sin pasos de quads, pasarelas guía ni gaviotas. */
		double BandArea(double BX0, double BX1) const
		{
			double Area = 0.0;
			for (int32 i = 0; i < Out.Items.Num(); ++i)
			{
				const FItem& Item = Out.Items[i];
				if (!Alive[i] || Item.bOverlay || Item.Role == EItemRole::QuadLane || Item.Role == EItemRole::GuidePath) { continue; }
				if (Item.Pos.X >= BX0 && Item.Pos.X < BX1) { Area += Item.CoreArea(); }
			}
			return Area;
		}

		/**
		 * Un elemento de la categoría Cat en la banda [BX0, BX1) (o en Where, si se da), con su corrillo a veces. Devuelve
		 * cuántos ha puesto y, en OutArea, lo que ocupan.
		 */
		int32 PlaceFillItem(ETNBeachCategory Cat, double BX0, double BX1, double T, double* OutArea, const FVector2D* Where = nullptr, EItemRole Role = EItemRole::Fill)
		{
			if (OutArea) { *OutArea = 0.0; }
			const ETNBeachElement E = PickElement(Rng, Cat, T, Counts);
			if (E == ETNBeachElement::Count) { return 0; }
			const FElementRule Rule = RuleOf(E);
			FItem Item = Make(E, FVector2D::ZeroVector, Role);
			const double Reach = Item.Radius + FMath::Abs(Item.Axis().Y) * Item.HalfLength;
			if (Where)
			{
				Item.Pos = *Where;
			}
			else
			{
				const double FillX = Rng.Range(BX0, BX1);
				Item.Pos = FVector2D(FillX, PickY(Rng, Rule.SideBias, SideLimit(E) - Reach));
			}
			if (!TryAddWithArc(Item, ItemPad)) { return 0; }
			int32 Placed = 1;
			double Area = Item.CoreArea();
			if (Rule.ClusterChance > 0.0 && Rng.Chance(Rule.ClusterChance))
			{
				// Corrillo: 1-ClusterMax más iguales alrededor (cocos, conchas, algas, cangrejos, minas...).
				const int32 More = Rng.RangeInt(1, FMath::Max(1, Rule.ClusterMax));
				for (int32 m = 0; m < More && Counts[static_cast<int32>(E)] < Rule.MaxPerRound; ++m)
				{
					FItem Mate = MakeItem(Rng, E, Rule, Rng.Range(Rule.SizeMin, Rule.SizeMax), 0.0);
					Mate.Role = Role;
					const double Ang = Rng.Range(0.0, TNProcMap::TwoPi);
					Mate.Pos = Item.Pos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Item.Core + Mate.Core + Rng.Range(150.0, 700.0));
					if (TryAddWithArc(Mate, ItemPad * 0.5))
					{
						Area += Mate.CoreArea();
						++Placed;
					}
				}
			}
			// Delante de las plataformas y los castillos del relleno, a veces un trampolín.
			if ((E == ETNBeachElement::WobblyPlatform || E == ETNBeachElement::MovingPlatform || E == ETNBeachElement::SandCastleHuge) && Rng.Chance(0.4))
			{
				if (PlaceLauncherBefore(Item.Pos, Item.Radius, ETNBeachElement::Trampoline)) { ++Placed; }
			}
			if (OutArea) { *OutArea = Area; }
			return Placed;
		}

		/** Relleno por bandas de 50 m: un cupo de enemigos y decorado y trampas hasta la ocupación de la banda, sin cerrar nunca el paso. */
		void FillBands()
		{
			for (double BX = ItemsStartX; BX < ItemsEndX - 1.0; BX += BandLength)
			{
				const double BX1 = FMath::Min(BX + BandLength, ItemsEndX);
				const double T = ProgressOfX(0.5 * (BX + BX1));
				const int32 BandStart = Out.Items.Num();
				// Enemigos: un cupo por banda, más hacia el mar (los cangrejos, a veces en grupo, cuentan como uno).
				const int32 Quota = FMath::FloorToInt32(1.2 + 2.2 * T + Rng.Unit());
				int32 EnemiesPlaced = 0;
				for (int32 Attempt = 0; Attempt < 40 && EnemiesPlaced < Quota; ++Attempt)
				{
					if (PlaceFillItem(ETNBeachCategory::Enemy, BX, BX1, T, nullptr) > 0) { ++EnemiesPlaced; }
				}
				// Decorado y trampas hasta la ocupación de la banda.
				const double Target = BandCoverage(T) * (BX1 - BX) * 2.0 * (HalfWidth - SideMargin);
				double Area = BandArea(BX, BX1);
				int32 Added = 0;
				for (int32 Attempt = 0; Attempt < 640 && Area < Target && Added < 110; ++Attempt)
				{
					const double WD = 0.58 - 0.12 * T;
					const double WT = 0.42 + 0.12 * T;
					const ETNBeachCategory Cat = Rng.Unit() * (WD + WT) < WD ? ETNBeachCategory::Decor : ETNBeachCategory::Trap;
					double AddedArea = 0.0;
					Added += PlaceFillItem(Cat, BX, BX1, T, &AddedArea);
					Area += AddedArea;
				}
				RestorePassage(BandStart);
			}
		}

		/**
		 * Algo pequeño (7 m de huella como mucho) de cualquier categoría para tapar una línea recta: basura, algas, minas...
		 * bOpenOnly: solo lo que no cierra el paso (algas, minas, trampas pequeñas).
		 */
		ETNBeachElement PickPlugElement(double T, bool bOpenOnly)
		{
			const int32 Num = static_cast<int32>(ETNBeachElement::Count);
			double Total = 0.0;
			TArray<double> W;
			W.Init(0.0, Num);
			for (int32 i = 0; i < Num; ++i)
			{
				const ETNBeachElement E = static_cast<ETNBeachElement>(i);
				const FElementRule Rule = RuleOf(E);
				if (Rule.bSpecial || Rule.bOverlay || Rule.ExtentMax > 0.0 || TNBeach::FootprintRadius(E) > 700.0 || T < Rule.MinT || T > Rule.MaxT
					|| Counts[i] >= Rule.MaxPerRound || (bOpenOnly && Rule.bBlocking))
				{
					continue;
				}
				W[i] = Rule.Weight;
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
			return ETNBeachElement::Count;
		}

		/**
		 * Ninguna línea recta hacia el mar libre de más de MaxStraightRun: cada 4 m a lo ancho se mira la fila de casillas de
		 * 2 m a lo largo (lo que ocupa cada elemento y los pasos de quads, las pozas, las trincheras y lo alto de las crestas)
		 * y, donde hay un tramo libre más largo, se pone en él algo pequeño (basura, algas, una mina, un trampolín...). Lo que
		 * cierra el paso se queda solo si sigue habiendo paso en toda la playa (la ventana de TryAdd no basta: las líneas
		 * largas van junto a la selva, por los huecos de las filas); si no, se prueba con algo que no lo cierra.
		 */
		void PlugStraightLines()
		{
			constexpr double Cell = 200.0;
			const double Usable = SideReach - 100.0;
			const int32 NXc = FMath::CeilToInt32((ItemsEndX - ItemsStartX) / Cell);
			const int32 NYc = FMath::CeilToInt32(2.0 * Usable / Cell);
			TArray<uint8> Occ;
			Occ.Init(0, NXc * NYc);
			auto MarkDisc = [&Occ, NXc, NYc, Usable](const FVector2D& A, const FVector2D& B, double R)
			{
				const int32 IX0 = FMath::Max(0, FMath::FloorToInt32((FMath::Min(A.X, B.X) - R - ItemsStartX) / Cell));
				const int32 IX1 = FMath::Min(NXc - 1, FMath::CeilToInt32((FMath::Max(A.X, B.X) + R - ItemsStartX) / Cell));
				const int32 IY0 = FMath::Max(0, FMath::FloorToInt32((FMath::Min(A.Y, B.Y) - R + Usable) / Cell));
				const int32 IY1 = FMath::Min(NYc - 1, FMath::CeilToInt32((FMath::Max(A.Y, B.Y) + R + Usable) / Cell));
				for (int32 IY = IY0; IY <= IY1; ++IY)
				{
					for (int32 IX = IX0; IX <= IX1; ++IX)
					{
						const FVector2D C(ItemsStartX + (IX + 0.5) * Cell, -Usable + (IY + 0.5) * Cell);
						double T = 0.0;
						if (TNProcMap::DistPointSegment(C, A, B, T) <= R) { Occ[IY * NXc + IX] = 1; }
					}
				}
			};
			// Lo que ya obliga a desviarse o a trepar: lo que ocupa cada elemento (y los pasos de quads), las pozas con su
			// orilla, las trincheras y lo alto de las crestas.
			for (int32 i = 0; i < Out.Items.Num(); ++i)
			{
				const FItem& Item = Out.Items[i];
				if (!Alive[i] || Item.bOverlay) { continue; }
				MarkDisc(Item.EndA(), Item.EndB(), Item.Core);
			}
			MarkTerrainObstacles(MarkDisc);
			const int32 MaxCells = FMath::CeilToInt32(MaxStraightRun / Cell);
			int32 Plugs = 0;
			for (int32 IY = 1; IY < NYc; IY += 2)
			{
				int32 Run = 0;
				for (int32 IX = 0; IX < NXc; ++IX)
				{
					if (Occ[IY * NXc + IX])
					{
						Run = 0;
						continue;
					}
					if (++Run < MaxCells) { continue; }
					const double LineY = -Usable + (IY + 0.5) * Cell;
					// Lo que queda libre detrás de lo que se ponga cuenta ya para el tramo siguiente.
					int32 RunAfter = Run / 2;
					bool bOpenOnly = false;
					for (int32 Try = 0; Try < 16; ++Try)
					{
						// Algo pequeño (basura, algas, minas, un trampolín...) en cualquier punto del tramo, pegado a lo que ya hay
						// si hace falta: cabe también junto a la selva y entre lo que hay.
						const double Along = Rng.Range(0.2, 0.8);
						const double PlugX = ItemsStartX + (IX - Run + 1 + Along * Run) * Cell;
						const ETNBeachElement E = PickPlugElement(ProgressOfX(PlugX), bOpenOnly);
						if (E == ETNBeachElement::Count) { break; }
						const double JitterY = Rng.Range(-120.0, 120.0);
						FItem Item = Make(E, FVector2D(PlugX, 0.0), EItemRole::Plug);
						const double Limit = SideLimit(E) - (Item.Radius + FMath::Abs(Item.Axis().Y) * Item.HalfLength) - 1.0;
						Item.Pos.Y = FMath::Clamp(LineY + JitterY, -Limit, Limit);
						if (!TryAddWithArc(Item, 0.0)) { continue; }
						if (Item.bBlocking && !Grid.IsConnected())
						{
							Remove(Out.Items.Num() - 1);
							bOpenOnly = true;
							continue;
						}
						MarkDisc(Item.EndA(), Item.EndB(), Item.Core);
						++Plugs;
						RunAfter = FMath::Max(0, IX - FMath::CeilToInt32((FMath::Max(Item.EndA().X, Item.EndB().X) + Item.Core - ItemsStartX) / Cell));
						break;
					}
					Run = RunAfter;
				}
			}
			Out.NumPlugs = Plugs;
		}

		// ── Puntos del terreno fijo y cierre ──

		/** Cimas de las crestas, tramos de trinchera, caminos alternativos (corredores separados) y pozas que cortan un corredor. */
		void AddTerrainInterest()
		{
			for (const FRidge& Ridge : Ridges())
			{
				double BestU = 0.0;
				double BestH = -1.0;
				for (double U = -0.8; U <= 0.801; U += 0.2)
				{
					const double H = RidgeCrestHeight(Ridge, U * Ridge.HalfLength);
					if (H > BestH)
					{
						BestH = H;
						BestU = U * Ridge.HalfLength;
					}
				}
				FInterestPoint Top;
				Top.Kind = EInterestKind::Summit;
				Top.Pos = RidgeCrestPoint(Ridge, BestU);
				Top.To = Top.Pos;
				Out.Interest.Add(Top);
			}
			for (const FTrench& Trench : Trenches())
			{
				for (int32 k = 0; k + 1 < Trench.Points.Num(); k += 2)
				{
					FInterestPoint Point;
					Point.Kind = EInterestKind::Trench;
					Point.Pos = 0.5 * (Trench.Points[k] + Trench.Points[k + 1]);
					Point.To = Point.Pos;
					Out.Interest.Add(Point);
				}
			}
			for (double X = ItemsStartX; X < ItemsEndX; X += 6000.0)
			{
				if (CorridorSplit(X) < 0.7) { continue; }
				for (int32 K = 0; K < NumCorridors; ++K)
				{
					const FCorridorSample C = CorridorAt(K, X);
					if (C.Weight < 0.7) { continue; }
					FInterestPoint Point;
					Point.Kind = EInterestKind::Detour;
					Point.Pos = FVector2D(X, C.Y);
					Point.To = Point.Pos;
					Out.Interest.Add(Point);
				}
			}
			for (const FPool& Pool : Pools())
			{
				if (Pool.Corridor < 0) { continue; }
				FInterestPoint Swim;
				Swim.Kind = EInterestKind::Shortcut;
				Swim.Pos = Pool.Center - FVector2D(Pool.OuterR(), 0.0);
				Swim.To = Pool.Center + FVector2D(Pool.OuterR(), 0.0);
				Out.Interest.Add(Swim);
			}
		}

		void Finish()
		{
			// Fuera lo quitado (el orden se conserva: el mismo en todas las máquinas).
			TArray<FItem> Kept;
			Kept.Reserve(Out.Items.Num());
			for (int32 i = 0; i < Out.Items.Num(); ++i)
			{
				if (Alive[i]) { Kept.Add(Out.Items[i]); }
			}
			Out.Items = MoveTemp(Kept);
			for (const FItem& Item : Out.Items)
			{
				switch (TNBeach::CategoryOf(Item.Element))
				{
					case ETNBeachCategory::Decor: ++Out.NumDecor; break;
					case ETNBeachCategory::Trap: ++Out.NumTraps; break;
					default: ++Out.NumEnemies; break;
				}
				switch (Item.Element)
				{
					case ETNBeachElement::QuadLane: ++Out.NumQuadLanes; break;
					case ETNBeachElement::GullZone: ++Out.NumGullZones; break;
					case ETNBeachElement::GiantCrab: ++Out.NumCrabs; break;
					case ETNBeachElement::SeaUrchin: ++Out.NumUrchins; break;
					case ETNBeachElement::Lizard: ++Out.NumLizards; break;
					case ETNBeachElement::SandDungeon: ++Out.NumDungeons; break;
					default: break;
				}
				if (Item.Role == EItemRole::Military) { ++Out.NumMilitary; }
				if (Item.Role == EItemRole::Castle) { ++Out.NumCastles; }
				if (RuleOf(Item.Element).bLauncher) { ++Out.NumLaunchers; }
				if (HasSeat(Item)) { Out.Stamps.Add(MakeStamp(Item)); }
			}
			// Ocupación de cada banda.
			const int32 Bands = NumBands();
			Out.BandCover.Init(0.f, Bands);
			const double BandSurface = BandLength * 2.0 * (HalfWidth - SideMargin);
			for (const FItem& Item : Out.Items)
			{
				if (Item.bOverlay || Item.Role == EItemRole::QuadLane || Item.Role == EItemRole::GuidePath) { continue; }
				const int32 Band = FMath::Clamp(FMath::FloorToInt32((Item.Pos.X - ItemsStartX) / BandLength), 0, Bands - 1);
				Out.BandCover[Band] += static_cast<float>(Item.CoreArea() / BandSurface);
			}
			double Sum = 0.0;
			double First = 0.0;
			const int32 FirstBands = FMath::Max(1, Bands / 3);
			for (int32 b = 0; b < Bands; ++b)
			{
				Sum += Out.BandCover[b];
				if (b < FirstBands) { First += Out.BandCover[b]; }
			}
			Out.CoverMean = Bands > 0 ? Sum / Bands : 0.0;
			Out.CoverFirstThird = First / FirstBands;
			for (FInterestPoint& Point : Out.Interest) { Point.Z = SandZ(Point.Pos.X, Point.Pos.Y); }
			Out.bPassageOk = Grid.IsConnected();
		}
	};

	/**
	 * Reparto de la ronda con Seed (determinista: el mismo en el servidor y en cada cliente). Orden: castillos con salas
	 * (el principal con sus alas), pasos de quads (de lado a lado: antes de que los castillos les quiten sitio), castillos
	 * enormes, gaviotas, la tropa de las trincheras, filas que obligan a zigzaguear, trampas destacadas (las grandes, antes
	 * de que no quepan), puestos militares, rincones, lanzadores, pasarela guía (a veces), relleno por bandas, catapultas
	 * que falten y tapones de las líneas rectas. Lo que cierra el paso se comprueba al ponerlo (en una ventana de ±60 m;
	 * los tapones, en toda la playa) y, en toda la playa, tras cada fila, cada banda y cada pasada que cierra.
	 */
	inline void GenerateRound(int32 Seed, FRoundLayout& Out)
	{
		Out = FRoundLayout();
		Out.Seed = Seed;
		FBuilder Builder(Out, Seed);
		Builder.PlaceDungeons();
		Builder.PlaceQuadLanes();
		Builder.PlaceCastles();
		Builder.PlaceGullZones();
		Builder.PlaceTrenchGarrison();
		Builder.RestorePassage(0);
		Builder.PlaceBarrierRows();
		Builder.PlaceFeaturedTraps();
		Builder.PlaceMilitaryPosts();
		Builder.RestorePassage(0);
		Builder.PlaceNooks();
		Builder.RestorePassage(0);
		Builder.PlaceLaunchers();
		Builder.PlaceGuidePath();
		Builder.FillBands();
		Builder.EnsureCatapults(MinCatapults);
		Builder.PlugStraightLines();
		Builder.RestorePassage(0);
		Builder.AddTerrainInterest();
		Builder.Finish();
	}
}
