#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapTerrain.h"

/**
 * Vegetación y rocas sueltas del mapa (lógica pura): especies de cada bioma y reparto determinista
 * sobre el terreno ya calculado. El actor construye una malla por especie y variante y la instancia;
 * aquí solo se decide qué crece dónde, con qué tamaño y cuánto se hunde en la pendiente.
 *
 * Dos pasadas sobre rejillas con jitter: la grande (árboles, peñascos) cubre todo el terreno, también
 * el exterior; la pequeña (arbustos, helechos, hierba, flores, juncos, piedras) solo lo que se ve desde
 * el camino. Cada especie crece en manchas (bosque, sotobosque, pradera, pedregal) de un ruido lento,
 * así hay bosques y claros en vez de un reparto uniforme; en los taludes crecen las que se agarran a
 * la pendiente (helechos, arbustos, hierba), inclinadas con ella y hundidas lo justo para no flotar.
 */
namespace TNProcMap
{
	/** Forma (malla) de una especie. */
	enum class EFloraShape : uint8
	{
		BroadTree,     ///< Árbol de copa redonda (selva, lagunas, parques).
		Ceiba,         ///< Gigante de la selva: raíces tabulares y copa en parasol.
		Palm,          ///< Palmera de tronco curvo.
		MangroveTree,  ///< Mangle: raíces zancudas en arco y copa ancha y baja.
		YoungSequoia,  ///< Secuoya joven (las gigantes son features del layout).
		Cypress,       ///< Ciprés de pantano, cónico y esbelto.
		Pine,          ///< Pino de pisos.
		Fir,           ///< Abeto estrecho y denso.
		Willow,        ///< Sauce llorón.
		Acacia,        ///< Acacia de copa plana (desierto).
		DeadTree,      ///< Árbol seco sin hojas.
		CharredTree,   ///< Árbol calcinado (volcán), con brasas.
		Ornamental,    ///< Árbol de parque de copa esférica.
		Fern,          ///< Helecho de frondas arqueadas.
		Bush,          ///< Arbusto de varias masas.
		Grass,         ///< Mata de hierba.
		Flowers,       ///< Mata con flores de colores.
		Reeds,         ///< Juncos y eneas (orilla y agua somera).
		Saguaro,       ///< Cactus columnar con brazos.
		Barrel,        ///< Cactus barril con flor.
		DryBush,       ///< Matojo seco de ramitas.
		AshBush,       ///< Arbusto de ceniza (volcán).
		Hedge,         ///< Seto recortado (zona humana).
		Umbrella,      ///< Sombrilla de playa con mástil (zona humana).
		Creeper,       ///< Enredadera o musgo: manta de hojas pegada a la pared (sigue su normal).
		BananaPlant,   ///< Platanera de hojas enormes (selva).
		Bamboo,        ///< Mata de bambú con nudos y penachos.
		TreeFern,      ///< Helecho arbóreo.
		SeaGrape,      ///< Uva de playa: arbolito bajo de hojas redondas.
		Pandanus,      ///< Pándano de raíces zancudas y penachos de hojas afiladas.
		FanPalm,       ///< Palmito de hojas en abanico.
		Casuarina,     ///< Casuarina de ramillas colgantes (costa).
		JoshuaTree,    ///< Árbol de Josué (desierto).
		Birch,         ///< Abedul de tronco blanco.
		Rock,          ///< Peñasco suelto.
		Stones,        ///< Corro de piedras pequeñas.
		Count
	};

	constexpr int32 NumFloraShapes = static_cast<int32>(EFloraShape::Count);

	/** Nombre de una forma (depuración y herramientas). */
	inline const char* FloraShapeName(EFloraShape Shape)
	{
		static const char* Names[] = { "BroadTree", "Ceiba", "Palm", "MangroveTree", "YoungSequoia", "Cypress", "Pine", "Fir", "Willow", "Acacia",
			"DeadTree", "CharredTree", "Ornamental", "Fern", "Bush", "Grass", "Flowers", "Reeds", "Saguaro", "Barrel", "DryBush", "AshBush", "Hedge",
			"Umbrella", "Creeper", "BananaPlant", "Bamboo", "TreeFern", "SeaGrape", "Pandanus", "FanPalm", "Casuarina", "JoshuaTree", "Birch", "Rock", "Stones" };
		static_assert(sizeof(Names) / sizeof(Names[0]) == static_cast<int32>(EFloraShape::Count), "FloraShapeName desfasado");
		return Names[static_cast<int32>(Shape)];
	}

