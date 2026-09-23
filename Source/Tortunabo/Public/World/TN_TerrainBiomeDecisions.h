#pragma once

#include "CoreMinimal.h"
#include "World/TN_TerrainModuleAsset.h"
#include "World/TN_TerrainModuleDecisions.h"
#include "Math/RandomStream.h"

/**
 * Biomas de los módulos de terreno como funciones PURAS (sin UWorld ni actores):
 *   - paleta de color de vértice de cada bioma,
 *   - siembra determinista del bosque de algas a partir de la máscara del asset,
 *   - reparto de biomas por regiones a lo largo del camino del mapa.
 *
 * El diseño de cada bioma (forma del terreno) se decide offline en
 * Scripts/gen_terrain_modules.py; aquí solo se juega con lo que el asset ya trae.
 */
namespace TNTerrainBiome
{
	constexpr int32 NumBiomes = 3;

	/** Paleta de color de vértice del bioma, en espacio lineal. */
	inline TNTerrainModule::FModuleColors ColorsFor(ETNTerrainBiome Biome, double WaterLevel)
	{
		TNTerrainModule::FModuleColors Colors;
		Colors.WaterLevel = WaterLevel;
		switch (Biome)
		{
			case ETNTerrainBiome::Water:
				// Castillo de arena: suelo claro y húmedo, murallas del mismo tono.
				Colors.Floor = FLinearColor(0.58f, 0.47f, 0.29f);
				Colors.Cliff = FLinearColor(0.30f, 0.23f, 0.14f);
				Colors.CliffAlt = FLinearColor(0.44f, 0.36f, 0.22f);
				Colors.High = FLinearColor(0.40f, 0.31f, 0.18f);
				Colors.HighAlt = FLinearColor(0.47f, 0.35f, 0.19f);
				Colors.Wet = FLinearColor(0.12f, 0.10f, 0.07f);
				break;
			case ETNTerrainBiome::Algae:
				Colors.Floor = FLinearColor(0.16f, 0.25f, 0.08f);
				Colors.Cliff = FLinearColor(0.05f, 0.07f, 0.04f);
				Colors.CliffAlt = Colors.Cliff;
				Colors.High = FLinearColor(0.05f, 0.09f, 0.035f);
				Colors.HighAlt = Colors.High;
				Colors.Wet = FLinearColor(0.04f, 0.06f, 0.03f);
				break;
			default:
				break;   // Arena: la paleta por defecto de FModuleColors.
		}
		return Colors;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Bosque de algas
	// ─────────────────────────────────────────────────────────────────────────

	/** Forma de cada instancia; índice del ISM del tile. */
	enum class EFoliageShape : uint8
	{
		Stalk,   // tallo alto (cilindro)
		Frond,   // hoja alta y afilada (cono)
		Bush     // mata baja (esfera aplastada)
	};
	constexpr int32 NumFoliageShapes = 3;

	/** Lado de las mallas básicas del motor (/Engine/BasicShapes), en uu. */
	constexpr double EngineShapeSize = 100.0;

	struct FFoliageInstance
	{
		EFoliageShape Shape = EFoliageShape::Stalk;
		FTransform Transform;
		FLinearColor Color = FLinearColor::Green;
	};

	struct FFoliageSettings
	{
		/** Separación de la rejilla de siembra, en uu. Una semilla por celda como máximo. */
		double Spacing = 350.0;
		/** Proporción de matas bajas y de hojas afiladas; el resto son tallos. */
		double BushShare = 0.25;
		double FrondShare = 0.3;
		/** Alto de tallos y hojas, en uu. */
		double MinTall = 400.0;
		double MaxTall = 1200.0;
	};

	/** Densidad del bosque (0..1) en un punto local al módulo: byte bajo de la máscara, bilineal. */
	inline double SampleFoliageDensity(const TNTerrainModule::FModuleField& Field, TArrayView<const uint16> Mask,
		const FVector2D& Local)
	{
		if (!Field.IsValid() || Mask.Num() != Field.Resolution * Field.Resolution) { return 0.0; }
		const double Step = Field.Step();
		const int32 R = Field.Resolution;
		const double FI = FMath::Clamp(Local.X / Step + (R - 1) * 0.5, 0.0, R - 1.0);
		const double FJ = FMath::Clamp(Local.Y / Step + (R - 1) * 0.5, 0.0, R - 1.0);
		const int32 I0 = FMath::Min(FMath::FloorToInt32(FI), R - 2);
		const int32 J0 = FMath::Min(FMath::FloorToInt32(FJ), R - 2);
		auto At = [&](int32 I, int32 J) { return static_cast<double>(Mask[Field.SourceIndex(I, J)] & 0xFF) / 255.0; };
		return FMath::Lerp(FMath::Lerp(At(I0, J0), At(I0 + 1, J0), FI - I0),
			FMath::Lerp(At(I0, J0 + 1), At(I0 + 1, J0 + 1), FI - I0), FJ - J0);
	}

	/**
	 * Siembra el bosque de algas: una rejilla de Spacing con desplazamiento aleatorio por
	 * celda; cada celda planta con probabilidad igual a la densidad de la máscara. Todo sale
	 * de Seed, así que cada máquina planta el mismo bosque sin replicar instancias.
	 * Transformadas en espacio local del módulo, con el pie enterrado 20 uu.
	 */
	inline TArray<FFoliageInstance> BuildFoliage(const TNTerrainModule::FModuleField& Field, TArrayView<const uint16> Mask,
		int32 Seed, const FFoliageSettings& Settings = FFoliageSettings())
	{
		TArray<FFoliageInstance> Instances;
		if (!Field.IsValid() || Mask.Num() != Field.Resolution * Field.Resolution || Settings.Spacing <= 0.0)
		{
			return Instances;
		}

		FRandomStream Stream(Seed);
		const double Half = Field.Size * 0.5;
		const int32 Cells = FMath::FloorToInt32(Field.Size / Settings.Spacing);
		for (int32 CI = 0; CI < Cells; ++CI)
		{
			for (int32 CJ = 0; CJ < Cells; ++CJ)
			{
				// Se consumen siempre los mismos números por celda: la densidad de una celda no
				// cambia lo que se planta en las demás (un diseñador puede retocar la máscara).
				const double JitterX = Stream.FRand();
				const double JitterY = Stream.FRand();
				const double Roll = Stream.FRand();
				const double ShapeRoll = Stream.FRand();
				const double SizeRoll = Stream.FRand();
				const double YawRoll = Stream.FRand();
				const double TintRoll = Stream.FRand();

				const FVector2D Local(-Half + (CI + JitterX) * Settings.Spacing, -Half + (CJ + JitterY) * Settings.Spacing);
				if (Roll >= SampleFoliageDensity(Field, Mask, Local)) { continue; }

				FFoliageInstance& Instance = Instances.AddDefaulted_GetRef();
				const double Ground = TNTerrainModule::SampleHeight(Field, Local) - 20.0;
				const double Tall = FMath::Lerp(Settings.MinTall, Settings.MaxTall, SizeRoll * SizeRoll);
				FVector Scale;
				double PivotZ = 0.0;
				if (ShapeRoll < Settings.BushShare)
				{
					Instance.Shape = EFoliageShape::Bush;
					const double Width = FMath::Lerp(150.0, 320.0, SizeRoll);
					Scale = FVector(Width, Width * 0.8, Width * 0.55) / EngineShapeSize;
					PivotZ = Width * 0.2;   // la mitad inferior de la esfera queda enterrada
				}
				else if (ShapeRoll < Settings.BushShare + Settings.FrondShare)
				{
					Instance.Shape = EFoliageShape::Frond;
					Scale = FVector(90.0, 60.0, Tall) / EngineShapeSize;
					PivotZ = Tall * 0.5;
				}
				else
				{
					Instance.Shape = EFoliageShape::Stalk;
					Scale = FVector(45.0, 45.0, Tall) / EngineShapeSize;
					PivotZ = Tall * 0.5;
				}
				Instance.Transform = FTransform(FRotator(0.0, YawRoll * 360.0, 0.0),
					FVector(Local.X, Local.Y, Ground + PivotZ), Scale);
				Instance.Color = FMath::Lerp(FLinearColor(0.05f, 0.20f, 0.04f), FLinearColor(0.22f, 0.36f, 0.06f),
					static_cast<float>(TintRoll));
			}
		}
		return Instances;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Regiones del mapa
	// ─────────────────────────────────────────────────────────────────────────

	/** Bioma deseado para una celda del camino. Secondary != Primary: celda de frontera.
	 *  bOpen: la región es abierta (explanada de arena o mar con islas): entre celdas
	 *  conectadas de la misma región no hay pared. */
	struct FCellBiome
	{
		ETNTerrainBiome Primary = ETNTerrainBiome::Sand;
		ETNTerrainBiome Secondary = ETNTerrainBiome::Sand;
		bool bOpen = false;

		bool IsMixed() const { return Primary != Secondary; }
	};

	/** Probabilidad (%) de que una región de ese bioma sea abierta. Las algas son siempre
	 *  bosque cerrado. */
	inline int32 OpenRegionChance(ETNTerrainBiome Biome)
	{
		switch (Biome)
		{
			case ETNTerrainBiome::Sand:  return 30;
			case ETNTerrainBiome::Water: return 55;
			default:                     return 0;
		}
	}

	/** Celda de una región de mar abierto: dentro no hay fronteras y el camino es único. */
	inline bool IsOpenWater(const FCellBiome& Cell)
	{
		return Cell.bOpen && Cell.Primary == ETNTerrainBiome::Water;
	}

	/**
	 * Tipo de borde del lado compartido por dos celdas de la misma región abierta:
	 *   - mar: agua entre cualquier par de celdas vecinas, conectadas o no (un solo mar sin
	 *     fronteras; el camino de islas lo marcan los módulos);
	 *   - explanada: abierto solo entre celdas conectadas.
	 * Cresta en todos los demás casos (otra región, fuera del mapa, explanadas sin conexión).
	 */
	inline ETNTerrainEdge EdgeBetween(const FCellBiome& A, const FCellBiome& B, bool bConnected)
	{
		if (!A.bOpen || !B.bOpen || A.Primary != B.Primary) { return ETNTerrainEdge::Crest; }
		switch (A.Primary)
		{
			case ETNTerrainBiome::Water: return ETNTerrainEdge::Water;
			case ETNTerrainBiome::Sand:  return bConnected ? ETNTerrainEdge::Open : ETNTerrainEdge::Crest;
			default:                     return ETNTerrainEdge::Crest;
		}
	}

	/**
	 * Reparte biomas a lo largo de un camino de PathLength celdas: 1 región por cada 4
	 * celdas (máximo NumBiomes), en un orden de biomas sorteado y con longitudes parecidas.
	 * La primera celda de cada región, salvo la primera, es una frontera mixta
	 * (anterior -> nuevo). RandRange(Min, Max) inclusivo, como en ATN_GridMapGenerator.
	 */
	inline TArray<FCellBiome> PlanPathBiomes(int32 PathLength, TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
	{
		TArray<FCellBiome> Plan;
		if (PathLength <= 0) { return Plan; }

		ETNTerrainBiome Order[NumBiomes] = { ETNTerrainBiome::Sand, ETNTerrainBiome::Water, ETNTerrainBiome::Algae };
		for (int32 I = NumBiomes - 1; I > 0; --I)
		{
			Swap(Order[I], Order[RandRange(0, I)]);
		}
		const int32 Regions = FMath::Clamp(PathLength / 4, 1, NumBiomes);
		bool bOpenRegion[NumBiomes] = {};
		for (int32 K = 0; K < Regions; ++K)
		{
			bOpenRegion[K] = RandRange(0, 99) < OpenRegionChance(Order[K]);
		}

		// Cortes: región k empieza en ~k * L / Regions, con ±1 celda de juego.
		TArray<int32> Starts = { 0 };
		for (int32 K = 1; K < Regions; ++K)
		{
			const int32 Nominal = K * PathLength / Regions;
			if (Starts.Last() + 2 > PathLength - 1) { break; }   // sin sitio: menos regiones
			Starts.Add(FMath::Clamp(Nominal + RandRange(-1, 1), Starts.Last() + 2, PathLength - 1));
		}

		Plan.SetNum(PathLength);
		int32 Region = 0;
		for (int32 I = 0; I < PathLength; ++I)
		{
			while (Region + 1 < Starts.Num() && I >= Starts[Region + 1]) { ++Region; }
			Plan[I].Primary = Order[Region];
			Plan[I].Secondary = Order[Region];
			Plan[I].bOpen = bOpenRegion[Region];
			if (Region > 0 && I == Starts[Region])
			{
				// Frontera: la forma es la del bioma que empieza; el color funde con el anterior.
				Plan[I].Secondary = Order[Region - 1];
			}
		}
		return Plan;
	}

	/**
	 * Cuánto encaja un módulo (Biome, Secondary) con lo que pide una celda: 3 = mismo
	 * bioma y misma frontera; 2 = misma pareja de biomas en otro orden; 1 = el bioma
	 * principal coincide; 0 = no encaja.
	 */
	inline int32 BiomeMatchScore(ETNTerrainBiome ModuleBiome, ETNTerrainBiome ModuleSecondary, const FCellBiome& Wanted)
	{
		if (ModuleBiome == Wanted.Primary && ModuleSecondary == Wanted.Secondary) { return 3; }
		if (Wanted.IsMixed() && ModuleBiome == Wanted.Secondary && ModuleSecondary == Wanted.Primary) { return 2; }
		if (ModuleBiome == Wanted.Primary) { return 1; }
		return 0;
	}
}
