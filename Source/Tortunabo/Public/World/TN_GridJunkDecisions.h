#pragma once

#include "CoreMinimal.h"
#include "World/TN_GridTerrainDecisions.h"

/**
 * Basura que viste el mapa en grid, como funciones PURAS (mismo contrato que
 * TN_GridTerrainDecisions.h). Decide qué objetos hay, dónde y de qué color;
 * ATN_GridTerrainTile solo los convierte en instancias.
 *
 * Idea: el camino discurre ENTRE basura. La barrera real sigue siendo el talud del
 * terreno (es lo que garantizan los tests de TNGridTerrain); los objetos lo tapan para
 * que no se lea como una pared:
 *   - Frente: fila de objetos SÓLIDOS apilados al pie del talud, dentro del pasillo.
 *   - Talud y cresta: objetos de DECORADO (sin colisión) que rompen la silueta. El
 *     jugador no llega hasta ellos, y sin colisión no pueden servir de escalera.
 *   - Interior: algún obstáculo sólido suelto y desperdicios pequeños sin colisión.
 *
 * Reglas de jugabilidad (cubiertas por tests):
 *   - Ningún objeto sólido invade el carril central libre del pasillo.
 *   - Ningún objeto sólido sube por encima de MaxSolidTopFraction de la pared: subirse
 *     a uno no acerca a la meseta.
 *
 * Los candidatos salen de una rejilla GLOBAL con jitter por hash de (celda de rejilla,
 * semilla): cada objeto pertenece a la celda del grid que contiene su centro, no hay
 * duplicados entre celdas y servidor y clientes generan exactamente lo mismo.
 */

namespace TNGridJunk
{
	enum class EJunkShape : uint8
	{
		Cube,
		Sphere,
		Cylinder,
		Cone
	};

	constexpr int32 NumShapes = 4;

	/** Lado de las formas básicas del motor, en uu. */
	constexpr double EngineShapeSize = 100.0;

	/** Un objeto sólido nunca asoma por encima de esta fracción de WallHeight. */
	constexpr double MaxSolidTopFraction = 0.45;

	/** Profundidad de la fila de objetos sólidos al pie del talud, hacia dentro del pasillo. */
	constexpr double FrontBandDepth = 260.0;

	struct FJunkInstance
	{
		EJunkShape Shape = EJunkShape::Cube;
		/** true = bloquea al jugador. false = decorado sin colisión. */
		bool bSolid = false;
		/** En espacio local de la celda (origen en su centro). */
		FTransform Transform;
		FLinearColor Color = FLinearColor::White;
		/** Posición en coordenadas de grid y radio en planta, para validaciones. */
		FVector2D GridPosition = FVector2D::ZeroVector;
		double Radius = 0.0;
		/** Cota del punto más alto del objeto. */
		double TopHeight = 0.0;
	};

	/** Tipo de objeto: forma, tamaños (uu) y tramo de la paleta de TNGridTerrain. */
	struct FJunkKind
	{
		EJunkShape Shape;
		FVector MinSize;
		FVector MaxSize;
		int32 PaletteStart;
		int32 PaletteCount;
		double Weight;
		double MaxTiltDegrees;
		/** Solo en talud y cresta (demasiado grande para el pasillo). */
		bool bBackgroundOnly;
	};

	inline const TArray<FJunkKind>& GetJunkKinds()
	{
		static const TArray<FJunkKind> Kinds = {
			// Bolsa de basura: esfera aplastada. Lo más abundante.
			{ EJunkShape::Sphere,   FVector(70, 70, 45),    FVector(140, 130, 85),   0, 4, 0.40, 12.0, false },
			// Caja de cartón o madera.
			{ EJunkShape::Cube,     FVector(70, 70, 60),    FVector(170, 150, 130),  4, 3, 0.16, 14.0, false },
			// Bidón, de pie o algo caído.
			{ EJunkShape::Cylinder, FVector(85, 85, 110),   FVector(100, 100, 135),  7, 4, 0.12, 25.0, false },
			// Neumático: cilindro ancho y bajo.
			{ EJunkShape::Cylinder, FVector(130, 130, 35),  FVector(175, 175, 50),   11, 1, 0.10, 35.0, false },
			// Electrodoméstico: nevera, lavadora.
			{ EJunkShape::Cube,     FVector(100, 95, 150),  FVector(125, 115, 230),  12, 2, 0.08, 10.0, false },
			// Cono de obra.
			{ EJunkShape::Cone,     FVector(55, 55, 85),    FVector(70, 70, 105),    14, 1, 0.05, 20.0, false },
			// Tubería o tablón tumbado.
			{ EJunkShape::Cylinder, FVector(28, 28, 300),   FVector(45, 45, 520),    7, 4, 0.05, 90.0, true },
			// Contenedor o casco de barca: la pieza grande que rompe la silueta.
			{ EJunkShape::Cube,     FVector(420, 210, 200), FVector(640, 260, 270),  7, 4, 0.04, 8.0, true },
		};
		return Kinds;
	}