	/** Variantes de malla por especie (forma y tono distintos). */
	constexpr int32 FloraVariants = 3;

	/** Dónde puede crecer una especie según la cota respecto al agua. */
	namespace FloraZone
	{
		constexpr uint8 Land = 1u << 0;      ///< Tierra firme (40 cm o más sobre el agua).
		constexpr uint8 Shore = 1u << 1;     ///< Orilla, a ras de agua.
		constexpr uint8 Shallows = 1u << 2;  ///< Agua somera (hasta 1,8 m de fondo).
		constexpr uint8 Deep = 1u << 3;      ///< Agua honda (hasta 4,5 m): mangles y cipreses de las pozas.
	}

	/** Manchas en las que crece cada especie (mismo ruido = crecen juntas). */
	enum class EFloraPatch : uint8
	{
		None,      ///< En todas partes.
		Forest,    ///< Bosques y claros (~90 m).
		Under,     ///< Sotobosque (~45 m).
		Meadow,    ///< Praderas (~30 m).
		Rocks,     ///< Pedregales (~60 m).
		Count
	};

	struct FFloraSpecies
	{
		EFloraShape Shape = EFloraShape::Bush;
		/** 0 = grande (rejilla de 4 m en todo el terreno), 1 = pequeña (rejilla de 1,8 m cerca de los caminos). */
		uint8 Pass = 0;
		uint8 Zones = FloraZone::Land;
		/** Instancias por cada 100 m² donde su mancha es plena. */
		double Density = 1.0;
		double ScaleMin = 0.8;
		double ScaleMax = 1.2;
		/** Sesgo del tamaño: > 1, muchos pequeños y pocos grandes. */
		double ScaleSkew = 1.0;
		/** Pendiente del terreno (grados): máxima y mínima (las enredaderas, solo en pared). */
		double SlopeMax = 35.0;
		double SlopeMin = 0.0;
		/** Distancia al borde del camino (cm): mínima y máxima. */
		double EdgeMin = 300.0;
		double EdgeMax = 1e9;
		/** Umbral de su mancha en [0, 1]: 0 = crece en todas partes; más alto, manchas más raras. */
		double Patch = 0.0;
		EFloraPatch PatchKind = EFloraPatch::None;
		/** Radio de la base a escala 1 (cm): cuánto se hunde en pendiente para no flotar. */
		double Footprint = 50.0;
		/** 0 = crece vertical, 1 = perpendicular al terreno (solo con 1 se pega a paredes de más de 40°). */
		double Lean = 0.0;
	};

	/** Una planta o roca colocada (espacio del mapa). */
	struct FFloraInstance
	{
		int32 Biome = 0;       ///< BiomeIndex
		int32 Species = 0;     ///< Índice en la tabla de su bioma.
		int32 Variant = 0;
		FVector Location = FVector::ZeroVector;
		double Yaw = 0.0;      ///< Grados.
		double Scale = 1.0;
		/** Inclinación: hacia LeanDir (horizontal, cuesta abajo) LeanDeg grados. */
		FVector2D LeanDir = FVector2D(1.0, 0.0);
		double LeanDeg = 0.0;
	};

