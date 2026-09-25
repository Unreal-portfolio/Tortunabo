#pragma once

#include "CoreMinimal.h"
#include "World/TN_TerrainModuleDecisions.h"

/**
 * Túnel de verdad, como función PURA: el heightfield del módulo es el SUELO y, en la huella
 * del túnel, dos capas más en la misma rejilla lo cubren — el TECHO (cima de la colina) y la
 * BÓVEDA (lo que se ve desde dentro). Todo va en la misma malla que el suelo:
 *
 *   - Donde una capa coincide con el suelo (anillo lateral de la huella), su vértice ES el
 *     vértice del suelo: techo, bóveda y suelo quedan cosidos sin juntas.
 *   - En los quads con las cuatro esquinas cubiertas se añaden la cara del techo (hacia
 *     arriba) y la de la bóveda (hacia abajo).
 *   - En el borde de la huella donde techo y bóveda no coinciden (las dos bocas) se añade
 *     la pared que los une: la ladera en la que se abre la cueva.
 *
 * El resultado es una superficie cerrada y orientable (cada arista del casquete la comparten
 * exactamente dos triángulos, en sentidos opuestos), sin secciones ni piezas aparte.
 */
namespace TNTerrainTunnel
{
	/** Capas del túnel de un módulo, con la codificación de Heights (0 = sin techo). */
	struct FTunnelLayers
	{
		TArrayView<const uint16> Roof;
		TArrayView<const uint16> Ceiling;
		/** Suelo del asset tal como se generó (sin costa): la costura se decide comparando
		 *  valores codificados, así que es exacta aunque luego la costa mueva el suelo. */
		TArrayView<const uint16> SourceFloor;

		bool IsValid(int32 Resolution) const
		{
			const int32 Count = Resolution * Resolution;
			return Resolution >= 2 && Roof.Num() == Count && Ceiling.Num() == Count && SourceFloor.Num() == Count;
		}
	};

