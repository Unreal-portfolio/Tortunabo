#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "World/ProcMap/TN_ProcMapMath.h"

/**
 * Modelo de datos PURO del mapa procedural (sin UObject). Lo rellena
 * TNProcMap::GenerateLayout (TN_ProcMapGenerate.h) y lo consumen el terreno
 * (TN_ProcMapTerrain.h) y el actor ATN_ProcMapGenerator.
 *
 * Espacio del mapa (local al generador, en cm):
 *   X ∈ [0, WorldSize]  → ancho
 *   Y ∈ [0, WorldSize]  → avance: la salida está al sur (Y≈0) y la playa y el
 *                          mar abierto al norte (Y≈WorldSize y más allá)
 *   Z = 0               → nivel del mar (agua de todo el mapa)
 */

namespace TNProcMap
{
	constexpr int32 NumBiomes = static_cast<int32>(ETNProcBiome::Count);

	inline int32 BiomeIndex(ETNProcBiome B) { return static_cast<int32>(B); }
	inline ETNProcBiome BiomeFromIndex(int32 I) { return static_cast<ETNProcBiome>(FMath::Clamp(I, 0, NumBiomes - 1)); }

	/** Biomas cuyo camino va sobre agua (isletas / pasarelas) en vez de suelo continuo. */
	inline bool IsWetBiome(ETNProcBiome B) { return B == ETNProcBiome::Water || B == ETNProcBiome::Mangrove; }

	// ─────────────────────────────────────────────────────────────────────────
	// Flags de muestra de camino
	// ─────────────────────────────────────────────────────────────────────────

	namespace PathFlags
	{
		constexpr uint32 None       = 0;
		constexpr uint32 Elevated   = 1u << 0;   ///< Tablero de puente colosal (malla, no terreno).
		constexpr uint32 Colossal   = 1u << 1;   ///< Cima de mesa colosal (terreno alto).
		constexpr uint32 Tunnel     = 1u << 2;   ///< Tramo bajo la mesa: cueva.
		constexpr uint32 TowerTop   = 1u << 3;   ///< Plataforma alta de torre / extremo de mesa.
		constexpr uint32 Slide      = 1u << 4;   ///< Tobogán-cascada de bajada (no caminable).
		constexpr uint32 GeyserBase = 1u << 5;   ///< Géiser al pie de una subida.
		constexpr uint32 Islet      = 1u << 6;   ///< Sobre agua: el suelo lo ponen las isletas.
		constexpr uint32 Boardwalk  = 1u << 7;   ///< Manglar: pasarela de tablones.
		constexpr uint32 Gap        = 1u << 8;   ///< Dentro de un hueco de salto.
		constexpr uint32 Start      = 1u << 9;
		constexpr uint32 End        = 1u << 10;
		constexpr uint32 UnderTower = 1u << 11;  ///< Lo ocupa la base de una torre: no se talla.
		constexpr uint32 Portal     = 1u << 12;  ///< Muestra en frontera de módulo.
		constexpr uint32 Lane       = 1u << 13;  ///< Tramo con carriles 2vs2.
		constexpr uint32 Shore      = 1u << 14;  ///< Bajada final al mar.
		constexpr uint32 RiverCross = 1u << 15;  ///< Cruza el río (va en puente).
		constexpr uint32 CliffUp    = 1u << 16;  ///< Primera muestra tras un escalón de subida.
		constexpr uint32 Junction   = 1u << 17;  ///< Junto a la unión de una rama o senda: sin huecos, obstáculos ni peligros.

		/** Muestras que no forman suelo de terreno a la altura del camino. */
		constexpr uint32 NotTerrain = Elevated | Islet | Boardwalk;
		/** Muestras donde no se colocan huecos, ramas ni peligros. */
		constexpr uint32 Special = Elevated | Colossal | Tunnel | TowerTop | Slide | GeyserBase | Islet | Boardwalk
			| Gap | Start | End | UnderTower | Portal | Shore | RiverCross | CliffUp | Junction;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Parámetros
	// ─────────────────────────────────────────────────────────────────────────

	/** Todo en cm salvo donde se indica. Rellenado desde FTNProcMapProfile. */
	struct FGenParams
	{
		uint32 Seed = 1337;

		int32 GridSize = 6;
		double ModuleSize = 40000.0;
		/** Resolución del raster de módulos y biomas. */
		double CellSize = 400.0;

		/** Fracción de módulos únicos que recorre el camino principal. */
		double Coverage = 0.78;
		int32 NumCrossings = 2;
		int32 NumBranches = 12;
		/** Bifurcaciones en carriles paralelos con puzles (2vs2). */
		int32 NumLanes = 0;
		int32 BranchMaxModules = 3;