	/** Especies de un bioma (tabla fija: el orden es el índice de FFloraInstance::Species). */
	inline void FloraSpeciesFor(ETNProcBiome Biome, TArray<FFloraSpecies>& Out)
	{
		Out.Reset();
		using EP = EFloraPatch;
		constexpr uint8 L = FloraZone::Land;
		constexpr uint8 Sh = FloraZone::Shore;
		constexpr uint8 W = FloraZone::Shallows;
		constexpr uint8 D = FloraZone::Deep;
		auto Add = [&Out](EFloraShape Shape, uint8 Pass, uint8 Zones, double Density, double S0, double S1, double Skew, double Slope,
			double Edge0, double Edge1, double Patch, EP Kind, double Foot, double Lean)
		{
			FFloraSpecies S;
			S.Shape = Shape; S.Pass = Pass; S.Zones = Zones; S.Density = Density;
			S.ScaleMin = S0; S.ScaleMax = S1; S.ScaleSkew = Skew; S.SlopeMax = Slope;
			S.EdgeMin = Edge0; S.EdgeMax = Edge1; S.Patch = Patch; S.PatchKind = Kind; S.Footprint = Foot; S.Lean = Lean;
			Out.Add(S);
		};
		constexpr double Far = 1e9;
		// Enredaderas y musgo: solo en paredes (45° o más), pegados a ellas.
		auto AddCreeper = [&Add, &Out](double Density)
		{
			Add(EFloraShape::Creeper, 1, FloraZone::Land, Density, 0.6, 1.5, 1.0, 88.0, 60.0, 6000.0, 0.22, EFloraPatch::Under, 40.0, 1.0);
			Out.Last().SlopeMin = 45.0;
		};
		switch (Biome)
		{
			case ETNProcBiome::Jungle:
				// Selva muy poblada: árbol de copa dominante, ceibas, helechos arbóreos, bambuzales y palmeras.
				Add(EFloraShape::BroadTree, 0, L, 2.8, 0.5, 1.5, 1.6, 48.0, 350.0, Far, 0.20, EP::Forest, 35.0, 0.08);
				Add(EFloraShape::Ceiba, 0, L, 0.15, 0.7, 1.35, 1.5, 32.0, 1200.0, Far, 0.40, EP::Forest, 90.0, 0.03);
				Add(EFloraShape::TreeFern, 0, L, 0.7, 0.6, 1.4, 1.2, 45.0, 250.0, Far, 0.30, EP::Under, 25.0, 0.08);
				Add(EFloraShape::Bamboo, 0, L, 0.35, 0.7, 1.3, 1.0, 40.0, 300.0, Far, 0.55, EP::Under, 50.0, 0.05);
				Add(EFloraShape::Palm, 0, L, 0.45, 0.65, 1.3, 1.0, 35.0, 300.0, Far, 0.0, EP::None, 30.0, 0.10);
				Add(EFloraShape::Rock, 0, L, 0.2, 0.3, 1.8, 2.5, 85.0, 250.0, Far, 0.55, EP::Rocks, 100.0, 0.6);
				Add(EFloraShape::BananaPlant, 1, L, 1.2, 0.6, 1.3, 1.2, 40.0, 120.0, 3000.0, 0.35, EP::Under, 30.0, 0.15);
				Add(EFloraShape::Fern, 1, L, 6.0, 0.45, 1.4, 1.3, 72.0, 60.0, 7000.0, 0.20, EP::Under, 50.0, 0.6);
				Add(EFloraShape::Bush, 1, L, 4.5, 0.45, 1.6, 1.4, 75.0, 100.0, 8000.0, 0.25, EP::Under, 60.0, 0.5);
				Add(EFloraShape::Grass, 1, L, 11.0, 0.6, 1.5, 1.0, 72.0, 0.0, 4000.0, 0.28, EP::Meadow, 20.0, 0.7);
				Add(EFloraShape::Flowers, 1, L, 2.0, 0.6, 1.3, 1.0, 60.0, 0.0, 4000.0, 0.50, EP::Meadow, 25.0, 0.7);
				AddCreeper(7.0);
				break;
			case ETNProcBiome::Beach:
				// Playa menos poblada: palmeras y casuarinas, y junto al camino uva de playa, pándanos y palmitos.
				Add(EFloraShape::Palm, 0, L, 1.0, 0.55, 1.4, 1.2, 32.0, 300.0, Far, 0.30, EP::Forest, 30.0, 0.12);
				Add(EFloraShape::Casuarina, 0, L, 0.25, 0.7, 1.3, 1.2, 35.0, 500.0, Far, 0.50, EP::Forest, 30.0, 0.05);
				Add(EFloraShape::Rock, 0, L | Sh, 0.45, 0.3, 2.0, 2.5, 88.0, 200.0, Far, 0.50, EP::Rocks, 100.0, 0.6);
				Add(EFloraShape::SeaGrape, 1, L, 0.8, 0.6, 1.4, 1.2, 40.0, 150.0, 3500.0, 0.35, EP::Under, 60.0, 0.1);
				Add(EFloraShape::Pandanus, 1, L, 0.35, 0.7, 1.3, 1.1, 35.0, 250.0, 4000.0, 0.45, EP::Under, 40.0, 0.08);
				Add(EFloraShape::FanPalm, 1, L, 0.9, 0.6, 1.4, 1.2, 45.0, 120.0, 3500.0, 0.40, EP::Meadow, 35.0, 0.1);
				Add(EFloraShape::Grass, 1, L, 10.0, 0.6, 1.5, 1.0, 72.0, 0.0, 4000.0, 0.25, EP::Meadow, 20.0, 0.7);
				Add(EFloraShape::Bush, 1, L, 1.2, 0.4, 1.3, 1.5, 70.0, 100.0, 7000.0, 0.40, EP::Under, 60.0, 0.5);
				Add(EFloraShape::Stones, 1, L | Sh, 2.5, 0.5, 1.4, 1.0, 60.0, 30.0, 4000.0, 0.45, EP::Rocks, 40.0, 0.8);
				break;
			case ETNProcBiome::Desert:
				Add(EFloraShape::Saguaro, 0, L, 0.4, 0.6, 1.4, 1.2, 30.0, 300.0, Far, 0.30, EP::Forest, 35.0, 0.0);
				Add(EFloraShape::JoshuaTree, 0, L, 0.2, 0.7, 1.3, 1.1, 30.0, 400.0, Far, 0.45, EP::Forest, 30.0, 0.03);
				Add(EFloraShape::Acacia, 0, L, 0.15, 0.7, 1.3, 1.0, 22.0, 600.0, Far, 0.40, EP::Forest, 30.0, 0.05);
				Add(EFloraShape::DeadTree, 0, L, 0.08, 0.6, 1.2, 1.0, 35.0, 500.0, Far, 0.0, EP::None, 30.0, 0.10);
				Add(EFloraShape::Rock, 0, L, 0.8, 0.3, 2.4, 2.5, 88.0, 200.0, Far, 0.40, EP::Rocks, 100.0, 0.6);
				Add(EFloraShape::Barrel, 1, L, 1.6, 0.5, 1.5, 1.3, 60.0, 80.0, 6000.0, 0.30, EP::Under, 40.0, 0.3);
				Add(EFloraShape::DryBush, 1, L, 3.2, 0.45, 1.5, 1.3, 70.0, 60.0, 7000.0, 0.20, EP::Under, 50.0, 0.4);
				Add(EFloraShape::Grass, 1, L, 3.0, 0.5, 1.2, 1.0, 65.0, 0.0, 4000.0, 0.40, EP::Meadow, 20.0, 0.7);
				Add(EFloraShape::Stones, 1, L, 2.5, 0.5, 1.4, 1.0, 65.0, 20.0, 4000.0, 0.40, EP::Rocks, 40.0, 0.8);
				break;
			case ETNProcBiome::Volcanic:
				Add(EFloraShape::CharredTree, 0, L, 0.8, 0.55, 1.4, 1.3, 42.0, 350.0, Far, 0.30, EP::Forest, 30.0, 0.10);
				Add(EFloraShape::DeadTree, 0, L, 0.3, 0.6, 1.3, 1.0, 40.0, 350.0, Far, 0.40, EP::Forest, 30.0, 0.10);
				Add(EFloraShape::Pine, 0, L, 0.25, 0.45, 1.1, 1.8, 40.0, 350.0, Far, 0.55, EP::Forest, 30.0, 0.05);
				Add(EFloraShape::Rock, 0, L, 1.2, 0.3, 2.6, 2.5, 88.0, 200.0, Far, 0.30, EP::Rocks, 100.0, 0.6);
				Add(EFloraShape::AshBush, 1, L, 3.2, 0.45, 1.4, 1.3, 72.0, 60.0, 7000.0, 0.30, EP::Under, 50.0, 0.5);
				Add(EFloraShape::Fern, 1, L, 2.4, 0.45, 1.2, 1.3, 70.0, 60.0, 6000.0, 0.40, EP::Under, 50.0, 0.6);
				Add(EFloraShape::Stones, 1, L, 3.0, 0.5, 1.5, 1.0, 70.0, 20.0, 4000.0, 0.30, EP::Rocks, 40.0, 0.8);
				AddCreeper(2.0);
				break;
			case ETNProcBiome::Water:
				Add(EFloraShape::Willow, 0, L, 0.6, 0.7, 1.35, 1.2, 30.0, 400.0, Far, 0.30, EP::Forest, 40.0, 0.05);
				Add(EFloraShape::BroadTree, 0, L, 0.9, 0.5, 1.3, 1.5, 40.0, 350.0, Far, 0.35, EP::Forest, 35.0, 0.08);
				Add(EFloraShape::Birch, 0, L, 0.35, 0.6, 1.3, 1.3, 40.0, 350.0, Far, 0.45, EP::Forest, 25.0, 0.05);
				Add(EFloraShape::Cypress, 0, L | Sh | W, 0.3, 0.6, 1.3, 1.2, 35.0, 450.0, Far, 0.50, EP::Forest, 40.0, 0.03);
				Add(EFloraShape::Rock, 0, L | Sh, 0.2, 0.3, 1.6, 2.5, 85.0, 200.0, Far, 0.55, EP::Rocks, 100.0, 0.6);
				Add(EFloraShape::Reeds, 1, Sh | W, 10.0, 0.6, 1.4, 1.0, 40.0, 50.0, 6000.0, 0.15, EP::Meadow, 40.0, 0.2);
				Add(EFloraShape::Bush, 1, L, 3.0, 0.45, 1.4, 1.4, 70.0, 100.0, 7000.0, 0.30, EP::Under, 60.0, 0.5);
				Add(EFloraShape::Grass, 1, L | Sh, 8.0, 0.6, 1.5, 1.0, 72.0, 0.0, 4000.0, 0.25, EP::Meadow, 20.0, 0.7);
				Add(EFloraShape::Flowers, 1, L, 1.8, 0.6, 1.3, 1.0, 60.0, 0.0, 4000.0, 0.50, EP::Meadow, 25.0, 0.7);
				AddCreeper(4.0);
				break;
			case ETNProcBiome::Rocky:
				Add(EFloraShape::Pine, 0, L, 1.8, 0.5, 1.5, 1.6, 45.0, 350.0, Far, 0.30, EP::Forest, 30.0, 0.05);
				Add(EFloraShape::Fir, 0, L, 0.9, 0.6, 1.4, 1.3, 42.0, 350.0, Far, 0.40, EP::Forest, 30.0, 0.05);
				Add(EFloraShape::Birch, 0, L, 0.3, 0.6, 1.3, 1.3, 40.0, 350.0, Far, 0.50, EP::Forest, 25.0, 0.05);
				Add(EFloraShape::Rock, 0, L, 1.3, 0.3, 2.6, 2.5, 88.0, 200.0, Far, 0.25, EP::Rocks, 100.0, 0.6);
				Add(EFloraShape::Bush, 1, L, 3.2, 0.4, 1.3, 1.4, 75.0, 80.0, 7000.0, 0.30, EP::Under, 60.0, 0.5);
				Add(EFloraShape::Grass, 1, L, 8.0, 0.5, 1.3, 1.0, 72.0, 0.0, 4000.0, 0.25, EP::Meadow, 20.0, 0.7);
				Add(EFloraShape::Flowers, 1, L, 1.5, 0.5, 1.2, 1.0, 60.0, 0.0, 4000.0, 0.50, EP::Meadow, 25.0, 0.7);
				Add(EFloraShape::Stones, 1, L, 3.0, 0.5, 1.5, 1.0, 75.0, 20.0, 4000.0, 0.25, EP::Rocks, 40.0, 0.8);
				AddCreeper(3.0);
				break;
			case ETNProcBiome::Mangrove:
				Add(EFloraShape::MangroveTree, 0, L | Sh | W | D, 3.0, 0.55, 1.5, 1.3, 40.0, 350.0, Far, 0.15, EP::Forest, 60.0, 0.05);
				Add(EFloraShape::YoungSequoia, 0, L | Sh | W | D, 0.45, 0.55, 1.6, 1.4, 38.0, 700.0, Far, 0.25, EP::Forest, 60.0, 0.03);
				Add(EFloraShape::Cypress, 0, L | Sh | W | D, 0.8, 0.6, 1.4, 1.2, 35.0, 450.0, Far, 0.30, EP::Forest, 40.0, 0.03);
				Add(EFloraShape::Pandanus, 0, L | Sh, 0.4, 0.7, 1.3, 1.1, 35.0, 300.0, Far, 0.45, EP::Forest, 40.0, 0.08);
				Add(EFloraShape::TreeFern, 0, L, 0.3, 0.6, 1.3, 1.2, 45.0, 250.0, Far, 0.45, EP::Under, 25.0, 0.08);
				Add(EFloraShape::Palm, 0, L, 0.15, 0.6, 1.2, 1.0, 35.0, 300.0, Far, 0.0, EP::None, 30.0, 0.10);
				Add(EFloraShape::Reeds, 1, Sh | W, 9.0, 0.6, 1.4, 1.0, 40.0, 50.0, 6000.0, 0.15, EP::Meadow, 40.0, 0.2);
				Add(EFloraShape::Fern, 1, L, 5.0, 0.45, 1.3, 1.3, 72.0, 60.0, 7000.0, 0.20, EP::Under, 50.0, 0.6);
				Add(EFloraShape::Bush, 1, L, 3.5, 0.45, 1.5, 1.4, 75.0, 100.0, 7000.0, 0.25, EP::Under, 60.0, 0.5);
				Add(EFloraShape::Grass, 1, L | Sh, 7.0, 0.6, 1.4, 1.0, 72.0, 0.0, 4000.0, 0.25, EP::Meadow, 20.0, 0.7);
				Add(EFloraShape::Flowers, 1, L, 1.4, 0.6, 1.3, 1.0, 60.0, 0.0, 4000.0, 0.50, EP::Meadow, 25.0, 0.7);
				AddCreeper(6.0);
				break;
			case ETNProcBiome::Human:
			default:
				Add(EFloraShape::Ornamental, 0, L, 0.6, 0.7, 1.3, 1.0, 20.0, 300.0, Far, 0.30, EP::Forest, 30.0, 0.0);
				Add(EFloraShape::BroadTree, 0, L, 0.4, 0.6, 1.2, 1.3, 30.0, 400.0, Far, 0.40, EP::Forest, 35.0, 0.05);
				Add(EFloraShape::Birch, 0, L, 0.25, 0.6, 1.2, 1.2, 30.0, 350.0, Far, 0.50, EP::Forest, 25.0, 0.03);
				Add(EFloraShape::Palm, 0, L, 0.2, 0.7, 1.2, 1.0, 25.0, 300.0, Far, 0.0, EP::None, 30.0, 0.05);
				Add(EFloraShape::Umbrella, 1, L, 0.25, 0.85, 1.15, 1.0, 12.0, 150.0, 3000.0, 0.45, EP::Meadow, 20.0, 0.0);
				Add(EFloraShape::Hedge, 1, L, 0.9, 0.7, 1.4, 1.0, 20.0, 150.0, 5000.0, 0.45, EP::Under, 80.0, 0.0);
				Add(EFloraShape::FanPalm, 1, L, 0.4, 0.7, 1.3, 1.1, 35.0, 150.0, 3500.0, 0.45, EP::Meadow, 35.0, 0.05);
				Add(EFloraShape::Bush, 1, L, 2.0, 0.5, 1.2, 1.2, 60.0, 100.0, 6000.0, 0.35, EP::Under, 60.0, 0.5);
				Add(EFloraShape::Grass, 1, L, 7.0, 0.6, 1.3, 1.0, 65.0, 0.0, 4000.0, 0.25, EP::Meadow, 20.0, 0.7);
				Add(EFloraShape::Flowers, 1, L, 2.5, 0.6, 1.3, 1.0, 60.0, 0.0, 4000.0, 0.40, EP::Meadow, 25.0, 0.7);
				AddCreeper(3.0);
				break;
		}
	}

