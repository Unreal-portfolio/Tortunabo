// ─────────────────────────────────────────────────────────────────────────────
// ATN_ProcMapGenerator — construcción de geometría: terreno, agua, límites y
// estructuras (tableros colosales, techos de cueva, isletas, labios, pasarelas).
// ─────────────────────────────────────────────────────────────────────────────

#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapTerrain.h"
#include "World/ProcMap/TN_ProcWaterActors.h"
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
	const double Spacing = Settings ? Settings->VertexSpacing : 250.0;
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

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	const TArray<FProcMeshTangent> NoTangents;

	// Índices comunes a todos los tiles. A=(x,y), B=(x+1,y), C=(x,y+1): la cara
	// frontal de UE es (C-A)x(B-A), así que (A,C,B) y (B,C,D) miran hacia +Z.
	Tris.Reserve(TileQuads * TileQuads * 6);
	for (int32 y = 0; y < TileQuads; ++y)
	{
		for (int32 x = 0; x < TileQuads; ++x)
		{
			const int32 A = y * Side + x;
			const int32 B = A + 1;
			const int32 C = A + Side;
			const int32 D = C + 1;
			Tris.Add(A); Tris.Add(C); Tris.Add(B);
			Tris.Add(B); Tris.Add(C); Tris.Add(D);
		}
	}

	for (int32 Ty = 0; Ty < TilesY; ++Ty)
	{
		for (int32 Tx = 0; Tx < TilesX; ++Tx)
		{
			Verts.Reset(); Normals.Reset(); UVs.Reset(); Colors.Reset();
			for (int32 y = 0; y <= TileQuads; ++y)
			{
				for (int32 x = 0; x <= TileQuads; ++x)
				{
					const int32 GX = Tx * TileQuads + x;
					const int32 GY = Ty * TileQuads + y;
					const double H = HeightAt(GX, GY);
					const FVector2D P = LatticeOrigin + FVector2D(GX * Spacing, GY * Spacing);
					Verts.Add(FVector(P.X, P.Y, H));

					const double Dx = (HeightAt(GX + 1, GY) - HeightAt(GX - 1, GY)) / (2.0 * Spacing);
					const double Dy = (HeightAt(GX, GY + 1) - HeightAt(GX, GY - 1)) / (2.0 * Spacing);
					const FVector N = FVector(-Dx, -Dy, 1.0).GetSafeNormal();
					Normals.Add(N);
					UVs.Add(P / 500.0);

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
					Colors.Add(Col);
				}
			}

			UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this);
			Tile->SetupAttachment(RootComponent);
			Tile->bUseAsyncCooking = true;
			Tile->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Tile->RegisterComponent();
			Tile->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, NoTangents, true);
			if (TerrainMat) { Tile->SetMaterial(0, TerrainMat); }
			TerrainTiles.Add(Tile);
		}
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
		WaterPlane = NewObject<UStaticMeshComponent>(this);
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
		else if (UMaterialInstanceDynamic* MID = WaterPlane->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.3f, 0.5f));
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
	FTNProcMeshBuffers Rock, Wood, Lava, SlideWater;
	const FLinearColor RockColor(0.32f, 0.29f, 0.26f);
	const FLinearColor WoodColor(0.45f, 0.3f, 0.16f);
	const TArray<FPathSample>& M = Layout.Main;

	// ── Tableros de puentes colosales ───────────────────────────────────────
	for (const FCrossing& C : Layout.Crossings)
	{
		if (C.Type != ETNProcCrossingType::Bridge) { continue; }
		const FRouteStep& High = Layout.Route[C.HighStep];
		TArray<TArray<FVector>> Rings;
		for (int32 i = High.FirstSample; i <= High.LastSample; ++i)
		{
			const FPathSample& S = M[i];
			const FVector2D N = LeftNormal(S.Dir);
			const double Hw = S.Width * 0.5 + 120.0;
			// Perfil de roca: cubierta plana y panza irregular (la cara superior mira arriba).
			const double Sag = 120.0 * FMath::Sin(Pi * static_cast<double>(i - High.FirstSample) / FMath::Max(1, High.LastSample - High.FirstSample));
			const FVector2D Profile[6] = {
				FVector2D(-Hw, 0.0), FVector2D(Hw, 0.0), FVector2D(Hw - 90.0, -320.0),
				FVector2D(Hw * 0.45, -700.0 - Sag), FVector2D(-Hw * 0.45, -700.0 - Sag), FVector2D(-Hw + 90.0, -320.0) };
			TArray<FVector> Ring;
			for (const FVector2D& Pr : Profile)
			{
				const FVector2D XY = S.P + N * Pr.X;
				Ring.Add(FVector(XY.X, XY.Y, C.TopZ + Pr.Y));
			}
			Rings.Add(Ring);
		}
		Rock.AddSweep(Rings, true, RockColor);
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
				Rock.AddPrism(F.Polygon, F.Location.Z, -520.0, FLinearColor(0.55f, 0.5f, 0.38f));
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
				const int32 From = FMath::Clamp(F.PathIndex, 0, M.Num() - 1);
				const int32 To = FMath::Clamp(F.Aux2, 0, M.Num() - 1);
				for (int32 i = From; i < To; ++i)
				{
					const FPathSample& A = M[i];
					const FPathSample& B = M[i + 1];
					const FVector2D Mid = (A.P + B.P) * 0.5;
					const FVector2D Dir = (B.P - A.P).GetSafeNormal();
					const double Len = FVector2D::Distance(A.P, B.P);
					Wood.AddBox(FVector(Mid.X, Mid.Y, (A.Z + B.Z) * 0.5 - 12.0), FVector(Dir.X, Dir.Y, 0.0),
						FVector(Len * 0.5 + 8.0, A.Width * 0.5, 12.0), WoodColor);
					if ((i - From) % 3 == 0)
					{
						for (int32 Sd = -1; Sd <= 1; Sd += 2)
						{
							const FVector2D Post = A.P + LeftNormal(Dir) * (Sd * (A.Width * 0.5 - 15.0));
							Wood.AddBox(FVector(Post.X, Post.Y, A.Z - 120.0), FVector(Dir.X, Dir.Y, 0.0), FVector(12.0, 12.0, 110.0), WoodColor * 0.7f);
						}
					}
				}
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
			case EFeature::SlideZone:
			{
				// Lámina de agua sobre la bajada: el tobogán en sí es terreno empinado.
				const int32 From = FMath::Clamp(F.PathIndex, 0, M.Num() - 1);
				const int32 To = FMath::Clamp(F.Aux, 0, M.Num() - 1);
				for (int32 i = From; i < To; ++i)
				{
					const FPathSample& A = M[i];
					const FPathSample& B = M[i + 1];
					const FVector2D NA = LeftNormal(A.Dir) * (A.Width * 0.45);
					const FVector2D NB = LeftNormal(B.Dir) * (B.Width * 0.45);
					SlideWater.AddQuad(FVector(A.P - NA, A.Z + 12.0), FVector(A.P + NA, A.Z + 12.0), FVector(B.P + NB, B.Z + 12.0), FVector(B.P - NB, B.Z + 12.0),
						FVector::UpVector, FLinearColor(0.6f, 0.85f, 1.f));
				}
				break;
			}
			default:
				break;
		}
	}

	// ── Componentes ─────────────────────────────────────────────────────────
	const TArray<FProcMeshTangent> NoTangents;
	UMaterialInterface* BasicMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInterface* VertexMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));

	StructureMesh = NewObject<UProceduralMeshComponent>(this);
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

	DecorMesh = NewObject<UProceduralMeshComponent>(this);
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
		UBoxComponent* Wall = NewObject<UBoxComponent>(this);
		Wall->SetupAttachment(RootComponent);
		Wall->SetBoxExtent(Def.Extent);
		Wall->SetCollisionProfileName(TEXT("InvisibleWall"));
		Wall->SetHiddenInGame(true);
		Wall->RegisterComponent();
		Wall->SetRelativeLocation(Def.Center);
		BoundaryWalls.Add(Wall);
	}
}