		double PathWidthMin = 400.0;
		double PathWidthMax = 3500.0;
		double PortalWidthMin = 800.0;
		double PortalWidthMax = 3000.0;
		/** Tramos estrechos: probabilidad por tramo de ~100 m. */
		double NarrowChance = 0.22;
		/** Longitud del camino dentro de un módulo / distancia recta entre portales. */
		double Sinuosity = 1.8;
		double SampleSpacing = 400.0;

		/** Huecos de salto (salto 2 m corriendo, dive 4 m). */
		double GapMin = 130.0;
		double GapMax = 390.0;
		double GapsPerKm = 3.0;
		double IsletGapMin = 130.0;
		double IsletGapMax = 330.0;
		/** Desnivel máximo subible de un salto (1,5 m) con margen. */
		double MaxStepUp = 120.0;

		double MaxPathSlope = 0.2;
		/** Desnivel entre módulos por encima del cual hay géiser (subida) o tobogán (bajada). */
		double SmoothTransitionMax = 900.0;
		double SlideAngleDeg = 55.0;
		double ColossalHeightMin = 4000.0;
		double ColossalHeightMax = 5500.0;
		double TowerRadius = 1100.0;

		ETNProcEmptyModuleMode EmptyMode = ETNProcEmptyModuleMode::Mixed;
		/** 0 = automático según el tamaño del grid. */
		int32 NumBiomeRegions = 0;
		bool bRiver = false;
		bool bForceFinalBeach = true;

		/** Cada cuántos portales del camino hay una pila de huevos de respawn. */
		int32 EggNestEveryNPortals = 2;
		double StartClearingRadius = 2500.0;

		/** Muros naturales del borde (sur, este, oeste). */
		double WallInsetMin = 2500.0;
		double WallInsetMax = 6000.0;
		double WallHeight = 5500.0;
		/** Distancia mínima del camino al borde del mapa. */
		double MapEdgeClearance = 9000.0;
		/** Distancia de la costa al borde norte del mapa (hacia dentro). */
		double CoastInset = 6000.0;

		/** Tamaño de un escalón de la lógica de dificultad [0,1] (0 fácil, 1 difícil). */
		double Difficulty01 = 0.5;
	};

	// ─────────────────────────────────────────────────────────────────────────
	// Resultado
	// ─────────────────────────────────────────────────────────────────────────

	struct FModule
	{
		int32 Id = INDEX_NONE;
		FIntPoint GridCoord = FIntPoint::ZeroValue;
		FVector2D Seed = FVector2D::ZeroVector;
		FVector2D Centroid = FVector2D::ZeroVector;
		int32 CellCount = 0;
		ETNProcBiome Biome = ETNProcBiome::Jungle;
		int32 Region = INDEX_NONE;
		/** Altura base del módulo (cm sobre el mar). */
		double Level = 0.0;
		int32 VisitCount = 0;
		/** Qué es este módulo si el camino no pasa por él. */
		ETNProcEmptyModuleMode EmptyKind = ETNProcEmptyModuleMode::Elevated;
		bool bHasBranch = false;

		TArray<int32> Neighbors;
		/** Celdas de frontera compartida con cada vecino (mismo índice que Neighbors). */
		TArray<int32> SharedBorder;
		/** Punto medio aproximado de la frontera con cada vecino. */
		TArray<FVector2D> BorderMid;
	};

	struct FPortal
	{
		int32 From = INDEX_NONE;
		int32 To = INDEX_NONE;
		FVector2D Point = FVector2D::ZeroVector;
		/** Dirección de cruce From → To. */
		FVector2D Dir = FVector2D(0.0, 1.0);
		double Width = 1500.0;
	};

	struct FRouteStep
	{
		int32 Module = INDEX_NONE;
		/** Segunda pasada por un módulo ya recorrido (cruce colosal). */
		bool bCrossingPass = false;
		int32 CrossingIndex = INDEX_NONE;
		/** En un cruce, si esta pasada es la alta. */
		bool bHigh = false;
		int32 EntryPortal = INDEX_NONE;
		int32 ExitPortal = INDEX_NONE;
		int32 FirstSample = INDEX_NONE;
		int32 LastSample = INDEX_NONE;
	};

	struct FCrossing
	{
		int32 Module = INDEX_NONE;
		int32 FirstPassStep = INDEX_NONE;
		int32 SecondPassStep = INDEX_NONE;
		int32 HighStep = INDEX_NONE;
		int32 LowStep = INDEX_NONE;
		ETNProcCrossingType Type = ETNProcCrossingType::Bridge;
		/** Cota del tablero / cima de la mesa. */
		double TopZ = 0.0;
		FVector2D CrossPoint = FVector2D::ZeroVector;
	};

	struct FPathSample
	{
		FVector2D P = FVector2D::ZeroVector;
		/** Tangente unitaria en el plano. */
		FVector2D Dir = FVector2D(0.0, 1.0);
		/** Cota del suelo caminable en esta muestra. */
		double Z = 0.0;
		double Width = 1500.0;
		/** Distancia acumulada a lo largo del camino. */
		double S = 0.0;
		int32 Step = INDEX_NONE;
		int32 Module = INDEX_NONE;
		ETNProcBiome Biome = ETNProcBiome::Jungle;
		uint32 Flags = PathFlags::None;
	};

