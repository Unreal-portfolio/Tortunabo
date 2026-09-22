#pragma once

#include "CoreMinimal.h"
#include "World/TN_GridPathDecisions.h"
#include "World/TN_GridTerrainDecisions.h"
#include "World/TN_TerrainModuleAsset.h"

/**
 * Módulos de terreno como funciones PURAS: sin UWorld, sin actores, sin estado.
 * ATN_TerrainModuleTile y ATN_GridMapGenerator delegan aquí; los tests cubren
 * exactamente lo que se juega.
 *
 * Un módulo es un heightfield cuadrado (UTN_TerrainModuleAsset) que ocupa una celda del
 * grid. Todos comparten el mismo perfil de borde, de modo que casan entre sí con
 * cualquier rotación. Aquí se decide:
 *   - qué salidas tiene cada topología y cómo rotan (máscara de lados),
 *   - qué rotación necesita un módulo para ofrecer unas salidas dadas,
 *   - la altura interpolada en un punto del módulo y la malla de la celda.
 */
namespace TNTerrainModule
{
	constexpr int32 NumTopologies = 6;

	/** Bit del lado, según la numeración de TNGridLogic (Norte 0, Este 1, Sur 2, Oeste 3). */
	constexpr uint8 SideBit(int32 Side) { return static_cast<uint8>(1 << Side); }

	constexpr uint8 MaskNorth = SideBit(TNGridLogic::SideNorth);
	constexpr uint8 MaskEast  = SideBit(TNGridLogic::SideEast);
	constexpr uint8 MaskSouth = SideBit(TNGridLogic::SideSouth);
	constexpr uint8 MaskWest  = SideBit(TNGridLogic::SideWest);

	/** Salidas de la topología sin rotar. */
	inline uint8 ExitMask(ETNTerrainModuleTopology Topology)
	{
		switch (Topology)
		{
			case ETNTerrainModuleTopology::Straight:   return MaskSouth | MaskNorth;
			case ETNTerrainModuleTopology::CurveLeft:  return MaskSouth | MaskWest;
			case ETNTerrainModuleTopology::CurveRight: return MaskSouth | MaskEast;
			case ETNTerrainModuleTopology::TLeft:      return MaskSouth | MaskNorth | MaskWest;
			case ETNTerrainModuleTopology::TRight:     return MaskSouth | MaskNorth | MaskEast;
			default:                                   return MaskSouth | MaskNorth | MaskEast | MaskWest;
		}
	}

	/** Máscara tras girar el módulo YawSteps cuartos de vuelta (yaw +90° lleva Norte a Este). */
	inline uint8 RotateExitMask(uint8 Mask, int32 YawSteps)
	{
		const int32 Steps = ((YawSteps % TNGridLogic::NumSides) + TNGridLogic::NumSides) % TNGridLogic::NumSides;
		uint8 Rotated = 0;
		for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
		{
			if (Mask & SideBit(Side))
			{
				Rotated |= SideBit((Side + Steps) % TNGridLogic::NumSides);
			}
		}
		return Rotated;
	}

	/** Salidas que necesita una celda de camino ya clasificada (recta o giro, con su yaw). */
	inline uint8 RequiredExitMask(const TNGridLogic::FTNGridCell& Cell)
	{
		const uint8 Canonical = (Cell.Type == TNGridLogic::ETNGridTileType::Turn)
			? (MaskSouth | MaskEast)
			: (MaskSouth | MaskNorth);
		return RotateExitMask(Canonical, Cell.YawSteps);
	}

	/**
	 * Primer YawSteps (0..3) con el que la topología ofrece EXACTAMENTE las salidas pedidas,
	 * o INDEX_NONE. Con topologías simétricas (recta, cruz) hay varias soluciones; se toma
	 * la de menor giro, así que el resultado es determinista.
	 */
	inline int32 YawStepsForExits(ETNTerrainModuleTopology Topology, uint8 RequiredMask)
	{
		const uint8 Mask = ExitMask(Topology);
		for (int32 Steps = 0; Steps < TNGridLogic::NumSides; ++Steps)
		{
			if (RotateExitMask(Mask, Steps) == RequiredMask) { return Steps; }
		}
		return INDEX_NONE;
	}