	/** Mezclador entero (finalizador de murmur3): determinista en cualquier plataforma. */
	inline uint32 MixBits(uint32 Value)
	{
		Value ^= Value >> 16;
		Value *= 0x85ebca6bu;
		Value ^= Value >> 13;
		Value *= 0xc2b2ae35u;
		Value ^= Value >> 16;
		return Value;
	}

	/** Aleatorio en [0, 1) ligado a una celda de la rejilla de candidatos. */
	inline double HashRandom(int32 GridX, int32 GridY, int32 Seed, uint32 Salt)
	{
		const uint32 Mixed = MixBits(static_cast<uint32>(GridX) * 0x9e3779b1u
			^ MixBits(static_cast<uint32>(GridY) * 0x7f4a7c15u ^ MixBits(static_cast<uint32>(Seed) + Salt * 0x632be5abu)));
		return (Mixed >> 8) / 16777216.0;
	}

	inline int32 PickKind(double Roll, bool bAllowBackgroundOnly)
	{
		const TArray<FJunkKind>& Kinds = GetJunkKinds();
		double Total = 0.0;
		for (const FJunkKind& Kind : Kinds)
		{
			if (bAllowBackgroundOnly || !Kind.bBackgroundOnly) { Total += Kind.Weight; }
		}

		double Cursor = Roll * Total;
		for (int32 i = 0; i < Kinds.Num(); ++i)
		{
			if (!bAllowBackgroundOnly && Kinds[i].bBackgroundOnly) { continue; }
			Cursor -= Kinds[i].Weight;
			if (Cursor <= 0.0) { return i; }
		}
		return 0;
	}

	/** Zona del mapa en la que cae un candidato. Decide densidad y si el objeto es sólido. */
	enum class EJunkZone : uint8
	{
		None,
		Interior,
		Front,
		Background
	};

	inline EJunkZone ClassifyZone(const TNGridTerrain::FTerrainContext& Context, const TNGridTerrain::FTerrainSample& Sample)
	{
		const FTNGridTerrainSettings& S = Context.Settings;
		if (Sample.EdgeDistance < -FrontBandDepth) { return EJunkZone::Interior; }
		if (Sample.EdgeDistance <= 0.0) { return EJunkZone::Front; }
		if (Sample.EdgeDistance <= S.BankWidth + S.JunkCrestDepth) { return EJunkZone::Background; }
		return EJunkZone::None;
	}

