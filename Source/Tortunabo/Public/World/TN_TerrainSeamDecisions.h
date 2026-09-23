#pragma once

#include "CoreMinimal.h"
#include "World/TN_GridTerrainDecisions.h"
#include "World/TN_TerrainModuleDecisions.h"

/**
 * Fusión de bordes entre módulos vecinos como funciones PURAS.
 *
 * Cada módulo se diseña aislado; si se colocaran tal cual, la línea de cada celda se
 * leería en el terreno (cuadrícula). Aquí el tile funde su heightfield con el de sus
 * vecinos (hasta 8: lados y esquinas) en una banda a cada lado de la línea compartida:
 *
 *   altura(P) = Σ φ_i(P) · h_i(P) / Σ φ_i(P)
 *
 *   - φ_i(P) = w(dx) · w(dy), con d la distancia firmada de P al cuadrado de la celda i
 *     en cada eje y w(d) = 1 - smoothstep(-Band, +Band, d). Como w(d) + w(-d) = 1, sobre
 *     las celdas que rodean P los pesos suman 1 (partición de la unidad).
 *   - h_i(P): altura del módulo i en P. Fuera de su cuadrado se extiende por reflejo
 *     especular sobre su propio borde (un lago que llega al borde "continúa" al otro lado).
 *
 * Los dos tiles de un borde (y los cuatro de una esquina) evalúan la misma suma en los
 * mismos puntos, así que la costura es exacta sin que los bordes de los módulos coincidan:
 * dos lagos que se tocan se unen y el cambio de bioma se funde también en color. Una celda
 * sin módulo no aporta y los pesos se renormalizan.
 *
 * Todo se expresa en el espacio local del tile que se construye (origen en su centro,
 * +X Norte, +Y Este, sin rotar): cada vecino lleva su centro y su giro relativo.
 */
namespace TNTerrainSeam
{
	/** Una celda que aporta a la fusión: el propio módulo o un vecino. */
	struct FSeamCell
	{
		/** Alturas colocadas del módulo (con costa y espejo). */
		TNTerrainModule::FModuleField Field;
		/** Centro de la celda en el espacio local del tile que se construye, en uu. */
		FVector2D Center = FVector2D::ZeroVector;
		/** Cuartos de vuelta del módulo respecto al tile que se construye. */
		int32 RelYawSteps = 0;
		/** Paleta del bioma principal y, si es mixto, la del secundario con su máscara. */
		TNTerrainModule::FModuleColors Colors;
		TNTerrainModule::FModuleColors BlendColors;
		TArrayView<const uint16> BiomeMask;
		bool bMixed = false;
	};

	struct FSeamSettings
	{
		/** Media anchura de la banda de fusión a cada lado de la línea compartida, en uu. */
		double Band = 4000.0;
	};

	/** V girado Steps cuartos de vuelta (yaw +90° lleva +X a +Y, como FRotator). */
	inline FVector2D RotateSteps(const FVector2D& V, int32 Steps)
	{
		switch (((Steps % 4) + 4) % 4)
		{
			case 1:  return FVector2D(-V.Y, V.X);
			case 2:  return FVector2D(-V.X, -V.Y);
			case 3:  return FVector2D(V.Y, -V.X);
			default: return V;
		}
	}

	/** Peso de un eje: 1 dentro del cuadrado, 0,5 sobre su borde, 0 a Band por fuera. */
	inline double AxisWeight(double SignedOutside, double Band)
	{
		return 1.0 - TNGridTerrain::SmoothStep(-Band, Band, SignedOutside);
	}

	/** φ de la celda en P (espacio del tile). */
	inline double CellWeight(const FSeamCell& Cell, const FVector2D& P, const FSeamSettings& Settings)
	{
		const double Half = Cell.Field.Size * 0.5;
		const double DX = FMath::Abs(P.X - Cell.Center.X) - Half;
		const double DY = FMath::Abs(P.Y - Cell.Center.Y) - Half;
		return AxisWeight(DX, Settings.Band) * AxisWeight(DY, Settings.Band);
	}

	/** Refleja una coordenada local sobre el borde del cuadrado [-Half, Half]. */
	inline double ReflectAxis(double V, double Half)
	{
		if (V > Half)  { return FMath::Max(2.0 * Half - V, -Half); }
		if (V < -Half) { return FMath::Min(-2.0 * Half - V, Half); }
		return V;
	}

	/** P (espacio del tile) en el espacio local colocado de la celda, reflejado dentro de ella. */
	inline FVector2D CellLocal(const FSeamCell& Cell, const FVector2D& P)
	{
		const FVector2D Local = RotateSteps(P - Cell.Center, -Cell.RelYawSteps);
		const double Half = Cell.Field.Size * 0.5;
		return FVector2D(ReflectAxis(Local.X, Half), ReflectAxis(Local.Y, Half));
	}

