// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachRaceGenerator — construcción del terreno fijo de la playa: teselas de arena
// con colisión (y los asientos de la ronda), repisa y acantilado de roca con peñascos al
// pie, fondo y superficie del mar, muros invisibles y el agua nadable de la meta.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/ProcMap/TN_ProcWaterActors.h"
#include "Core/TN_Log.h"
#include "Algo/BinarySearch.h"
#include "Async/ParallelFor.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "TN_BeachRaceKit.h"

namespace TNBeachBuild
{
	constexpr int32 TileQuads = TNBeachRaceKit::TerrainTileQuads;

	struct FTileData
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
	};

	/**
	 * Color de la arena (y del suelo de la selva en los bancos): manchas lentas, crestas algo más claras y hondonadas algo
	 * más oscuras (Relief: la cota sobre la cuesta sin relieve), arena húmeda cerca de la roca y alrededor y dentro de las
	 * pozas, pisada en el fondo de las trincheras y apisonada en las rodadas de los quads (Tint). El alfa es la máscara de
	 * camino de M_ProcTerrain: 1 en la arena (grano, guijarros y marcas del viento) y 0 en la selva.
	 */
	FLinearColor SandColor(double X, double Y, const FVector& N, float Tint, double Relief)
	{
		const uint32 S = TNBeachLayout::TerrainSeed;
		const FLinearColor Sand(0.86f, 0.76f, 0.52f);
		const FLinearColor Damp(0.66f, 0.55f, 0.36f);
		const FLinearColor Soil(0.23f, 0.17f, 0.1f);
		const FLinearColor Leaf(0.1f, 0.3f, 0.08f);
		const FLinearColor Stone(0.25f, 0.25f, 0.2f);
		const double Patch = 0.5 + 0.5 * TNProcMap::Noise2(S + 21u, X / 2600.0, Y / 2600.0);
		const double Crest = FMath::Clamp(Relief / 250.0, -1.0, 1.0);
		FLinearColor C = Sand * static_cast<float>((0.93 + 0.1 * Patch) * (1.0 + 0.05 * Crest));
		C = TNProcMesh::TNProcLerpColor(C, Damp, static_cast<float>(0.3 * TNProcMap::SmoothStep(TNBeachLayout::Length - 7000.0, TNBeachLayout::Length - 1500.0, X)));
		const FVector2D P(X, Y);
		const int32 PoolIndex = TNBeachLayout::PoolAt(P, 1.35);
		if (PoolIndex != INDEX_NONE)
		{
			const double U = TNBeachLayout::PoolU(TNBeachLayout::Pools()[PoolIndex], P);
			C = TNProcMesh::TNProcLerpColor(C, Damp * 0.85f, static_cast<float>(0.75 * (1.0 - TNProcMap::SmoothStep(0.95, 1.35, U))));
		}
		// Solo cerca de las trincheras (28-31 % del recorrido: 220-250 m con 800 m): del 15 al 45 %.
		if (X > 0.15 * TNBeachLayout::Length && X < 0.45 * TNBeachLayout::Length)
		{
			const double Trench = TNBeachLayout::TrenchDistance(P, 500.0);
			if (Trench < 400.0) { C = TNProcMesh::TNProcLerpColor(C, Damp, static_cast<float>(0.35 * (1.0 - TNProcMap::SmoothStep(150.0, 400.0, Trench)))); }
		}
		if (Tint > 0.f) { C = C * (1.f - Tint); }
		const double Outside = FMath::Abs(Y) - TNBeachLayout::HalfWidth;
		const double Jungle = FMath::Max(TNProcMap::SmoothStep(400.0, 3000.0, Outside), TNProcMap::SmoothStep(-2600.0, -5200.0, X));
		if (Jungle > 0.0)
		{
			FLinearColor Floor = TNProcMesh::TNProcLerpColor(Leaf, Soil, static_cast<float>(0.5 + 0.5 * TNProcMap::Noise2(S + 23u, X / 1800.0, Y / 1800.0)));
			Floor = TNProcMesh::TNProcLerpColor(Floor, Stone, static_cast<float>(TNProcMap::SmoothStep(0.8, 0.55, N.Z)));
			C = TNProcMesh::TNProcLerpColor(C, Floor, static_cast<float>(Jungle));
		}
		C.A = static_cast<float>(1.0 - Jungle);
		return C;
	}

	/**
	 * Malla de la tesela de vértices [I0, I1] x [J0, J1] de la rejilla: alturas con los asientos de la ronda y normales
	 * suaves por diferencias centradas con los vecinos de fuera de la tesela (así cuadran las costuras).
	 */
	void ComputeTile(const TArray<double>& Xs, const TArray<double>& Ys, int32 I0, int32 I1, int32 J0, int32 J1, const TArray<TNBeachLayout::FStamp>& Stamps,
		FTileData& Out)
	{
		const int32 XA = FMath::Max(0, I0 - 1);
		const int32 XB = FMath::Min(Xs.Num() - 1, I1 + 1);
		const int32 YA = FMath::Max(0, J0 - 1);
		const int32 YB = FMath::Min(Ys.Num() - 1, J1 + 1);
		const int32 PW = XB - XA + 1;
		const int32 PH = YB - YA + 1;
		// Solo los asientos que llegan a esta tesela.
		TArray<TNBeachLayout::FStamp> Near;
		for (const TNBeachLayout::FStamp& Stamp : Stamps)
		{
			const double R = Stamp.Radius + Stamp.Blend;
			if (FMath::Max(Stamp.A.X, Stamp.B.X) + R >= Xs[XA] && FMath::Min(Stamp.A.X, Stamp.B.X) - R <= Xs[XB]
				&& FMath::Max(Stamp.A.Y, Stamp.B.Y) + R >= Ys[YA] && FMath::Min(Stamp.A.Y, Stamp.B.Y) - R <= Ys[YB])
			{
				Near.Add(Stamp);
			}
		}
		TArray<double> Z;
		TArray<float> Tints;
		Z.SetNumUninitialized(PW * PH);
		Tints.SetNumZeroed(PW * PH);
		for (int32 j = YA; j <= YB; ++j)
		{
			for (int32 i = XA; i <= XB; ++i)
			{
				const int32 K = (j - YA) * PW + (i - XA);
				Z[K] = TNBeachLayout::StampedZ(Near, Xs[i], Ys[j], TNBeachLayout::SandZ(Xs[i], Ys[j]), &Tints[K]);
			}
		}
		auto At = [&Z, XA, YA, PW](int32 IX, int32 IY) { return Z[(IY - YA) * PW + (IX - XA)]; };
		const int32 W = I1 - I0 + 1;
		const int32 H = J1 - J0 + 1;
		Out.Verts.Reset(W * H);
		Out.Normals.Reset(W * H);
		Out.UVs.Reset(W * H);
		Out.Colors.Reset(W * H);
		Out.Tris.Reset((W - 1) * (H - 1) * 6);
		for (int32 j = J0; j <= J1; ++j)
		{
			for (int32 i = I0; i <= I1; ++i)
			{
				const int32 Ia = FMath::Max(XA, i - 1);
				const int32 Ib = FMath::Min(XB, i + 1);
				const int32 Ja = FMath::Max(YA, j - 1);
				const int32 Jb = FMath::Min(YB, j + 1);
				const double DzDx = (At(Ib, j) - At(Ia, j)) / FMath::Max(1.0, Xs[Ib] - Xs[Ia]);
				const double DzDy = (At(i, Jb) - At(i, Ja)) / FMath::Max(1.0, Ys[Jb] - Ys[Ja]);
				const FVector Normal = FVector(-DzDx, -DzDy, 1.0).GetSafeNormal();
				const FVector P(Xs[i], Ys[j], At(i, j));
				Out.Verts.Add(P);
				Out.Normals.Add(Normal);
				Out.UVs.Add(FVector2D(P.X, P.Y) / 500.0);
				Out.Colors.Add(SandColor(P.X, P.Y, Normal, Tints[(j - YA) * PW + (i - XA)], P.Z - TNBeachLayout::BaseZ(P.X, P.Y)));
			}
		}
		// Cara de arriba hacia +Z: en UE la frontal de (A, B, C) es la de normal (C - A) x (B - A).
		for (int32 j = 0; j + 1 < H; ++j)
		{
			for (int32 i = 0; i + 1 < W; ++i)
			{
				const int32 V00 = j * W + i;
				const int32 V10 = V00 + 1;
				const int32 V01 = V00 + W;
				const int32 V11 = V01 + 1;
				Out.Tris.Add(V00);
				Out.Tris.Add(V01);
				Out.Tris.Add(V10);
				Out.Tris.Add(V10);
				Out.Tris.Add(V01);
				Out.Tris.Add(V11);
			}
		}
	}

	/** Solo tienen colisión las teselas a las que se puede llegar (dentro de los muros y un poco más). */
	bool TileNeedsCollision(double X0, double X1, double Y0, double Y1)
	{
		const double Reach = TNBeachLayout::SideWallY + 2500.0;
		return X1 >= TNBeachLayout::BackWallX - 2500.0 && Y1 >= -Reach && Y0 <= Reach;
	}

	/**
	 * Solo dan sombra las teselas de la playa y el pie de los bancos (las dunas se leen por su sombra); las de la selva
	 * lejana, no: cada tesela cubre mucho mapa de sombras y son mallas sin Nanite. Tampoco las que empiezan detrás de la
	 * salida: con el sol a la espalda de la salida, el cerro de detrás dejaba a oscuras los huevos y los primeros metros.
	 */
	bool TileCastsShadow(double X0, double X1, double Y0, double Y1)
	{
		const double Reach = TNBeachLayout::HalfWidth + 6000.0;
		return X0 >= TNBeachLayout::BackWallX - 4000.0 && X1 >= TNBeachLayout::BackWallX - 4000.0 && Y1 >= -Reach && Y0 <= Reach;
	}

	UProceduralMeshComponent* NewTile(AActor* Owner, USceneComponent* Parent, bool bShadow)
	{
		UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(Owner, NAME_None, RF_Transient | RF_DuplicateTransient);
		Tile->ComponentTags.Add(TNBeachRaceKit::GeneratedTag());
		Tile->SetupAttachment(Parent);
		Tile->bUseAsyncCooking = false;
		Tile->SetCanEverAffectNavigation(false);
		Tile->SetCastShadow(bShadow);
		// Dato de primitiva 0 = 1: M_ProcTerrain aplica el relieve por normales y, donde el alfa es 1, grano y guijarros.
		Tile->SetCustomPrimitiveDataFloat(0, 1.f);
		Tile->RegisterComponent();
		return Tile;
	}

	void UploadTile(UProceduralMeshComponent* Tile, const FTileData& Data, bool bCollision, UMaterialInterface* Mat)
	{
		if (!Tile) { return; }
		Tile->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		const TArray<FProcMeshTangent> NoTangents;
		Tile->CreateMeshSection_LinearColor(0, Data.Verts, Data.Tris, Data.Normals, Data.UVs, Data.Colors, NoTangents, bCollision);
		if (Mat) { Tile->SetMaterial(0, Mat); }
	}

	/** Roca de la repisa: gris cálido con líquenes y arena por encima donde aún está medio enterrada. */
	FLinearColor LedgeColor(const FVector& Centroid, int32 A, int32 B)
	{
		const double Tone = 0.85 + 0.25 * TNBeachRaceKit::Hash01(A, B, 0x1ED6Eu);
		FLinearColor C = FLinearColor(0.5f, 0.46f, 0.4f) * static_cast<float>(Tone);
		if (TNBeachRaceKit::Hash01(B, A, 0x11C4Eu) < 0.07) { C = FLinearColor(0.7f, 0.57f, 0.25f) * static_cast<float>(Tone); }
		const double Dust = TNProcMap::SmoothStep(TNBeachLayout::Length - 1500.0, TNBeachLayout::Length - 2300.0, Centroid.X);
		C = TNProcMesh::TNProcLerpColor(C, FLinearColor(0.8f, 0.7f, 0.48f), static_cast<float>(0.7 * Dust));
		return C;
	}

	/** Pared del acantilado: estratos, banda mojada oscura en la línea del agua y algas por debajo. */
	FLinearColor FaceColor(double Z, int32 A, int32 B)
	{
		const double Tone = 0.85 + 0.25 * TNBeachRaceKit::Hash01(A, B, 0xFACEu);
		FLinearColor C = FLinearColor(0.4f, 0.36f, 0.31f) * static_cast<float>(Tone);
		C = C * static_cast<float>(1.0 + 0.07 * (FMath::Sin(Z / 170.0 + 0.6 * A) > 0.0 ? 1.0 : -1.0));
		if (Z < 160.0 && Z > -80.0) { C = C * 0.55f; }
		if (Z <= -80.0) { C = TNProcMesh::TNProcLerpColor(C * 0.5f, FLinearColor(0.12f, 0.19f, 0.12f), 0.6f); }
		return C;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Todo junto
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildAll()
{
	// Nada al cocinar. En el editor, en cada cliente y en el servidor (hace falta su colisión), igual: no se replica malla.
	if (IsTemplate() || !GetWorld() || IsRunningCommandlet()) { return; }
	const uint32 Key = HashCombine(GetTypeHash(JungleDensity), TNBeachRaceKit::BuildVersion);
	// Hecho y con todo en su sitio (si el editor quitara los componentes creados, se rehacen).
	bool bAlive = bBuilt && Key == BuiltKey && TerrainTiles.Num() > 0 && CliffMesh && CliffMesh->GetNumSections() > 0;
	for (int32 i = 0; bAlive && i < TerrainTiles.Num(); ++i)
	{
		const UProceduralMeshComponent* Tile = TerrainTiles[i];
		bAlive = IsValid(Tile) && Tile->IsRegistered();
	}
	for (int32 i = 0; bAlive && i < FloraComps.Num(); ++i)
	{
		const UInstancedStaticMeshComponent* Comp = FloraComps[i];
		bAlive = IsValid(Comp) && Comp->IsRegistered();
	}
	if (bAlive) { return; }
	ClearGenerated();

	const double T0 = FPlatformTime::Seconds();
	BuildTerrain();
	BuildCliff();
	BuildSeabedAndSea();
	BuildWalls();
	BuildStartGrove();
	BuildFinishDecor();
	BuildFeatures();
	BuildPoolWater();
	BuildStartEggs();
	if (!IsRunningDedicatedServer()) { BuildJungle(); }
	bBuilt = true;
	BuiltKey = Key;
	// Las tapas recién hechas, como diga la ronda (cerradas o ya rotas, sin saltos).
	ApplyStartEggs(false);
	BuildFootprints();

	int32 Tris = 0;
	TInlineComponentArray<UProceduralMeshComponent*> Meshes;
	GetComponents(Meshes);
	for (UProceduralMeshComponent* Comp : Meshes)
	{
		for (int32 s = 0; Comp && s < Comp->GetNumSections(); ++s)
		{
			if (const FProcMeshSection* Section = Comp->GetProcMeshSection(s)) { Tris += Section->ProcIndexBuffer.Num() / 3; }
		}
	}
	int32 Instances = 0;
	for (const UInstancedStaticMeshComponent* Comp : FloraComps)
	{
		if (Comp) { Instances += Comp->GetInstanceCount(); }
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] terreno fijo: %d teselas, %d triángulos, %d árboles y plantas de la selva en %d componentes · %.0f ms."),
		TerrainTiles.Num(), Tris, Instances, FloraComps.Num(), (FPlatformTime::Seconds() - T0) * 1000.0);
}