	/**
	 * Basura de la celda Coord. Vacío si el contexto no es válido o JunkSpacing <= 0.
	 */
	inline TArray<FJunkInstance> BuildTileJunk(const TNGridTerrain::FTerrainContext& Context, int32 Seed, const FIntPoint& Coord)
	{
		using namespace TNGridTerrain;

		TArray<FJunkInstance> Junk;
		const FTNGridTerrainSettings& S = Context.Settings;
		if (!IsContextValid(Context) || S.JunkSpacing <= 0.f) { return Junk; }

		const double Spacing = S.JunkSpacing;
		const FVector2D Center = CellCenter(Context.CellSize, Coord);
		const FVector2D TileMin = Center - FVector2D(Context.CellSize * 0.5);
		const FVector2D TileMax = Center + FVector2D(Context.CellSize * 0.5);
		const double MaxSolidTop = S.WallHeight * MaxSolidTopFraction;

		const int32 MinX = FMath::FloorToInt32(TileMin.X / Spacing) - 1;
		const int32 MaxX = FMath::FloorToInt32(TileMax.X / Spacing) + 1;
		const int32 MinY = FMath::FloorToInt32(TileMin.Y / Spacing) - 1;
		const int32 MaxY = FMath::FloorToInt32(TileMax.Y / Spacing) + 1;

		for (int32 X = MinX; X <= MaxX; ++X)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				auto Random = [X, Y, Seed](uint32 Salt) { return HashRandom(X, Y, Seed, Salt); };

				const FVector2D P((X + 0.15 + 0.7 * Random(1)) * Spacing, (Y + 0.15 + 0.7 * Random(2)) * Spacing);
				// Intervalo semiabierto: un candidato en el borde pertenece a una sola celda.
				if (P.X < TileMin.X || P.X >= TileMax.X || P.Y < TileMin.Y || P.Y >= TileMax.Y) { continue; }

				const FTerrainSample Sample = EvaluateTerrain(Context, P);
				const EJunkZone Zone = ClassifyZone(Context, Sample);
				if (Zone == EJunkZone::None) { continue; }

				// Interior: casi todo desperdicio pequeño sin colisión; de vez en cuando un obstáculo.
				const bool bLitter = Zone == EJunkZone::Interior && Random(3) < 0.7;
				double Chance = 0.0;
				switch (Zone)
				{
					case EJunkZone::Interior:   Chance = bLitter ? 0.10 : 0.16; break;
					case EJunkZone::Front:      Chance = 0.90; break;
					// El talud va casi cubierto: si se ve el terreno de detrás, vuelve a leerse como pared.
					default:                    Chance = FMath::Lerp(0.97, 0.35, Sample.EdgeDistance / (S.BankWidth + S.JunkCrestDepth)); break;
				}
				if (Random(4) >= Chance) { continue; }

				const FJunkKind& Kind = GetJunkKinds()[PickKind(Random(5), Zone == EJunkZone::Background)];
				FVector Size = FMath::Lerp(Kind.MinSize, Kind.MaxSize, Random(6));
				if (bLitter) { Size *= 0.35; }
				else if (Zone == EJunkZone::Background) { Size *= FMath::Lerp(1.3, 2.2, Random(7)); }

				FJunkInstance Instance;
				Instance.Shape = Kind.Shape;
				Instance.bSolid = !bLitter && Zone != EJunkZone::Background;
				Instance.GridPosition = P;
				Instance.Radius = FMath::Max(Size.X, Size.Y) * 0.5;

				// Medio hundido en el montón: asoma un 70% de su altura.
				const double VisibleHeight = Size.Z * 0.7;
				if (Instance.bSolid)
				{
					const bool bInLane = Sample.CenterlineDistance - Instance.Radius < S.InteriorLaneHalfWidth;
					const bool bTooTall = Sample.Height + VisibleHeight > MaxSolidTop;
					if (bInLane || bTooTall) { continue; }
				}
				Instance.TopHeight = Sample.Height + VisibleHeight;

				const double Tilt = Kind.MaxTiltDegrees;
				const FRotator Rotation((Random(8) * 2.0 - 1.0) * Tilt, Random(9) * 360.0, (Random(10) * 2.0 - 1.0) * Tilt);
				const FVector Location(P.X - Center.X, P.Y - Center.Y, Sample.Height + VisibleHeight - Size.Z * 0.5);
				Instance.Transform = FTransform(Rotation, Location, Size / EngineShapeSize);

				const int32 PaletteIndex = Kind.PaletteStart + FMath::Min(Kind.PaletteCount - 1, FMath::FloorToInt32(Random(11) * Kind.PaletteCount));
				// Basura vieja: cada objeto se apaga hacia un tono de mugre en distinta medida.
				const float Shade = static_cast<float>(0.75 + 0.35 * Random(12));
				const float Grime = static_cast<float>(0.30 + 0.40 * Random(13));
				Instance.Color = FMath::Lerp(JunkPaletteColor(PaletteIndex) * Shade, FLinearColor(0.085f, 0.07f, 0.055f), Grime);

				Junk.Add(Instance);
			}
		}

		return Junk;
	}
}