	/** Rejilla de una pasada: celdas de Cell cm desde Origin (una candidata por celda). */
	struct FFloraGrid
	{
		FVector2D Origin = FVector2D::ZeroVector;
		double Cell = 450.0;
		int32 NX = 0;
		int32 NY = 0;
	};

	namespace FloraDetail
	{
		/** Lado de celda de cada pasada (cm). */
		constexpr double PassCell[2] = { 400.0, 180.0 };
		/** La pasada pequeña solo cerca de los caminos: más lejos no se ve. */
		constexpr double SmallPassEdge = 8000.0;
		/** Taludes junto al camino (desde WallSlope grados, a menos de WallEdge): lo que más se ve desde él, doble densidad. */
		constexpr double WallSlope = 35.0;
		constexpr double WallEdge = 4000.0;
		constexpr double WallBoost = 2.0;

		/** Valor de la mancha de un tipo en P, en [0, 1] (media ~0,5). */
		inline double PatchValue(uint32 Seed, EFloraPatch Kind, const FVector2D& P)
		{
			static const double Scales[static_cast<int32>(EFloraPatch::Count)] = { 1.0, 9000.0, 4500.0, 3000.0, 6000.0 };
			const int32 K = static_cast<int32>(Kind);
			if (K <= 0) { return 1.0; }
			return 0.5 + 0.5 * Fbm2(Seed + 0xF10Au + static_cast<uint32>(K) * 101u, P.X / Scales[K], P.Y / Scales[K], 3) * 1.6;
		}
	}