	enum class EBranchKind : uint8
	{
		/** Alternativa corta: más riesgo (huecos, peligros). */
		Risky,
		/** Alternativa tranquila: más larga, con recompensas. */
		Scenic,
		/** Carril 2vs2: paralelo al principal, con puzle de lanzamiento. */
		Lane,
		/** Ruta alta: sube poco a poco por encima del cauce y baja en tobogán. */
		High,
		/** Rodeo corto alrededor de un peñasco. */
		Bypass,
		/** Senda larga que une dos zonas del principal por el terreno libre entre ellas. */
		Trail,
		/** Enlace que teje la red: sale o llega a una senda (u otra rama) en vez de al principal. */
		Link
	};

	/**
	 * Rama, senda o enlace. ForkSample/RejoinSample son muestras del principal: donde sale y
	 * donde vuelve, o, si un extremo está en otra rama (FromBranch/ToBranch), la muestra del
	 * principal de igual progreso (para las etiquetas de progreso del terreno y la carrera).
	 */
	struct FBranch
	{
		TArray<FPathSample> Samples;
		int32 ForkSample = INDEX_NONE;
		int32 RejoinSample = INDEX_NONE;
		EBranchKind Kind = EBranchKind::Scenic;
		int32 Side = 1;
		/** Rama de la que sale (INDEX_NONE = el principal) y su muestra. */
		int32 FromBranch = INDEX_NONE;
		int32 FromSample = INDEX_NONE;
		/** Rama a la que llega (INDEX_NONE = el principal) y su muestra. */
		int32 ToBranch = INDEX_NONE;
		int32 ToSample = INDEX_NONE;
	};

	enum class EFeature : uint8
	{
		StartArea,
		/**
		 * Meta: Location = centro de la línea (Z = fondo), Target = orilla del agua en el eje, Width = ancho
		 * de la boca en la línea, Height = ancho al fondo del volumen, Length = fondo del volumen, Radius =
		 * radio interior del arco, Aux = semilla.
		 */
		Finish,
		Geyser,        ///< Location = base, Target = aterrizaje.
		SlideZone,     ///< Polilínea de tobogán: PathIndex = primera muestra, Aux = última.
		Tower,         ///< Pilar colosal de terreno. Radius, Height = cota de la cima.
		Mesa,          ///< Mesa colosal: Aux = índice de cruce.
		Deck,          ///< Tablero de puente colosal: Aux = índice de cruce.
		TunnelRoof,    ///< Techo de la cueva sobre el tramo bajo.
		Gap,           ///< Hueco de salto: Location = centro, Dir, Length = hueco, Width = ancho del camino.
		Islet,         ///< Polygon = contorno, Location.Z = cota de la cima.
		Boardwalk,     ///< Tramo de pasarela: PathIndex..Aux sobre el camino.
		EggNest,       ///< Pila de huevos de respawn. Aux = orden en el camino.
		ThrowWall,     ///< Muro para lanzar al compañero (2vs2). Height = altura del muro.
		SabotageGate,  ///< Compuerta que se levanta en un carril cuando el otro pulsa su interruptor.
		SabotageSwitch,///< Interruptor que fastidia al otro carril. Aux = índice de la compuerta que activa.
		RiverBridge,   ///< Puente sobre el río: Location = centro, Dir, Length = luz.
		LavaPool,
		Island,        ///< Isla decorativa en lagunas.
		/** Cono volcánico: Location = centro (Z = base), Radius = base, Height = altura, Width = Ø del cráter, Length = hondura del cráter. */
		Volcano,
		/** Pilar de roca bajo el tablero de un puente colosal: Radius, Height = cota de su cima, Aux = cruce, PathIndex = muestra. */
		DeckPillar,
		/** Peñasco en el camino: Location = centro (Z = suelo), Radius, Height, Aux = semilla de forma. */
		Boulder,
		/** Aguja o mogote de roca en una explanada: Location, Radius, Height, Aux = semilla. */
		RockSpire,
		/** Tronco caído que se salta: Location = centro, Dir = eje, Length, Radius. */
		Log,
		/** Árbol gigante tipo secuoya con raíces zancudas (manglar): Location (XY), Radius = tronco, Height. */
		GiantTree,
		/**
		 * Puerta de una muralla: Location = centro en el eje del muro a la cota del suelo, Dir = eje del
		 * muro, Width = luz a lo largo del muro, Length = grueso en la base, Radius = radio del arco,
		 * Height = cota del adarve, Aux = cruce, PathIndex = muestra del tramo bajo, Aux2 = del alto.
		 */
		Gate,
		/**
		 * Formación temática (Aux = EFormation, Aux2 = semilla). Arcos sobre el camino: Location = centro
		 * a la cota del suelo, Dir = el camino, Width = luz entre pies, Height = altura libre, Length =
		 * fondo. En explanadas: Location sobre el suelo del camino, Radius = huella, Height = alto. Hitos
		 * lejanos: Location (XY), Radius = base, Height = alto (se asientan en el terreno al construirlos).
		 */
		Formation,
		/**
		 * Cueva: el camino principal atraviesa una loma por un túnel de roca. PathIndex..Aux = primera y
		 * última muestra, Radius = grueso del techo, Height = factor de altura libre, Aux2 = semilla.
		 */
		Cave,
		/**
		 * Obstáculo de objetos en el camino (Aux = EPathProp, Aux2 = semilla): Location = centro (Z =
		 * suelo), Dir = orientación, Radius = semiancho de la huella, Height = alto, Length = largo de los
		 * alargados (barca, vagoneta, valla, fila de conos).
		 */
		PathProp,
		/**
		 * Plaza redonda a media altura de un puente colosal de piedra o de hierro: Location = centro sobre
		 * el tablero (Z = cota del tablero), Dir = eje del tablero, Radius = radio, Aux = cruce, Aux2 =
		 * semilla (su bit 0 dice a qué lado va la atalaya), Height = 1 si hay pila debajo, Length = su S.
		 */
		BridgePlaza,
		/** Recompensa de puntos colocada en un sitio concreto (cima de una atalaya o de un parkour): Location. */
		BonusPickup,
		/** Medusa saltarina colocada en un sitio concreto (atajo para subir): Location (Z = suelo). */
		Bouncer,
		/**
		 * Torre de escalada junto al camino, del estilo del bioma: Location = centro de su base (Z = suelo),
		 * Dir = sus escalones bajan hacia -Dir, Height = alto de la cima (300 o 400), Aux2 = semilla. La
		 * recompensa (BonusPickup) va en lo alto y la medusa (Bouncer) al pie de su cara de +Dir.
		 */
		ClimbTower,
		Count
	};

