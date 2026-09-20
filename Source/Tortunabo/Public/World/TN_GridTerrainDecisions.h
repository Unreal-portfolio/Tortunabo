#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "World/TN_GridTerrainTypes.h"

/**
 * Terreno del mapa en grid como funciones PURAS: sin UWorld, sin actores, sin estado.
 * ATN_GridTerrainTile delega aquí, de modo que lo que cubren los tests es exactamente
 * la geometría que se juega.
 *
 * Modelo: una única función de altura H(P) sobre todo el grid, derivada del camino.
 *   - Pasillo: puntos a menos de un semiancho de la línea central del camino → suelo
 *     con relieve propio (dunas suaves, afloramientos de roca, charcos) y un carril
 *     central siempre libre de obstáculos.
 *   - Talud: al salir del semiancho la altura sube hasta la cresta en BankWidth. Es la
 *     barrera real; visualmente es un montón de basura (ver TN_GridJunkDecisions.h).
 *     Medir distancia a una polilínea redondea los giros de forma natural.
 *   - Relleno: más allá del talud, meseta con dunas, roca o cuencas bajo el agua.
 *
 * Todo se muestrea en coordenadas LOCALES AL GRID (no a la celda): X = eje de filas
 * (avance), Y = eje de columnas. Dos celdas vecinas evalúan el mismo punto en su borde
 * común y obtienen el mismo valor, así que el terreno no tiene costuras.
 */

namespace TNGridTerrain
{
	/** Estilos de celda. El camino nunca repite el estilo de la celda anterior. */
	enum class ETNTerrainStyle : uint8
	{
		Dunes,
		Rocks,
		Marsh
	};

	constexpr int32 NumStyles = 3;

	/** Parámetros que varían de celda a celda; se interpolan entre centros de celda. */
	struct FCellStyle
	{
		double HalfWidth = 0.0;
		double DuneWeight = 0.0;
		double RockWeight = 0.0;
		double Wetness = 0.0;
	};

	/** Resultado de evaluar el terreno en un punto. */
	struct FTerrainSample
	{
		double Height = 0.0;
		/** 1 en el suelo del pasillo, 0 fuera del talud. */
		double CorridorMask = 0.0;
		double RockWeight = 0.0;
		/** 1 sobre un afloramiento de roca del interior del pasillo. */
		double Obstacle = 0.0;
		/** Distancia a la línea central serpenteante del camino. */
		double CenterlineDistance = 0.0;
		/** Distancia al borde del pasillo: negativa dentro, positiva sobre el talud y más allá. */
		double EdgeDistance = 0.0;
	};

