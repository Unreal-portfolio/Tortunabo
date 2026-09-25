// ─────────────────────────────────────────────────────────────────────────────
// ATN_ProcMapGenerator — construcción de geometría: terreno, agua, límites y
// estructuras (tableros colosales, techos de cueva, isletas, labios, pasarelas).
// ─────────────────────────────────────────────────────────────────────────────

#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapTerrain.h"
#include "World/ProcMap/TN_ProcWaterActors.h"
#include "World/ProcMap/TN_ProcMapActorUtils.h"
#include "Core/TN_Log.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Async/ParallelFor.h"

namespace
{
	/** Buffers de una sección de malla procedural con caras planas orientadas. */
	struct FTNProcMeshBuffers
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;

		bool IsEmpty() const { return Tris.Num() == 0; }

		/**
		 * Añade un triángulo cuya cara visible mira hacia Hint. En UE la cara frontal
		 * de (A,B,C) es la de normal (C-A)x(B-A) (la que calculan MeshUtilities y
		 * CalculateTangentsForMesh): se orienta N = (B-A)x(C-A) hacia Hint y se emite
		 * A, C, B.
		 */
		void AddTri(const FVector& A, const FVector& B, const FVector& C, const FVector& Hint, const FLinearColor& Color)
		{
			FVector N = FVector::CrossProduct(B - A, C - A);
			if (N.SizeSquared() < 1e-4) { return; }
			N.Normalize();
			const bool bFlip = FVector::DotProduct(N, Hint) < 0.0;
			const FVector P1 = bFlip ? C : B;
			const FVector P2 = bFlip ? B : C;
			if (bFlip) { N = -N; }
			const int32 Base = Verts.Num();
			const FVector Pts[3] = { A, P1, P2 };
			for (const FVector& P : Pts)
			{
				Verts.Add(P);
				Normals.Add(N);
				// UV triplanar simple según la orientación de la cara.
				const double Ax = FMath::Abs(N.X), Ay = FMath::Abs(N.Y), Az = FMath::Abs(N.Z);
				FVector2D UV = Az >= Ax && Az >= Ay ? FVector2D(P.X, P.Y) : (Ax >= Ay ? FVector2D(P.Y, P.Z) : FVector2D(P.X, P.Z));
				UVs.Add(UV / 400.0);
				Colors.Add(Color);
			}
			Tris.Add(Base);
			Tris.Add(Base + 2);
			Tris.Add(Base + 1);
		}