	/** Peso del bioma secundario de la celda en un punto local (byte alto de la máscara). */
	inline double SecondaryWeight(const FSeamCell& Cell, const FVector2D& Local)
	{
		const TNTerrainModule::FModuleField& Field = Cell.Field;
		if (!Cell.bMixed || Cell.BiomeMask.Num() != Field.Resolution * Field.Resolution) { return 0.0; }
		const double Step = Field.Step();
		const int32 I = FMath::RoundToInt32(Local.X / Step + (Field.Resolution - 1) * 0.5);
		const int32 J = FMath::RoundToInt32(Local.Y / Step + (Field.Resolution - 1) * 0.5);
		return static_cast<double>(Cell.BiomeMask[Field.SourceIndex(I, J)] >> 8) / 255.0;
	}

	/** Altura fundida en P. Cells vacío o sin peso en P: 0. */
	inline double FusedHeight(TArrayView<const FSeamCell> Cells, const FVector2D& P, const FSeamSettings& Settings)
	{
		double Sum = 0.0;
		double WeightSum = 0.0;
		for (const FSeamCell& Cell : Cells)
		{
			const double Weight = CellWeight(Cell, P, Settings);
			if (Weight <= 0.0 || !Cell.Field.IsValid()) { continue; }
			Sum += Weight * TNTerrainModule::SampleHeight(Cell.Field, CellLocal(Cell, P));
			WeightSum += Weight;
		}
		return WeightSum > 0.0 ? Sum / WeightSum : 0.0;
	}

	/** Color fundido en P para una altura y normal ya fundidas. */
	inline FLinearColor FusedColor(TArrayView<const FSeamCell> Cells, const FVector2D& P, double Height,
		const FVector& Normal, const FSeamSettings& Settings)
	{
		FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
		double WeightSum = 0.0;
		for (const FSeamCell& Cell : Cells)
		{
			const double Weight = CellWeight(Cell, P, Settings);
			if (Weight <= 0.0 || !Cell.Field.IsValid()) { continue; }
			FLinearColor Color = TNTerrainModule::SampleModuleColor(Cell.Colors, Height, Normal);
			const double Secondary = SecondaryWeight(Cell, CellLocal(Cell, P));
			if (Secondary > 0.0)
			{
				Color = FMath::Lerp(Color, TNTerrainModule::SampleModuleColor(Cell.BlendColors, Height, Normal),
					static_cast<float>(Secondary));
			}
			Sum += Color * static_cast<float>(Weight);
			WeightSum += Weight;
		}
		if (WeightSum <= 0.0) { return FLinearColor::Black; }
		FLinearColor Result = Sum / static_cast<float>(WeightSum);
		Result.A = 1.f;
		return Result;
	}

	/**
	 * Malla fundida del tile Cells[0] (mismo trazado de vértices que BuildModuleMesh).
	 * Las normales salen de diferencias centrales sobre la altura fundida, también en el
	 * borde (se evalúa una muestra más allá), así que dos tiles vecinos comparten posición
	 * y normal en cada vértice del borde: no queda ni costura ni raya de luz.
	 */
	inline TNGridTerrain::FTileMesh BuildFusedMesh(TArrayView<const FSeamCell> Cells, const FSeamSettings& Settings = FSeamSettings())
	{
		TNGridTerrain::FTileMesh Mesh;
		if (Cells.Num() == 0 || !Cells[0].Field.IsValid()) { return Mesh; }

		const TNTerrainModule::FModuleField& Own = Cells[0].Field;
		const int32 R = Own.Resolution;
		const double Step = Own.Step();
		const double Half = Own.Size * 0.5;

		// Alturas en una rejilla con un anillo extra alrededor (índices -1..R).
		const int32 Padded = R + 2;
		TArray<double> Heights;
		Heights.SetNumUninitialized(Padded * Padded);
		for (int32 I = -1; I <= R; ++I)
		{
			for (int32 J = -1; J <= R; ++J)
			{
				const FVector2D P(Cells[0].Center.X + I * Step - Half, Cells[0].Center.Y + J * Step - Half);
				Heights[(I + 1) * Padded + (J + 1)] = FusedHeight(Cells, P, Settings);
			}
		}
		auto At = [&](int32 I, int32 J) { return Heights[(I + 1) * Padded + (J + 1)]; };

		Mesh.Vertices.Reserve(R * R);
		Mesh.Normals.Reserve(R * R);
		Mesh.Colors.Reserve(R * R);
		for (int32 I = 0; I < R; ++I)
		{
			for (int32 J = 0; J < R; ++J)
			{
				const double Height = At(I, J);
				const double SlopeX = (At(I + 1, J) - At(I - 1, J)) / (2.0 * Step);
				const double SlopeY = (At(I, J + 1) - At(I, J - 1)) / (2.0 * Step);
				const FVector Normal = FVector(-SlopeX, -SlopeY, 1.0).GetSafeNormal();
				const FVector2D P(I * Step - Half, J * Step - Half);
				Mesh.Vertices.Add(FVector(P.X, P.Y, Height));
				Mesh.Normals.Add(Normal);
				Mesh.Colors.Add(FusedColor(Cells, P + Cells[0].Center, Height, Normal, Settings));
			}
		}
		Mesh.Triangles = TNGridTerrain::BuildGridTriangles(R);
		return Mesh;
	}
}