	/** Malla de una celda, en espacio local de la celda (origen en su centro). */
	struct FTileMesh
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
	};

	/** Datos derivados del mapa, comunes a todas las celdas. Se construye con BuildContext. */
	struct FTerrainContext
	{
		int32 GridSize = 0;
		double CellSize = 0.0;
		FTNGridTerrainSettings Settings;
		TArray<FVector2D> Centerline;
		TArray<FCellStyle> CellStyles;
		FVector2D NoiseOffset = FVector2D::ZeroVector;
		/** CellSize / 2000. Escala las longitudes de onda de los rasgos grandes (dunas,
		 *  serpenteo, charcas) para que un grid de celdas mayores no repita más por celda. */
		double FeatureScale = 1.0;
	};

	inline FVector2D CellCenter(double CellSize, const FIntPoint& Coord)
	{
		return FVector2D(Coord.Y * CellSize, Coord.X * CellSize);
	}

	inline FCellStyle MakeStyle(ETNTerrainStyle Style, const FTNGridTerrainSettings& Settings)
	{
		const double Min = Settings.CorridorHalfWidthMin;
		const double Max = Settings.CorridorHalfWidthMax;
		switch (Style)
		{
			case ETNTerrainStyle::Dunes: return { Max, 1.0, 0.1, 0.0 };
			case ETNTerrainStyle::Rocks: return { Min, 0.4, 1.0, 0.0 };
			default:                     return { (Min + Max) * 0.5, 0.25, 0.2, 1.0 };
		}
	}

	/**
	 * Estilo por celda (indexado fila * GridSize + columna). Las celdas de camino se
	 * asignan en orden de recorrido y nunca repiten el estilo de la anterior.
	 * @param RandRange Functor (Min, Max) → int32 en [Min, Max] inclusive.
	 */
	inline TArray<int32> AssignCellStyles(int32 GridSize, const TArray<FIntPoint>& Path,
		TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
	{
		TArray<int32> Styles;
		if (GridSize <= 0) { return Styles; }
		Styles.Init(INDEX_NONE, GridSize * GridSize);

		int32 Previous = INDEX_NONE;
		for (const FIntPoint& Cell : Path)
		{
			// Saltar 1 o 2 posiciones sobre 3 estilos nunca cae en el mismo.
			const int32 Style = (Previous == INDEX_NONE)
				? RandRange(0, NumStyles - 1)
				: (Previous + 1 + RandRange(0, NumStyles - 2)) % NumStyles;
			Styles[Cell.Y * GridSize + Cell.X] = Style;
			Previous = Style;
		}

		for (int32& Style : Styles)
		{
			if (Style == INDEX_NONE) { Style = RandRange(0, NumStyles - 1); }
		}
		return Styles;
	}

	/** Línea central del camino, con un tramo extra de una celda hacia fuera en la
	 *  entrada (Sur de la primera celda) y en la salida (Norte de la última). */
	inline TArray<FVector2D> BuildCenterline(double CellSize, const TArray<FIntPoint>& Path)
	{
		TArray<FVector2D> Centerline;
		if (Path.Num() == 0) { return Centerline; }

		Centerline.Reserve(Path.Num() + 2);
		Centerline.Add(CellCenter(CellSize, Path[0]) - FVector2D(CellSize, 0.0));
		for (const FIntPoint& Cell : Path)
		{
			Centerline.Add(CellCenter(CellSize, Cell));
		}
		Centerline.Add(CellCenter(CellSize, Path.Last()) + FVector2D(CellSize, 0.0));
		return Centerline;
	}

	/** El contexto es utilizable: hay camino y los anchos dejan sitio a una pared completa. */
	inline bool IsContextValid(const FTerrainContext& Context)
	{
		const FTNGridTerrainSettings& S = Context.Settings;
		return Context.GridSize > 0
			&& Context.CellSize > 0.0
			&& Context.Centerline.Num() >= 2
			&& Context.CellStyles.Num() == Context.GridSize * Context.GridSize
			&& S.CorridorHalfWidthMin <= S.CorridorHalfWidthMax
			&& S.CorridorMeander * UE_DOUBLE_SQRT_2 + S.WallOutlineJitter < S.CorridorHalfWidthMin
			&& S.InteriorLaneHalfWidth > 0.0
			&& S.InteriorLaneHalfWidth * 1.6 + S.WallOutlineJitter < S.CorridorHalfWidthMin
			&& S.InteriorRockHeight < S.WallHeight * 0.5
			&& S.CorridorHalfWidthMax + S.BankWidth <= Context.CellSize * 0.5;
	}

	/** Función pura de sus entradas: la misma semilla da siempre el mismo contexto. */
	inline FTerrainContext BuildContext(int32 Seed, int32 GridSize, double CellSize,
		const TArray<FIntPoint>& Path, const FTNGridTerrainSettings& Settings)
	{
		FTerrainContext Context;
		Context.GridSize = GridSize;
		Context.CellSize = CellSize;
		Context.FeatureScale = CellSize / 2000.0;
		Context.Settings = Settings;
		Context.Centerline = BuildCenterline(CellSize, Path);

		FRandomStream Stream(Seed);
		// Desfase no entero: el ruido Perlin vale 0 en todos los puntos de retícula entera.
		Context.NoiseOffset = FVector2D(Stream.FRandRange(0.f, 4096.f) + 0.37, Stream.FRandRange(0.f, 4096.f) + 0.61);

		const TArray<int32> StyleIndices = AssignCellStyles(GridSize, Path,
			[&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); });
		Context.CellStyles.Reserve(StyleIndices.Num());
		for (const int32 Index : StyleIndices)
		{
			Context.CellStyles.Add(MakeStyle(static_cast<ETNTerrainStyle>(Index), Settings));
		}
		return Context;
	}

	inline double DistanceToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double LengthSquared = AB.SizeSquared();
		const double T = LengthSquared > 0.0 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / LengthSquared, 0.0, 1.0) : 0.0;
		return FVector2D::Distance(P, A + AB * T);
	}

	inline double DistanceToCenterline(const FTerrainContext& Context, const FVector2D& P)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 i = 1; i < Context.Centerline.Num(); ++i)
		{
			Best = FMath::Min(Best, DistanceToSegment(P, Context.Centerline[i - 1], Context.Centerline[i]));
		}
		return Best;
	}

	/** Distancia al borde exterior del grid (negativa fuera). */
	inline double DistanceToGridBorder(const FTerrainContext& Context, const FVector2D& P)
	{
		const double Low = -Context.CellSize * 0.5;
		const double High = (Context.GridSize - 0.5) * Context.CellSize;
		return FMath::Min(FMath::Min(P.X - Low, High - P.X), FMath::Min(P.Y - Low, High - P.Y));
	}

	inline double SmoothStep01(double T)
	{
		const double C = FMath::Clamp(T, 0.0, 1.0);
		return C * C * (3.0 - 2.0 * C);
	}

	inline double SmoothStep(double Edge0, double Edge1, double Value)
	{
		return SmoothStep01((Value - Edge0) / (Edge1 - Edge0));
	}

	/** Ruido Perlin en [-1, 1] con la longitud de onda dada en uu. */
	inline double Noise(const FTerrainContext& Context, const FVector2D& P, double Wavelength)
	{
		return FMath::PerlinNoise2D(P / Wavelength + Context.NoiseOffset);
	}

	/** fBm de 3 octavas, aproximadamente en [-1, 1]. */
	inline double Fbm(const FTerrainContext& Context, const FVector2D& P, double Wavelength)
	{
		return (Noise(Context, P, Wavelength)
			+ 0.5 * Noise(Context, P, Wavelength * 0.5)
			+ 0.25 * Noise(Context, P, Wavelength * 0.25)) / 1.75;
	}

	/** Ruido de crestas en [0, 1]: picos afilados donde el Perlin cruza por cero. */
	inline double Ridged(const FTerrainContext& Context, const FVector2D& P, double Wavelength)
	{
		const double Ridge = 1.0 - FMath::Abs(Noise(Context, P, Wavelength));
		return Ridge * Ridge;
	}

	/** Estilo interpolado entre los cuatro centros de celda más cercanos. */
	inline FCellStyle SampleStyle(const FTerrainContext& Context, const FVector2D& P)
	{
		const double Row = P.X / Context.CellSize;
		const double Col = P.Y / Context.CellSize;
		const int32 Row0 = FMath::FloorToInt32(Row);
		const int32 Col0 = FMath::FloorToInt32(Col);
		const double RowAlpha = SmoothStep01(Row - Row0);
		const double ColAlpha = SmoothStep01(Col - Col0);

		auto StyleAt = [&Context](int32 InRow, int32 InCol) -> const FCellStyle&
		{
			const int32 R = FMath::Clamp(InRow, 0, Context.GridSize - 1);
			const int32 C = FMath::Clamp(InCol, 0, Context.GridSize - 1);
			return Context.CellStyles[R * Context.GridSize + C];
		};

		const FCellStyle& S00 = StyleAt(Row0, Col0);
		const FCellStyle& S10 = StyleAt(Row0 + 1, Col0);
		const FCellStyle& S01 = StyleAt(Row0, Col0 + 1);
		const FCellStyle& S11 = StyleAt(Row0 + 1, Col0 + 1);

		auto Blend = [RowAlpha, ColAlpha](double V00, double V10, double V01, double V11)
		{
			return FMath::Lerp(FMath::Lerp(V00, V10, RowAlpha), FMath::Lerp(V01, V11, RowAlpha), ColAlpha);
		};

		return {
			Blend(S00.HalfWidth, S10.HalfWidth, S01.HalfWidth, S11.HalfWidth),
			Blend(S00.DuneWeight, S10.DuneWeight, S01.DuneWeight, S11.DuneWeight),
			Blend(S00.RockWeight, S10.RockWeight, S01.RockWeight, S11.RockWeight),
			Blend(S00.Wetness, S10.Wetness, S01.Wetness, S11.Wetness)
		};
	}

	/** Altura del terreno fuera del pasillo: cresta + dunas + roca, con cuencas de marisma. */
	inline double SampleHighGround(const FTerrainContext& Context, const FVector2D& P,
		const FCellStyle& Style, double CenterlineDistance)
	{
		const FTNGridTerrainSettings& S = Context.Settings;

		const double Dunes = S.DuneAmplitude * Style.DuneWeight * (Fbm(Context, P, 1600.0 * Context.FeatureScale) * 0.5 + 0.5);
		const double Rocks = S.RockAmplitude * Style.RockWeight * Ridged(Context, P, 700.0 * Context.FeatureScale)
			* (0.6 + 0.4 * Noise(Context, P, 230.0));
		const double High = S.WallHeight + Dunes + FMath::Max(0.0, Rocks);

		// Las cuencas solo se abren lejos del camino y del borde: nunca rebajan una pared.
		const double HalfCell = Context.CellSize * 0.5;
		const double Scale = Context.FeatureScale;
		const double FarFromPath = SmoothStep(HalfCell + 100.0 * Scale, HalfCell + 500.0 * Scale, CenterlineDistance);
		const double FarFromBorder = SmoothStep(300.0 * Scale, 700.0 * Scale, DistanceToGridBorder(Context, P));
		const double Pool = SmoothStep(0.30, 0.55, Style.Wetness * (Noise(Context, P, 1900.0 * Scale) * 0.5 + 0.5));
		const double BasinMask = Pool * FarFromPath * FarFromBorder;

		return FMath::Lerp(High, static_cast<double>(S.WaterLevel - S.BasinDepth), BasinMask);
	}

	/**
	 * Suelo del pasillo. No es plano: relieve suave de duna en todo el ancho, afloramientos
	 * de roca y charcos de marisma. Los afloramientos son obstáculos de verdad (bordes casi
	 * verticales), por eso NUNCA nacen dentro del carril central: siempre queda un paso
	 * libre de InteriorLaneHalfWidth a cada lado de la línea central serpenteante.
	 * @param OutObstacle Cuánto de afloramiento hay en el punto (0..1), para el color.
	 */
	inline double SampleCorridorFloor(const FTerrainContext& Context, const FVector2D& P,
		const FCellStyle& Style, double CenterlineDistance, double& OutObstacle)
	{
		const FTNGridTerrainSettings& S = Context.Settings;
		const double Scale = Context.FeatureScale;

		const double Ripple = S.FloorRippleAmplitude * Fbm(Context, P, 320.0);
		const double Relief = S.InteriorReliefAmplitude * (0.5 + 0.5 * Style.DuneWeight) * Fbm(Context, P, 1100.0 * Scale);

		const double OffLane = SmoothStep(S.InteriorLaneHalfWidth, S.InteriorLaneHalfWidth * 1.6, CenterlineDistance);
		const double Blob = SmoothStep(0.56, 0.72, Noise(Context, P + FVector2D(911.0, 2203.0), 430.0 * Scale) * 0.5 + 0.5);
		OutObstacle = Blob * OffLane;
		const double Outcrop = S.InteriorRockHeight * (0.45 + 0.55 * Style.RockWeight)
			* (0.75 + 0.25 * Noise(Context, P, 160.0)) * OutObstacle;

		const double Puddle = S.PuddleDepth * Style.Wetness
			* SmoothStep(0.50, 0.72, Noise(Context, P + FVector2D(3301.0, 707.0), 650.0 * Scale) * 0.5 + 0.5);

		return Ripple + Relief + Outcrop - Puddle;
	}

	inline FTerrainSample EvaluateTerrain(const FTerrainContext& Context, const FVector2D& P)
	{
		const FTNGridTerrainSettings& S = Context.Settings;
		const double Scale = Context.FeatureScale;
		const FCellStyle Style = SampleStyle(Context, P);

		// Deformación del dominio: la distancia se mide desde un punto desplazado por ruido
		// de baja frecuencia, así que el pasillo serpentea en vez de seguir rectas perfectas.
		// Se apaga junto al borde del grid para no mover la entrada ni la salida.
		const double MeanderFade = SmoothStep(0.0, 800.0 * Scale, DistanceToGridBorder(Context, P));
		const FVector2D Meander = FVector2D(Noise(Context, P, 1300.0 * Scale), Noise(Context, P + FVector2D(5171.0, 3137.0), 1300.0 * Scale))
			* (S.CorridorMeander * MeanderFade);
		const double Distance = DistanceToCenterline(Context, P + Meander);

		// El contorno irregular solo empuja la pared HACIA el pasillo (el término es >= 0):
		// estrecha el paso, pero nunca adelgaza la pared entre dos pasillos vecinos.
		// Su longitud de onda escala con la celda: si el ruido varía demasiado deprisa
		// respecto a su amplitud, estira el talud y deja tramos de pared caminables.
		const double Jitter = S.WallOutlineJitter * (0.35 + 0.65 * Style.RockWeight)
			* (Noise(Context, P, 300.0 * Scale) * 0.5 + 0.5);
		const double WallDistance = Distance + Jitter;

		FTerrainSample Sample;
		Sample.RockWeight = Style.RockWeight;
		Sample.CenterlineDistance = Distance;
		Sample.EdgeDistance = WallDistance - Style.HalfWidth;
		// El talud solo puede estrecharse (más vertical), nunca ensancharse: así la cresta
		// sigue completa dentro del margen que reserva el invariante de anchos.
		const double LocalBankWidth = S.BankWidth * (0.65 + 0.35 * (Noise(Context, P, 520.0 * Scale) * 0.5 + 0.5));
		Sample.CorridorMask = 1.0 - SmoothStep(Style.HalfWidth, Style.HalfWidth + LocalBankWidth, WallDistance);

		double Obstacle = 0.0;
		const double Floor = SampleCorridorFloor(Context, P, Style, Distance, Obstacle);
		Sample.Obstacle = Obstacle * Sample.CorridorMask;
		const double High = SampleHighGround(Context, P, Style, Distance);
		Sample.Height = FMath::Lerp(High, Floor, Sample.CorridorMask);
		return Sample;
	}

	inline double SampleHeight(const FTerrainContext& Context, const FVector2D& P)
	{
		return EvaluateTerrain(Context, P).Height;
	}

	/** Paleta compartida por los objetos de basura y por el moteado de los montones. */
	constexpr int32 JunkPaletteSize = 15;

	inline FLinearColor JunkPaletteColor(int32 Index)
	{
		switch (((Index % JunkPaletteSize) + JunkPaletteSize) % JunkPaletteSize)
		{
			// Bolsas
			case 0:  return FLinearColor(0.012f, 0.012f, 0.014f);
			case 1:  return FLinearColor(0.16f, 0.17f, 0.18f);
			case 2:  return FLinearColor(0.62f, 0.62f, 0.58f);
			case 3:  return FLinearColor(0.03f, 0.16f, 0.42f);
			// Cartón y madera
			case 4:  return FLinearColor(0.30f, 0.19f, 0.09f);
			case 5:  return FLinearColor(0.21f, 0.12f, 0.05f);
			case 6:  return FLinearColor(0.40f, 0.30f, 0.17f);
			// Bidones, tuberías y contenedores
			case 7:  return FLinearColor(0.02f, 0.12f, 0.36f);
			case 8:  return FLinearColor(0.26f, 0.07f, 0.02f);
			case 9:  return FLinearColor(0.42f, 0.03f, 0.03f);
			case 10: return FLinearColor(0.04f, 0.22f, 0.09f);
			// Neumáticos
			case 11: return FLinearColor(0.010f, 0.010f, 0.011f);
			// Electrodomésticos
			case 12: return FLinearColor(0.66f, 0.67f, 0.66f);
			case 13: return FLinearColor(0.30f, 0.31f, 0.32f);
			// Conos de obra
			default: return FLinearColor(0.80f, 0.20f, 0.02f);
		}
	}

	/** Entero pseudoaleatorio estable para una casilla de moteado (~45 uu). */
	inline uint32 SpeckleHash(const FVector2D& P)
	{
		const uint32 X = static_cast<uint32>(FMath::FloorToInt32(P.X / 45.0));
		const uint32 Y = static_cast<uint32>(FMath::FloorToInt32(P.Y / 45.0));
		uint32 Value = X * 0x9e3779b1u ^ (Y * 0x85ebca6bu + 0x7f4a7c15u);
		Value ^= Value >> 15;
		Value *= 0x2c1b3c6du;
		Value ^= Value >> 12;
		return Value;
	}

	/** Color de vértice final: todo el "material" del terreno se decide aquí. */
	inline FLinearColor SampleColor(const FTerrainContext& Context, const FVector2D& P,
		const FTerrainSample& Sample, const FVector& Normal)
	{
		const FTNGridTerrainSettings& S = Context.Settings;

		const double Slope = 1.0 - Normal.Z;
		const double Cliff = SmoothStep(0.20, 0.55, Slope);
		const double Rock = FMath::Clamp(
			Cliff * (0.55 + 0.45 * Sample.RockWeight)
			+ 0.30 * Sample.RockWeight * (1.0 - Sample.CorridorMask)
			+ 0.85 * Sample.Obstacle, 0.0, 1.0);
		// Húmedo solo por debajo de la cota del suelo: charcos y orillas, no el pasillo entero.
		const double Wet = SmoothStep(S.WaterLevel + 45.0, S.WaterLevel + 5.0, Sample.Height);

		// Todo lo que no es suelo del pasillo es montón de basura: tono oscuro salpicado de
		// trozos de color. Es lo que hace que el talud deje de leerse como una pared de roca.
		const uint32 Speckle = SpeckleHash(P);
		const bool bSpeckled = (Speckle & 0xffffu) / 65535.0 < S.HeapSpeckleAmount;
		// Entre los restos asoma arena a manchas, para que el montón no sea una masa uniforme.
		const FLinearColor HeapBase = FMath::Lerp(S.HeapColor, S.SandColor,
			static_cast<float>(0.22 * SmoothStep(0.45, 0.75, Noise(Context, P, 900.0) * 0.5 + 0.5)));
		const FLinearColor Heap = bSpeckled ? JunkPaletteColor(static_cast<int32>(Speckle >> 16)) : HeapBase;
		const double HeapMask = FMath::Clamp(FMath::Max(1.0 - Sample.CorridorMask, Sample.Obstacle), 0.0, 1.0);

		FLinearColor Color = FMath::Lerp(S.PathColor, Heap, static_cast<float>(HeapMask));
		Color = FMath::Lerp(Color, S.RockColor, static_cast<float>(Rock * 0.35));
		Color = FMath::Lerp(Color, S.WetSandColor, static_cast<float>(Wet));

		// Vetas horizontales en las paredes, onduladas por ruido: lectura de roca sedimentaria.
		const double Strata = FMath::Sin(Sample.Height * (UE_DOUBLE_TWO_PI / S.StrataPeriod) + 2.5 * Noise(Context, P, 600.0));
		const float Tint = static_cast<float>(1.0 + 0.09 * Noise(Context, P, 95.0) + 0.07 * Noise(Context, P, 740.0)
			+ 0.20 * Cliff * Strata);
		return FLinearColor(Color.R * Tint, Color.G * Tint, Color.B * Tint, 1.f);
	}

	/** Índices de una rejilla de V×V vértices (índice = i * V + j), con la cara hacia +Z. */
	inline TArray<int32> BuildGridTriangles(int32 V)
	{
		TArray<int32> Triangles;
		Triangles.Reserve((V - 1) * (V - 1) * 6);
		for (int32 i = 0; i < V - 1; ++i)
		{
			for (int32 j = 0; j < V - 1; ++j)
			{
				const int32 I0 = i * V + j;
				const int32 I1 = (i + 1) * V + j;
				const int32 I2 = (i + 1) * V + (j + 1);
				const int32 I3 = i * V + (j + 1);
				// Unreal toma como cara frontal la de sentido horario: con este orden el
				// producto vectorial (B-A)x(C-A) apunta a -Z y la cara visible mira a +Z.
				Triangles.Append({ I0, I3, I1, I1, I3, I2 });
			}
		}
		return Triangles;
	}

	/**
	 * Malla de la celda Coord. Las alturas se muestrean en una rejilla con un anillo de
	 * margen para sacar las normales por diferencias centrales de H (no de la malla):
	 * el borde común de dos celdas tiene posiciones Y normales idénticas.
	 */
	inline FTileMesh BuildTileMesh(const FTerrainContext& Context, const FIntPoint& Coord)
	{
		FTileMesh Mesh;
		const int32 V = Context.Settings.VertsPerSide;
		if (!IsContextValid(Context) || V < 2) { return Mesh; }

		const int32 Padded = V + 2;
		const double Step = Context.CellSize / (V - 1);
		const FVector2D Center = CellCenter(Context.CellSize, Coord);

		// Coordenada de grid como (celda - 0.5 + i/(V-1)) * CellSize: el último vértice de
		// una celda y el primero de la siguiente evalúan la MISMA expresión.
		auto GridPoint = [&](int32 i, int32 j)
		{
			return FVector2D(
				(Coord.Y - 0.5 + static_cast<double>(i) / (V - 1)) * Context.CellSize,
				(Coord.X - 0.5 + static_cast<double>(j) / (V - 1)) * Context.CellSize);
		};

		TArray<FTerrainSample> Samples;
		Samples.SetNum(Padded * Padded);
		for (int32 i = -1; i <= V; ++i)
		{
			for (int32 j = -1; j <= V; ++j)
			{
				Samples[(i + 1) * Padded + (j + 1)] = EvaluateTerrain(Context, GridPoint(i, j));
			}
		}
		auto HeightAt = [&](int32 i, int32 j) { return Samples[(i + 1) * Padded + (j + 1)].Height; };

		Mesh.Vertices.Reserve(V * V);
		Mesh.Normals.Reserve(V * V);
		Mesh.UVs.Reserve(V * V);
		Mesh.Colors.Reserve(V * V);
		for (int32 i = 0; i < V; ++i)
		{
			for (int32 j = 0; j < V; ++j)
			{
				const FVector2D P = GridPoint(i, j);
				const FTerrainSample& Sample = Samples[(i + 1) * Padded + (j + 1)];
				const double SlopeX = (HeightAt(i + 1, j) - HeightAt(i - 1, j)) / (2.0 * Step);
				const double SlopeY = (HeightAt(i, j + 1) - HeightAt(i, j - 1)) / (2.0 * Step);
				const FVector Normal = FVector(-SlopeX, -SlopeY, 1.0).GetSafeNormal();

				Mesh.Vertices.Add(FVector(P.X - Center.X, P.Y - Center.Y, Sample.Height));
				Mesh.Normals.Add(Normal);
				Mesh.UVs.Add(P / Context.CellSize);
				Mesh.Colors.Add(SampleColor(Context, P, Sample, Normal));
			}
		}

		Mesh.Triangles = BuildGridTriangles(V);
		return Mesh;
	}
}