	/** Obstáculos de objetos del camino, por bioma (Aux de EFeature::PathProp). */
	enum class EPathProp : int32
	{
		CrateStack, BarrelGroup, Barricade, HayBales, Sandcastle, Rowboat, BeachSet, Totem, RuinColumn,
		GiantMushrooms, SkullRock, PotteryJars, CrystalSpikes, Cairn, MineCart, CrabTraps, MarketStall, ConeLine,
		Count
	};

	/** Tipo de formación temática (Aux de EFeature::Formation). */
	enum class EFormation : int32
	{
		// Arcos que cruzan el camino (se pasa por debajo).
		StoneArch,      ///< Arco natural de roca (arenisca, granito, musgo, obsidiana según el bioma).
		WhaleRibs,      ///< Costillar de ballena (playa).
		RootArch,       ///< Raíces gigantes en arco (manglar).
		TempleGate,     ///< Pórtico de templo en ruinas (selva).
		// En explanadas del camino, con carriles libres a los lados.
		Shipwreck,      ///< Barco varado de costado con el mástil roto (playa).
		StoneHead,      ///< Cabeza colosal de piedra (selva).
		BasaltColumns,  ///< Columnas hexagonales de basalto (volcán).
		Fumarole,       ///< Cono de fumarola con azufre (volcán).
		Hoodoo,         ///< Chimenea de hadas: roca en capas con sombrero (desierto, roca).
		BalancedRock,   ///< Peñasco en equilibrio sobre un pedestal (desierto, roca).
		Wagon,          ///< Carreta de lona abandonada (desierto).
		Cannon,         ///< Cañón antiguo con balas apiladas (guerra: acantilados).
		Sandbags,       ///< Parapeto de sacos terreros (guerra: zona humana).
		Bunker,         ///< Búnker de hormigón con tronera (guerra: zona humana).
		WatchTower,     ///< Torre de vigía de madera (guerra: zona humana).
		TankWreck,      ///< Carro de combate abandonado (guerra: zona humana).
		// Hitos lejanos del paisaje (sin colisión).
		Pyramid,        ///< Pirámide escalonada con escalinata (selva).
		Lighthouse,     ///< Faro a rayas junto a la costa (playa).
		Mesa,           ///< Mesa de techo plano con estratos (desierto).
		SeaStack,       ///< Farallón en el mar (playa, roca).
		CastleRuin,     ///< Castillo en ruinas con torre (roca).
		Windmill,       ///< Molino de viento (zona humana).
		StiltHut,       ///< Palafito de pescador (manglar, lagunas).
		Count
	};

