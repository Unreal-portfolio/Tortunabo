#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"

/**
 * Malla del fantasmita de tortuga (ATN_SpectatorGhost, Docs/Fantasma_Espectador.md), construida en ejecución: cuerpo
 * (cabeza, cuello, caparazón con escudos y peto), dos aletas delanteras, ojitos con brillo y una cola que sale de debajo
 * del caparazón en lugar de las patas traseras y acaba enroscada hacia arriba, como la de Casper. Normales suaves (el
 * material de nube difumina el borde con ellas) y color de vértice lineal con la opacidad en el alfa, para los
 * materiales translúcidos del mapa procedural (M_ProcFXCloud y M_ProcFXSoft: emisivo = color, opacidad = alfa x
 * «Opacity»). Espacio local en cm: +X hacia delante (a la tortuga que mira), +Z arriba.
 */
namespace TNGhostMesh
{
	/** Color sRGB 0xRRGGBB a lineal (la malla procedural lo pasa tal cual al material) con la opacidad en el alfa. */
	inline FLinearColor Col(uint32 Hex, float Alpha)
	{
		return FLinearColor(TNProcRuntimeMesh::SRGBToLinear(((Hex >> 16) & 255) / 255.f), TNProcRuntimeMesh::SRGBToLinear(((Hex >> 8) & 255) / 255.f),
			TNProcRuntimeMesh::SRGBToLinear((Hex & 255) / 255.f), Alpha);
	}

	/** Buffers de una sección con vértices compartidos y normales suaves. */
	struct FSmoothMesh
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;

		void Reset()
		{
			Verts.Reset();
			Tris.Reset();
			Normals.Reset();
			UVs.Reset();
			Colors.Reset();
		}

		int32 AddVertex(const FVector& Position, const FVector& Normal, const FLinearColor& Color)
		{
			Verts.Add(Position);
			Normals.Add(Normal.GetSafeNormal());
			UVs.Add(FVector2D::ZeroVector);
			Colors.Add(Color);
			return Verts.Num() - 1;
		}

		/** Triángulo con la cara hacia Out (en UE la cara de delante de A, B, C es la de normal (C-A)x(B-A)). */
		void AddTri(int32 A, int32 B, int32 C, const FVector& Out)
		{
			const FVector Front = FVector::CrossProduct(Verts[C] - Verts[A], Verts[B] - Verts[A]);
			if (FVector::DotProduct(Front, Out) >= 0.0)
			{
				Tris.Add(A);
				Tris.Add(B);
				Tris.Add(C);
			}
			else
			{
				Tris.Add(A);
				Tris.Add(C);
				Tris.Add(B);
			}
		}

		void AddQuad(int32 A, int32 B, int32 C, int32 D, const FVector& Out)
		{
			AddTri(A, B, C, Out);
			AddTri(A, C, D, Out);
		}

