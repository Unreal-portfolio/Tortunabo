#pragma once

#include "CoreMinimal.h"
#include "TN_BeachTypes.generated.h"

/**
 * Modo carrera en la playa (rama claude/modo-carrera; Docs/Modo_Carrera.md): tipos compartidos por el terreno fijo, el
 * reparto procedural de elementos y cada elemento. Es el contrato entre las piezas: los nombres de clase, las huellas y
 * las categorías de aquí son los que usa el generador (ATN_BeachRaceGenerator) para colocar y crear cada elemento.
 *
 * Escala: la tortuga es una cría de unos 5 cm y en el juego mide ~1,4 m, así que todo lo demás va a TNBeach::Scale
 * veces su tamaño real (un coco de 15 cm mide 4,2 m; una sombrilla de 2 m, 56 m; una palmera de 10 m, 280 m).
 */

/** Qué es cada elemento de la playa (el generador lo coloca; su clase es TNBeach::ClassNameOf). */
UENUM(BlueprintType)
enum class ETNBeachElement : uint8
{
	// ── Decorado (ATN_BeachDecor): sin reglas de juego, colisión simple ──
	Coconut            UMETA(DisplayName = "Coco"),
	StrandedJellyfish  UMETA(DisplayName = "Medusa varada"),
	SixPackRings       UMETA(DisplayName = "Anillas de latas cortadas"),
	RedBra             UMETA(DisplayName = "Sujetador rojo"),
	Clam               UMETA(DisplayName = "Almeja"),
	DecorShell         UMETA(DisplayName = "Concha de adorno"),
	Starfish           UMETA(DisplayName = "Estrella de mar"),
	Rock               UMETA(DisplayName = "Roca"),
	RockCluster        UMETA(DisplayName = "Grupo de rocas"),
	ShipSailWreck      UMETA(DisplayName = "Restos de vela de barco"),
	MossyLog           UMETA(DisplayName = "Tronco con musgo"),
	OldPlanks          UMETA(DisplayName = "Tablones viejos"),
	FishingNet         UMETA(DisplayName = "Red de pesca"),
	PlasticCup         UMETA(DisplayName = "Vaso de plástico"),
	Bottle             UMETA(DisplayName = "Botella"),
	Lollipop           UMETA(DisplayName = "Chupachups"),
	WatermelonRind     UMETA(DisplayName = "Corteza de sandía roída"),
	Straw              UMETA(DisplayName = "Pajita"),
	PlantedUmbrella    UMETA(DisplayName = "Sombrilla clavada"),
	BeachChair         UMETA(DisplayName = "Silla de playa"),
	SandCastleSmall    UMETA(DisplayName = "Castillo de arena pequeño"),
	SandCastleHuge     UMETA(DisplayName = "Castillo de arena enorme"),
	Driftwood          UMETA(DisplayName = "Madera a la deriva"),
	/** Tramo de pasarela de madera vieja sobre la arena, que se puede recorrer (Extent = largo). */
	Boardwalk          UMETA(DisplayName = "Pasarela de madera vieja"),
	/** Caminito marcado con palos de madera y cuerda que guía hacia el mar (Extent = largo). */
	WoodenPostPath     UMETA(DisplayName = "Caminito de palos de madera"),
	// Basura y cosas de la playa (también ATN_BeachDecor, a escala TNBeach::Scale).
	SodaCan            UMETA(DisplayName = "Lata"),
	BottleCaps         UMETA(DisplayName = "Chapas de botella"),
	FlipFlop           UMETA(DisplayName = "Chanclas"),
	JuiceBox           UMETA(DisplayName = "Brick de zumo"),
	Buoy               UMETA(DisplayName = "Boya"),
	BeachTowel         UMETA(DisplayName = "Toalla"),
	SunscreenBottle    UMETA(DisplayName = "Bote de crema solar"),
	PopsicleSticks     UMETA(DisplayName = "Palitos de helado"),
	SnackShells        UMETA(DisplayName = "Cáscaras de pipas y pistachos"),
	RopePiece          UMETA(DisplayName = "Trozo de cuerda"),
	Sunglasses         UMETA(DisplayName = "Gafas de sol"),
	ToyBucket          UMETA(DisplayName = "Cubito de juguete"),
	BeachBall          UMETA(DisplayName = "Pelota hinchable"),
	Frisbee            UMETA(DisplayName = "Disco volador"),
	Cuttlebone         UMETA(DisplayName = "Hueso de sepia"),
	RubberDuck         UMETA(DisplayName = "Patito de goma"),
	GullFeather        UMETA(DisplayName = "Pluma de gaviota"),
	// ── Decorado militar (la tropa de Tortunavy): también ATN_BeachDecor ──
	Sandbags           UMETA(DisplayName = "Parapeto de sacos terreros"),
	AmmoCrate          UMETA(DisplayName = "Caja de munición"),
	TankTrap           UMETA(DisplayName = "Erizo antitanque"),
	MilitaryHelmet     UMETA(DisplayName = "Casco militar"),
	CamoNet            UMETA(DisplayName = "Red de camuflaje"),
	Jerrycan           UMETA(DisplayName = "Bidón"),
	ToySoldiers        UMETA(DisplayName = "Soldaditos de juguete"),
	// ── Trampas e interacciones ──
	BarbedWire         UMETA(DisplayName = "Alambre de espino"),
	Seaweed            UMETA(DisplayName = "Algas que enredan"),
	WobblyPlatform     UMETA(DisplayName = "Plataforma sobre un hoyo"),
	BrokenBucket       UMETA(DisplayName = "Cubo roto"),
	SpadeRamp          UMETA(DisplayName = "Pala: trampolín o puente"),
	SandDungeon        UMETA(DisplayName = "Castillo de arena con salas"),
	ShellGate          UMETA(DisplayName = "Puerta de conchas"),
	/** Almeja o concha gigante que se cierra y atrapa unos segundos; al abrirse, suelta a la tortuga con un saltito. */
	ClamTrap           UMETA(DisplayName = "Concha que atrapa"),
	/** Plataforma que se mueve (de lado a lado o arriba y abajo) sobre un hoyo, un charco o entre dos alturas. */
	MovingPlatform     UMETA(DisplayName = "Plataforma móvil"),
	/** Catapulta de playa (cuchara, palo de polo y tapón) que lanza hacia el mar a quien se sube. */
	Catapult           UMETA(DisplayName = "Catapulta"),
	/** Mina de juguete enterrada (se ve un poco): al pisarla explota y lanza a la tortuga en bola unos metros atrás. */
	Mine               UMETA(DisplayName = "Mina"),
	// ── Enemigos y amenazas ──
	GiantCrab          UMETA(DisplayName = "Cangrejo gigante con pinza"),
	SeaUrchin          UMETA(DisplayName = "Erizo de mar"),
	Lizard             UMETA(DisplayName = "Lagarto que se esconde"),
	QuadLane           UMETA(DisplayName = "Paso de quads"),
	GullZone           UMETA(DisplayName = "Zona de gaviotas y pelícanos"),
	Count              UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ETNBeachCategory : uint8
{
	Decor,
	Trap,
	Enemy
};

/** Datos con los que el generador crea un elemento (replicados: cada máquina construye igual su malla). */
USTRUCT(BlueprintType)
struct FTNBeachElementSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Beach")
	ETNBeachElement Element = ETNBeachElement::Coconut;