	/** Clase de colocación de una formación. */
	inline bool IsArchFormation(EFormation K) { return K == EFormation::StoneArch || K == EFormation::WhaleRibs || K == EFormation::RootArch || K == EFormation::TempleGate; }
	inline bool IsLandmarkFormation(EFormation K) { return K >= EFormation::Pyramid; }

	/** Caja de muerte orientada (en planta según Dir): caer dentro es morir y reaparecer en los huevos. */
	struct FKillBox
	{
		FVector Center = FVector::ZeroVector;
		FVector2D Dir = FVector2D(1.0, 0.0);
		/** Semiejes: a lo largo de Dir, a lo ancho y en altura. */
		FVector Half = FVector::ZeroVector;

		bool Contains(const FVector& P) const
		{
			const FVector2D Rel(P.X - Center.X, P.Y - Center.Y);
			return FMath::Abs(FVector2D::DotProduct(Rel, Dir)) <= Half.X
				&& FMath::Abs(FVector2D::DotProduct(Rel, FVector2D(-Dir.Y, Dir.X))) <= Half.Y
				&& FMath::Abs(P.Z - Center.Z) <= Half.Z;
		}
	};

	/** Cuánto se extiende la zanja de un hueco de salto a cada lado del camino (cm). */
	constexpr double GapTrenchSide = 1500.0;

	/** Aux2 de un hueco de salto que es un río de lava (en las cámaras de las cuevas del volcán). */
	constexpr int32 GapLava = 1;

	/** Estilo de un hueco normal (Aux de EFeature::Gap): labios, postes que lo parten o tronco de equilibrio. */
	enum class EGapStyle : int32 { Lips = 0, Posts = 1, Beam = 2 };

	/**
	 * Postes de un hueco con estilo Posts: filas a lo largo que parten el hueco en saltos cortos y, en
	 * cada fila, postes cada ~4 m a lo ancho del camino (cimas de 90-130 cm, a ±30 cm de la cota).
	 */
	struct FGapPost { FVector2D P; double Radius = 50.0; double TopZ = 0.0; };

	struct FFeature;
	inline bool IsLavaGap(const FFeature& F);

	/** Cuánto se extiende a cada lado del camino la zanja de un hueco (la del río de lava no sale de la cueva). */
	inline double GapTrenchSideOf(const FFeature& F);

	struct FFeature
	{
		EFeature Type = EFeature::Count;
		FVector Location = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
		FVector2D Dir = FVector2D(0.0, 1.0);
		double Length = 0.0;
		double Width = 0.0;
		double Height = 0.0;
		double Radius = 0.0;
		/** Muestra del camino principal asociada (o de la rama si BranchIndex válido). */
		int32 PathIndex = INDEX_NONE;
		int32 BranchIndex = INDEX_NONE;
		int32 Aux = INDEX_NONE;
		int32 Aux2 = INDEX_NONE;
		ETNProcBiome Biome = ETNProcBiome::Jungle;
		TArray<FVector2D> Polygon;
	};

	inline bool IsLavaGap(const FFeature& F) { return F.Type == EFeature::Gap && F.Aux2 == GapLava; }

	inline EGapStyle GapStyleOf(const FFeature& F)
	{
		return F.Type == EFeature::Gap && !IsLavaGap(F) && F.Aux >= 0 && F.Aux <= 2 ? static_cast<EGapStyle>(F.Aux) : EGapStyle::Lips;
	}

	/** Postes de un hueco de estilo Posts (vacío en los demás); MaxJump, el salto más largo entre ellos. */
	inline void GapPostsOf(const FFeature& F, double MaxJump, TArray<FGapPost>& Out)
	{
		Out.Reset();
		if (GapStyleOf(F) != EGapStyle::Posts) { return; }
		const int32 Rows = FMath::Max(1, FMath::CeilToInt(F.Length / FMath::Max(150.0, MaxJump)) - 1);
		const double PathW = FMath::Max(300.0, F.Width - 500.0);
		const int32 Cols = FMath::Clamp(FMath::FloorToInt(PathW / 400.0), 1, 6);
		const FVector2D C(F.Location.X, F.Location.Y);
		const FVector2D N(-F.Dir.Y, F.Dir.X);
		for (int32 r = 0; r < Rows; ++r)
		{
			const double Along = -F.Length * 0.5 + F.Length * (r + 1) / (Rows + 1);
			for (int32 k = 0; k < Cols; ++k)
			{
				const uint32 H = HashCell(0x9057u ^ static_cast<uint32>(F.PathIndex), r, k);
				const double Across = (Cols == 1 ? 0.0 : -PathW * 0.5 + PathW * (k + 0.5) / Cols) + (static_cast<double>(H & 0xFF) / 255.0 - 0.5) * 60.0;
				FGapPost Post;
				Post.P = C + F.Dir * Along + N * Across;
				Post.Radius = 45.0 + 20.0 * ((H >> 8) & 0xFF) / 255.0;
				Post.TopZ = F.Location.Z - 25.0 + 60.0 * ((H >> 16) & 0xFF) / 255.0;
				Out.Add(Post);
			}
		}
	}