	inline FFloraGrid FloraGridFor(const FVector2D& Min, const FVector2D& Max, int32 Pass)
	{
		FFloraGrid G;
		G.Origin = Min;
		G.Cell = FloraDetail::PassCell[FMath::Clamp(Pass, 0, 1)];
		G.NX = FMath::Max(1, FMath::CeilToInt((Max.X - Min.X) / G.Cell));
		G.NY = FMath::Max(1, FMath::CeilToInt((Max.Y - Min.Y) / G.Cell));
		return G;
	}

	/**
	 * Reparte las filas [Row0, Row1) de la rejilla G de una pasada. Tables: especies de cada bioma
	 * (FloraSpeciesFor). Q: Height(P) (cm), Normal(P) (FVector unitario), Edge(P) (cm al borde del
	 * camino más cercano; 0 sobre él) y Blocked(P) (estructuras, huevos...); DensityScale multiplica
	 * todas las densidades. Cada celda usa su propio
	 * generador (semilla, pasada, celda): repartir las filas entre hilos da el mismo resultado.
	 */
	template <typename FQuery>
	void PlaceFloraRows(const FLayout& L, const TArray<FFloraSpecies> (&Tables)[NumBiomes], const FQuery& Q, int32 Pass,
		const FFloraGrid& G, int32 Row0, int32 Row1, TArray<FFloraInstance>& Out, double DensityScale = 1.0)
	{
		using namespace FloraDetail;
		const uint32 Seed = L.Params.Seed ^ 0xF1024A5u;
		const double CellArea = G.Cell * G.Cell / 1.0e6;   // en unidades de 100 m²
		for (int32 cy = FMath::Max(0, Row0); cy < FMath::Min(G.NY, Row1); ++cy)
		{
			for (int32 cx = 0; cx < G.NX; ++cx)
			{
				FRng R((static_cast<uint64>(HashCell(Seed + static_cast<uint32>(Pass) * 7919u, cx, cy)) << 17) ^ static_cast<uint64>(cy));
				const FVector2D P = G.Origin + FVector2D((cx + R.Unit()) * G.Cell, (cy + R.Unit()) * G.Cell);
				// Nunca sobre el suelo del camino.
				const double Edge = Q.Edge(P);
				if (Edge <= 0.0 || (Pass == 1 && Edge > SmallPassEdge)) { continue; }

				// Bioma sorteado con sus pesos: mezcla natural en las transiciones.
				double W[NumBiomes];
				L.BiomeWeightsAt(P, W);
				const double Pick = R.Unit();
				double Acc = 0.0;
				int32 B = NumBiomes - 1;
				for (int32 k = 0; k < NumBiomes; ++k)
				{
					Acc += W[k];
					if (Pick < Acc) { B = k; break; }
				}
				const TArray<FFloraSpecies>& Table = Tables[B];

				const double H = Q.Height(P);
				uint8 Zone = 0;
				if (H >= SeaLevel + 40.0) { Zone = FloraZone::Land; }
				else if (H >= SeaLevel - 40.0) { Zone = FloraZone::Shore; }
				else if (H >= SeaLevel - 180.0) { Zone = FloraZone::Shallows; }
				else if (H >= SeaLevel - 450.0) { Zone = FloraZone::Deep; }
				if (Zone == 0) { continue; }
				const FVector N = Q.Normal(P);
				const double Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(N.Z, -1.0, 1.0)));

				// Especie: cada una con su probabilidad (densidad x mancha); si no toca ninguna, nada. Si entre
				// todas pasan de 1 (celda llena, como en los taludes), se reparten en proporción.
				const double Boost = Pass == 1 && Slope >= WallSlope && Edge <= WallEdge ? WallBoost : 1.0;
				constexpr int32 MaxSpecies = 16;
				double Prob[MaxSpecies];
				double Total = 0.0;
				for (int32 s = 0; s < Table.Num() && s < MaxSpecies; ++s)
				{
					const FFloraSpecies& Sp = Table[s];
					Prob[s] = 0.0;
					if (Sp.Pass != Pass || (Sp.Zones & Zone) == 0 || Slope > Sp.SlopeMax || Slope < Sp.SlopeMin || Edge < Sp.EdgeMin || Edge > Sp.EdgeMax) { continue; }
					const double Cover = Sp.Patch <= 0.0 ? 1.0 : SmoothStep(Sp.Patch - 0.08, Sp.Patch + 0.08, PatchValue(Seed, Sp.PatchKind, P));
					Prob[s] = Sp.Density * DensityScale * Boost * Cover * CellArea;
					Total += Prob[s];
				}
				const double U = R.Unit() * FMath::Max(1.0, Total);
				double Sum = 0.0;
				int32 Chosen = INDEX_NONE;
				for (int32 s = 0; s < Table.Num() && s < MaxSpecies; ++s)
				{
					Sum += Prob[s];
					if (Prob[s] > 0.0 && U < Sum) { Chosen = s; break; }
				}
				if (Chosen == INDEX_NONE || Q.Blocked(P)) { continue; }
				const FFloraSpecies& Sp = Table[Chosen];

				FFloraInstance I;
				I.Biome = B;
				I.Species = Chosen;
				I.Variant = R.RangeInt(0, FloraVariants - 1);
				I.Yaw = R.Range(0.0, 360.0);
				I.Scale = LerpD(Sp.ScaleMin, Sp.ScaleMax, FMath::Pow(R.Unit(), Sp.ScaleSkew));
				// Inclinada con la ladera (cuesta abajo) según su Lean, nunca más de 40° salvo lo que se pega a
				// la pared (Lean 1).
				const FVector2D Down(N.X, N.Y);
				I.LeanDir = Down.SizeSquared() > 1e-8 ? Down.GetSafeNormal() : FVector2D(1.0, 0.0);
				I.LeanDeg = FMath::Min(Sp.Lean >= 1.0 ? 88.0 : 40.0, Slope * Sp.Lean);
				// Base en lo más bajo de su huella (no flota por el lado de abajo); las que siguen la ladera,
				// menos hundidas.
				const double Foot = Sp.Footprint * I.Scale;
				double Low = H;
				for (int32 k = 0; k < 4; ++k)
				{
					const FVector2D D = k == 0 ? FVector2D(1.0, 0.0) : k == 1 ? FVector2D(-1.0, 0.0) : k == 2 ? FVector2D(0.0, 1.0) : FVector2D(0.0, -1.0);
					Low = FMath::Min(Low, Q.Height(P + D * Foot));
				}
				const double Z = H - FMath::Min(H - Low, 1.2 * Foot) * (1.0 - Sp.Lean) - 6.0 - 4.0 * I.Scale;
				I.Location = FVector(P.X, P.Y, Z);
				Out.Add(I);
			}
		}
	}
}