		/** Crea la sección 0 del componente (sin colisión) con el material. */
		void Create(UProceduralMeshComponent* Comp, UMaterialInterface* Material) const
		{
			if (!Comp)
			{
				return;
			}
			Comp->ClearAllMeshSections();
			if (Tris.Num() == 0)
			{
				return;
			}
			const TArray<FProcMeshTangent> NoTangents;
			Comp->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, NoTangents, false, false);
			if (Material)
			{
				Comp->SetMaterial(0, Material);
			}
		}

		/** Mueve los vértices de la sección 0 (misma topología que al crearla). */
		void Update(UProceduralMeshComponent* Comp) const
		{
			if (!Comp || Tris.Num() == 0)
			{
				return;
			}
			const TArray<FProcMeshTangent> NoTangents;
			Comp->UpdateMeshSection_LinearColor(0, Verts, Normals, UVs, Colors, NoTangents, false);
		}
	};

	/** Elipsoide (o una franja de latitudes, en grados) con normales suaves; ColorAt recibe el punto de la esfera unidad. */
	inline void AddEllipsoid(FSmoothMesh& Mesh, const FVector& Center, const FVector& Radii, int32 Rings, int32 Segments,
		TFunctionRef<FLinearColor(const FVector&)> ColorAt, float LatFromDeg = -90.f, float LatToDeg = 90.f)
	{
		const int32 First = Mesh.Verts.Num();
		for (int32 Ring = 0; Ring <= Rings; ++Ring)
		{
			const float Lat = FMath::DegreesToRadians(FMath::Lerp(LatFromDeg, LatToDeg, static_cast<float>(Ring) / Rings));
			for (int32 Seg = 0; Seg <= Segments; ++Seg)
			{
				const float Lon = UE_TWO_PI * Seg / Segments;
				const FVector Unit(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat));
				const FVector Normal(Unit.X / Radii.X, Unit.Y / Radii.Y, Unit.Z / Radii.Z);
				Mesh.AddVertex(Center + Unit * Radii, Normal, ColorAt(Unit));
			}
		}
		const int32 Row = Segments + 1;
		for (int32 Ring = 0; Ring < Rings; ++Ring)
		{
			for (int32 Seg = 0; Seg < Segments; ++Seg)
			{
				const int32 A = First + Ring * Row + Seg;
				const FVector Out = (Mesh.Verts[A] + Mesh.Verts[A + Row + 1]) * 0.5 - Center;
				Mesh.AddQuad(A, A + 1, A + Row + 1, A + Row, Out);
			}
		}
	}

	/** Cabeza, cuello, caparazón con escudos (bandas algo más azules) y el peto que hace de reborde, y los coloretes. */
	inline void BuildBody(FSmoothMesh& Mesh)
	{
		const FLinearColor ShellLight = Col(0xE2F1FC, 0.9f);
		const FLinearColor ShellPlate = Col(0xAFD3F0, 0.9f);
		const FLinearColor ShellTop = Col(0xC9E4F8, 0.9f);
		AddEllipsoid(Mesh, FVector(-4.0, 0.0, 7.0), FVector(30.0, 26.0, 19.0), 7, 18, [&](const FVector& Unit)
		{
			if (Unit.Z > 0.8f)
			{
				return ShellTop;
			}
			const float Around = static_cast<float>((FMath::Atan2(Unit.Y, Unit.X) + UE_DOUBLE_PI) / UE_DOUBLE_TWO_PI);
			const int32 Plate = FMath::FloorToInt(Around * 6.f + (Unit.Z > 0.45f ? 0.5f : 0.f));
			return (Plate % 2) ? ShellPlate : ShellLight;
		}, 0.f, 90.f);
		AddEllipsoid(Mesh, FVector(-4.0, 0.0, 5.0), FVector(31.5, 27.5, 8.5), 5, 18, [](const FVector& Unit)
		{
			return FMath::Lerp(Col(0xF4FAFF, 0.85f), Col(0xBFDDF3, 0.75f), FMath::Clamp(0.5f - 0.5f * static_cast<float>(Unit.Z), 0.f, 1.f));
		});
		auto Skin = [](const FVector& Unit)
		{
			return FMath::Lerp(Col(0xBBDCF4, 0.8f), Col(0xF6FBFF, 0.9f), FMath::Clamp(0.5f + 0.5f * static_cast<float>(Unit.Z), 0.f, 1.f));
		};
		AddEllipsoid(Mesh, FVector(16.0, 0.0, 9.0), FVector(13.0, 11.5, 10.0), 6, 14, Skin);
		AddEllipsoid(Mesh, FVector(31.0, 0.0, 15.0), FVector(18.0, 16.0, 15.0), 8, 16, Skin);
		for (const double Side : { -1.0, 1.0 })
		{
			AddEllipsoid(Mesh, FVector(41.0, Side * 11.5, 9.5), FVector(4.0, 2.2, 2.8), 3, 8, [](const FVector&)
			{
				return Col(0xFFB3CB, 0.55f);
			});
		}
	}

	/** Ojos grandes y oscuros con dos brillos cada uno, y una boquita en «o». */
	inline void BuildEyes(FSmoothMesh& Mesh)
	{
		for (const double Side : { -1.0, 1.0 })
		{
			AddEllipsoid(Mesh, FVector(46.0, Side * 7.2, 18.0), FVector(2.8, 3.8, 5.6), 5, 12, [](const FVector&)
			{
				return Col(0x121A2E, 1.f);
			});
			AddEllipsoid(Mesh, FVector(48.4, Side * 6.2, 20.6), FVector(1.0, 1.2, 1.4), 3, 8, [](const FVector&)
			{
				return Col(0xFFFFFF, 1.f);
			});
			AddEllipsoid(Mesh, FVector(48.5, Side * 8.0, 16.2), FVector(0.6, 0.7, 0.8), 2, 6, [](const FVector&)
			{
				return Col(0xFFFFFF, 0.9f);
			});
		}
		AddEllipsoid(Mesh, FVector(47.8, 0.0, 9.0), FVector(1.4, 2.6, 2.0), 3, 10, [](const FVector&)
		{
			return Col(0x3A2440, 0.95f);
		});
	}

	/** Aleta delantera (espacio del hombro: Side -1 izquierda, +1 derecha), plana y hacia fuera. */
	inline void BuildFlipper(FSmoothMesh& Mesh, float Side)
	{
		AddEllipsoid(Mesh, FVector(-4.0, 12.0 * Side, -1.0), FVector(10.0, 15.0, 2.8), 4, 12, [](const FVector& Unit)
		{
			return FMath::Lerp(Col(0xC6E2F6, 0.8f), Col(0xEEF7FF, 0.88f), FMath::Clamp(0.5f + 0.5f * static_cast<float>(Unit.Z), 0.f, 1.f));
		});
	}

	/** Cola: anillos a lo largo de ella, lados de cada anillo y su largo (cm). */
	constexpr int32 TailRings = 14;
	constexpr int32 TailSides = 10;
	constexpr float TailLength = 64.f;

	/**
	 * Punto de la cola a lo largo S (0 en la base, bajo el caparazón; 1 en la punta): baja un poco, se enrosca hacia
	 * arriba al final y la recorre una onda de lado a lado (Swish 0-1: más amplia en el vuelo al huevo).
	 */
	inline FVector TailPoint(float S, float Time, float Swish)
	{
		const float X = -20.f - TailLength * S;
		const float Drop = 15.f * FMath::Sin(UE_PI * FMath::Min(S, 0.8f));
		const float Curl = 30.f * FMath::SmoothStep(0.55f, 1.f, S);
		const float Wave = FMath::Sin(Time * 4.6f - S * 5.4f);
		const float Y = (6.f + 8.f * Swish) * FMath::Pow(S, 1.15f) * Wave;
		const float Z = 1.f - Drop + Curl + 3.f * S * FMath::Sin(Time * 3.2f - S * 4.f + 1.3f);
		return FVector(X, Y, Z);
	}

	/** Cola en el instante Time: tubo que adelgaza hasta la punta (topología fija: se crea una vez y luego se mueve). */
	inline void BuildTail(FSmoothMesh& Mesh, float Time, float Swish)
	{
		Mesh.Reset();
		const float Step = 1.f / TailRings;
		for (int32 Ring = 0; Ring <= TailRings; ++Ring)
		{
			const float S = Ring * Step;
			const FVector Center = TailPoint(S, Time, Swish);
			FVector Tangent = TailPoint(FMath::Min(1.f, S + Step * 0.5f), Time, Swish) - TailPoint(FMath::Max(0.f, S - Step * 0.5f), Time, Swish);
			Tangent = Tangent.GetSafeNormal(UE_SMALL_NUMBER, FVector(-1.0, 0.0, 0.0));
			FVector SideAxis = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0, 1.0, 0.0));
			const FVector UpAxis = FVector::CrossProduct(SideAxis, Tangent).GetSafeNormal();
			const float Radius = FMath::Lerp(15.f, 1.f, FMath::Pow(S, 0.8f));
			const FLinearColor Color = FMath::Lerp(Col(0xE6F4FF, 0.85f), Col(0xF4FAFF, 0.2f), S);
			for (int32 k = 0; k < TailSides; ++k)
			{
				const float Angle = UE_TWO_PI * k / TailSides;
				const FVector Radial = SideAxis * FMath::Cos(Angle) + UpAxis * FMath::Sin(Angle);
				Mesh.AddVertex(Center + Radial * Radius, Radial, Color);
			}
		}
		// Punta: un vértice que cierra el último anillo.
		const FVector TipCenter = TailPoint(1.f, Time, Swish);
		const FVector TipDir = (TipCenter - TailPoint(1.f - Step, Time, Swish)).GetSafeNormal(UE_SMALL_NUMBER, FVector(-1.0, 0.0, 0.0));
		const int32 Tip = Mesh.AddVertex(TipCenter + TipDir * 1.5, TipDir, Col(0xF4FAFF, 0.15f));
		for (int32 Ring = 0; Ring < TailRings; ++Ring)
		{
			for (int32 k = 0; k < TailSides; ++k)
			{
				const int32 A = Ring * TailSides + k;
				const int32 B = Ring * TailSides + (k + 1) % TailSides;
				const int32 C = B + TailSides;
				const int32 D = A + TailSides;
				Mesh.AddQuad(A, B, C, D, Mesh.Normals[A] + Mesh.Normals[B]);
			}
		}
		const int32 Last = TailRings * TailSides;
		for (int32 k = 0; k < TailSides; ++k)
		{
			const int32 A = Last + k;
			const int32 B = Last + (k + 1) % TailSides;
			Mesh.AddTri(A, B, Tip, Mesh.Normals[A] + Mesh.Normals[B] + TipDir);
		}
	}

	/** Material del fantasma: nube (borde difuminado) para el cuerpo, suave para ojos; si faltan, el de color de vértice. */
	inline UMaterialInterface* LoadMaterial(bool bSoftEdge)
	{
		UMaterialInterface* Material = nullptr;
		if (bSoftEdge)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFXCloud.M_ProcFXCloud"), nullptr, LOAD_NoWarn);
		}
		if (!Material)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFXSoft.M_ProcFXSoft"), nullptr, LOAD_NoWarn);
		}
		if (!Material)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"), nullptr, LOAD_NoWarn);
		}
		return Material;
	}
}