	/** Semilla de la variante (forma, color, animación): la misma en todas las máquinas. */
	UPROPERTY(BlueprintReadOnly, Category = "Beach")
	int32 Seed = 0;

	/** Tamaño relativo a la huella nominal (TNBeach::FootprintRadius), 0,7-1,4. */
	UPROPERTY(BlueprintReadOnly, Category = "Beach")
	float SizeScale = 1.f;

	/** Parámetro propio del elemento (p. ej. longitud de un alambre o de un paso de quads, en cm). */
	UPROPERTY(BlueprintReadOnly, Category = "Beach")
	float Extent = 0.f;
};

namespace TNBeach
{
	/** Veces el tamaño real al que va todo (la tortuga, una cría de ~5 cm, mide ~1,4 m en el juego). */
	constexpr double Scale = 28.0;

	/** Recorrido de la carrera (cm): de la línea de salida al borde del acantilado, y ancho de la playa jugable. */
	constexpr double CourseLength = 120000.0;
	constexpr double CourseWidth = 28000.0;

	/**
	 * Meta: acantilado de rocas al final de la playa, de unos 55 cm reales (5-6 veces la tortuga): se salta desde su
	 * borde y se cae al agua, que es la meta. Altura (cm) del borde sobre el agua.
	 */
	constexpr double CliffHeight = 1550.0;