void ATN_BeachRaceGenerator::ClearGenerated()
{
	for (UProceduralMeshComponent* Tile : TerrainTiles)
	{
		if (Tile) { Tile->DestroyComponent(); }
	}
	TerrainTiles.Reset();
	for (UInstancedStaticMeshComponent* Comp : FloraComps)
	{
		if (Comp) { Comp->DestroyComponent(); }
	}
	FloraComps.Reset();
	FloraMeshes.Reset();
	for (UBoxComponent* Wall : Walls)
	{
		if (Wall) { Wall->DestroyComponent(); }
	}
	Walls.Reset();
	for (UStaticMeshComponent* Lid : StartEggLids)
	{
		if (Lid) { Lid->DestroyComponent(); }
	}
	StartEggLids.Reset();
	StartEggLidMeshes.Reset();
	bEggsAnimating = false;
	// Por si alguno se quedó fuera de las listas (una copia del actor, una recarga).
	TInlineComponentArray<UActorComponent*> Leftovers;
	GetComponents(Leftovers);
	for (UActorComponent* Comp : Leftovers)
	{
		if (Comp && Comp->ComponentHasTag(TNBeachRaceKit::GeneratedTag())) { Comp->DestroyComponent(); }
	}
	for (UProceduralMeshComponent* Comp : { CliffMesh.Get(), SeabedMesh.Get(), SeaMesh.Get(), GroveSolidMesh.Get(), GroveDecoMesh.Get(), FinishSolidMesh.Get(),
			 FinishDecoMesh.Get(), FloatMesh.Get(), FootprintMesh.Get(), FeatureMesh.Get(), FeatureDecoMesh.Get(), PoolMesh.Get() })
	{
		if (Comp) { Comp->ClearAllMeshSections(); }
	}
	GridXs.Reset();
	GridYs.Reset();
	TilesX = 0;
	TilesY = 0;
	bBuilt = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Arena
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildTerrain()
{
	// Filas (X): cada 3 m de la selva de detrás de la salida hasta debajo de la repisa de roca, cada 9 m más atrás.
	GridXs.Reset();
	{
		TArray<double> Back;
		double X = TNBeachLayout::TerrainGridMaxX;
		while (X > TNBeachLayout::TerrainGridFineMinX)
		{
			Back.Add(X);
			X -= TNBeachLayout::TerrainGridStep;
		}
		while (X > -24000.0)
		{
			Back.Add(X);
			X -= 900.0;
		}
		Back.Add(-24000.0);
		for (int32 i = Back.Num() - 1; i >= 0; --i) { GridXs.Add(Back[i]); }
	}
	// Columnas (Y): cada 3 m en la playa y el pie de los bancos, cada vez más separadas (hasta 18 m) hacia la selva.
	GridYs.Reset();
	{
		TArray<double> Side;
		double Y = 0.0;
		double Step = TNBeachLayout::TerrainGridStep;
		while (Y < TNBeachLayout::TerrainGridFineHalfY)
		{
			Side.Add(Y);
			Y += Step;
		}
		while (Y < TNBeachLayout::HalfWidth + 46000.0)
		{
			Side.Add(Y);
			Step = FMath::Min(1800.0, Step * 1.07);
			Y += Step;
		}
		Side.Add(TNBeachLayout::HalfWidth + 46000.0);
		for (int32 j = Side.Num() - 1; j >= 1; --j) { GridYs.Add(-Side[j]); }
		for (const double V : Side) { GridYs.Add(V); }
	}

	const int32 Q = TNBeachBuild::TileQuads;
	TilesX = FMath::DivideAndRoundUp(GridXs.Num() - 1, Q);
	TilesY = FMath::DivideAndRoundUp(GridYs.Num() - 1, Q);
	const int32 NumTiles = TilesX * TilesY;

	// Datos de cada tesela en paralelo (lógica pura); los componentes, después, en el hilo de juego.
	TArray<TNBeachBuild::FTileData> Data;
	Data.SetNum(NumTiles);
	const TArray<double>& Xs = GridXs;
	const TArray<double>& Ys = GridYs;
	const TArray<TNBeachLayout::FStamp>& Stamps = Layout.Stamps;
	const int32 NumTilesX = TilesX;
	ParallelFor(NumTiles, [&Data, &Xs, &Ys, &Stamps, NumTilesX, Q](int32 Index)
	{
		const int32 I0 = (Index % NumTilesX) * Q;
		const int32 J0 = (Index / NumTilesX) * Q;
		TNBeachBuild::ComputeTile(Xs, Ys, I0, FMath::Min(I0 + Q, Xs.Num() - 1), J0, FMath::Min(J0 + Q, Ys.Num() - 1), Stamps, Data[Index]);
	});

	UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial();
	TerrainTiles.SetNum(NumTiles);
	for (int32 Index = 0; Index < NumTiles; ++Index)
	{
		const int32 I0 = (Index % TilesX) * Q;
		const int32 J0 = (Index / TilesX) * Q;
		const int32 I1 = FMath::Min(I0 + Q, GridXs.Num() - 1);
		const int32 J1 = FMath::Min(J0 + Q, GridYs.Num() - 1);
		UProceduralMeshComponent* Tile = TNBeachBuild::NewTile(this, BeachRoot, TNBeachBuild::TileCastsShadow(GridXs[I0], GridXs[I1], GridYs[J0], GridYs[J1]));
		TNBeachBuild::UploadTile(Tile, Data[Index], TNBeachBuild::TileNeedsCollision(GridXs[I0], GridXs[I1], GridYs[J0], GridYs[J1]), Mat);
		TerrainTiles[Index] = Tile;
	}
}

void ATN_BeachRaceGenerator::BuildTerrainTile(int32 Index, const TArray<TNBeachLayout::FStamp>& Stamps)
{
	BuildTerrainTiles({ Index }, Stamps);
}

void ATN_BeachRaceGenerator::BuildTerrainTiles(const TArray<int32>& Indices, const TArray<TNBeachLayout::FStamp>& Stamps)
{
	if (TilesX <= 0 || Indices.Num() == 0) { return; }
	const int32 Q = TNBeachBuild::TileQuads;
	TArray<TNBeachBuild::FTileData> Data;
	Data.SetNum(Indices.Num());
	const TArray<double>& Xs = GridXs;
	const TArray<double>& Ys = GridYs;
	const int32 NumTilesX = TilesX;
	ParallelFor(Indices.Num(), [&Data, &Indices, &Xs, &Ys, &Stamps, NumTilesX, Q](int32 k)
	{
		const int32 Index = Indices[k];
		const int32 I0 = (Index % NumTilesX) * Q;
		const int32 J0 = (Index / NumTilesX) * Q;
		TNBeachBuild::ComputeTile(Xs, Ys, I0, FMath::Min(I0 + Q, Xs.Num() - 1), J0, FMath::Min(J0 + Q, Ys.Num() - 1), Stamps, Data[k]);
	});
	UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial();
	for (int32 k = 0; k < Indices.Num(); ++k)
	{
		const int32 Index = Indices[k];
		if (!TerrainTiles.IsValidIndex(Index)) { continue; }
		const int32 I0 = (Index % TilesX) * Q;
		const int32 J0 = (Index / TilesX) * Q;
		const int32 I1 = FMath::Min(I0 + Q, GridXs.Num() - 1);
		const int32 J1 = FMath::Min(J0 + Q, GridYs.Num() - 1);
		UProceduralMeshComponent* Tile = TerrainTiles[Index];
		if (!IsValid(Tile))
		{
			Tile = TNBeachBuild::NewTile(this, BeachRoot, TNBeachBuild::TileCastsShadow(GridXs[I0], GridXs[I1], GridYs[J0], GridYs[J1]));
			TerrainTiles[Index] = Tile;
		}
		TNBeachBuild::UploadTile(Tile, Data[k], TNBeachBuild::TileNeedsCollision(GridXs[I0], GridXs[I1], GridYs[J0], GridYs[J1]), Mat);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Asientos por partes (la ronda nueva): las teselas se calculan en otro hilo y se suben unas pocas por fotograma
// (TN_BeachRaceGenerator_Round.cpp)
// ─────────────────────────────────────────────────────────────────────────────

/** Teselas del terreno calculadas con los asientos de una ronda, a la espera de subirse. */
struct FTNBeachTileBatch
{
	TArray<int32> Indices;
	TArray<TNBeachBuild::FTileData> Data;
	int32 Next = 0;
};

void ATN_BeachRaceGenerator::FindTouchedTiles(const TArray<double>& Xs, const TArray<double>& Ys, int32 NumTilesX, int32 NumTilesY,
	const TArray<TNBeachLayout::FStamp>& OldStamps, const TArray<TNBeachLayout::FStamp>& NewStamps, bool bAll, TArray<int32>& OutIndices)
{
	OutIndices.Reset();
	if (NumTilesX <= 0 || NumTilesY <= 0 || Xs.Num() < 2 || Ys.Num() < 2) { return; }
	// Solo las teselas que tocan un asiento nuevo o uno de la ronda anterior (para quitarlo), con dos filas de margen para
	// que las normales de los bordes cuadren.
	const int32 Quads = TNBeachBuild::TileQuads;
	const double Margin = 700.0;
	const TArray<TNBeachLayout::FStamp>* Lists[2] = { &OldStamps, &NewStamps };
	for (int32 Ty = 0; Ty < NumTilesY; ++Ty)
	{
		for (int32 Tx = 0; Tx < NumTilesX; ++Tx)
		{
			const int32 I0 = FMath::Min(Tx * Quads, Xs.Num() - 1);
			const int32 I1 = FMath::Min(I0 + Quads, Xs.Num() - 1);
			const int32 J0 = FMath::Min(Ty * Quads, Ys.Num() - 1);
			const int32 J1 = FMath::Min(J0 + Quads, Ys.Num() - 1);
			bool bTouch = bAll;
			for (int32 l = 0; l < 2 && !bTouch; ++l)
			{
				for (const TNBeachLayout::FStamp& Stamp : *Lists[l])
				{
					const double R = Stamp.Radius + Stamp.Blend + Margin;
					if (FMath::Max(Stamp.A.X, Stamp.B.X) + R >= Xs[I0] && FMath::Min(Stamp.A.X, Stamp.B.X) - R <= Xs[I1]
						&& FMath::Max(Stamp.A.Y, Stamp.B.Y) + R >= Ys[J0] && FMath::Min(Stamp.A.Y, Stamp.B.Y) - R <= Ys[J1])
					{
						bTouch = true;
						break;
					}
				}
			}
			if (bTouch) { OutIndices.Add(Ty * NumTilesX + Tx); }
		}
	}
}

TSharedPtr<FTNBeachTileBatch> ATN_BeachRaceGenerator::ComputeTileBatch(const TArray<double>& Xs, const TArray<double>& Ys, int32 NumTilesX,
	const TArray<int32>& Indices, const TArray<TNBeachLayout::FStamp>& Stamps)
{
	TSharedPtr<FTNBeachTileBatch> Batch = MakeShared<FTNBeachTileBatch>();
	if (NumTilesX <= 0 || Indices.Num() == 0) { return Batch; }
	Batch->Indices = Indices;
	Batch->Data.SetNum(Indices.Num());
	const int32 Q = TNBeachBuild::TileQuads;
	TArray<TNBeachBuild::FTileData>& Data = Batch->Data;
	// Lógica pura (alturas con los asientos, normales y colores): en paralelo, desde el hilo que sea.
	ParallelFor(Indices.Num(), [&Data, &Indices, &Xs, &Ys, &Stamps, NumTilesX, Q](int32 k)
	{
		const int32 Index = Indices[k];
		const int32 I0 = (Index % NumTilesX) * Q;
		const int32 J0 = (Index / NumTilesX) * Q;
		TNBeachBuild::ComputeTile(Xs, Ys, I0, FMath::Min(I0 + Q, Xs.Num() - 1), J0, FMath::Min(J0 + Q, Ys.Num() - 1), Stamps, Data[k]);
	});
	return Batch;
}

bool ATN_BeachRaceGenerator::UploadPendingTiles(double BudgetSeconds)
{
	if (!PendingTiles.IsValid())
	{
		return true;
	}
	FTNBeachTileBatch& Batch = *PendingTiles;
	if (TilesX <= 0)
	{
		Batch.Next = Batch.Indices.Num();
		return true;
	}
	const double T0 = FPlatformTime::Seconds();
	const int32 Q = TNBeachBuild::TileQuads;
	UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial();
	bool bFirst = true;
	// La subida (con la colisión cocinada al momento) va en el hilo de juego: unas pocas por fotograma.
	while (Batch.Next < Batch.Indices.Num() && (bFirst || FPlatformTime::Seconds() - T0 < BudgetSeconds))
	{
		bFirst = false;
		const int32 k = Batch.Next++;
		const int32 Index = Batch.Indices[k];
		if (!TerrainTiles.IsValidIndex(Index)) { continue; }
		const int32 I0 = (Index % TilesX) * Q;
		const int32 J0 = (Index / TilesX) * Q;
		const int32 I1 = FMath::Min(I0 + Q, GridXs.Num() - 1);
		const int32 J1 = FMath::Min(J0 + Q, GridYs.Num() - 1);
		UProceduralMeshComponent* Tile = TerrainTiles[Index];
		if (!IsValid(Tile))
		{
			Tile = TNBeachBuild::NewTile(this, BeachRoot, TNBeachBuild::TileCastsShadow(GridXs[I0], GridXs[I1], GridYs[J0], GridYs[J1]));
			TerrainTiles[Index] = Tile;
		}
		TNBeachBuild::UploadTile(Tile, Batch.Data[k], TNBeachBuild::TileNeedsCollision(GridXs[I0], GridXs[I1], GridYs[J0], GridYs[J1]), Mat);
		// Ya subida: sus datos sobran.
		Batch.Data[k] = TNBeachBuild::FTileData();
	}
	return Batch.Next >= Batch.Indices.Num();
}

double ATN_BeachRaceGenerator::MeshGroundZ(double X, double Y) const
{
	const int32 NX = GridXs.Num();
	const int32 NY = GridYs.Num();
	if (NX < 2 || NY < 2) { return TNBeachLayout::SandZ(X, Y); }
	const int32 I = FMath::Clamp(Algo::UpperBound(GridXs, X) - 1, 0, NX - 2);
	const int32 J = FMath::Clamp(Algo::UpperBound(GridYs, Y) - 1, 0, NY - 2);
	const double X0 = GridXs[I];
	const double X1 = GridXs[I + 1];
	const double Y0 = GridYs[J];
	const double Y1 = GridYs[J + 1];
	const double A = FMath::Clamp((X - X0) / FMath::Max(1.0, X1 - X0), 0.0, 1.0);
	const double B = FMath::Clamp((Y - Y0) / FMath::Max(1.0, Y1 - Y0), 0.0, 1.0);
	const double Z00 = TNBeachLayout::SandZ(X0, Y0);
	const double Z10 = TNBeachLayout::SandZ(X1, Y0);
	const double Z01 = TNBeachLayout::SandZ(X0, Y1);
	const double Z11 = TNBeachLayout::SandZ(X1, Y1);
	// Los mismos triángulos que las teselas: (V00, V01, V10) y (V10, V01, V11).
	if (A + B <= 1.0) { return Z00 + A * (Z10 - Z00) + B * (Z01 - Z00); }
	return Z11 + (1.0 - A) * (Z01 - Z11) + (1.0 - B) * (Z10 - Z11);
}

// ─────────────────────────────────────────────────────────────────────────────
// Repisa y acantilado de roca
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildCliff()
{
	TNProcMesh::FTNProcMeshBuffers Top;
	TNProcMesh::FTNProcMeshBuffers Face;
	TNProcMesh::FTNProcMeshBuffers Rocks;
	const double CourseLen = TNBeachLayout::Length;

	// Repisa: filas desde 24 m antes del borde (enterrada bajo la arena) hasta el filo; columnas, las de la rejilla del
	// terreno, con un poco de desorden y cada vértice a su altura (caras planas de roca).
	static const double BackRows[] = { -2400.0, -2050.0, -1700.0, -1350.0, -1000.0, -700.0, -450.0 };
	constexpr int32 NumBack = static_cast<int32>(UE_ARRAY_COUNT(BackRows));
	constexpr int32 NumRows = NumBack + 1;
	const int32 NC = GridYs.Num();
	if (NC < 2) { return; }
	TArray<FVector> Ledge;
	Ledge.SetNum(NumRows * NC);
	for (int32 c = 0; c < NC; ++c)
	{
		const double Spacing = c + 1 < NC ? GridYs[c + 1] - GridYs[c] : GridYs[c] - GridYs[c - 1];
		for (int32 r = 0; r < NumRows; ++r)
		{
			const bool bEdge = r == NumBack;
			const double Jy = (c > 0 && c + 1 < NC) ? (TNBeachRaceKit::Hash01(r, c, 0x51u) - 0.5) * 0.45 * Spacing : 0.0;
			const double Y = GridYs[c] + Jy;
			const double X = bEdge ? TNBeachLayout::EdgeX(Y) : CourseLen + BackRows[r] + (TNBeachRaceKit::Hash01(c, r, 0x52u) - 0.5) * 160.0;
			// El filo, limpio y un poco levantado; detrás, bultos de hasta ±35 cm.
			const double Bump = bEdge ? 10.0 + 25.0 * TNBeachRaceKit::Hash01(r, c, 0x54u) : (TNBeachRaceKit::Hash01(r, c, 0x53u) - 0.5) * 70.0;
			Ledge[r * NC + c] = FVector(X, Y, TNBeachLayout::SandZ(X, Y) + TNBeachLayout::RockOffset(X) + Bump);
		}
	}
	for (int32 r = 0; r + 1 < NumRows; ++r)
	{
		for (int32 c = 0; c + 1 < NC; ++c)
		{
			const FVector& A = Ledge[r * NC + c];
			const FVector& B = Ledge[(r + 1) * NC + c];
			const FVector& C = Ledge[(r + 1) * NC + c + 1];
			const FVector& D = Ledge[r * NC + c + 1];
			Top.AddTri(A, B, C, FVector::UpVector, TNBeachBuild::LedgeColor((A + B + C) / 3.0, r * 2, c));
			Top.AddTri(A, C, D, FVector::UpVector, TNBeachBuild::LedgeColor((A + C + D) / 3.0, r * 2 + 1, c));
		}
	}

	// Pared: del filo al fondo del mar, casi vertical y un poco socavada (nunca sobresale del filo: se cae al agua).
	static const double Depths[] = { 0.0, 0.12, 0.33, 0.55, 0.78, 1.0 };
	static const double Undercut[] = { 0.0, 60.0, 140.0, 200.0, 240.0, 280.0 };
	constexpr int32 NumFace = static_cast<int32>(UE_ARRAY_COUNT(Depths));
	TArray<FVector> Wall;
	Wall.SetNum(NumFace * NC);
	for (int32 c = 0; c < NC; ++c)
	{
		const FVector& Lip = Ledge[NumBack * NC + c];
		const double Bottom = TNBeachLayout::SeabedZ(Lip.X + 200.0, Lip.Y) - 200.0;
		for (int32 k = 0; k < NumFace; ++k)
		{
			const double Inset = Undercut[k] * (0.45 + 0.55 * TNBeachRaceKit::Hash01(k, c, 0x55u));
			const double Jy = k > 0 && c > 0 && c + 1 < NC ? (TNBeachRaceKit::Hash01(c, k, 0x56u) - 0.5) * 120.0 : 0.0;
			Wall[k * NC + c] = FVector(Lip.X - Inset, Lip.Y + Jy, FMath::Lerp(Lip.Z, Bottom, Depths[k]));
		}
	}
	for (int32 k = 0; k + 1 < NumFace; ++k)
	{
		for (int32 c = 0; c + 1 < NC; ++c)
		{
			const FVector& A = Wall[k * NC + c];
			const FVector& B = Wall[k * NC + c + 1];
			const FVector& C = Wall[(k + 1) * NC + c + 1];
			const FVector& D = Wall[(k + 1) * NC + c];
			Face.AddQuad(A, B, C, D, FVector(1.0, 0.0, 0.0), TNBeachBuild::FaceColor(0.25 * (A.Z + B.Z + C.Z + D.Z), k, c));
		}
	}

	// Peñascos al pie de los cabos (fuera de la zona donde se cae al agua) y algunos sobre la repisa junto a la selva.
	TNProcMap::FRng Rng(static_cast<uint64>(TNBeachLayout::TerrainSeed) * 31ull + 7ull);
	const FLinearColor RockC(0.42f, 0.38f, 0.33f);
	for (const double Side : { -1.0, 1.0 })
	{
		for (double U = TNBeachLayout::HalfWidth - 2600.0; U < TNBeachLayout::HalfWidth + 40000.0; U += Rng.Range(1600.0, 2800.0))
		{
			const double Y = Side * U;
			const FVector Base(TNBeachLayout::EdgeX(Y) + Rng.Range(250.0, 1000.0), Y, TNBeachLayout::SeabedZ(TNBeachLayout::EdgeX(Y) + 600.0, Y) - 100.0);
			TNProcMesh::TNProcAddBoulder(Rocks, Base, Rng.Range(350.0, 950.0), Rng.Range(700.0, 1900.0), static_cast<uint32>(Rng.RangeInt(1, 1 << 20)), RockC);
		}
		for (double U = TNBeachLayout::HalfWidth - 1800.0; U < TNBeachLayout::HalfWidth + 6000.0; U += Rng.Range(900.0, 1800.0))
		{
			const double Y = Side * U;
			const double X = CourseLen - Rng.Range(300.0, 1500.0);
			TNProcMesh::TNProcAddBoulder(Rocks, FVector(X, Y, TNBeachLayout::GroundZ(X, Y) - 30.0), Rng.Range(200.0, 500.0), Rng.Range(250.0, 600.0),
				static_cast<uint32>(Rng.RangeInt(1, 1 << 20)), RockC * 1.1f);
		}
	}

	// Caras planas con su color, sin relieve del terreno (dato de primitiva 0 a 0, como las formaciones pintadas).
	UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial();
	TNBeachRaceKit::Upload(CliffMesh, 0, Top, Mat, true);
	TNBeachRaceKit::Upload(CliffMesh, 1, Face, Mat, true);
	TNBeachRaceKit::Upload(CliffMesh, 2, Rocks, Mat, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Fondo y superficie del mar
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildSeabedAndSea()
{
	const double CourseLen = TNBeachLayout::Length;
	// Fondo: desde debajo de la repisa hasta 900 m mar adentro, con las columnas de la rejilla una sí y otra no.
	TArray<double> Xs;
	{
		double X = CourseLen - 1000.0;
		double Step = 400.0;
		while (X < CourseLen + 90000.0)
		{
			Xs.Add(X);
			X += Step;
			if (X > CourseLen + 1000.0) { Step = FMath::Min(6000.0, Step * 1.3); }
		}
		Xs.Add(CourseLen + 90000.0);
	}
	TArray<double> Ys;
	for (int32 j = 0; j < GridYs.Num(); j += 2) { Ys.Add(GridYs[j]); }
	if (GridYs.Num() > 0 && Ys.Last() != GridYs.Last()) { Ys.Add(GridYs.Last()); }

	TNBeachBuild::FTileData Bed;
	const int32 W = Xs.Num();
	const int32 H = Ys.Num();
	for (int32 j = 0; j < H; ++j)
	{
		for (int32 i = 0; i < W; ++i)
		{
			const double X = Xs[i];
			const double Y = Ys[j];
			const double Z = TNBeachLayout::SeabedZ(X, Y);
			const double DzDx = (TNBeachLayout::SeabedZ(X + 100.0, Y) - TNBeachLayout::SeabedZ(X - 100.0, Y)) / 200.0;
			const double DzDy = (TNBeachLayout::SeabedZ(X, Y + 100.0) - TNBeachLayout::SeabedZ(X, Y - 100.0)) / 200.0;
			Bed.Verts.Add(FVector(X, Y, Z));
			Bed.Normals.Add(FVector(-DzDx, -DzDy, 1.0).GetSafeNormal());
			Bed.UVs.Add(FVector2D(X, Y) / 500.0);
			FLinearColor C = FLinearColor(0.6f, 0.51f, 0.34f) * static_cast<float>(FMath::Lerp(1.0, 0.55, TNProcMap::SmoothStep(-1100.0, -4000.0, Z)));
			C.A = 0.35f;
			Bed.Colors.Add(C);
		}
	}
	for (int32 j = 0; j + 1 < H; ++j)
	{
		for (int32 i = 0; i + 1 < W; ++i)
		{
			const int32 V00 = j * W + i;
			const int32 V10 = V00 + 1;
			const int32 V01 = V00 + W;
			const int32 V11 = V01 + 1;
			Bed.Tris.Add(V00);
			Bed.Tris.Add(V01);
			Bed.Tris.Add(V10);
			Bed.Tris.Add(V10);
			Bed.Tris.Add(V01);
			Bed.Tris.Add(V11);
		}
	}
	if (SeabedMesh)
	{
		const TArray<FProcMeshTangent> NoTangents;
		SeabedMesh->SetCustomPrimitiveDataFloat(0, 1.f);
		SeabedMesh->CreateMeshSection_LinearColor(0, Bed.Verts, Bed.Tris, Bed.Normals, Bed.UVs, Bed.Colors, NoTangents, true);
		if (UMaterialInterface* Mat = TNBeachRaceKit::TerrainMaterial()) { SeabedMesh->SetMaterial(0, Mat); }
	}

	// Superficie: del pie del acantilado al horizonte (6 km) y muy a los lados; el terreno la tapa donde está por encima.
	TNProcMesh::FTNProcMeshBuffers Sea;
	static const double SeaX[] = { -1500.0, 3000.0, 12000.0, 40000.0, 150000.0, 600000.0 };
	static const double SeaY[] = { -600000.0, -150000.0, -60000.0, -25000.0, -8000.0, 0.0, 8000.0, 25000.0, 60000.0, 150000.0, 600000.0 };
	const FLinearColor Turquoise = TNBeachRaceKit::Hex(0x2FB5C8u);
	for (int32 i = 0; i + 1 < static_cast<int32>(UE_ARRAY_COUNT(SeaX)); ++i)
	{
		for (int32 j = 0; j + 1 < static_cast<int32>(UE_ARRAY_COUNT(SeaY)); ++j)
		{
			const double Z = TNBeachLayout::WaterZ;
			Sea.AddQuad(FVector(CourseLen + SeaX[i], SeaY[j], Z), FVector(CourseLen + SeaX[i + 1], SeaY[j], Z), FVector(CourseLen + SeaX[i + 1], SeaY[j + 1], Z),
				FVector(CourseLen + SeaX[i], SeaY[j + 1], Z), FVector::UpVector, Turquoise);
		}
	}
	// El mar animado del mapa procedural, con la hondura y la espuma a la escala de la playa (11 m de agua al pie).
	UMaterialInterface* SeaMat = TNBeachRaceKit::SeaMaterial();
	if (SeaMat)
	{
		SeaMaterial = UMaterialInstanceDynamic::Create(SeaMat, this);
		if (SeaMaterial)
		{
			SeaMaterial->SetFlags(RF_Transient);
			SeaMaterial->SetScalarParameterValue(TEXT("DepthRange"), 1500.f);
			SeaMaterial->SetScalarParameterValue(TEXT("FoamWidth"), 160.f);
			SeaMat = SeaMaterial.Get();
		}
	}
	TNBeachRaceKit::Upload(SeaMesh, 0, Sea, SeaMat, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Muros invisibles y agua nadable
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildWalls()
{
	// A los lados (8 m fuera de la playa jugable, también mar adentro), detrás de la salida y en el mar; altísimos, para
	// que nada que vuele (gaviotas, lanzamientos) saque a una tortuga.
	const double X0 = TNBeachLayout::BackWallX - 600.0;
	const double X1 = TNBeachLayout::Length + TNBeachLayout::FinishWaterReach;
	const double Yw = TNBeachLayout::SideWallY;
	const double Zc = 50000.0;
	const double Zh = 70000.0;
	struct FWallDef
	{
		FVector Center;
		FVector Extent;
	};
	const FWallDef Defs[] = {
		{ FVector(0.5 * (X0 + X1), -(Yw + 300.0), Zc), FVector(0.5 * (X1 - X0), 300.0, Zh) },
		{ FVector(0.5 * (X0 + X1), Yw + 300.0, Zc), FVector(0.5 * (X1 - X0), 300.0, Zh) },
		{ FVector(TNBeachLayout::BackWallX - 300.0, 0.0, Zc), FVector(300.0, Yw + 600.0, Zh) },
		{ FVector(X1 + 300.0, 0.0, Zc), FVector(300.0, Yw + 600.0, Zh) },
	};
	for (const FWallDef& Def : Defs)
	{
		UBoxComponent* Wall = NewObject<UBoxComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
		Wall->ComponentTags.Add(TNBeachRaceKit::GeneratedTag());
		Wall->SetupAttachment(BeachRoot);
		Wall->SetBoxExtent(Def.Extent, false);
		Wall->SetCollisionProfileName(TEXT("InvisibleWall"));
		Wall->SetCanEverAffectNavigation(false);
		Wall->SetHiddenInGame(true);
		Wall->RegisterComponent();
		Wall->SetRelativeLocation(Def.Center);
		Walls.Add(Wall);
	}
}

void ATN_BeachRaceGenerator::SpawnWaterVolume()
{
	UWorld* World = GetWorld();
	if (!World || WaterVolume.IsValid()) { return; }
	// Local en cada máquina (la natación la predicen servidor y cliente): el agua de meta entera, del pie del acantilado
	// (bajo la repisa) a 300 m mar adentro, con el techo a ras del agua (se nada con el centro de la cápsula por debajo).
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	ATN_ProcWaterVolume* Water = World->SpawnActor<ATN_ProcWaterVolume>(ATN_ProcWaterVolume::StaticClass(), GetActorTransform(), Params);
	if (!Water) { return; }
	const double X0 = TNBeachLayout::Length - 600.0;
	const double X1 = TNBeachLayout::Length + TNBeachLayout::FinishWaterReach;
	const double Z0 = TNBeachLayout::SeabedFootZ - 5000.0;
	const double Z1 = TNBeachLayout::WaterZ;
	const FVector Center = GetActorTransform().TransformPosition(FVector(0.5 * (X0 + X1), 0.0, 0.5 * (Z0 + Z1)));
	Water->AddWaterBox(Center, FVector(0.5 * (X1 - X0), TNBeachLayout::HalfWidth + TNBeachLayout::FinishWaterSide, 0.5 * (Z1 - Z0)));
	// Las pozas: rebanadas de 2 m donde hay agua honda (se nada en medio y se sale por la orilla andando).
	TArray<FBox> PoolBoxes;
	for (const TNBeachLayout::FPool& Pool : TNBeachLayout::Pools()) { TNBeachLayout::PoolSwimBoxes(Pool, PoolBoxes); }
	for (const FBox& Box : PoolBoxes)
	{
		Water->AddWaterBox(GetActorTransform().TransformPosition(Box.GetCenter()), Box.GetExtent());
	}
	WaterVolume = Water;
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] agua nadable: la de meta y %d pozas en %d cajas."), TNBeachLayout::Pools().Num(), PoolBoxes.Num());
}