	/** Estilo de los puentes colosales. */
	enum class EBridgeStyle : uint8 { Rope, Stone, Trestle, Iron };

	/** Estilo según el bioma del cruce (y la semilla, para que no sean todos iguales). */
	inline EBridgeStyle BridgeStyleFor(ETNProcBiome Biome, uint32 Seed)
	{
		const bool bOdd = ((Seed * 2654435761u) >> 13) & 1u;
		switch (Biome)
		{
			case ETNProcBiome::Rocky:    return bOdd ? EBridgeStyle::Stone : EBridgeStyle::Rope;
			case ETNProcBiome::Desert:   return bOdd ? EBridgeStyle::Trestle : EBridgeStyle::Stone;
			case ETNProcBiome::Human:    return bOdd ? EBridgeStyle::Stone : EBridgeStyle::Iron;
			case ETNProcBiome::Volcanic: return EBridgeStyle::Iron;
			case ETNProcBiome::Beach:    return bOdd ? EBridgeStyle::Trestle : EBridgeStyle::Rope;
			default:                     return EBridgeStyle::Rope;
		}
	}

	/** Estilo del puente colosal del cruce C (bioma de su módulo y semilla del mapa). */
	struct FLayout;
	inline EBridgeStyle BridgeStyleOf(const FLayout& L, int32 C);

	/**
	 * Plaza de un puente: atalaya de bloques (3 m, escalones de 1 m) a un lado del eje y la medusa al pie
	 * de su otra cara; aquí se decide dónde, para que la malla y lo que aparece encima cuadren.
	 */
	namespace PlazaDims
	{
		constexpr double TowerHalf = 90.0;
		constexpr double TowerH = 300.0;
		constexpr double StepDepth = 90.0;

		/** Centro (XY) de la atalaya de la plaza F. */
		inline FVector2D TowerAt(const FFeature& F)
		{
			const double Side = (F.Aux2 & 1) ? 1.0 : -1.0;
			return FVector2D(F.Location.X, F.Location.Y) + FVector2D(-F.Dir.Y, F.Dir.X) * (Side * F.Radius * 0.52);
		}

		/** Pie de la medusa: al otro lado de la atalaya respecto a sus escalones (que bajan hacia -Dir). */
		inline FVector2D BouncerAt(const FFeature& F)
		{
			return TowerAt(F) + F.Dir * (TowerHalf + 210.0);
		}
	}
	inline double GapTrenchSideOf(const FFeature& F) { return IsLavaGap(F) ? 250.0 : GapTrenchSide; }

	/**
	 * Muralla de un cruce: el adarve (el tramo alto, del ancho del camino) entre dos parapetos que no
	 * se saltan, con almenas, y caras en talud hasta el suelo.
	 */
	namespace WallDims
	{
		/** Grosor y alto (sobre el adarve) de los parapetos; alto de las almenas sobre ellos. */
		constexpr double Parapet = 110.0;
		constexpr double ParapetH = 150.0;
		constexpr double MerlonH = 110.0;
		/** Talud de las caras (horizontal por vertical: unos 83°). */
		constexpr double Batter = 0.12;
		/** Fábrica sobre la clave del arco de la puerta, hasta el adarve. */
		constexpr double Crown = 500.0;

		/** Semigrueso de la muralla a Depth cm bajo el adarve, con WalkHalf el semiancho del adarve. */
		inline double HalfAt(double WalkHalf, double Depth) { return WalkHalf + Parapet + Batter * FMath::Max(0.0, Depth); }
	}

	/**
	 * Playa de la meta: el camino llega recto y los brazos del cauce se abren en arco (tangentes a él)
	 * hasta el mar, así la playa se va descubriendo al avanzar. La línea de meta cruza toda la boca
	 * unos metros mar adentro, bajo el arco.
	 */
	namespace FinishDims
	{
		/** La orilla del agua queda esta distancia antes de la línea de costa: el acantilado (los brazos) sigue en pie pasada la línea de meta, así nadie sale al mar sin cruzarla. */
		constexpr double WaterInset = 700.0;
		/** Distancia de la orilla a la línea de meta, ya dentro del agua (unos 3,5 m desde donde moja). */
		constexpr double LineInWater = 280.0;
		/** El camino sigue bajo el agua pasada la línea. */
		constexpr double PastLine = 1500.0;
		/** Tramo recto del ancho de llegada antes de que los brazos empiecen a abrirse. */
		constexpr double FlareStart = 1000.0;
		/** Giro de los brazos (desde la dirección del camino) al llegar a la orilla. */
		constexpr double FlareTurnDeg = 65.0;
		/** Ancho mínimo de la llegada a la playa. */
		constexpr double ArrivalWidth = 2000.0;
		/** Cota de la arena al empezar la playa y en la orilla; pendiente del fondo hasta la línea y
		 *  pasada ella (llega al fondo del mar al final del camino) y cota mínima. */
		constexpr double BeachZ = 120.0;
		constexpr double WaterEdgeZ = 5.0;
		constexpr double SeaSlope = 0.22;
		constexpr double SeaSlopePastLine = 0.3;
		constexpr double SeaFloorMin = -650.0;
		/** Distancia preferida de la llegada a la costa (la playa mide lo que queda hasta el agua). */
		constexpr double EndNear = 5500.0;
		constexpr double EndFar = 8000.0;
		/** Fondo del volumen de meta desde la línea. */
		constexpr double TriggerDepth = 3000.0;
	}