	/** Vista sobre el heightfield de un módulo, con su tamaño en el mundo. */
	struct FModuleField
	{
		int32 Resolution = 0;
		double Size = 0.0;
		TArrayView<const uint16> Heights;
		double HeightScale = 0.25;
		int32 HeightZero = 32768;

		bool IsValid() const
		{
			return Resolution >= 2 && Size > 0.0 && Heights.Num() == Resolution * Resolution;
		}

		double HeightAt(int32 I, int32 J) const
		{
			const int32 Row = FMath::Clamp(I, 0, Resolution - 1);
			const int32 Col = FMath::Clamp(J, 0, Resolution - 1);
			return (static_cast<double>(Heights[Row * Resolution + Col]) - HeightZero) * HeightScale;
		}

		/** Separación entre vértices, en uu. */
		double Step() const { return Size / (Resolution - 1); }
	};

	inline FModuleField MakeField(const UTN_TerrainModuleAsset& Asset, double Size)
	{
		FModuleField Field;
		Field.Resolution = Asset.Resolution;
		Field.Size = Size;
		Field.Heights = Asset.Heights;
		Field.HeightScale = Asset.HeightScale;
		Field.HeightZero = Asset.HeightZero;
		return Field;
	}

	/** Altura interpolada (bilineal) en un punto local al módulo, con origen en su centro. */
	inline double SampleHeight(const FModuleField& Field, const FVector2D& Local)
	{
		const double Step = Field.Step();
		const double FI = FMath::Clamp(Local.X / Step + (Field.Resolution - 1) * 0.5, 0.0, Field.Resolution - 1.0);
		const double FJ = FMath::Clamp(Local.Y / Step + (Field.Resolution - 1) * 0.5, 0.0, Field.Resolution - 1.0);
		const int32 I0 = FMath::Min(FMath::FloorToInt32(FI), Field.Resolution - 2);
		const int32 J0 = FMath::Min(FMath::FloorToInt32(FJ), Field.Resolution - 2);
		const double AI = FI - I0;
		const double AJ = FJ - J0;
		return FMath::Lerp(
			FMath::Lerp(Field.HeightAt(I0, J0), Field.HeightAt(I0 + 1, J0), AI),
			FMath::Lerp(Field.HeightAt(I0, J0 + 1), Field.HeightAt(I0 + 1, J0 + 1), AI),
			AJ);
	}

	/** Alturas crudas de un lado, recorrido en el sentido en que crece el otro eje. */
	inline TArray<uint16> EdgeHeights(const FModuleField& Field, int32 Side)
	{
		TArray<uint16> Edge;
		Edge.Reserve(Field.Resolution);
		const int32 Last = Field.Resolution - 1;
		for (int32 K = 0; K < Field.Resolution; ++K)
		{
			int32 I = 0;
			int32 J = 0;
			switch (Side)
			{
				case TNGridLogic::SideNorth: I = Last; J = K; break;
				case TNGridLogic::SideSouth: I = 0;    J = K; break;
				case TNGridLogic::SideEast:  I = K;    J = Last; break;
				default:                     I = K;    J = 0; break;
			}
			Edge.Add(Field.Heights[I * Field.Resolution + J]);
		}
		return Edge;
	}

	/** Los cuatro lados son idénticos entre sí y simétricos respecto a su punto medio. */
	inline bool HasCanonicalBorder(const FModuleField& Field)
	{
		if (!Field.IsValid()) { return false; }
		const TArray<uint16> North = EdgeHeights(Field, TNGridLogic::SideNorth);
		for (int32 K = 0; K < North.Num(); ++K)
		{
			if (North[K] != North[North.Num() - 1 - K]) { return false; }
		}
		for (int32 Side = 1; Side < TNGridLogic::NumSides; ++Side)
		{
			if (EdgeHeights(Field, Side) != North) { return false; }
		}
		return true;
	}

	/** Dos módulos casan por cualquier lado: mismo borde canónico en ambos. */
	inline bool BordersMatch(const FModuleField& A, const FModuleField& B)
	{
		return HasCanonicalBorder(A) && HasCanonicalBorder(B)
			&& EdgeHeights(A, TNGridLogic::SideNorth) == EdgeHeights(B, TNGridLogic::SideNorth);
	}