	/**
	 * Añade techo, bóveda y bocas a Mesh, que debe ser la malla del suelo de Field tal como
	 * la hace TNTerrainModule::BuildModuleMesh (un vértice por muestra, índice I * R + J).
	 * Devuelve false, sin tocar Mesh, si las capas o la malla no casan con el campo.
	 */
	inline bool AppendTunnelLayers(TNGridTerrain::FTileMesh& Mesh, const TNTerrainModule::FModuleField& Field,
		const FTunnelLayers& Layers, const TNTerrainModule::FModuleColors& Colors)
	{
		const int32 R = Field.Resolution;
		if (!Field.IsValid() || !Layers.IsValid(R) || Mesh.Vertices.Num() != R * R)
		{
			return false;
		}

		const double Step = Field.Step();
		const double Half = Field.Size * 0.5;
		auto Covered = [&](int32 I, int32 J)
		{
			return I >= 0 && I < R && J >= 0 && J < R && Layers.Roof[Field.SourceIndex(I, J)] != 0;
		};
		// Altura de una capa en (I, J): la capa sobre el suelo YA COLOCADO (con costa), fuera
		// de la huella el propio suelo. Así las normales del borde no ven un escalón.
		auto LayerHeight = [&](TArrayView<const uint16> Layer, int32 I, int32 J)
		{
			I = FMath::Clamp(I, 0, R - 1);
			J = FMath::Clamp(J, 0, R - 1);
			const int32 K = Field.SourceIndex(I, J);
			if (Layer[K] == 0) { return Field.HeightAt(I, J); }
			return Field.HeightAt(I, J) + (static_cast<double>(Layer[K]) - Layers.SourceFloor[K]) * Field.HeightScale;
		};
		auto AddVertex = [&](TArrayView<const uint16> Layer, int32 I, int32 J, bool bFacingDown)
		{
			const double SlopeX = (LayerHeight(Layer, I + 1, J) - LayerHeight(Layer, I - 1, J)) / (2.0 * Step);
			const double SlopeY = (LayerHeight(Layer, I, J + 1) - LayerHeight(Layer, I, J - 1)) / (2.0 * Step);
			FVector Normal = FVector(-SlopeX, -SlopeY, 1.0).GetSafeNormal();
			if (bFacingDown) { Normal = -Normal; }
			const double Height = LayerHeight(Layer, I, J);
			const FVector2D Local(I * Step - Half, J * Step - Half);
			Mesh.Vertices.Add(FVector(Local.X, Local.Y, Height));
			Mesh.Normals.Add(Normal);
			// La bóveda mira hacia abajo: se colorea como pared (roca), no como suelo.
			const FVector ColorNormal = bFacingDown ? FVector(Normal.X, Normal.Y, 0.0).GetSafeNormal() : Normal;
			Mesh.Colors.Add(TNTerrainModule::SampleModuleColor(Colors, Height, ColorNormal,
				TNTerrainModule::ColorVariation(Local)));
			return Mesh.Vertices.Num() - 1;
		};

		// Vértices de techo y bóveda; los que coinciden con otra capa reutilizan su índice.
		TArray<int32> RoofIndex;
		TArray<int32> CeilingIndex;
		RoofIndex.Init(INDEX_NONE, R * R);
		CeilingIndex.Init(INDEX_NONE, R * R);
		for (int32 I = 0; I < R; ++I)
		{
			for (int32 J = 0; J < R; ++J)
			{
				if (!Covered(I, J)) { continue; }
				const int32 K = Field.SourceIndex(I, J);
				const int32 FloorVertex = I * R + J;
				const uint16 Floor = Layers.SourceFloor[K];
				const uint16 Roof = FMath::Max(Layers.Roof[K], Floor);
				const uint16 Ceiling = FMath::Clamp(Layers.Ceiling[K], Floor, Roof);
				RoofIndex[FloorVertex] = Roof == Floor ? FloorVertex : AddVertex(Layers.Roof, I, J, false);
				if (Roof == Floor)
				{
					// Vértice de la costura: ahora es borde del techo, no del talud que queda
					// dentro de la roca. Su normal sigue a la superficie de arriba.
					const double SlopeX = (LayerHeight(Layers.Roof, I + 1, J) - LayerHeight(Layers.Roof, I - 1, J)) / (2.0 * Step);
					const double SlopeY = (LayerHeight(Layers.Roof, I, J + 1) - LayerHeight(Layers.Roof, I, J - 1)) / (2.0 * Step);
					Mesh.Normals[FloorVertex] = FVector(-SlopeX, -SlopeY, 1.0).GetSafeNormal();
				}
				CeilingIndex[FloorVertex] = Ceiling == Floor ? FloorVertex
					: (Ceiling == Roof ? RoofIndex[FloorVertex] : AddVertex(Layers.Ceiling, I, J, true));
			}
		}

		const int32 FloorVertexCount = R * R;
		auto IsFloor = [&](int32 Index) { return Index < FloorVertexCount; };
		auto AddTriangle = [&](int32 A, int32 B, int32 C)
		{
			if (A == B || B == C || A == C) { return; }
			// Un triángulo de solo vértices del suelo repetiría una cara del suelo.
			if (IsFloor(A) && IsFloor(B) && IsFloor(C)) { return; }
			Mesh.Triangles.Append({ A, B, C });
		};
		// Sentido: la cara visible de Unreal es la opuesta a (B-A)x(C-A) (ver BuildGridTriangles).
		auto AddFacing = [&](int32 A, int32 B, int32 C, const FVector& Outward)
		{
			const FVector Cross = FVector::CrossProduct(Mesh.Vertices[B] - Mesh.Vertices[A], Mesh.Vertices[C] - Mesh.Vertices[A]);
			if (FVector::DotProduct(Cross, Outward) > 0.0) { Swap(B, C); }
			AddTriangle(A, B, C);
		};
		auto QuadCovered = [&](int32 I, int32 J)
		{
			return I >= 0 && J >= 0 && I < R - 1 && J < R - 1
				&& Covered(I, J) && Covered(I + 1, J) && Covered(I + 1, J + 1) && Covered(I, J + 1);
		};

		for (int32 I = 0; I < R - 1; ++I)
		{
			for (int32 J = 0; J < R - 1; ++J)
			{
				if (!QuadCovered(I, J)) { continue; }
				const int32 V0 = I * R + J, V1 = (I + 1) * R + J, V2 = (I + 1) * R + J + 1, V3 = I * R + J + 1;
				// Techo con el mismo orden que el suelo (cara hacia +Z); bóveda al revés.
				AddTriangle(RoofIndex[V0], RoofIndex[V3], RoofIndex[V1]);
				AddTriangle(RoofIndex[V1], RoofIndex[V3], RoofIndex[V2]);
				AddTriangle(CeilingIndex[V0], CeilingIndex[V1], CeilingIndex[V3]);
				AddTriangle(CeilingIndex[V1], CeilingIndex[V2], CeilingIndex[V3]);
			}
		}

		// Bocas: cada arista de la rejilla con un quad cubierto a un solo lado cierra el
		// casquete con la pared techo-bóveda (degenerada donde las dos capas coinciden).
		auto AddPortal = [&](int32 A, int32 B, bool bFirstSide, bool bSecondSide, const FVector& FirstToSecond)
		{
			if (bFirstSide == bSecondSide) { return; }
			const FVector Outward = bFirstSide ? FirstToSecond : -FirstToSecond;
			const int32 RA = RoofIndex[A], RB = RoofIndex[B], CA = CeilingIndex[A], CB = CeilingIndex[B];
			AddFacing(RA, RB, CB, Outward);
			AddFacing(RA, CB, CA, Outward);
		};
		for (int32 I = 0; I < R; ++I)
		{
			for (int32 J = 0; J < R; ++J)
			{
				if (!Covered(I, J)) { continue; }
				// Arista (I, J)-(I, J+1): quads (I-1, J) y (I, J).
				if (Covered(I, J + 1))
				{
					AddPortal(I * R + J, I * R + J + 1, QuadCovered(I - 1, J), QuadCovered(I, J), FVector::ForwardVector);
				}
				// Arista (I, J)-(I+1, J): quads (I, J-1) y (I, J).
				if (Covered(I + 1, J))
				{
					AddPortal(I * R + J, (I + 1) * R + J, QuadCovered(I, J - 1), QuadCovered(I, J), FVector::RightVector);
				}
			}
		}
		return true;
	}
}