	inline ETNBeachCategory CategoryOf(ETNBeachElement E)
	{
		if (E < ETNBeachElement::BarbedWire) { return ETNBeachCategory::Decor; }
		if (E < ETNBeachElement::GiantCrab) { return ETNBeachCategory::Trap; }
		return ETNBeachCategory::Enemy;
	}

	/** Nombre de la clase C++ (sin la A) que crea cada elemento; el generador la busca en /Script/Tortunabo. */
	inline const TCHAR* ClassNameOf(ETNBeachElement E)
	{
		switch (E)
		{
		case ETNBeachElement::BarbedWire:     return TEXT("TN_BeachBarbedWire");
		case ETNBeachElement::Seaweed:        return TEXT("TN_BeachSeaweed");
		case ETNBeachElement::WobblyPlatform: return TEXT("TN_BeachWobblyPlatform");
		case ETNBeachElement::BrokenBucket:   return TEXT("TN_BeachBrokenBucket");
		case ETNBeachElement::SpadeRamp:      return TEXT("TN_BeachSpadeRamp");
		case ETNBeachElement::SandDungeon:    return TEXT("TN_BeachSandDungeon");
		case ETNBeachElement::ShellGate:      return TEXT("TN_BeachShellGate");
		case ETNBeachElement::ClamTrap:       return TEXT("TN_BeachClamTrap");
		case ETNBeachElement::MovingPlatform: return TEXT("TN_BeachMovingPlatform");
		case ETNBeachElement::Catapult:       return TEXT("TN_BeachCatapult");
		case ETNBeachElement::Mine:           return TEXT("TN_BeachMine");
		case ETNBeachElement::GiantCrab:      return TEXT("TN_BeachGiantCrab");
		case ETNBeachElement::SeaUrchin:      return TEXT("TN_BeachSeaUrchin");
		case ETNBeachElement::Lizard:         return TEXT("TN_BeachLizard");
		case ETNBeachElement::QuadLane:       return TEXT("TN_BeachQuadLane");
		case ETNBeachElement::GullZone:       return TEXT("TN_BeachGullZone");
		default:                              return TEXT("TN_BeachDecor");
		}
	}