		/** Quad A-B-C-D en orden de contorno. */
		void AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Hint, const FLinearColor& Color)
		{
			AddTri(A, B, C, Hint, Color);
			AddTri(A, C, D, Hint, Color);
		}

		/** Caja orientada: AxisX horizontal unitario, Z arriba. */
		void AddBox(const FVector& Center, const FVector& AxisX, const FVector& Half, const FLinearColor& Color)
		{
			const FVector X = AxisX.GetSafeNormal2D().IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : AxisX.GetSafeNormal2D();
			const FVector Y(-X.Y, X.X, 0.0);
			const FVector Z(0.0, 0.0, 1.0);
			auto P = [&](double Sx, double Sy, double Sz) { return Center + X * (Sx * Half.X) + Y * (Sy * Half.Y) + Z * (Sz * Half.Z); };
			AddQuad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Z, Color);
			AddQuad(P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), -Z, Color);
			AddQuad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), X, Color);
			AddQuad(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), -X, Color);
			AddQuad(P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), Y, Color);
			AddQuad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), -Y, Color);
		}

		/** Viga de sección cuadrada (semilado Half) entre dos puntos cualesquiera: cuerdas, cables, péndolas. */
		void AddBeam(const FVector& A, const FVector& B, double Half, const FLinearColor& Color)
		{
			const FVector D = B - A;
			const double Len = D.Size();
			if (Len < 1.0) { return; }
			const FVector X = D / Len;
			FVector Y = FVector::CrossProduct(FVector::UpVector, X);
			if (Y.SizeSquared() < 1e-6) { Y = FVector(0.0, 1.0, 0.0); }
			Y.Normalize();
			const FVector Z = FVector::CrossProduct(X, Y);
			const FVector C[4] = { (Y + Z) * Half, (Z - Y) * Half, (-Y - Z) * Half, (Y - Z) * Half };
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector& P0 = C[k];
				const FVector& P1 = C[(k + 1) % 4];
				AddQuad(A + P0, A + P1, B + P1, B + P0, P0 + P1, Color);
			}
		}

		/** Prisma vertical de un polígono (isletas, pozas). */
		void AddPrism(const TArray<FVector2D>& Poly, double ZTop, double ZBottom, const FLinearColor& Color, bool bSides = true)
		{
			if (Poly.Num() < 3) { return; }
			FVector2D C = FVector2D::ZeroVector;
			for (const FVector2D& V : Poly) { C += V; }
			C = C / static_cast<double>(Poly.Num());
			const FVector Top(C.X, C.Y, ZTop);
			for (int32 i = 0; i < Poly.Num(); ++i)
			{
				const FVector2D& A = Poly[i];
				const FVector2D& B = Poly[(i + 1) % Poly.Num()];
				AddTri(Top, FVector(A.X, A.Y, ZTop), FVector(B.X, B.Y, ZTop), FVector::UpVector, Color);
				if (bSides)
				{
					const FVector2D Mid = (A + B) * 0.5 - C;
					AddQuad(FVector(A.X, A.Y, ZBottom), FVector(B.X, B.Y, ZBottom), FVector(B.X, B.Y, ZTop), FVector(A.X, A.Y, ZTop),
						FVector(Mid.X, Mid.Y, 0.0), Color * 0.8f);
				}
			}
		}

		/**
		 * Barrido: Rings[i][k] son los puntos del perfil en la muestra i. Cada cara
		 * mira hacia fuera del centroide de su anillo.
		 */
		void AddSweep(const TArray<TArray<FVector>>& Rings, bool bClosedProfile, const FLinearColor& Color)
		{
			for (int32 i = 0; i + 1 < Rings.Num(); ++i)
			{
				const TArray<FVector>& R0 = Rings[i];
				const TArray<FVector>& R1 = Rings[i + 1];
				FVector C0 = FVector::ZeroVector;
				for (const FVector& V : R0) { C0 += V; }
				C0 /= static_cast<double>(FMath::Max(1, R0.Num()));
				const int32 Count = bClosedProfile ? R0.Num() : R0.Num() - 1;
				for (int32 k = 0; k < Count; ++k)
				{
					const int32 K1 = (k + 1) % R0.Num();
					const FVector Mid = (R0[k] + R0[K1]) * 0.5;
					AddQuad(R0[k], R0[K1], R1[K1], R1[k], Mid - C0, Color);
				}
			}
		}
	};

	FLinearColor TNProcLerpColor(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return A + (B - A) * FMath::Clamp(T, 0.f, 1.f);
	}

	/** Tono aleatorio estable por índice (vetas de los tablones). */
	float TNProcTone(int32 Index, uint32 Seed)
	{
		uint32 H = static_cast<uint32>(Index) * 2654435761u ^ Seed;
		H ^= H >> 15; H *= 2246822519u; H ^= H >> 13;
		return 0.82f + 0.36f * static_cast<float>(H & 0xFFFF) / 65535.f;
	}

	/** Polilínea de un tramo de camino con cota y media anchura, recorrible por distancia en planta. */
	struct FTNPlankLine
	{
		TArray<FVector> P;
		TArray<double> HalfW;
		TArray<double> Acc;

		void Add(const FVector& Pt, double Hw)
		{
			Acc.Add(P.Num() == 0 ? 0.0 : Acc.Last() + FVector::Dist2D(P.Last(), Pt));
			P.Add(Pt);
			HalfW.Add(Hw);
		}

		double Length() const { return Acc.Num() > 0 ? Acc.Last() : 0.0; }

		void At(double S, FVector& OutP, FVector& OutDir, double& OutHw) const
		{
			int32 i = 0;
			while (i + 2 < Acc.Num() && Acc[i + 1] < S) { ++i; }
			const int32 j = FMath::Min(i + 1, P.Num() - 1);
			const double Span = FMath::Max(1e-3, Acc[j] - Acc[i]);
			const double T = FMath::Clamp((S - Acc[i]) / Span, 0.0, 1.0);
			OutP = FMath::Lerp(P[i], P[j], T);
			OutDir = (P[j] - P[i]).GetSafeNormal2D();
			if (OutDir.IsNearlyZero()) { OutDir = FVector(1.0, 0.0, 0.0); }
			OutHw = FMath::Lerp(HalfW[i], HalfW[j], T);
		}
	};

	/** Tablones atravesados con junta entre S0 y S1, con dos largueros debajo. */
	void TNProcAddPlanks(FTNProcMeshBuffers& Wood, const FTNPlankLine& Line, double S0, double S1, const FLinearColor& Base, uint32 Seed)
	{
		constexpr double Pitch = 34.0;
		constexpr double Board = 29.0;
		constexpr double Thick = 6.0;
		int32 Index = 0;
		for (double S = S0 + Board * 0.5; S < S1; S += Pitch, ++Index)
		{
			FVector C, Dir;
			double Hw = 0.0;
			Line.At(S, C, Dir, Hw);
			// Cada tablón algo distinto: largo, tono y un leve giro.
			const double Jitter = (TNProcTone(Index, Seed + 7u) - 1.0f) * 0.12;
			const FVector D = (Dir + FVector(-Dir.Y, Dir.X, 0.0) * Jitter).GetSafeNormal2D();
			const double Len = Hw * (0.96 + 0.08 * (TNProcTone(Index, Seed + 3u) - 0.82f) / 0.36f);
			Wood.AddBox(C - FVector(0.0, 0.0, Thick), D, FVector(Board * 0.5, Len, Thick), Base * TNProcTone(Index, Seed));
		}
		// Largueros: vigas a lo largo, bajo los tablones.
		constexpr double Step = 300.0;
		for (double S = S0; S < S1; S += Step)
		{
			FVector A, B, DirA, DirB;
			double HwA = 0.0, HwB = 0.0;
			Line.At(S, A, DirA, HwA);
			Line.At(FMath::Min(S + Step, S1), B, DirB, HwB);
			for (const double Side : { -0.7, 0.7 })
			{
				const FVector NA = FVector(-DirA.Y, DirA.X, 0.0) * (Side * HwA);
				const FVector NB = FVector(-DirB.Y, DirB.X, 0.0) * (Side * HwB);
				Wood.AddBeam(A + NA - FVector(0.0, 0.0, Thick * 2.0 + 8.0), B + NB - FVector(0.0, 0.0, Thick * 2.0 + 8.0), 9.0, Base * 0.6f);
			}
		}
	}

	/** Ruido de valor estable en [-1, 1] por índices (forma de rocas y troncos). */
	double TNProcHashNoise(int32 A, int32 B, uint32 Seed)
	{
		uint32 H = static_cast<uint32>(A) * 73856093u ^ static_cast<uint32>(B) * 19349663u ^ Seed * 83492791u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return static_cast<double>(H & 0xFFFF) / 32767.5 - 1.0;
	}

	/**
	 * Cuerpo de revolución irregular: anillos de Seg vértices a las alturas Z (sobre Base) con
	 * radios R, cada vértice con ruido radial; se cierra por arriba. Para rocas, agujas y troncos
	 * de árbol (eje vertical).
	 */
	void TNProcAddLathe(FTNProcMeshBuffers& Mesh, const FVector& Base, const TArray<double>& Z, const TArray<double>& R, double Jitter,
		uint32 Seed, const FLinearColor& Color, int32 Seg = 10)
	{
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r < Z.Num(); ++r)
		{
			TArray<FVector> Ring;
			for (int32 k = 0; k < Seg; ++k)
			{
				const double A = TNProcMap::TwoPi * (k + 0.37 * TNProcHashNoise(r, k, Seed + 5u)) / Seg;
				const double Rad = FMath::Max(1.0, R[r] * (1.0 + Jitter * TNProcHashNoise(r, k, Seed)));
				Ring.Add(Base + FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, Z[r] + Jitter * 0.3 * R[r] * TNProcHashNoise(k, r, Seed + 9u)));
			}
			Rings.Add(Ring);
		}
		Mesh.AddSweep(Rings, true, Color);
		const TArray<FVector>& Top = Rings.Last();
		FVector C = FVector::ZeroVector;
		for (const FVector& V : Top) { C += V; }
		C /= static_cast<double>(Top.Num());
		C.Z += R.Last() * 0.3;
		for (int32 k = 0; k < Top.Num(); ++k) { Mesh.AddTri(C, Top[k], Top[(k + 1) % Top.Num()], FVector::UpVector, Color); }
	}

	/** Peñasco: elipsoide irregular hundido 25 cm en el suelo. */
	void TNProcAddBoulder(FTNProcMeshBuffers& Mesh, const FVector& Base, double Radius, double Height, uint32 Seed, const FLinearColor& Color)
	{
		TArray<double> Z, R;
		for (int32 r = 0; r <= 5; ++r)
		{
			const double T = r / 6.0;
			Z.Add(-25.0 + Height * (0.5 - 0.5 * FMath::Cos(T * PI)) * 1.1);
			R.Add(Radius * FMath::Sin(FMath::Lerp(0.35, 0.92, T) * PI));
		}
		TNProcAddLathe(Mesh, Base, Z, R, 0.22, Seed, Color * (0.85f + 0.3f * static_cast<float>(0.5 + 0.5 * TNProcHashNoise(1, 2, Seed))), 9);
	}

	/** Tronco caído (cilindro irregular) entre A y B, con tapas y muñones de ramas. */
	void TNProcAddLog(FTNProcMeshBuffers& Mesh, const FVector& A, const FVector& B, double Radius, uint32 Seed, const FLinearColor& Bark, const FLinearColor& Cut)
	{
		const FVector Axis = (B - A).GetSafeNormal();
		if (Axis.IsNearlyZero()) { return; }
		FVector U = FVector::CrossProduct(Axis, FVector::UpVector);
		if (U.SizeSquared() < 1e-6) { U = FVector(1.0, 0.0, 0.0); }
		U.Normalize();
		const FVector V = FVector::CrossProduct(Axis, U);
		constexpr int32 Seg = 9;
		constexpr int32 Stations = 6;
		TArray<TArray<FVector>> Rings;
		for (int32 St = 0; St <= Stations; ++St)
		{
			const FVector C = FMath::Lerp(A, B, static_cast<double>(St) / Stations);
			TArray<FVector> Ring;
			for (int32 k = 0; k < Seg; ++k)
			{
				const double Ang = TNProcMap::TwoPi * k / Seg;
				const double Rad = Radius * (1.0 + 0.1 * TNProcHashNoise(St, k, Seed));
				Ring.Add(C + (U * FMath::Cos(Ang) + V * FMath::Sin(Ang)) * Rad);
			}
			Rings.Add(Ring);
		}
		Mesh.AddSweep(Rings, true, Bark);
		for (int32 End = 0; End < 2; ++End)
		{
			const TArray<FVector>& Ring = End == 0 ? Rings[0] : Rings.Last();
			const FVector C = End == 0 ? A : B;
			const FVector Out = End == 0 ? -Axis : Axis;
			for (int32 k = 0; k < Seg; ++k) { Mesh.AddTri(C, Ring[k], Ring[(k + 1) % Seg], Out, Cut); }
		}
		for (int32 b = 0; b < 2; ++b)
		{
			const double T = 0.3 + 0.4 * (0.5 + 0.5 * TNProcHashNoise(b, 7, Seed));
			const double Ang = PI * TNProcHashNoise(b, 3, Seed);
			const FVector Dir = (U * FMath::Cos(Ang) + V * FMath::Sin(Ang) + Axis * 0.3).GetSafeNormal();
			const FVector P0 = FMath::Lerp(A, B, T);
			Mesh.AddBeam(P0, P0 + Dir * (Radius * 2.2), Radius * 0.28, Bark * 0.9f);
		}
	}

	/** Postes a ambos bordes cada PostEvery y cuerda de barandilla con comba entre ellos. */
	void TNProcAddRopeRails(FTNProcMeshBuffers& Wood, const FTNPlankLine& Line, double S0, double S1, double PostEvery, double PostDown,
		double RailH, const FLinearColor& PostColor, const FLinearColor& RopeColor)
	{
		const int32 NumPosts = FMath::Max(1, FMath::RoundToInt((S1 - S0) / PostEvery));
		for (const double Side : { -1.0, 1.0 })
		{
			FVector PrevTop = FVector::ZeroVector;
			for (int32 k = 0; k <= NumPosts; ++k)
			{
				const double S = S0 + (S1 - S0) * k / NumPosts;
				FVector C, Dir;
				double Hw = 0.0;
				Line.At(S, C, Dir, Hw);
				const FVector N(-Dir.Y, Dir.X, 0.0);
				const FVector Base = C + N * (Side * (Hw - 12.0));
				Wood.AddBox(Base + FVector(0.0, 0.0, (RailH + 10.0 - PostDown) * 0.5), Dir, FVector(7.0, 7.0, (RailH + 10.0 + PostDown) * 0.5), PostColor);
				const FVector Top = Base + FVector(0.0, 0.0, RailH);
				if (k > 0)
				{
					// Cuerda en tres tramos con comba.
					FVector Last = PrevTop;
					for (int32 t = 1; t <= 3; ++t)
					{
						const double U = t / 3.0;
						const FVector Pt = FMath::Lerp(PrevTop, Top, U) - FVector(0.0, 0.0, 22.0 * 4.0 * U * (1.0 - U));
						Wood.AddBeam(Last, Pt, 2.5, RopeColor);
						Last = Pt;
					}
				}
				PrevTop = Top;
			}
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Materiales y colores
// ─────────────────────────────────────────────────────────────────────────────

UMaterialInterface* ATN_ProcMapGenerator::ResolveMaterial(UMaterialInterface* Preferred, const TCHAR* FallbackPath) const
{
	if (Preferred)
	{
		return Preferred;
	}
	return FallbackPath ? LoadObject<UMaterialInterface>(nullptr, FallbackPath) : nullptr;
}

void ATN_ProcMapGenerator::ResolveBiomeColors(ETNProcBiome Biome, FLinearColor& Ground, FLinearColor& Path, FLinearColor& Rock, FLinearColor& Bed) const
{
	if (const UTN_ProcBiomeDataAsset* Asset = Settings ? Settings->FindBiome(Biome) : nullptr)
	{
		Ground = Asset->GroundColor;
		Path = Asset->PathColor;
		Rock = Asset->RockColor;
		Bed = Asset->BedColor;
		return;
	}
	TN_DefaultBiomeColors(Biome, Ground, Path, Rock, Bed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Terreno
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::BuildTerrain()
{
	using namespace TNProcMap;
	const double Spacing = Settings ? Settings->VertexSpacing : 150.0;
	const int32 TileQuads = Settings ? Settings->TileQuads : 48;
	const double Margin = Settings ? Settings->OuterMargin : 25000.0;
	const double Sea = Settings ? Settings->SeaExtent : 30000.0;

	LatticeSpacing = Spacing;
	LatticeOrigin = FVector2D(-Margin, -Margin);
	const int32 QuadsX = FMath::CeilToInt((Layout.WorldSize + 2.0 * Margin) / Spacing / TileQuads) * TileQuads;
	const int32 QuadsY = FMath::CeilToInt((Layout.WorldSize + Margin + Sea) / Spacing / TileQuads) * TileQuads;
	LatticeNX = QuadsX + 1;
	LatticeNY = QuadsY + 1;
	const int32 NumVerts = LatticeNX * LatticeNY;

	// ── Alturas en paralelo (lógica pura, filas disjuntas) ──────────────────
	TNProcMap::FTerrainBuilder Builder;
	Builder.Build(Layout, LatticeOrigin, Spacing, LatticeNX, LatticeNY);
	Heights.SetNumUninitialized(NumVerts);
	PathMask.SetNumUninitialized(NumVerts);
	const int32 RowsPerChunk = 16;
	const int32 NumChunks = (LatticeNY + RowsPerChunk - 1) / RowsPerChunk;
	ParallelFor(NumChunks, [&](int32 Chunk)
	{
		Builder.ComputeRows(Chunk * RowsPerChunk, FMath::Min(LatticeNY, (Chunk + 1) * RowsPerChunk), Heights, PathMask);
	});
	Builder.ExportPathEdgeDistance(PathDist);

	// ── Colores por bioma ───────────────────────────────────────────────────
	FLinearColor Ground[NumBiomes], PathC[NumBiomes], Rock[NumBiomes], Bed[NumBiomes];
	for (int32 b = 0; b < NumBiomes; ++b)
	{
		ResolveBiomeColors(BiomeFromIndex(b), Ground[b], PathC[b], Rock[b], Bed[b]);
	}

	auto HeightAt = [&](int32 X, int32 Y)
	{
		return static_cast<double>(Heights[FMath::Clamp(Y, 0, LatticeNY - 1) * LatticeNX + FMath::Clamp(X, 0, LatticeNX - 1)]);
	};

	UMaterialInterface* TerrainMat = ResolveMaterial(Settings ? Settings->TerrainMaterial.Get() : nullptr,
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	const uint32 ColorSeed = Layout.Params.Seed ^ 0xC0105u;

	const int32 TilesX = QuadsX / TileQuads;
	const int32 TilesY = QuadsY / TileQuads;
	const int32 Side = TileQuads + 1;
	const TArray<FProcMeshTangent> NoTangents;

	// Datos de cada tesela en paralelo (con 1,5 m de resolución son millones de vértices); la
	// creación de los componentes va después, en el hilo de juego.
	struct FTileData
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
	};
	TArray<FTileData> TileData;
	TileData.SetNum(TilesX * TilesY);
	ParallelFor(TileData.Num(), [&](int32 TileIndex)
	{
		const int32 Tx = TileIndex % TilesX;
		const int32 Ty = TileIndex / TilesX;
		FTileData& T = TileData[TileIndex];
		// A=(x,y), B=(x+1,y), C=(x,y+1), D=(x+1,y+1). La cara frontal de UE es
		// (C-A)x(B-A): (A,C,B)+(B,C,D) o (A,C,D)+(A,D,B) miran hacia +Z. La
		// diagonal sigue la curva de nivel (TNProcMap::SplitAlongAD).
		T.Tris.Reserve(TileQuads * TileQuads * 6);
		for (int32 y = 0; y < TileQuads; ++y)
		{
			for (int32 x = 0; x < TileQuads; ++x)
			{
				const int32 GX = Tx * TileQuads + x;
				const int32 GY = Ty * TileQuads + y;
				const int32 A = y * Side + x;
				const int32 B = A + 1;
				const int32 C = A + Side;
				const int32 D = C + 1;
				if (TNProcMap::SplitAlongAD(HeightAt(GX, GY), HeightAt(GX + 1, GY), HeightAt(GX, GY + 1), HeightAt(GX + 1, GY + 1)))
				{
					T.Tris.Add(A); T.Tris.Add(C); T.Tris.Add(D);
					T.Tris.Add(A); T.Tris.Add(D); T.Tris.Add(B);
				}
				else
				{
					T.Tris.Add(A); T.Tris.Add(C); T.Tris.Add(B);
					T.Tris.Add(B); T.Tris.Add(C); T.Tris.Add(D);
				}
			}
		}
		T.Verts.Reserve(Side * Side); T.Normals.Reserve(Side * Side); T.UVs.Reserve(Side * Side); T.Colors.Reserve(Side * Side);
		for (int32 y = 0; y <= TileQuads; ++y)
		{
			for (int32 x = 0; x <= TileQuads; ++x)
			{
				const int32 GX = Tx * TileQuads + x;
				const int32 GY = Ty * TileQuads + y;
				const double H = HeightAt(GX, GY);
				const FVector2D P = LatticeOrigin + FVector2D(GX * Spacing, GY * Spacing);
				T.Verts.Add(FVector(P.X, P.Y, H));
				const double Dx = (HeightAt(GX + 1, GY) - HeightAt(GX - 1, GY)) / (2.0 * Spacing);
				const double Dy = (HeightAt(GX, GY + 1) - HeightAt(GX, GY - 1)) / (2.0 * Spacing);
				const FVector N = FVector(-Dx, -Dy, 1.0).GetSafeNormal();
				T.Normals.Add(N);
				T.UVs.Add(P / 500.0);
				double W[NumBiomes];
				Layout.BiomeWeightsAt(P, W);
				FLinearColor G(0.f, 0.f, 0.f, 0.f), Pc(0.f, 0.f, 0.f, 0.f), R(0.f, 0.f, 0.f, 0.f), Bd(0.f, 0.f, 0.f, 0.f);
				for (int32 b = 0; b < NumBiomes; ++b)
				{
					const float Wb = static_cast<float>(W[b]);
					G += Ground[b] * Wb; Pc += PathC[b] * Wb; R += Rock[b] * Wb; Bd += Bed[b] * Wb;
				}
				const float Mask = PathMask[GY * LatticeNX + GX] / 255.f;
				FLinearColor Col = G;
				Col = TNProcLerpColor(Col, R, static_cast<float>(TNProcMap::SmoothStep(0.84, 0.6, N.Z)));
				Col = TNProcLerpColor(Col, Pc, Mask);
				Col = TNProcLerpColor(Col, Bd, static_cast<float>(TNProcMap::SmoothStep(30.0, -120.0, H)));
				const float Var = 0.9f + 0.2f * static_cast<float>(0.5 + 0.5 * TNProcMap::Noise2(ColorSeed, P.X / 700.0, P.Y / 700.0));
				Col = Col * Var;
				Col.A = Mask;
				T.Colors.Add(Col);
			}
		}
	});

	for (FTileData& T : TileData)
	{
		UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
		Tile->SetupAttachment(RootComponent);
		Tile->bUseAsyncCooking = true;
		Tile->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Tile->RegisterComponent();
		Tile->CreateMeshSection_LinearColor(0, T.Verts, T.Tris, T.Normals, T.UVs, T.Colors, NoTangents, true);
		if (TerrainMat) { Tile->SetMaterial(0, TerrainMat); }
		TerrainTiles.Add(Tile);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Agua
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::BuildWater()
{
	// Plano de agua a nivel del mar sobre todo el mallado: el terreno lo tapa donde está por encima.
	if (BasicPlane)
	{
		WaterPlane = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		WaterPlane->SetupAttachment(RootComponent);
		WaterPlane->SetStaticMesh(BasicPlane);
		WaterPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WaterPlane->SetCastShadow(false);
		WaterPlane->RegisterComponent();
		const double SizeX = (LatticeNX - 1) * LatticeSpacing;
		const double SizeY = (LatticeNY - 1) * LatticeSpacing;
		WaterPlane->SetRelativeLocation(FVector(LatticeOrigin.X + SizeX * 0.5, LatticeOrigin.Y + SizeY * 0.5, TNProcMap::SeaLevel + 2.0));
		WaterPlane->SetRelativeScale3D(FVector(SizeX / 100.0, SizeY / 100.0, 1.0));
		if (UMaterialInterface* WaterMat = Settings ? Settings->WaterMaterial.Get() : nullptr)
		{
			WaterPlane->SetMaterial(0, WaterMat);
		}
		else
		{
			TNProcActors::Tint(WaterPlane, FLinearColor(0.05f, 0.3f, 0.5f));
		}
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Volumen nadable: rectángulos codiciosos sobre celdas de 4x4 quads con agua profunda.
	const int32 Step = 4;
	const int32 CW = (LatticeNX - 1) / Step;
	const int32 CH = (LatticeNY - 1) / Step;
	TArray<float> CellMin;
	CellMin.SetNumUninitialized(CW * CH);
	for (int32 cy = 0; cy < CH; ++cy)
	{
		for (int32 cx = 0; cx < CW; ++cx)
		{
			float MinH = TNumericLimits<float>::Max();
			for (int32 y = cy * Step; y <= (cy + 1) * Step; ++y)
			{
				for (int32 x = cx * Step; x <= (cx + 1) * Step; ++x)
				{
					MinH = FMath::Min(MinH, Heights[y * LatticeNX + x]);
				}
			}
			CellMin[cy * CW + cx] = MinH;
		}
	}
	const float DeepEnough = -90.f;
	TArray<uint8> Covered;
	Covered.Init(0, CW * CH);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	ATN_ProcWaterVolume* Water = World->SpawnActor<ATN_ProcWaterVolume>(ATN_ProcWaterVolume::StaticClass(), GetActorTransform(), Params);
	if (!Water)
	{
		return;
	}
	SpawnedActors.Add(Water);

	const double CellSize = Step * LatticeSpacing;
	for (int32 cy = 0; cy < CH; ++cy)
	{
		for (int32 cx = 0; cx < CW; ++cx)
		{
			const int32 Idx = cy * CW + cx;
			if (Covered[Idx] || CellMin[Idx] > DeepEnough) { continue; }
			int32 W = 1;
			float MinH = CellMin[Idx];
			while (cx + W < CW && !Covered[cy * CW + cx + W] && CellMin[cy * CW + cx + W] <= DeepEnough)
			{
				MinH = FMath::Min(MinH, CellMin[cy * CW + cx + W]);
				++W;
			}
			int32 H = 1;
			bool bGrow = true;
			while (bGrow && cy + H < CH)
			{
				for (int32 k = 0; k < W; ++k)
				{
					const int32 J = (cy + H) * CW + cx + k;
					if (Covered[J] || CellMin[J] > DeepEnough) { bGrow = false; break; }
				}
				if (bGrow)
				{
					for (int32 k = 0; k < W; ++k) { MinH = FMath::Min(MinH, CellMin[(cy + H) * CW + cx + k]); }
					++H;
				}
			}
			for (int32 yy = 0; yy < H; ++yy)
			{
				for (int32 xx = 0; xx < W; ++xx) { Covered[(cy + yy) * CW + cx + xx] = 1; }
			}
			const double Bottom = static_cast<double>(MinH) - 150.0;
			// El volumen se mide por el centro de la cápsula: techo a nivel del mar.
			const double Top = TNProcMap::SeaLevel;
			const FVector2D Min2 = LatticeOrigin + FVector2D(cx * CellSize, cy * CellSize);
			const FVector2D Size2(W * CellSize, H * CellSize);
			const FVector CenterMap(Min2.X + Size2.X * 0.5, Min2.Y + Size2.Y * 0.5, (Bottom + Top) * 0.5);
			Water->AddWaterBox(MapToWorld(CenterMap), FVector(Size2.X * 0.5, Size2.Y * 0.5, (Top - Bottom) * 0.5));
		}
	}
	UE_LOG(LogTortunabo, Log, TEXT("[ProcMap] Agua nadable: %d cajas."), Water->NumBoxes());
}

// ─────────────────────────────────────────────────────────────────────────────
// Estructuras
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::BuildStructures()
{
	using namespace TNProcMap;
	FTNProcMeshBuffers Rock, Wood, Lava, SlideWater, Foliage;
	const FLinearColor RockColor(0.32f, 0.29f, 0.26f);
	const FLinearColor WoodColor(0.45f, 0.3f, 0.16f);
	const TArray<FPathSample>& M = Layout.Main;

	// ── Puentes colosales: puente colgante de tablones entre las torres ─────
	// Tablones con junta sobre dos largueros, postes y barandilla de cuerda, y en cada
	// apoyo (borde de torre o pilar de roca) mástiles de los que cuelgan los cables
	// principales, con comba hasta casi el tablero en mitad del vano, y sus péndolas.
	const FLinearColor RopeColor(0.52f, 0.42f, 0.27f);
	for (int32 c = 0; c < Layout.Crossings.Num(); ++c)
	{
		const FCrossing& C = Layout.Crossings[c];
		if (C.Type != ETNProcCrossingType::Bridge) { continue; }
		const FRouteStep& High = Layout.Route[C.HighStep];
		FTNPlankLine Line;
		for (int32 i = High.FirstSample; i <= High.LastSample; ++i)
		{
			Line.Add(FVector(M[i].P, C.TopZ), M[i].Width * 0.5);
		}
		const double TowerR = Layout.Params.TowerRadius;
		const double S0 = TowerR - 150.0;
		const double S1 = Line.Length() - TowerR + 150.0;
		if (S1 <= S0) { continue; }
		const uint32 Seed = Layout.Params.Seed ^ (0xB21D6u + static_cast<uint32>(c));
		TNProcAddPlanks(Wood, Line, S0, S1, WoodColor, Seed);
		TNProcAddRopeRails(Wood, Line, S0, S1, 300.0, 0.0, 105.0, WoodColor * 0.65f, RopeColor);

		// Apoyos: bordes de las torres y pilares de roca de este cruce.
		TArray<double> Supports = { S0 };
		for (const FFeature& F : Layout.Features)
		{
			if (F.Type == EFeature::DeckPillar && F.Aux == c)
			{
				Supports.Add(M[F.PathIndex].S - M[High.FirstSample].S);
			}
		}
		Supports.Add(S1);
		Supports.Sort();
		constexpr double MastH = 750.0;
		constexpr double Low = 130.0;
		for (int32 k = 0; k < Supports.Num(); ++k)
		{
			FVector P0, Dir0;
			double Hw0 = 0.0;
			Line.At(Supports[k], P0, Dir0, Hw0);
			for (const double Side : { -1.0, 1.0 })
			{
				const FVector Base = P0 + FVector(-Dir0.Y, Dir0.X, 0.0) * (Side * (Hw0 + 25.0));
				Wood.AddBox(Base + FVector(0.0, 0.0, MastH * 0.5 - 60.0), Dir0, FVector(16.0, 16.0, MastH * 0.5 + 60.0), WoodColor * 0.55f);
			}
			if (k == 0) { continue; }
			// Vano entre el apoyo anterior y este: cable parabólico y péndolas cada 3 m.
			const double A = Supports[k - 1];
			const double B = Supports[k];
			const int32 NumSeg = FMath::Max(2, FMath::RoundToInt((B - A) / 300.0));
			for (const double Side : { -1.0, 1.0 })
			{
				FVector Prev = FVector::ZeroVector;
				for (int32 t = 0; t <= NumSeg; ++t)
				{
					const double U = static_cast<double>(t) / NumSeg;
					FVector P, Dir;
					double Hw = 0.0;
					Line.At(FMath::Lerp(A, B, U), P, Dir, Hw);
					const FVector Edge = P + FVector(-Dir.Y, Dir.X, 0.0) * (Side * (Hw + 25.0));
					const FVector Cable = Edge + FVector(0.0, 0.0, Low + (MastH - Low) * FMath::Square(2.0 * U - 1.0));
					if (t > 0) { Wood.AddBeam(Prev, Cable, 4.5, RopeColor * 0.8f); }
					if (t > 0 && t < NumSeg) { Wood.AddBeam(Cable, Edge + FVector(0.0, 0.0, 10.0), 2.0, RopeColor); }
					Prev = Cable;
				}
			}
		}
	}

	// ── Techos de cueva sobre el tramo bajo ─────────────────────────────────
	for (const FFeature& F : Layout.Features)
	{
		if (F.Type != EFeature::TunnelRoof) { continue; }
		const int32 From = FMath::Max(0, F.PathIndex - 1);
		const int32 To = FMath::Min(M.Num() - 1, F.Aux2 + 1);
		TArray<TArray<FVector>> Rings;
		for (int32 i = From; i <= To; ++i)
		{
			const FPathSample& S = M[i];
			const FVector2D N = LeftNormal(S.Dir);
			const double Hw = S.Width * 0.5 + 450.0;
			const double Z0 = S.Z + 750.0;
			const double ZTop = F.Height + 40.0;
			const FVector2D Profile[6] = {
				FVector2D(-Hw, Z0), FVector2D(-Hw * 0.55, Z0 + 180.0), FVector2D(Hw * 0.55, Z0 + 180.0),
				FVector2D(Hw, Z0), FVector2D(Hw, ZTop), FVector2D(-Hw, ZTop) };
			TArray<FVector> Ring;
			for (const FVector2D& Pr : Profile)
			{
				const FVector2D XY = S.P + N * Pr.X;
				Ring.Add(FVector(XY.X, XY.Y, Pr.Y));
			}
			Rings.Add(Ring);
		}
		Rock.AddSweep(Rings, true, RockColor * 0.85f);
	}

	for (const FFeature& F : Layout.Features)
	{
		switch (F.Type)
		{
			case EFeature::Islet:
			{
				// Hasta el lecho de la laguna (≈ -10 m): vistas desde el agua no quedan flotando.
				Rock.AddPrism(F.Polygon, F.Location.Z, -1300.0, FLinearColor(0.55f, 0.5f, 0.38f));
				break;
			}
			case EFeature::Gap:
			{
				// Labios de madera que reducen la zanja al hueco exacto.
				const FVector2D D = F.Dir;
				const double Outer = F.Height * 0.5 + 150.0;
				const double Inner = F.Length * 0.5;
				const double HalfLen = (Outer - Inner) * 0.5;
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					const FVector2D Center2 = FVector2D(F.Location.X, F.Location.Y) + D * (Side * (Inner + HalfLen));
					Wood.AddBox(FVector(Center2.X, Center2.Y, F.Location.Z - 60.0), FVector(D.X, D.Y, 0.0),
						FVector(HalfLen, F.Width * 0.5, 60.0), WoodColor);
				}
				break;
			}
			case EFeature::Boardwalk:
			{
				// Pasarela de tablones sobre postes hundidos en el agua, con cuerda a los lados.
				const int32 From = FMath::Clamp(F.PathIndex, 0, M.Num() - 1);
				const int32 To = FMath::Clamp(F.Aux2, 0, M.Num() - 1);
				if (To <= From) { break; }
				FTNPlankLine Line;
				for (int32 i = From; i <= To; ++i) { Line.Add(FVector(M[i].P, M[i].Z), M[i].Width * 0.5); }
				const uint32 Seed = Layout.Params.Seed ^ (0xB0A2Du + static_cast<uint32>(From));
				TNProcAddPlanks(Wood, Line, 0.0, Line.Length(), WoodColor * 0.9f, Seed);
				TNProcAddRopeRails(Wood, Line, 0.0, Line.Length(), 260.0, 320.0, 85.0, WoodColor * 0.6f, RopeColor);
				break;
			}
			case EFeature::RiverBridge:
			{
				const FVector2D D = F.Dir;
				const FVector2D N = LeftNormal(D);
				Wood.AddBox(F.Location - FVector(0.0, 0.0, 45.0), FVector(D.X, D.Y, 0.0), FVector(F.Length * 0.5, F.Width * 0.5, 45.0), WoodColor);
				for (int32 Sd = -1; Sd <= 1; Sd += 2)
				{
					const FVector2D Rail = FVector2D(F.Location.X, F.Location.Y) + N * (Sd * (F.Width * 0.5 - 10.0));
					Wood.AddBox(FVector(Rail.X, Rail.Y, F.Location.Z + 45.0), FVector(D.X, D.Y, 0.0), FVector(F.Length * 0.5, 10.0, 45.0), WoodColor * 0.8f);
				}
				break;
			}
			case EFeature::LavaPool:
			{
				TArray<FVector2D> Disc;
				for (int32 k = 0; k < 24; ++k)
				{
					const double A = TwoPi * k / 24.0;
					Disc.Add(FVector2D(F.Location.X, F.Location.Y) + DirFromAngle(A) * (F.Radius * (1.0 + 0.06 * FMath::Sin(A * 3.0))));
				}
				Lava.AddPrism(Disc, F.Location.Z, F.Location.Z - 10.0, FLinearColor(1.f, 0.35f, 0.05f), false);
				break;
			}
			case EFeature::Boulder:
			{
				const FVector2D C(F.Location.X, F.Location.Y);
				FLinearColor G, Pc, RockC, Bd;
				ResolveBiomeColors(F.Biome, G, Pc, RockC, Bd);
				TNProcAddBoulder(Rock, FVector(C, TerrainHeightMap(C)), F.Radius, F.Height, static_cast<uint32>(F.Aux), TNProcLerpColor(RockC, G, 0.25f));
				break;
			}
			case EFeature::RockSpire:
			{
				const FVector2D C(F.Location.X, F.Location.Y);
				// Aguja (esbelta, con sombrero de roca) o mogote (bajo y ancho, de techo plano).
				FLinearColor G, Pc, RockC, Bd;
				ResolveBiomeColors(F.Biome, G, Pc, RockC, Bd);
				const uint32 Seed = static_cast<uint32>(F.Aux);
				const FVector Base(C, TerrainHeightMap(C) - 40.0);
				const bool bSpire = F.Height > F.Radius * 2.0;
				TArray<double> Z, R;
				const int32 Rings = bSpire ? 8 : 5;
				for (int32 r = 0; r <= Rings; ++r)
				{
					const double T = static_cast<double>(r) / Rings;
					Z.Add(F.Height * T);
					R.Add(F.Radius * (bSpire ? FMath::Lerp(1.25, 0.55, T) : FMath::Lerp(1.15, 0.8, T * T)));
				}
				TNProcAddLathe(Rock, Base, Z, R, bSpire ? 0.18 : 0.12, Seed, RockC * (0.9f + 0.2f * static_cast<float>(0.5 + 0.5 * TNProcHashNoise(4, 4, Seed))), 11);
				if (bSpire)
				{
					TNProcAddBoulder(Rock, Base + FVector(0.0, 0.0, F.Height - 30.0), F.Radius * 1.25, F.Radius * 0.9, Seed + 77u, RockC * 0.85f);
				}
				break;
			}
			case EFeature::Log:
			{
				const FVector2D C(F.Location.X, F.Location.Y);
				const FVector2D Half = F.Dir * (F.Length * 0.5);
				const FVector A(C - Half, TerrainHeightMap(C - Half) + F.Radius * 0.85);
				const FVector B(C + Half, TerrainHeightMap(C + Half) + F.Radius * 0.85);
				const bool bCharred = F.Biome == ETNProcBiome::Volcanic;
				const FLinearColor Bark = bCharred ? FLinearColor(0.07f, 0.06f, 0.05f) : FLinearColor(0.3f, 0.2f, 0.11f);
				const FLinearColor Cut = bCharred ? FLinearColor(0.35f, 0.12f, 0.05f) : FLinearColor(0.62f, 0.48f, 0.3f);
				TNProcAddLog(Wood, A, B, F.Radius, static_cast<uint32>(F.Aux), Bark, Cut);
				break;
			}
			case EFeature::GiantTree:
			{
				const FVector2D C(F.Location.X, F.Location.Y);
				// Secuoya: tronco rojizo con base ensanchada, raíces zancudas en arco y copa cónica por capas.
				const uint32 Seed = static_cast<uint32>(F.Aux);
				const double Ground = TerrainHeightMap(C);
				const FVector Base(C, Ground - 60.0);
				const FLinearColor Bark(0.36f, 0.17f, 0.09f);
				TArray<double> Z, R;
				for (int32 r = 0; r <= 9; ++r)
				{
					const double T = r / 9.0;
					Z.Add(F.Height * 0.9 * T);
					R.Add(F.Radius * (T < 0.08 ? FMath::Lerp(1.9, 1.0, T / 0.08) : FMath::Lerp(1.0, 0.35, (T - 0.08) / 0.92)));
				}
				TNProcAddLathe(Wood, Base, Z, R, 0.05, Seed, Bark, 12);
				const int32 NumRoots = 8 + static_cast<int32>(4.0 * (0.5 + 0.5 * TNProcHashNoise(0, 0, Seed)));
				for (int32 k = 0; k < NumRoots; ++k)
				{
					const double Ang = TwoPi * (k + 0.4 * TNProcHashNoise(k, 1, Seed)) / NumRoots;
					const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
					const double Reach = F.Radius * (2.6 + 1.2 * (0.5 + 0.5 * TNProcHashNoise(k, 2, Seed)));
					const double Top = 250.0 + 250.0 * (0.5 + 0.5 * TNProcHashNoise(k, 3, Seed));
					FVector Prev = FVector(C + Dir * (F.Radius * 0.8), Ground + Top);
					for (int32 t = 1; t <= 4; ++t)
					{
						const double U = t / 4.0;
						const FVector2D Q = C + Dir * FMath::Lerp(F.Radius * 0.8, Reach, U);
						const FVector Pt(Q, FMath::Lerp(Ground + Top, TerrainHeightMap(Q) - 40.0, U * U) + 120.0 * FMath::Sin(U * PI));
						Wood.AddBeam(Prev, Pt, F.Radius * FMath::Lerp(0.22, 0.12, U), Bark * 0.9f);
						Prev = Pt;
					}
				}
				const int32 Clumps = 6;
				for (int32 k = 0; k < Clumps; ++k)
				{
					const double T = FMath::Lerp(0.45, 0.97, static_cast<double>(k) / (Clumps - 1));
					const double CR = F.Radius * FMath::Lerp(4.2, 1.4, T) * (0.85 + 0.3 * (0.5 + 0.5 * TNProcHashNoise(k, 5, Seed)));
					const FVector Off(TNProcHashNoise(k, 6, Seed) * F.Radius * 0.8, TNProcHashNoise(k, 7, Seed) * F.Radius * 0.8, 0.0);
					TNProcAddBoulder(Foliage, Base + Off + FVector(0.0, 0.0, F.Height * T), CR, CR * 0.9, Seed + 100u + k,
						FLinearColor(0.05f, 0.2f + 0.08f * static_cast<float>(k % 2), 0.06f));
				}
				break;
			}
			case EFeature::SlideZone:
			{
				// Lámina de agua sobre la bajada: el tobogán en sí es terreno empinado.
				const TArray<FPathSample>& S = F.BranchIndex == INDEX_NONE ? M : Layout.Branches[F.BranchIndex].Samples;
				const int32 From = FMath::Clamp(F.PathIndex, 0, S.Num() - 1);
				const int32 To = FMath::Clamp(F.Aux, 0, S.Num() - 1);
				// Rejilla fina apoyada en el terreno real (1 m a lo largo, 6 columnas a lo ancho) a 25 cm,
				// para que la lámina no se corte con él; espuma más blanca en los bordes y al pie.
				constexpr int32 Cols = 6;
				TArray<FVector> Prev;
				for (int32 i = From; i <= To; ++i)
				{
					const int32 Sub = i < To ? FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(S[i].P, S[i + 1].P) / 100.0)) : 1;
					for (int32 k = 0; k < Sub; ++k)
					{
						if (i == To && k > 0) { break; }
						const double U = static_cast<double>(k) / Sub;
						const FPathSample& A = S[i];
						const FPathSample& B = S[FMath::Min(i + 1, To)];
						const FVector2D C = FMath::Lerp(A.P, B.P, U);
						const FVector2D N = LeftNormal(FMath::Lerp(A.Dir, B.Dir, U).GetSafeNormal());
						const double Hw = FMath::Lerp(A.Width, B.Width, U) * 0.42;
						TArray<FVector> Row;
						for (int32 c = 0; c <= Cols; ++c)
						{
							const FVector2D Q = C + N * (Hw * (2.0 * c / Cols - 1.0));
							Row.Add(FVector(Q, TerrainHeightMap(Q) + 25.0));
						}
						if (Prev.Num() == Row.Num())
						{
							const float Foot = static_cast<float>(i - From) / FMath::Max(1, To - From);
							for (int32 c = 0; c < Cols; ++c)
							{
								const float Edge = (c == 0 || c == Cols - 1) ? 0.25f : 0.f;
								const FLinearColor Col = TNProcLerpColor(FLinearColor(0.55f, 0.82f, 1.f), FLinearColor(0.95f, 0.98f, 1.f), Edge + 0.5f * Foot * Foot);
								SlideWater.AddQuad(Prev[c], Prev[c + 1], Row[c + 1], Row[c], FVector::UpVector, Col);
							}
						}
						Prev = Row;
					}
				}
				break;
			}
			default:
				break;
		}
	}

	// ── Algas y nenúfares: manchas verdes flotando en las pozas junto al camino ─
	{
		FRng AlgaeRng(static_cast<uint64>(Layout.Params.Seed) * 0xA16Eull + 3ull);
		for (int32 i = 0; i < M.Num(); i += 3)
		{
			const FPathSample& Sm = M[i];
			if (Sm.Biome != ETNProcBiome::Mangrove && Sm.Biome != ETNProcBiome::Water) { continue; }
			for (int32 n = 0; n < 2; ++n)
			{
				const double Side = AlgaeRng.Chance(0.5) ? 1.0 : -1.0;
				const FVector2D Q = Sm.P + LeftNormal(Sm.Dir) * (Side * (Sm.Width * 0.5 + AlgaeRng.Range(150.0, 1800.0))) + Sm.Dir * AlgaeRng.Range(-300.0, 300.0);
				if (TerrainHeightMap(Q) > -40.0) { continue; }
				const double Rad = AlgaeRng.Range(60.0, 260.0);
				TArray<FVector2D> Poly;
				for (int32 k = 0; k < 9; ++k)
				{
					const double A = TwoPi * k / 9.0;
					Poly.Add(Q + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Rad * AlgaeRng.Range(0.6, 1.1)));
				}
				const bool bLily = Sm.Biome == ETNProcBiome::Water && AlgaeRng.Chance(0.4);
				const FLinearColor Col = bLily ? FLinearColor(0.12f, 0.42f, 0.1f) : FLinearColor(0.16f, 0.3f, 0.06f) * static_cast<float>(AlgaeRng.Range(0.8, 1.2));
				Foliage.AddPrism(Poly, TNProcMap::SeaLevel + 4.0, TNProcMap::SeaLevel - 2.0, Col, false);
			}
		}
	}

	// ── Componentes ─────────────────────────────────────────────────────────
	const TArray<FProcMeshTangent> NoTangents;
	UMaterialInterface* BasicMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInterface* VertexMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));

	StructureMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	StructureMesh->SetupAttachment(RootComponent);
	StructureMesh->bUseAsyncCooking = true;
	StructureMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	StructureMesh->RegisterComponent();
	if (!Rock.IsEmpty())
	{
		StructureMesh->CreateMeshSection_LinearColor(0, Rock.Verts, Rock.Tris, Rock.Normals, Rock.UVs, Rock.Colors, NoTangents, true);
		StructureMesh->SetMaterial(0, (Settings && Settings->RockMaterial) ? Settings->RockMaterial.Get() : (VertexMat ? VertexMat : BasicMat));
	}
	if (!Wood.IsEmpty())
	{
		StructureMesh->CreateMeshSection_LinearColor(1, Wood.Verts, Wood.Tris, Wood.Normals, Wood.UVs, Wood.Colors, NoTangents, true);
		StructureMesh->SetMaterial(1, (Settings && Settings->WoodMaterial) ? Settings->WoodMaterial.Get() : (VertexMat ? VertexMat : BasicMat));
	}

	DecorMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	DecorMesh->SetupAttachment(RootComponent);
	DecorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecorMesh->SetCastShadow(false);
	DecorMesh->RegisterComponent();
	if (!Lava.IsEmpty())
	{
		DecorMesh->CreateMeshSection_LinearColor(0, Lava.Verts, Lava.Tris, Lava.Normals, Lava.UVs, Lava.Colors, NoTangents, false);
		DecorMesh->SetMaterial(0, (Settings && Settings->LavaMaterial) ? Settings->LavaMaterial.Get() : (VertexMat ? VertexMat : BasicMat));
	}
	if (!SlideWater.IsEmpty())
	{
		DecorMesh->CreateMeshSection_LinearColor(1, SlideWater.Verts, SlideWater.Tris, SlideWater.Normals, SlideWater.UVs, SlideWater.Colors, NoTangents, false);
		UMaterialInterface* SlideMat = Settings && Settings->SlideWaterMaterial ? Settings->SlideWaterMaterial.Get()
			: (Settings && Settings->WaterMaterial ? Settings->WaterMaterial.Get() : (VertexMat ? VertexMat : BasicMat));
		DecorMesh->SetMaterial(1, SlideMat);
	}
	if (!Foliage.IsEmpty())
	{
		DecorMesh->CreateMeshSection_LinearColor(2, Foliage.Verts, Foliage.Tris, Foliage.Normals, Foliage.UVs, Foliage.Colors, NoTangents, false);
		DecorMesh->SetMaterial(2, VertexMat ? VertexMat : BasicMat);
	}

	// ── Límites invisibles del mapa (la costa norte queda abierta hasta el mar) ─
	const double World = Layout.WorldSize;
	const double Tall = 40000.0;
	double CoastMax = 0.0;
	for (int32 k = 0; k <= 40; ++k) { CoastMax = FMath::Max(CoastMax, Layout.CoastY(World * k / 40.0)); }
	struct FWallDef { FVector Center; FVector Extent; };
	const FWallDef Walls[4] = {
		{ FVector(-300.0, World * 0.5, 0.0), FVector(300.0, World, Tall) },
		{ FVector(World + 300.0, World * 0.5, 0.0), FVector(300.0, World, Tall) },
		{ FVector(World * 0.5, -300.0, 0.0), FVector(World, 300.0, Tall) },
		{ FVector(World * 0.5, CoastMax + 16000.0, 0.0), FVector(World, 300.0, Tall) } };
	for (const FWallDef& Def : Walls)
	{
		UBoxComponent* Wall = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
		Wall->SetupAttachment(RootComponent);
		Wall->SetBoxExtent(Def.Extent);
		Wall->SetCollisionProfileName(TEXT("InvisibleWall"));
		Wall->SetHiddenInGame(true);
		Wall->RegisterComponent();
		Wall->SetRelativeLocation(Def.Center);
		BoundaryWalls.Add(Wall);
	}
}