	/**
	 * Transform de la instancia de cubo (de CubeSize uu de lado, centrado) que materializa
	 * un puente: escalado a Length × Width × Thickness, girado Yaw y con la cara superior
	 * en DeckHeight.
	 */
	inline FTransform BridgeInstanceTransform(const FTNTerrainModuleBridge& Bridge, double CubeSize = 100.0)
	{
		const FVector Scale(Bridge.Length / CubeSize, Bridge.Width / CubeSize, Bridge.Thickness / CubeSize);
		const FVector Location(Bridge.Center.X, Bridge.Center.Y, Bridge.DeckHeight - Bridge.Thickness * 0.5);
		return FTransform(FRotator(0.f, Bridge.Yaw, 0.f), Location, Scale);
	}

	/** Colores de vértice del módulo, en espacio lineal. */
	struct FModuleColors
	{
		FLinearColor Floor = FLinearColor(0.62f, 0.44f, 0.21f);
		FLinearColor Cliff = FLinearColor(0.075f, 0.065f, 0.06f);
		FLinearColor High = FLinearColor(0.045f, 0.04f, 0.035f);
		FLinearColor Wet = FLinearColor(0.09f, 0.065f, 0.035f);
		/** Cota del agua en uu; por debajo el suelo se pinta húmedo. */
		double WaterLevel = -400.0;
		/** Por encima de esta cota el suelo deja de ser camino y pasa a montón. */
		double HighGroundStart = 400.0;
		double HighGroundEnd = 900.0;
	};

	inline FLinearColor SampleModuleColor(const FModuleColors& Colors, double Height, const FVector& Normal)
	{
		const double Cliff = TNGridTerrain::SmoothStep(0.20, 0.55, 1.0 - Normal.Z);
		const double High = TNGridTerrain::SmoothStep(Colors.HighGroundStart, Colors.HighGroundEnd, Height);
		const double Wet = TNGridTerrain::SmoothStep(Colors.WaterLevel + 60.0, Colors.WaterLevel, Height);

		FLinearColor Color = FMath::Lerp(Colors.Floor, Colors.High, static_cast<float>(High));
		Color = FMath::Lerp(Color, Colors.Cliff, static_cast<float>(Cliff * 0.6));
		Color = FMath::Lerp(Color, Colors.Wet, static_cast<float>(Wet));
		Color.A = 1.f;
		return Color;
	}

	/**
	 * Malla del módulo en su espacio local (origen en el centro, cara hacia +Z). Un vértice
	 * por muestra del heightfield; normales por diferencias centrales de las alturas, de
	 * modo que dos módulos vecinos, al compartir borde, comparten también posiciones.
	 */
	inline TNGridTerrain::FTileMesh BuildModuleMesh(const FModuleField& Field, const FModuleColors& Colors)
	{
		TNGridTerrain::FTileMesh Mesh;
		if (!Field.IsValid()) { return Mesh; }

		const int32 R = Field.Resolution;
		const double Step = Field.Step();
		const double Half = Field.Size * 0.5;
		Mesh.Vertices.Reserve(R * R);
		Mesh.Normals.Reserve(R * R);
		Mesh.Colors.Reserve(R * R);

		for (int32 I = 0; I < R; ++I)
		{
			for (int32 J = 0; J < R; ++J)
			{
				const double Height = Field.HeightAt(I, J);
				// En el borde la diferencia central se acorta a un lado: el vecino no está.
				const double SlopeX = (Field.HeightAt(I + 1, J) - Field.HeightAt(I - 1, J))
					/ (Step * ((I > 0 && I < R - 1) ? 2.0 : 1.0));
				const double SlopeY = (Field.HeightAt(I, J + 1) - Field.HeightAt(I, J - 1))
					/ (Step * ((J > 0 && J < R - 1) ? 2.0 : 1.0));
				const FVector Normal = FVector(-SlopeX, -SlopeY, 1.0).GetSafeNormal();

				Mesh.Vertices.Add(FVector(I * Step - Half, J * Step - Half, Height));
				Mesh.Normals.Add(Normal);
				Mesh.Colors.Add(SampleModuleColor(Colors, Height, Normal));
			}
		}

		Mesh.Triangles = TNGridTerrain::BuildGridTriangles(R);
		return Mesh;
	}
}