	/**
	 * Radio de la huella en planta (cm, con SizeScale = 1) que ocupa cada elemento en el suelo: el generador reparte con
	 * estas huellas (sin solapes y dejando paso) y cada elemento tiene que caber dentro de la suya. Para los que se
	 * extienden a lo ancho (alambre, paso de quads) es el semiancho a lo largo del camino; su largo va en Extent.
	 */
	inline double FootprintRadius(ETNBeachElement E)
	{
		switch (E)
		{
		case ETNBeachElement::Coconut:           return 260.0;
		case ETNBeachElement::StrandedJellyfish: return 700.0;
		case ETNBeachElement::SixPackRings:      return 450.0;
		case ETNBeachElement::RedBra:            return 550.0;
		case ETNBeachElement::Clam:              return 150.0;
		case ETNBeachElement::DecorShell:        return 180.0;
		case ETNBeachElement::Starfish:          return 250.0;
		case ETNBeachElement::Rock:              return 700.0;
		case ETNBeachElement::RockCluster:       return 1600.0;
		case ETNBeachElement::ShipSailWreck:     return 3500.0;
		case ETNBeachElement::MossyLog:          return 1400.0;
		case ETNBeachElement::OldPlanks:         return 900.0;
		case ETNBeachElement::FishingNet:        return 1300.0;
		case ETNBeachElement::PlasticCup:        return 250.0;
		case ETNBeachElement::Bottle:            return 400.0;
		case ETNBeachElement::Lollipop:          return 350.0;
		case ETNBeachElement::WatermelonRind:    return 500.0;
		case ETNBeachElement::Straw:             return 400.0;
		case ETNBeachElement::PlantedUmbrella:   return 1600.0;
		case ETNBeachElement::BeachChair:        return 1500.0;
		case ETNBeachElement::SandCastleSmall:   return 800.0;
		case ETNBeachElement::SandCastleHuge:    return 2600.0;
		case ETNBeachElement::Driftwood:         return 900.0;
		case ETNBeachElement::Boardwalk:         return 700.0;
		case ETNBeachElement::WoodenPostPath:    return 300.0;
		case ETNBeachElement::SodaCan:           return 220.0;
		case ETNBeachElement::BottleCaps:        return 150.0;
		case ETNBeachElement::FlipFlop:          return 550.0;
		case ETNBeachElement::JuiceBox:          return 250.0;
		case ETNBeachElement::Buoy:              return 550.0;
		case ETNBeachElement::BeachTowel:        return 1800.0;
		case ETNBeachElement::SunscreenBottle:   return 300.0;
		case ETNBeachElement::PopsicleSticks:    return 250.0;
		case ETNBeachElement::SnackShells:       return 300.0;
		case ETNBeachElement::RopePiece:         return 600.0;
		case ETNBeachElement::Sunglasses:        return 300.0;
		case ETNBeachElement::ToyBucket:         return 450.0;
		case ETNBeachElement::BeachBall:         return 550.0;
		case ETNBeachElement::Frisbee:           return 400.0;
		case ETNBeachElement::Cuttlebone:        return 250.0;
		case ETNBeachElement::RubberDuck:        return 200.0;
		case ETNBeachElement::GullFeather:       return 350.0;
		case ETNBeachElement::BarbedWire:        return 250.0;
		case ETNBeachElement::Seaweed:           return 700.0;
		case ETNBeachElement::WobblyPlatform:    return 900.0;
		case ETNBeachElement::BrokenBucket:      return 450.0;
		case ETNBeachElement::SpadeRamp:         return 700.0;
		case ETNBeachElement::SandDungeon:       return 4000.0;
		case ETNBeachElement::ShellGate:         return 600.0;
		case ETNBeachElement::ClamTrap:          return 900.0;
		case ETNBeachElement::MovingPlatform:    return 1200.0;
		case ETNBeachElement::Catapult:          return 900.0;
		case ETNBeachElement::Mine:              return 350.0;
		case ETNBeachElement::Sandbags:          return 1100.0;
		case ETNBeachElement::AmmoCrate:         return 700.0;
		case ETNBeachElement::TankTrap:          return 800.0;
		case ETNBeachElement::MilitaryHelmet:    return 450.0;
		case ETNBeachElement::CamoNet:           return 1500.0;
		case ETNBeachElement::Jerrycan:          return 500.0;
		case ETNBeachElement::ToySoldiers:       return 600.0;
		case ETNBeachElement::GiantCrab:         return 2500.0;
		case ETNBeachElement::SeaUrchin:         return 1200.0;
		case ETNBeachElement::Lizard:            return 1500.0;
		case ETNBeachElement::QuadLane:          return 1200.0;
		case ETNBeachElement::GullZone:          return 3000.0;
		default:                                 return 500.0;
		}
	}
}