	/** Resultado completo. bValid=false si la generación no encontró un mapa. */
	struct FLayout
	{
		FGenParams Params;
		bool bValid = false;
		/** Motivo de fallo o avisos (texto ASCII para el log). */
		const char* FailReason = "";

		double WorldSize = 0.0;
		int32 RasterW = 0;
		int32 RasterH = 0;
		TArray<int16> ModuleOfCell;
		/** Distancia (cm) de cada celda al borde de su módulo (los bordes del mapa cuentan). */
		TArray<float> BorderDist;
		/** Distancia (cm) de cada celda al módulo vecino más cercano (sin contar el borde del mapa). */
		TArray<float> ModuleDist;

		TArray<FModule> Modules;
		TArray<FPortal> Portals;
		TArray<FRouteStep> Route;
		TArray<FCrossing> Crossings;
		TArray<FPathSample> Main;
		TArray<FBranch> Branches;
		/** Zonas de muerte fijas (bajo los tableros de los puentes colosales). */
		TArray<FKillBox> KillBoxes;
		TArray<FFeature> Features;

		/** Río opcional: polilínea desde la costa hacia el interior. */
		TArray<FVector2D> River;
		TArray<double> RiverWidth;

		/** Campo suave de pesos de bioma (raster grueso) para transiciones naturales. */
		int32 BiomeW = 0;
		int32 BiomeH = 0;
		double BiomeCell = 800.0;
		TArray<float> BiomeWeights;
		/** Nivel base suavizado de módulos en el mismo raster grueso. */
		TArray<float> LevelField;
		/** Tipo de módulo vacío suavizado: 1 = elevado, 0 = accesible. */
		TArray<float> ElevatedField;

		FVector2D StartPoint = FVector2D::ZeroVector;
		FVector2D EndPoint = FVector2D::ZeroVector;

		int32 UniqueModulesOnRoute = 0;
		/** Diagnóstico: módulos en los que el caminante falló y se usó la curva de reserva. */
		int32 WalkFallbacks = 0;

		// ── Consultas ───────────────────────────────────────────────────────

		int32 CellIndex(int32 X, int32 Y) const { return Y * RasterW + X; }
		bool CellInside(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < RasterW && Y < RasterH; }

		FIntPoint CellOf(const FVector2D& P) const
		{
			return FIntPoint(FMath::Clamp(FMath::FloorToInt(P.X / Params.CellSize), 0, RasterW - 1),
				FMath::Clamp(FMath::FloorToInt(P.Y / Params.CellSize), 0, RasterH - 1));
		}

		FVector2D CellCenter(int32 X, int32 Y) const
		{
			return FVector2D((static_cast<double>(X) + 0.5) * Params.CellSize, (static_cast<double>(Y) + 0.5) * Params.CellSize);
		}

		int32 ModuleAt(const FVector2D& P) const
		{
			if (P.X < 0.0 || P.Y < 0.0 || P.X >= WorldSize || P.Y >= WorldSize) { return INDEX_NONE; }
			const FIntPoint C = CellOf(P);
			return ModuleOfCell[CellIndex(C.X, C.Y)];
		}

		double BorderDistAt(const FVector2D& P) const
		{
			if (P.X < 0.0 || P.Y < 0.0 || P.X >= WorldSize || P.Y >= WorldSize) { return 0.0; }
			const FIntPoint C = CellOf(P);
			return BorderDist[CellIndex(C.X, C.Y)];
		}

		/** Y de la línea de costa (irregular) para una X dada. */
		double CoastY(double X) const
		{
			return WorldSize - Params.CoastInset + 2500.0 * Fbm1(Params.Seed ^ 0xC0A57u, X / 30000.0, 3);
		}

		/** Distancia hacia dentro a la que está el muro del borde en un punto del perímetro. */
		double WallInset(double T) const
		{
			const double N = 0.5 + 0.5 * Fbm1(Params.Seed ^ 0xBA11u, T / 25000.0, 3);
			return LerpD(Params.WallInsetMin, Params.WallInsetMax, N);
		}

		/** Pesos de bioma interpolados (suman ~1). */
		void BiomeWeightsAt(const FVector2D& P, double OutW[NumBiomes]) const
		{
			for (int32 b = 0; b < NumBiomes; ++b) { OutW[b] = 0.0; }
			if (BiomeW <= 0) { return; }
			const double Fx = FMath::Clamp(P.X / BiomeCell - 0.5, 0.0, static_cast<double>(BiomeW - 1));
			const double Fy = FMath::Clamp(P.Y / BiomeCell - 0.5, 0.0, static_cast<double>(BiomeH - 1));
			const int32 X0 = FMath::Min(FMath::FloorToInt(Fx), BiomeW - 1);
			const int32 Y0 = FMath::Min(FMath::FloorToInt(Fy), BiomeH - 1);
			const int32 X1 = FMath::Min(X0 + 1, BiomeW - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, BiomeH - 1);
			const double Tx = Fx - X0;
			const double Ty = Fy - Y0;
			const double Wq[4] = { (1 - Tx) * (1 - Ty), Tx * (1 - Ty), (1 - Tx) * Ty, Tx * Ty };
			const int32 Cells[4] = { Y0 * BiomeW + X0, Y0 * BiomeW + X1, Y1 * BiomeW + X0, Y1 * BiomeW + X1 };
			for (int32 q = 0; q < 4; ++q)
			{
				for (int32 b = 0; b < NumBiomes; ++b)
				{
					OutW[b] += Wq[q] * BiomeWeights[Cells[q] * NumBiomes + b];
				}
			}
		}

		/** Interpolación bilineal de un campo escalar del raster grueso. */
		double SampleCoarse(const TArray<float>& Field, const FVector2D& P) const
		{
			if (BiomeW <= 0 || Field.Num() != BiomeW * BiomeH) { return 0.0; }
			const double Fx = FMath::Clamp(P.X / BiomeCell - 0.5, 0.0, static_cast<double>(BiomeW - 1));
			const double Fy = FMath::Clamp(P.Y / BiomeCell - 0.5, 0.0, static_cast<double>(BiomeH - 1));
			const int32 X0 = FMath::Min(FMath::FloorToInt(Fx), BiomeW - 1);
			const int32 Y0 = FMath::Min(FMath::FloorToInt(Fy), BiomeH - 1);
			const int32 X1 = FMath::Min(X0 + 1, BiomeW - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, BiomeH - 1);
			const double Tx = Fx - X0;
			const double Ty = Fy - Y0;
			const double A = LerpD(Field[Y0 * BiomeW + X0], Field[Y0 * BiomeW + X1], Tx);
			const double B = LerpD(Field[Y1 * BiomeW + X0], Field[Y1 * BiomeW + X1], Tx);
			return LerpD(A, B, Ty);
		}

		ETNProcBiome DominantBiomeAt(const FVector2D& P) const
		{
			double W[NumBiomes];
			BiomeWeightsAt(P, W);
			int32 Best = 0;
			for (int32 b = 1; b < NumBiomes; ++b) { if (W[b] > W[Best]) { Best = b; } }
			return BiomeFromIndex(Best);
		}

		double MainLength() const { return Main.Num() > 0 ? Main.Last().S : 0.0; }

		int32 CountFeatures(EFeature Type) const
		{
			int32 N = 0;
			for (const FFeature& F : Features) { if (F.Type == Type) { ++N; } }
			return N;
		}
	};

	/** Y de la orilla del agua en la playa de la meta (la llegada va recta hacia +Y desde EndPoint). */
	inline double FinishWaterY(const FLayout& L) { return L.CoastY(L.EndPoint.X) - FinishDims::WaterInset; }

	inline EBridgeStyle BridgeStyleOf(const FLayout& L, int32 C)
	{
		return BridgeStyleFor(L.Modules[L.Crossings[C].Module].Biome, L.Params.Seed ^ (0xB21D6u + static_cast<uint32>(C)));
	}

	/** Y de la línea de meta. */
	inline double FinishLineY(const FLayout& L) { return FinishWaterY(L) + FinishDims::LineInWater; }

	/** Ancho del camino principal a la altura Y de la playa final (interpolado entre muestras). */
	inline double FinishBeachWidthAt(const FLayout& L, double Y)
	{
		const TArray<FPathSample>& M = L.Main;
		for (int32 i = M.Num() - 1; i > 0; --i)
		{
			if ((M[i - 1].Flags & PathFlags::Shore) == 0) { break; }
			if (M[i - 1].P.Y <= Y)
			{
				const double Span = M[i].P.Y - M[i - 1].P.Y;
				const double T = Span > 1.0 ? FMath::Clamp((Y - M[i - 1].P.Y) / Span, 0.0, 1.0) : 1.0;
				return LerpD(M[i - 1].Width, M[i].Width, T);
			}
		}
		return M.Num() > 0 ? (Y > M.Last().P.Y ? M.Last().Width : M[0].Width) : 0.0;
	}
}
