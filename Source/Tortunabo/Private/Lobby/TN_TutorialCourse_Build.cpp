// ─────────────────────────────────────────────────────────────────────────────
// ATN_TutorialCourse — construcción (en cada máquina, igual en todas): las dos islas con el terreno del mapa procedural
// (material, colores de bioma, camino y paredes como ATN_ProcMapGenerator::BuildTerrain), su roca colgando, la
// vegetación y los objetos sueltos del generador (TN_ProcMapFlora.h con la misma semilla fija), el agua de la laguna y
// del arroyo, la cascada que se desvanece, las nubes, los carteles de cada estación y la fauna.
// ─────────────────────────────────────────────────────────────────────────────

#include "Lobby/TN_TutorialCourse.h"
#include "Core/TN_ProjectMaterials.h"
#include "Lobby/TN_TutorialFauna.h"
#include "TN_TutorialLayout.h"
#include "TN_TutorialTexts.h"
#include "TN_CastleKit.h"
#include "Playground/TN_PlaygroundMeshKit.h"
#include "Art/TN_Art.h"
#include "../Art/TN_ArtPieces.h"
#include "../World/ProcMap/TN_ProcMapMeshKit.h"
#include "../World/ProcMap/TN_ProcMapRuntimeMesh.h"
#include "../World/ProcMap/TN_ProcMapFloraMeshes.h"
#include "../World/ProcMap/TN_ProcMapPropMeshes.h"
#include "../World/ProcMap/TN_ProcMapTrailColors.h"
#include "World/ProcMap/TN_ProcMapFlora.h"
#include "World/ProcMap/TN_ProcMapTypes.h"
#include "World/ProcMap/TN_ProcWaterActors.h"
#include "Core/TN_Log.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace TNTutorialBuildDetail
{
	using namespace TNTutorial;
	using TNProcMesh::FTNProcMeshBuffers;

	/** Muestras de cada fila de la malla: meseta, talud y pasillo a cada lado, y la roca de debajo. */
	constexpr int32 NumPlateau = 10;
	constexpr int32 NumBank = 10;
	constexpr int32 NumCorridor = 16;
	constexpr int32 NumTop = NumPlateau + NumBank + NumCorridor + NumBank + NumPlateau + 1;
	constexpr int32 NumUnder = 9;

	/** Ajustes del mapa procedural (para que los colores y materiales sean los suyos); sin el asset, los de código. */
	const UTN_ProcMapSettings* ProcSettings()
	{
		return LoadObject<UTN_ProcMapSettings>(nullptr, TEXT("/Game/ProcMap/DA_ProcMapSettings.DA_ProcMapSettings"), nullptr, LOAD_NoWarn);
	}

	UMaterialInterface* LoadMat(const TCHAR* Path, UMaterialInterface* Fallback)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn);
		return Mat ? Mat : Fallback;
	}


	void BiomeColors(const UTN_ProcMapSettings* Settings, ETNProcBiome Biome, FLinearColor& Ground, FLinearColor& Path, FLinearColor& Rock, FLinearColor& Bed)
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

	/** Colores de todos los biomas (el camino, con el contraste y el tono de sendero del generador). */
	struct FPalette
	{
		FLinearColor Ground[TNProcMap::NumBiomes];
		FLinearColor PathC[TNProcMap::NumBiomes];
		FLinearColor Rock[TNProcMap::NumBiomes];
		FLinearColor Bed[TNProcMap::NumBiomes];

		explicit FPalette(const UTN_ProcMapSettings* Settings)
		{
			for (int32 b = 0; b < TNProcMap::NumBiomes; ++b)
			{
				FLinearColor Raw;
				BiomeColors(Settings, TNProcMap::BiomeFromIndex(b), Ground[b], Raw, Rock[b], Bed[b]);
				PathC[b] = TNProcMesh::TNTrailColor(TNProcMap::BiomeFromIndex(b), TNProcMesh::TNContrastPath(Raw, Ground[b], Rock[b]));
			}
		}

		void Blend(double X, FLinearColor& OutG, FLinearColor& OutP, FLinearColor& OutR, FLinearColor& OutB) const
		{
			double W[TNProcMap::NumBiomes];
			BiomeWeights(X, W);
			OutG = OutP = OutR = OutB = FLinearColor(0.f, 0.f, 0.f, 0.f);
			for (int32 b = 0; b < TNProcMap::NumBiomes; ++b)
			{
				const float Wb = static_cast<float>(W[b]);
				OutG += Ground[b] * Wb;
				OutP += PathC[b] * Wb;
				OutR += Rock[b] * Wb;
				OutB += Bed[b] * Wb;
			}
		}
	};

	/** Normales lisas por vértice: suma de las caras (ponderadas por el área) de los triángulos ya emitidos. */
	void AccumulateNormals(FTNProcMeshBuffers& B)
	{
		B.Normals.Init(FVector::ZeroVector, B.Verts.Num());
		for (int32 t = 0; t + 2 < B.Tris.Num(); t += 3)
		{
			const int32 I0 = B.Tris[t];
			const int32 I1 = B.Tris[t + 1];
			const int32 I2 = B.Tris[t + 2];
			// Cara frontal en UE: (C - A) x (B - A) con (A, B, C) en el orden emitido.
			const FVector Face = FVector::CrossProduct(B.Verts[I2] - B.Verts[I0], B.Verts[I1] - B.Verts[I0]);
			B.Normals[I0] += Face;
			B.Normals[I1] += Face;
			B.Normals[I2] += Face;
		}
		for (FVector& N : B.Normals)
		{
			N = N.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		}
	}

	/** Emite la rejilla Rows × Cols (vértices ya puestos a partir de Base) con la cara frontal hacia +Z de su parámetro. */
	void EmitGrid(FTNProcMeshBuffers& B, int32 Base, int32 Rows, int32 Cols, bool bFlip)
	{
		for (int32 r = 0; r + 1 < Rows; ++r)
		{
			for (int32 c = 0; c + 1 < Cols; ++c)
			{
				const int32 A = Base + r * Cols + c;
				const int32 Bi = Base + (r + 1) * Cols + c;
				const int32 C = Base + (r + 1) * Cols + c + 1;
				const int32 D = Base + r * Cols + c + 1;
				if (!bFlip)
				{
					B.Tris.Append({ A, C, Bi, A, D, C });
				}
				else
				{
					B.Tris.Append({ A, Bi, C, A, C, D });
				}
			}
		}
	}

	void PushVertex(FTNProcMeshBuffers& B, const FVector& P, const FLinearColor& Color, const FVector2D& UV)
	{
		B.Verts.Add(P);
		B.Normals.Add(FVector::UpVector);
		B.UVs.Add(UV);
		B.Colors.Add(Color);
	}

	/** Lugares laterales (D respecto al centro del pasillo) de las muestras de arriba de la fila X, de borde a borde. */
	void TopOffsets(double X, double (&Out)[NumTop])
	{
		const double C = CorridorCenter(X);
		const double W = CorridorHalfWidth(X);
		const double BW = BankWidth(X);
		const double RimL = -RimLeft(X) - C;
		const double RimR = RimRight(X) - C;
		int32 j = 0;
		for (int32 k = 0; k < NumPlateau; ++k) { Out[j++] = TNProcMap::LerpD(RimL, -(W + BW), static_cast<double>(k) / NumPlateau); }
		for (int32 k = 0; k < NumBank; ++k) { Out[j++] = TNProcMap::LerpD(-(W + BW), -W, static_cast<double>(k) / NumBank); }
		for (int32 k = 0; k < NumCorridor; ++k) { Out[j++] = TNProcMap::LerpD(-W, W, static_cast<double>(k) / NumCorridor); }
		for (int32 k = 0; k < NumBank; ++k) { Out[j++] = TNProcMap::LerpD(W, W + BW, static_cast<double>(k) / NumBank); }
		for (int32 k = 0; k <= NumPlateau; ++k) { Out[j++] = TNProcMap::LerpD(W + BW, RimR, static_cast<double>(k) / NumPlateau); }
	}

	/** Color del terreno de arriba (como ATN_ProcMapGenerator::BuildTerrain): suelo, roca en los taludes y el camino. */
	FLinearColor TopColor(const FPalette& Pal, const FVector& P, const FVector& N, double D)
	{
		FLinearColor G, Pc, R, Bd;
		Pal.Blend(P.X, G, Pc, R, Bd);
		const double W = CorridorHalfWidth(P.X);
		float Mask = static_cast<float>(Smooth(W + 30.0, W - 40.0, FMath::Abs(D)) * Smooth(0.45, 0.75, N.Z));
		// El rincón de la salida (el pasillo sube a la meseta) ya no es camino.
		Mask *= static_cast<float>(Smooth(-450.0, -250.0, P.X));
		const float Steep = static_cast<float>(Smooth(0.88, 0.55, N.Z));
		const float Cliff = static_cast<float>(Smooth(0.55, 0.15, N.Z));
		FLinearColor Col = TNProcMesh::TNProcLerpColor(G, R, Steep);
		Col = Col * FMath::Lerp(1.f, 0.62f, Cliff);
		const double Strata = FMath::Sin(P.Z / 170.0 + 1.3 * TNProcMap::Noise2(Seed + 7u, P.X / 3000.0, P.Y / 3000.0));
		Col = Col * (1.f + 0.07f * Steep * static_cast<float>(FMath::Clamp(Strata * 3.0, -1.0, 1.0)));
		Col = TNProcMesh::TNProcLerpColor(Col, Pc, Mask);
		Col = Col * (1.f - 2.0f * Mask * (1.f - Mask));
		// Lecho de la laguna.
		const double BedW = Band(Dims::PoolX0, Dims::PoolX1, P.X, 60.0) * Smooth(30.0, -120.0, P.Z - Dims::PoolWaterZ);
		Col = TNProcMesh::TNProcLerpColor(Col, Bd, static_cast<float>(BedW));
		const float Var = 0.9f + 0.2f * static_cast<float>(0.5 + 0.5 * TNProcMap::Noise2(Seed, P.X / 700.0, P.Y / 700.0));
		Col = Col * Var;
		Col.A = Mask;
		return Col;
	}

	/** Roca que cuelga bajo la isla: tierra bajo el borde, estratos de roca y más oscuro hacia la quilla. */
	FLinearColor UnderColor(const FPalette& Pal, const FVector& P, double U)
	{
		FLinearColor G, Pc, R, Bd;
		Pal.Blend(P.X, G, Pc, R, Bd);
		const FLinearColor Soil = G * 0.62f + FLinearColor(0.09f, 0.06f, 0.035f, 0.f);
		FLinearColor Col = TNProcMesh::TNProcLerpColor(Soil, R * 0.8f, static_cast<float>(Smooth(0.04, 0.3, U)));
		const double Band1 = FMath::Sin(P.Z / 130.0 + 2.0 * TNProcMap::Noise2(Seed + 41u, P.X / 1500.0, P.Y / 900.0));
		Col = Col * (0.9f + 0.12f * static_cast<float>(Band1));
		Col = Col * FMath::Lerp(1.f, 0.5f, static_cast<float>(FMath::Pow(U, 1.4)));
		Col.A = 0.f;
		return Col;
	}

	/**
	 * Espacio del reparto de la vegetación (PlaceFloraRows del mapa): X a lo largo del pasillo y D desde su centro, desplazados
	 * para empezar en 0. La transformación (X, D) → (X, Y) conserva las áreas: las densidades son las del mapa.
	 */
	constexpr double FloraXMin = Dims::PartA0 - 200.0;
	constexpr double FloraXMax = Dims::PartB1 + 200.0;
	constexpr double FloraDMin = -2600.0;
	constexpr double FloraDMax = 2600.0;

	/** Consultas del reparto (las del generador del mapa): la altura sobre el agua, la normal, el borde del camino y lo vetado. */
	struct FFloraQuery
	{
		static double X(const FVector2D& P) { return P.X + FloraXMin; }
		static double D(const FVector2D& P) { return P.Y + FloraDMin; }

		double Height(const FVector2D& P) const
		{
			const double Xs = X(P);
			return TNTutorial::SurfaceZ(Xs, D(P) + TNTutorial::CorridorCenter(Xs)) - Dims::PoolWaterZ;
		}

		FVector Normal(const FVector2D& P) const
		{
			const double Xs = X(P);
			const double Ys = D(P) + TNTutorial::CorridorCenter(Xs);
			constexpr double E = 40.0;
			const double Hx = (TNTutorial::SurfaceZ(Xs + E, Ys) - TNTutorial::SurfaceZ(Xs - E, Ys)) / (2.0 * E);
			const double Hy = (TNTutorial::SurfaceZ(Xs, Ys + E) - TNTutorial::SurfaceZ(Xs, Ys - E)) / (2.0 * E);
			return FVector(-Hx, -Hy, 1.0).GetSafeNormal();
		}

		/** Distancia al borde del pasillo (0 o menos sobre él: ahí no crece nada). */
		double Edge(const FVector2D& P) const { return FMath::Abs(D(P)) - TNTutorial::CorridorHalfWidth(X(P)); }

		/** Fuera de las islas, en su borde o junto a los cortes (el cañón, las puntas). */
		bool Blocked(const FVector2D& P) const
		{
			const double Xs = X(P);
			const int32 Part = TNTutorial::PartOf(Xs);
			if (Part == INDEX_NONE || TNTutorial::PartOf(Xs - 150.0) != Part || TNTutorial::PartOf(Xs + 150.0) != Part)
			{
				return true;
			}
			const double Ys = D(P) + TNTutorial::CorridorCenter(Xs);
			return Ys > TNTutorial::RimRight(Xs) - 120.0 || Ys < -TNTutorial::RimLeft(Xs) + 120.0;
		}
	};

	/**
	 * Pieza de arte de cada especie de la vegetación y de cada objeto suelto del tutorial (Docs/Arte_Assets.md): sus variantes
	 * y biomas comparten la malla de arte. Las de los siete biomas del recorrido (el volcán no sale).
	 */
	FName FloraSlot(TNProcMap::EFloraShape Shape, TNProcMap::EPropKind Prop)
	{
		using ES = TNProcMap::EFloraShape;
		using EP = TNProcMap::EPropKind;
		if (Shape == ES::Prop)
		{
			switch (Prop)
			{
				case EP::Crate:       return TN_ART("Lobby.Tutorial.Flora.Crate");
				case EP::WoodBarrel:  return TN_ART("Lobby.Tutorial.Flora.WoodBarrel");
				case EP::Barricade:   return TN_ART("Lobby.Tutorial.Flora.Barricade");
				case EP::TrafficCone: return TN_ART("Lobby.Tutorial.Flora.TrafficCone");
				case EP::HayBale:     return TN_ART("Lobby.Tutorial.Flora.HayBale");
				case EP::Bench:       return TN_ART("Lobby.Tutorial.Flora.Bench");
				case EP::LampPost:    return TN_ART("Lobby.Tutorial.Flora.LampPost");
				case EP::Mailbox:     return TN_ART("Lobby.Tutorial.Flora.Mailbox");
				case EP::Sacks:       return TN_ART("Lobby.Tutorial.Flora.Sacks");
				case EP::FlowerPot:   return TN_ART("Lobby.Tutorial.Flora.FlowerPot");
				case EP::Shell:       return TN_ART("Lobby.Tutorial.Flora.Shell");
				case EP::Starfish:    return TN_ART("Lobby.Tutorial.Flora.Starfish");
				case EP::SandBucket:  return TN_ART("Lobby.Tutorial.Flora.SandBucket");
				case EP::BeachTowel:  return TN_ART("Lobby.Tutorial.Flora.BeachTowel");
				case EP::Surfboard:   return TN_ART("Lobby.Tutorial.Flora.Surfboard");
				case EP::Parasol:     return TN_ART("Lobby.Tutorial.Flora.Parasol");
				case EP::Driftwood:   return TN_ART("Lobby.Tutorial.Flora.Driftwood");
				case EP::Coconuts:    return TN_ART("Lobby.Tutorial.Flora.Coconuts");
				case EP::Lifebuoy:    return TN_ART("Lobby.Tutorial.Flora.Lifebuoy");
				case EP::Mushrooms:   return TN_ART("Lobby.Tutorial.Flora.Mushrooms");
				case EP::ClayPot:     return TN_ART("Lobby.Tutorial.Flora.ClayPot");
				case EP::TikiTorch:   return TN_ART("Lobby.Tutorial.Flora.TikiTorch");
				case EP::SkullPost:   return TN_ART("Lobby.Tutorial.Flora.SkullPost");
				case EP::CattleSkull: return TN_ART("Lobby.Tutorial.Flora.CattleSkull");
				case EP::Bones:       return TN_ART("Lobby.Tutorial.Flora.Bones");
				case EP::Amphora:     return TN_ART("Lobby.Tutorial.Flora.Amphora");
				case EP::WagonWheel:  return TN_ART("Lobby.Tutorial.Flora.WagonWheel");
				case EP::Signpost:    return TN_ART("Lobby.Tutorial.Flora.Signpost");
				case EP::Tumbleweed:  return TN_ART("Lobby.Tutorial.Flora.Tumbleweed");
				case EP::Crystals:    return TN_ART("Lobby.Tutorial.Flora.Crystals");
				case EP::Stump:       return TN_ART("Lobby.Tutorial.Flora.Stump");
				case EP::Cairn:       return TN_ART("Lobby.Tutorial.Flora.Cairn");
				case EP::Lantern:     return TN_ART("Lobby.Tutorial.Flora.Lantern");
				case EP::CrabTrap:    return TN_ART("Lobby.Tutorial.Flora.CrabTrap");
				default:              return NAME_None;
			}
		}
		switch (Shape)
		{
			case ES::BroadTree:    return TN_ART("Lobby.Tutorial.Flora.BroadTree");
			case ES::Ceiba:        return TN_ART("Lobby.Tutorial.Flora.Ceiba");
			case ES::Palm:         return TN_ART("Lobby.Tutorial.Flora.Palm");
			case ES::MangroveTree: return TN_ART("Lobby.Tutorial.Flora.MangroveTree");
			case ES::YoungSequoia: return TN_ART("Lobby.Tutorial.Flora.YoungSequoia");
			case ES::Cypress:      return TN_ART("Lobby.Tutorial.Flora.Cypress");
			case ES::Pine:         return TN_ART("Lobby.Tutorial.Flora.Pine");
			case ES::Fir:          return TN_ART("Lobby.Tutorial.Flora.Fir");
			case ES::Willow:       return TN_ART("Lobby.Tutorial.Flora.Willow");
			case ES::Acacia:       return TN_ART("Lobby.Tutorial.Flora.Acacia");
			case ES::DeadTree:     return TN_ART("Lobby.Tutorial.Flora.DeadTree");
			case ES::Ornamental:   return TN_ART("Lobby.Tutorial.Flora.Ornamental");
			case ES::Fern:         return TN_ART("Lobby.Tutorial.Flora.Fern");
			case ES::Bush:         return TN_ART("Lobby.Tutorial.Flora.Bush");
			case ES::Grass:        return TN_ART("Lobby.Tutorial.Flora.Grass");
			case ES::Flowers:      return TN_ART("Lobby.Tutorial.Flora.Flowers");
			case ES::Reeds:        return TN_ART("Lobby.Tutorial.Flora.Reeds");
			case ES::Saguaro:      return TN_ART("Lobby.Tutorial.Flora.Saguaro");
			case ES::Barrel:       return TN_ART("Lobby.Tutorial.Flora.BarrelCactus");
			case ES::DryBush:      return TN_ART("Lobby.Tutorial.Flora.DryBush");
			case ES::Hedge:        return TN_ART("Lobby.Tutorial.Flora.Hedge");
			case ES::Umbrella:     return TN_ART("Lobby.Tutorial.Flora.Umbrella");
			case ES::Creeper:      return TN_ART("Lobby.Tutorial.Flora.Creeper");
			case ES::BananaPlant:  return TN_ART("Lobby.Tutorial.Flora.BananaPlant");
			case ES::Bamboo:       return TN_ART("Lobby.Tutorial.Flora.Bamboo");
			case ES::TreeFern:     return TN_ART("Lobby.Tutorial.Flora.TreeFern");
			case ES::SeaGrape:     return TN_ART("Lobby.Tutorial.Flora.SeaGrape");
			case ES::Pandanus:     return TN_ART("Lobby.Tutorial.Flora.Pandanus");
			case ES::FanPalm:      return TN_ART("Lobby.Tutorial.Flora.FanPalm");
			case ES::Casuarina:    return TN_ART("Lobby.Tutorial.Flora.Casuarina");
			case ES::JoshuaTree:   return TN_ART("Lobby.Tutorial.Flora.JoshuaTree");
			case ES::Birch:        return TN_ART("Lobby.Tutorial.Flora.Birch");
			case ES::Rock:         return TN_ART("Lobby.Tutorial.Flora.Rock");
			case ES::Stones:       return TN_ART("Lobby.Tutorial.Flora.Stones");
			default:               return NAME_None;
		}
	}

	/** Donde se ponen los carteles: marco del cartel (X hacia quien llega y un poco hacia el centro del pasillo). */
	FTransform SignFrame(int32 Index)
	{
		const FVector Feet = SignFeet(Index);
		const double C = CorridorCenter(Feet.X);
		const double Side = Feet.Y >= C ? 1.0 : -1.0;
		const FVector Face = FVector(-1.0, -Side * 0.45, 0.0).GetSafeNormal();
		return FTransform(FRotator(0.0, Face.Rotation().Yaw, 0.0), Feet);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Montaje
// ─────────────────────────────────────────────────────────────────────────────

void ATN_TutorialCourse::EnsureBuilt()
{
	UWorld* World = GetWorld();
	if (bBuilt || !World)
	{
		return;
	}
	bBuilt = true;
	const double T0 = FPlatformTime::Seconds();
	BuildIsland(0);
	BuildIsland(1);
	BuildDecor();
	BuildWater();
	BuildFlora();
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		BuildCascade();
		BuildClouds();
		BuildSigns();
	}
	SpawnLocalActors();
	UE_LOG(LogTortunabo, Log, TEXT("[Tutorial] Recorrido montado en %.0f ms."), (FPlatformTime::Seconds() - T0) * 1000.0);
	VisibilityTimer = 0.f;
	bLocalVisible = true;
	UpdateLocalVisibility();
}

UProceduralMeshComponent* ATN_TutorialCourse::NewMeshComponent(const TCHAR* DebugName, bool bCollision)
{
	UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), FName(DebugName)),
		RF_Transient);
	Comp->SetupAttachment(SceneRoot);
	// Colisión en el acto: el servidor pone a la tortuga encima en el mismo fotograma.
	Comp->bUseAsyncCooking = false;
	Comp->SetCanEverAffectNavigation(false);
	if (bCollision)
	{
		Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Comp->RegisterComponent();
	return Comp;
}

void ATN_TutorialCourse::BuildIsland(int32 Part)
{
	using namespace TNTutorialBuildDetail;
	const UTN_ProcMapSettings* Settings = ProcSettings();
	const FPalette Pal(Settings);
	const double X0 = Part == 0 ? Dims::PartA0 : Dims::PartB0;
	const double X1 = Part == 0 ? Dims::PartA1 : Dims::PartB1;
	const int32 Rows = FMath::RoundToInt((X1 - X0) / Dims::RowStep) + 1;

	// ── Arriba: pasillo, taludes y meseta, de borde a borde (con colisión) ──
	FTNProcMeshBuffers Top;
	FTNProcMeshBuffers Under;
	// La isla entera (arriba y la roca de debajo) es una pieza de arte, con el origen del recorrido de pivote: la colisión
	// de arriba no cambia.
	TNArt::FPieceLog Log(Part == 0 ? TEXT("TutorialIslandA") : TEXT("TutorialIslandB"));
	const int32 IslandPiece = Log.Begin(Part == 0 ? TN_ART("Lobby.Tutorial.IslandA") : TN_ART("Lobby.Tutorial.IslandB"), FTransform::Identity, { &Top, &Under });
	Top.Verts.Reserve(Rows * NumTop);
	TArray<double> Offsets;
	Offsets.SetNumUninitialized(Rows * NumTop);
	for (int32 r = 0; r < Rows; ++r)
	{
		const double X = FMath::Min(X1, X0 + r * Dims::RowStep);
		const double C = CorridorCenter(X);
		double D[NumTop];
		TopOffsets(X, D);
		for (int32 j = 0; j < NumTop; ++j)
		{
			const double Y = C + D[j];
			Offsets[r * NumTop + j] = D[j];
			PushVertex(Top, FVector(X, Y, SurfaceZ(X, Y)), FLinearColor::White, FVector2D(X, Y) / 500.0);
		}
	}
	EmitGrid(Top, 0, Rows, NumTop, false);
	AccumulateNormals(Top);
	for (int32 v = 0; v < Top.Verts.Num(); ++v)
	{
		Top.Colors[v] = TopColor(Pal, Top.Verts[v], Top.Normals[v], Offsets[v]);
	}

	// ── Debajo: la roca en cono a cada lado, del borde a la quilla, y las tapas de los extremos (sin colisión) ──
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const int32 Base = Under.Verts.Num();
		for (int32 r = 0; r < Rows; ++r)
		{
			const double X = FMath::Min(X1, X0 + r * Dims::RowStep);
			const FVector Rim = Top.Verts[r * NumTop + (Side == 0 ? 0 : NumTop - 1)];
			const FVector OtherRim = Top.Verts[r * NumTop + (Side == 0 ? NumTop - 1 : 0)];
			const double KeelZ = FMath::Min(Rim.Z, OtherRim.Z) - KeelDepth(X);
			const double KeelY = 0.3 * CorridorCenter(X);
			for (int32 k = 0; k <= NumUnder; ++k)
			{
				const double U = static_cast<double>(k) / NumUnder;
				const double In = FMath::Pow(U, 1.8);
				double Y = TNProcMap::LerpD(Rim.Y, KeelY, In);
				double Z = TNProcMap::LerpD(Rim.Z, KeelZ, U);
				if (k > 0 && k < NumUnder)
				{
					// Roca irregular: el borde sale y entra, con salientes.
					Y += (Rim.Y - KeelY) * 0.08 * TNProcMap::Noise2(Seed + 51u + static_cast<uint32>(Side), X / 520.0, U * 4.0);
					Z += 70.0 * TNProcMap::Noise2(Seed + 53u + static_cast<uint32>(Side), X / 380.0, U * 5.0);
				}
				const FVector P(X, Y, Z);
				PushVertex(Under, P, UnderColor(Pal, P, U), FVector2D(X, Z) / 500.0);
			}
		}
		// Cara hacia fuera: a un lado se recorre al revés.
		EmitGrid(Under, Base, Rows, NumUnder + 1, Side == 0);
	}

	// Tapas de los extremos (la punta de la salida, los dos lados del cañón y el borde de la cascada): abanico de roca.
	for (int32 End = 0; End < 2; ++End)
	{
		const int32 r = End == 0 ? 0 : Rows - 1;
		const double X = FMath::Min(X1, X0 + r * Dims::RowStep);
		TArray<FVector> Ring;
		for (int32 j = 0; j < NumTop; ++j) { Ring.Add(Top.Verts[r * NumTop + j]); }
		const int32 RightBase = (Rows + r) * (NumUnder + 1);
		for (int32 k = 1; k <= NumUnder; ++k) { Ring.Add(Under.Verts[RightBase + k]); }
		const int32 LeftBase = r * (NumUnder + 1);
		for (int32 k = NumUnder - 1; k >= 1; --k) { Ring.Add(Under.Verts[LeftBase + k]); }
		FVector Center = FVector::ZeroVector;
		for (const FVector& P : Ring) { Center += P; }
		Center /= FMath::Max(1, Ring.Num());
		const FVector Hint(End == 0 ? -1.0 : 1.0, 0.0, 0.0);
		const int32 CenterIdx = Under.Verts.Num();
		PushVertex(Under, Center, UnderColor(Pal, Center, 0.6), FVector2D(Center.Y, Center.Z) / 500.0);
		const int32 RingBase = Under.Verts.Num();
		for (const FVector& P : Ring)
		{
			const double Depth = FMath::Clamp((Ring[0].Z - P.Z) / FMath::Max(100.0, KeelDepth(X)), 0.0, 1.0);
			PushVertex(Under, P, UnderColor(Pal, P, 0.15 + 0.85 * Depth), FVector2D(P.Y, P.Z) / 500.0);
		}
		for (int32 k = 0; k < Ring.Num(); ++k)
		{
			TNPlaygroundKit::EmitOrientedTri(Under, CenterIdx, RingBase + k, RingBase + (k + 1) % Ring.Num(), Hint);
		}
	}
	AccumulateNormals(Under);
	Log.End(IslandPiece);

	UMaterialInterface* TerrainMat = Settings && Settings->TerrainMaterial ? Settings->TerrainMaterial.Get()
		: LoadMat(TEXT("/Game/ProcMap/Materials/M_ProcTerrain.M_ProcTerrain"), TNMaterials::VertexColor());
	UProceduralMeshComponent* Mesh = NewMeshComponent(Part == 0 ? TEXT("IslandA") : TEXT("IslandB"), true);
	// Dato de primitiva 0 = 1: M_ProcTerrain pone el relieve por normales y la textura del camino (como las teselas del mapa).
	Mesh->SetCustomPrimitiveDataFloat(0, 1.f);
	TNArt::UploadSection(Mesh, 0, Top, true, nullptr, &Log);
	TNArt::UploadSection(Mesh, 1, Under, false, nullptr, &Log);
	if (TerrainMat)
	{
		Mesh->SetMaterial(0, TerrainMat);
		Mesh->SetMaterial(1, TerrainMat);
	}
	TNArt::SpawnPieceArt(Mesh, Log);
	IslandMeshes.Add(Mesh);
}

void ATN_TutorialCourse::BuildDecor()
{
	using namespace TNTutorialBuildDetail;
	using TNCastleKit::Col;
	FTNProcMeshBuffers Solid;
	FTNProcMeshBuffers Deco;
	// Piezas que Arte puede sustituir (Docs/Arte_Assets.md), en locales del recorrido.
	TNArt::FPieceLog Log(TEXT("TutorialDecor"));

	// Tronco de equilibrio sobre el cañón, con sus estacas a cada lado.
	{
		const FVector A = LogStart();
		const FVector B = LogEnd();
		// Pivote: el eje del tronco en su punta de la terraza, +X a lo largo del tronco (baja hacia la isla B).
		TNArt::FPieceScope Piece(Log, TN_ART("Lobby.Tutorial.BalanceLog"), FTransform(FRotationMatrix::MakeFromX(B - A).Rotator(), A), { &Solid, &Deco });
		TNProcMesh::TNProcAddLog(Solid, A, B, Dims::LogRadius, Seed + 61u, Col(0x7A5232), Col(0xD9B98A));
		for (const FVector& End : { A, B })
		{
			for (const double S : { -1.0, 1.0 })
			{
				const FVector Foot = End + FVector(0.0, S * (Dims::LogRadius + 16.0), -60.0);
				TNProcMesh::TNProcAddCylinder(Deco, Foot, Foot + FVector(0.0, 0.0, 110.0), 9.0, 7.0, 6, Col(0x6B4426));
			}
		}
	}

	// Montículo de arena de la estación de rebuscar, con una pala y un cubo de juguete clavados.
	{
		const FVector Feet = MoundFeet();
		const double R = Dims::MoundRadius;
		TNArt::FPieceScope Piece(Log, TN_ART("Lobby.Tutorial.SearchMound"), TNArt::PiecePivot(Feet), { &Solid, &Deco });
		TNPlaygroundKit::AddEllipsoid(Solid, Feet + FVector(0.0, 0.0, -20.0), FVector::ForwardVector, FVector::RightVector, FVector::UpVector,
			FVector(R, R * 0.92, 95.0), 14, 7, Col(0xE4C489));
		TNPlaygroundKit::AddEllipsoid(Deco, Feet + FVector(-R * 0.35, R * 0.25, 45.0), FVector::ForwardVector, FVector::RightVector, FVector::UpVector,
			FVector(R * 0.45, R * 0.4, 38.0), 10, 5, Col(0xD9B474));
		const FVector Handle = Feet + FVector(R * 0.2, -R * 0.35, 60.0);
		TNPlaygroundKit::AddRod(Deco, Handle, Handle + FVector(-25.0, -18.0, 95.0), 4.0, 6, Col(0xFF6A52), FVector::ForwardVector);
		TNPlaygroundKit::AddAxisBox(Deco, Handle + FVector(4.0, 3.0, -10.0), FVector(16.0, 3.0, 20.0), Col(0xFF6A52));
		TNPlaygroundKit::AddFrustum(Deco, Feet + FVector(R * 0.95, R * 0.6, -4.0), Feet + FVector(R * 0.95, R * 0.6, 38.0), 22.0, 28.0, 12, Col(0x2EC4B6),
			Col(0x1E9C92), true, false);
	}

	// Peana de arena del cangrejo de prácticas (va y viene por encima).
	{
		const FVector Feet = DummyFeet();
		TNArt::FPieceScope Piece(Log, TN_ART("Lobby.Tutorial.DummyDais"), TNArt::PiecePivot(Feet), { &Solid, &Deco });
		TNPlaygroundKit::AddFrustum(Solid, Feet + FVector(0.0, 0.0, -30.0), Feet + FVector(0.0, 0.0, 6.0), 250.0, 225.0, 22, Col(0xE8CC92), Col(0xF0D49A), false, true);
		// Diana pintada en la peana.
		for (int32 k = 0; k < 3; ++k)
		{
			TNPlaygroundKit::AddAnnulus(Deco, Feet + FVector(0.0, 0.0, 7.0 + k * 0.5), FVector::UpVector, 60.0 + k * 55.0, 80.0 + k * 55.0, 28,
				k % 2 == 0 ? Col(0xFF6A52) : Col(0xFFF2D4));
		}
	}

	// Chevrones amarillos y rojos antes de la zanja del panzazo (como los saltos de panzazo del mapa).
	{
		const double X = Dims::TrenchX0 - 60.0;
		const double C = CorridorCenter(X);
		const double Z = FloorZ(X, 0.0) + 2.0;
		TNArt::FPieceScope Piece(Log, TN_ART("Lobby.Tutorial.Chevrons"), TNArt::PiecePivot(FVector(X, C, Z - 2.0)), { &Deco });
		for (int32 k = -2; k <= 2; ++k)
		{
			const double Y = C + k * 110.0;
			const FLinearColor Chevron = (k % 2 == 0) ? Col(0xFFCB3D) : Col(0xE63946);
			Deco.AddTri(FVector(X + 40.0, Y, Z), FVector(X - 30.0, Y - 45.0, Z), FVector(X - 30.0, Y + 45.0, Z), FVector::UpVector, Chevron);
		}
	}

	// Carteles de madera de cada estación (el rótulo lo pone BuildSigns): dos postes y una tabla con marco.
	for (int32 i = 0; i < NumStations; ++i)
	{
		const FTransform Frame = SignFrame(i);
		TNArt::FPieceScope Piece(Log, TN_ART("Lobby.Tutorial.Sign"), Frame, { &Deco });
		auto P = [&Frame](double X, double Y, double Z) { return Frame.TransformPosition(FVector(X, Y, Z)); };
		const FVector Fwd = Frame.GetUnitAxis(EAxis::X);
		const FVector Right = Frame.GetUnitAxis(EAxis::Y);
		for (const double S : { -1.0, 1.0 })
		{
			TNProcMesh::TNProcAddCylinder(Deco, P(-4.0, S * 110.0, -20.0), P(-4.0, S * 110.0, 205.0), 8.0, 7.0, 6, Col(0x6B4426));
		}
		const FTransform BoardXf(Frame.GetRotation(), P(0.0, 0.0, 170.0));
		TNPlaygroundKit::AddXfBox(Deco, BoardXf, FVector::ZeroVector, FVector(5.0, 140.0, 52.0), Col(0xB7875A));
		TNPlaygroundKit::AddXfBox(Deco, BoardXf, FVector(1.0, 0.0, 49.0), FVector(6.0, 144.0, 5.0), Col(0x7A5232));
		TNPlaygroundKit::AddXfBox(Deco, BoardXf, FVector(1.0, 0.0, -49.0), FVector(6.0, 144.0, 5.0), Col(0x7A5232));
		// Número de la estación en una chapa redonda encima.
		TNPlaygroundKit::AddDisc(Deco, P(6.5, 0.0, 250.0), Fwd, 30.0, 18, Col(0x12305A));
		TNPlaygroundKit::AddDisc(Deco, P(-6.5, 0.0, 250.0), -Fwd, 30.0, 18, Col(0x12305A));
		TNPlaygroundKit::AddRod(Deco, P(0.0, 0.0, 222.0), P(0.0, 0.0, 205.0), 3.0, 5, Col(0x6B4426), Right);
	}

	// Piedras a los lados del borde de la cascada.
	for (const double S : { -1.0, 1.0 })
	{
		const double X = Dims::LipX - 60.0;
		const double Y = CorridorCenter(X) + S * (Dims::StreamLipHalfW + 40.0);
		const FVector Base(X, Y, FloorZ(X, S * (Dims::StreamLipHalfW + 40.0)) - 10.0);
		TNArt::FPieceScope Piece(Log, TN_ART("Lobby.Tutorial.LipBoulder"), TNArt::PiecePivot(Base), { &Solid });
		TNProcMesh::TNProcAddBoulder(Solid, Base, 70.0, 60.0, Seed + 71u + (S > 0.0 ? 1u : 0u), Col(0x8A8378));
	}

	UMaterialInterface* Mat = TNCastleKit::VertexColorMaterial();
	DecorMesh = NewMeshComponent(TEXT("Decor"), true);
	if (!Solid.IsEmpty())
	{
		TNArt::UploadSection(DecorMesh, 0, Solid, true, nullptr, &Log);
	}
	if (!Deco.IsEmpty())
	{
		TNArt::UploadSection(DecorMesh, 1, Deco, false, nullptr, &Log);
	}
	if (Mat)
	{
		DecorMesh->SetMaterial(0, Mat);
		DecorMesh->SetMaterial(1, Mat);
	}
	// La malla de arte de cada pieza con sustituto, en su sitio (hija de los adornos: mismos ejes que los buffers).
	TNArt::SpawnPieceArt(DecorMesh, Log);
}

void ATN_TutorialCourse::BuildWater()
{
	using namespace TNTutorialBuildDetail;
	FTNProcMeshBuffers Water;
	const FLinearColor White(1.f, 1.f, 1.f, 1.f);

	// Laguna: el agua de talud a talud (el terreno la tapa donde queda por encima).
	{
		const double W = CorridorHalfWidth(0.5 * (Dims::PoolX0 + Dims::PoolX1)) + 70.0;
		const int32 Steps = 10;
		const int32 Base = Water.Verts.Num();
		for (int32 r = 0; r <= Steps; ++r)
		{
			const double X = TNProcMap::LerpD(Dims::PoolX0 - 10.0, Dims::PoolX1 + 10.0, static_cast<double>(r) / Steps);
			for (int32 c = 0; c <= 4; ++c)
			{
				const double Y = TNProcMap::LerpD(-W, W, c / 4.0);
				PushVertex(Water, FVector(X, Y, Dims::PoolWaterZ), White, FVector2D(X, Y) / 500.0);
			}
		}
		EmitGrid(Water, Base, Steps + 1, 5, false);
	}

	// Arroyo: del pueblo al borde de la cascada, a unos centímetros por debajo del suelo de al lado.
	{
		const int32 Base = Water.Verts.Num();
		int32 Rows = 0;
		for (double X = Dims::StreamX0 + 40.0; X <= Dims::LipX + 1.0; X += Dims::RowStep)
		{
			const double C = CorridorCenter(X);
			const double Hw = StreamHalfWidth(X) - 12.0;
			const double Z = FloorZ(X, FMath::Max(0.0, Hw + 40.0)) - 6.0;
			for (int32 c = 0; c <= 2; ++c)
			{
				const double Y = C + TNProcMap::LerpD(-Hw, Hw, c / 2.0);
				PushVertex(Water, FVector(X, Y, Z), White, FVector2D(X, Y) / 400.0);
			}
			++Rows;
		}
		EmitGrid(Water, Base, Rows, 3, false);
	}

	const UTN_ProcMapSettings* Settings = ProcSettings();
	UMaterialInterface* Mat = LoadMat(TEXT("/Game/ProcMap/Materials/MI_ProcSeaAnim.MI_ProcSeaAnim"),
		Settings && Settings->WaterMaterial ? Settings->WaterMaterial.Get() : TNMaterials::VertexColor());
	WaterMesh = NewMeshComponent(TEXT("Water"), false);
	WaterMesh->SetCastShadow(false);
	const TArray<FProcMeshTangent> NoTangents;
	WaterMesh->CreateMeshSection_LinearColor(0, Water.Verts, Water.Tris, Water.Normals, Water.UVs, Water.Colors, NoTangents, false);
	if (Mat)
	{
		WaterMesh->SetMaterial(0, Mat);
	}
}

void ATN_TutorialCourse::BuildCascade()
{
	using namespace TNTutorialBuildDetail;
	// La lámina del arroyo sale del borde, se curva y cae recta hacia el castillo. La opacidad (alfa del vértice en
	// M_ProcCascade) se va a cero entre el 32 % y el 55 % de la caída: a mitad de camino ya no hay cascada.
	const double LipZ = FloorZ(Dims::LipX, 0.0) + Dims::StreamDepth - 6.0;
	TArray<FVector> Line;
	for (int32 i = 0; i <= 12; ++i)
	{
		const double T = 0.06 * i;
		Line.Add(FVector(Dims::LipX + 5.0 + 230.0 * T, CorridorCenter(Dims::LipX), LipZ - 490.0 * T * T));
	}
	const double FallStop = 0.6 * Dims::Height;
	for (double Drop = LipZ - Line.Last().Z + 300.0; Drop < FallStop; Drop += 320.0)
	{
		Line.Add(FVector(Line.Last().X + 4.0, Line.Last().Y, LipZ - Drop));
	}
	FTNProcMeshBuffers Front;
	static const double Across[5] = { -0.5, -0.25, 0.0, 0.25, 0.5 };
	double Along = 0.0;
	for (int32 i = 0; i < Line.Num(); ++i)
	{
		if (i > 0) { Along += FVector::Dist(Line[i], Line[i - 1]) / 100.0; }
		const double Drop = LipZ - Line[i].Z;
		const double Frac = Drop / Dims::Height;
		const double Width = 2.0 * Dims::StreamLipHalfW * (1.0 + 0.9 * Smooth(0.0, 0.4, Frac));
		const float Fade = static_cast<float>(1.0 - Smooth(0.32, 0.55, Frac));
		const float Foam = static_cast<float>(Smooth(0.0, 0.05, Frac) * (1.0 - Smooth(0.1, 0.3, Frac)));
		for (int32 c = 0; c < 5; ++c)
		{
			FLinearColor Colr = TNProcMesh::TNProcLerpColor(FLinearColor(0.72f, 0.88f, 1.f), FLinearColor(0.97f, 0.99f, 1.f), 0.35f + 0.65f * Foam);
			Colr.A = (c == 0 || c == 4 ? 0.4f : 0.9f) * Fade;
			PushVertex(Front, Line[i] + FVector(0.0, Across[c] * Width, 0.0), Colr, FVector2D(Across[c] * Width / 100.0, Along));
		}
	}
	const int32 Rows = Line.Num();
	// Por las dos caras: se ve desde fuera y desde dentro (quien cae va dentro de la cascada).
	EmitGrid(Front, 0, Rows, 5, false);
	EmitGrid(Front, 0, Rows, 5, true);
	for (int32 v = 0; v < Front.Verts.Num(); ++v)
	{
		Front.Normals[v] = FVector(1.0, 0.0, 0.25).GetSafeNormal();
	}
	UMaterialInterface* Mat = LoadMat(TEXT("/Game/ProcMap/Materials/M_ProcCascade.M_ProcCascade"), TNMaterials::VertexColor());
	CascadeMesh = NewMeshComponent(TEXT("Cascade"), false);
	CascadeMesh->SetCastShadow(false);
	const TArray<FProcMeshTangent> NoTangents;
	CascadeMesh->CreateMeshSection_LinearColor(0, Front.Verts, Front.Tris, Front.Normals, Front.UVs, Front.Colors, NoTangents, false);
	if (Mat)
	{
		CascadeMesh->SetMaterial(0, Mat);
	}
}

void ATN_TutorialCourse::BuildClouds()
{
	using namespace TNTutorialBuildDetail;
	FTNProcMeshBuffers Clouds;
	TNProcMap::FRng Rng(static_cast<uint64>(Seed) * 2654435761ull + 17ull);
	// Cada nube (y cada jirón de bruma de la cascada) es una pieza de arte: su centro, escala 1 = Size RefSize.
	TNArt::FPieceLog Log(TEXT("TutorialClouds"));
	FName CloudSlot = TN_ART("Lobby.Tutorial.Cloud");
	double RefSize = 800.0;
	auto AddCluster = [&Clouds, &Rng, &Log, &CloudSlot, &RefSize](const FVector& Center, double Size, float Alpha)
	{
		TNArt::FPieceScope Piece(Log, CloudSlot, TNArt::PiecePivot(Center, 0.0, FVector(Size / RefSize)), { &Clouds });
		const int32 Puffs = Rng.RangeInt(4, 7);
		for (int32 p = 0; p < Puffs; ++p)
		{
			const double R = Size * Rng.Range(0.45, 1.0);
			const FVector Offset(Rng.Range(-1.3, 1.3) * Size, Rng.Range(-0.9, 0.9) * Size, Rng.Range(-0.2, 0.35) * Size);
			FLinearColor White(0.98f, 0.99f, 1.f, Alpha * static_cast<float>(Rng.Range(0.75, 1.0)));
			TNPlaygroundKit::AddEllipsoid(Clouds, Center + Offset, FVector::ForwardVector, FVector::RightVector, FVector::UpVector, FVector(R, R * 0.9, R * 0.55),
				12, 6, White);
		}
	};
	// Debajo y alrededor de las dos islas.
	for (double X = Dims::PartA0 + 600.0; X < Dims::PartB1; X += 1900.0)
	{
		if (PartOf(X) == INDEX_NONE) { continue; }
		const double Keel = SurfaceZ(X, 0.0) - KeelDepth(X);
		AddCluster(FVector(X + Rng.Range(-500.0, 500.0), Rng.Range(-900.0, 900.0), Keel - Rng.Range(500.0, 1400.0)), Rng.Range(600.0, 950.0), 0.85f);
		for (const double S : { -1.0, 1.0 })
		{
			const double Rim = S > 0.0 ? RimRight(X) : -RimLeft(X);
			AddCluster(FVector(X + Rng.Range(-600.0, 600.0), Rim + S * Rng.Range(900.0, 2600.0), SurfaceZ(X, Rim) - Rng.Range(300.0, 1800.0)),
				Rng.Range(400.0, 800.0), 0.8f);
		}
	}
	// Bruma donde se deshace la cascada (entre el 30 % y el 55 % de la caída), cada vez más tenue.
	CloudSlot = TN_ART("Lobby.Tutorial.CascadeMist");
	RefSize = 400.0;
	for (int32 k = 0; k < 7; ++k)
	{
		const double Frac = 0.3 + 0.04 * k;
		const double A = 0.9 * k;
		const FVector C(Dims::LipX + 120.0 + 260.0 * FMath::Cos(A), CorridorCenter(Dims::LipX) + 300.0 * FMath::Sin(A), -Frac * Dims::Height);
		AddCluster(C, 280.0 + 40.0 * k, 0.7f - 0.07f * k);
	}
	UMaterialInterface* Mat = LoadMat(TEXT("/Game/ProcMap/Materials/M_ProcFXCloud.M_ProcFXCloud"), TNMaterials::VertexColor());
	CloudMesh = NewMeshComponent(TEXT("Clouds"), false);
	CloudMesh->SetCastShadow(false);
	TNArt::UploadSection(CloudMesh, 0, Clouds, false, Mat, &Log);
	TNArt::SpawnPieceArt(CloudMesh, Log);
}

void ATN_TutorialCourse::BuildFlora()
{
	using namespace TNTutorialBuildDetail;
	using namespace TNProcMap;
	const double T0 = FPlatformTime::Seconds();
	const UTN_ProcMapSettings* Settings = ProcSettings();

	TArray<FFloraSpecies> Tables[NumBiomes];
	for (int32 b = 0; b < NumBiomes; ++b) { FloraSpeciesFor(BiomeFromIndex(b), Tables[b]); }

	FLayout Biomes;
	Biomes.Params.Seed = Seed;
	Biomes.BiomeCell = 400.0;
	Biomes.BiomeW = FMath::CeilToInt((FloraXMax - FloraXMin) / Biomes.BiomeCell) + 1;
	Biomes.BiomeH = FMath::CeilToInt((FloraDMax - FloraDMin) / Biomes.BiomeCell) + 1;
	Biomes.BiomeWeights.SetNumZeroed(Biomes.BiomeW * Biomes.BiomeH * NumBiomes);
	for (int32 cx = 0; cx < Biomes.BiomeW; ++cx)
	{
		double W[NumBiomes];
		BiomeWeights(FloraXMin + (cx + 0.5) * Biomes.BiomeCell, W);
		for (int32 cy = 0; cy < Biomes.BiomeH; ++cy)
		{
			for (int32 b = 0; b < NumBiomes; ++b)
			{
				Biomes.BiomeWeights[(cy * Biomes.BiomeW + cx) * NumBiomes + b] = static_cast<float>(W[b]);
			}
		}
	}

	const FFloraQuery Query;
	TArray<FFloraInstance> Placed;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const FFloraGrid Grid = FloraGridFor(FVector2D::ZeroVector, FVector2D(FloraXMax - FloraXMin, FloraDMax - FloraDMin), Pass);
		PlaceFloraRows(Biomes, Tables, Query, Pass, Grid, 0, Grid.NY, Placed, 1.0);
	}

	// Una malla por bioma, especie y variante (como ATN_ProcMapGenerator::BuildFlora), en el espacio local del recorrido.
	auto LookOf = [](const FFloraSpecies& Sp)
	{
		TNFloraMesh::FTNFloraLook Look = TNFloraMesh::TNFloraLookOf(Sp.Shape);
		if (Sp.Shape == EFloraShape::Prop)
		{
			const TNPropMesh::FTNPropLook Prop = TNPropMesh::TNPropLookOf(Sp.Prop);
			Look.Cull = Prop.Cull;
			Look.bShadow = Prop.bShadow;
			Look.bStretch = false;
		}
		return Look;
	};
	TMap<int32, TArray<FTransform>> ByMesh;
	for (const FFloraInstance& I : Placed)
	{
		const FFloraSpecies& Sp = Tables[I.Biome][I.Species];
		const double Xs = I.Location.X + FloraXMin;
		const double Ys = I.Location.Y + FloraDMin + CorridorCenter(Xs);
		const FVector Local(Xs, Ys, I.Location.Z + Dims::PoolWaterZ);
		const double Stretch = LookOf(Sp).bStretch
			? 0.88 + 0.24 * (0.5 + 0.5 * TNProcMesh::TNProcHashNoise(FMath::RoundToInt32(Xs), FMath::RoundToInt32(Ys), 0x5EEDu)) : 1.0;
		const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(I.Yaw));
		const FQuat Lean(FVector(-I.LeanDir.Y, I.LeanDir.X, 0.0), FMath::DegreesToRadians(I.LeanDeg));
		const int32 Key = (I.Biome * 64 + I.Species) * FloraVariants + I.Variant;
		ByMesh.FindOrAdd(Key).Add(FTransform(Lean * Yaw, Local, FVector(I.Scale, I.Scale, I.Scale * Stretch)));
	}

	UMaterialInterface* Material = Settings && Settings->FoliageMaterial ? Settings->FoliageMaterial.Get()
		: LoadMat(TEXT("/Game/ProcMap/Materials/M_ProcFoliage.M_ProcFoliage"), TNMaterials::VertexColor());
	int32 Total = 0;
	for (TPair<int32, TArray<FTransform>>& Entry : ByMesh)
	{
		const int32 Variant = Entry.Key % FloraVariants;
		const int32 Species = (Entry.Key / FloraVariants) % 64;
		const int32 BiomeIdx = Entry.Key / FloraVariants / 64;
		const FFloraSpecies& Sp = Tables[BiomeIdx][Species];
		const ETNProcBiome Biome = BiomeFromIndex(BiomeIdx);
		FLinearColor Ground, PathC, RockC, Bed;
		BiomeColors(Settings, Biome, Ground, PathC, RockC, Bed);
		FTNProcMeshBuffers Buffers;
		const uint32 MeshSeed = HashCell(Seed ^ 0xF10Au, (BiomeIdx * 64 + static_cast<int32>(Sp.Shape)) * 64 + static_cast<int32>(Sp.Prop), Variant);
		const bool bProp = Sp.Shape == EFloraShape::Prop;
		const bool bSolid = bProp && TNPropMesh::TNPropSolid(Sp.Prop);
		if (bProp)
		{
			TNPropMesh::TNPropBuild(Buffers, Sp.Prop, Variant, MeshSeed, TNPropMesh::TNPropCrystalColor(Biome), Biome == ETNProcBiome::Volcanic);
			// Como en el generador: la paleta de los objetos se decodifica una vez más para verse igual que en el mapa.
			for (FLinearColor& C : Buffers.Colors)
			{
				C = FLinearColor(TNProcRuntimeMesh::SRGBToLinear(C.R), TNProcRuntimeMesh::SRGBToLinear(C.G), TNProcRuntimeMesh::SRGBToLinear(C.B), C.A);
			}
		}
		else
		{
			TNFloraMesh::TNFloraBuild(Buffers, Sp.Shape, TNFloraMesh::TNFloraPaletteFor(Biome, Ground, RockC), Variant, MeshSeed);
		}
		const TNFloraMesh::FTNFloraWind Wind = TNFloraMesh::TNFloraWindOf(Sp.Shape);
		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(this, Buffers, Material, bSolid, Wind.Stiffness, Wind.Exponent);
		if (!Mesh)
		{
			continue;
		}
		RuntimeMeshes.Add(Mesh);
		const TNFloraMesh::FTNFloraLook Look = LookOf(Sp);
		UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		HISM->SetupAttachment(SceneRoot);
		HISM->SetStaticMesh(Mesh);
		HISM->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bSolid)
		{
			HISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		HISM->SetCanEverAffectNavigation(false);
		HISM->SetCastShadow(Look.bShadow);
		if (Wind.Stiffness > 0.f)
		{
			HISM->WorldPositionOffsetDisableDistance = Wind.DisableDistance;
		}
		else
		{
			HISM->bEvaluateWorldPositionOffset = false;
		}
		if (Look.Cull > 0.f)
		{
			HISM->SetCullDistances(FMath::RoundToInt(Look.Cull * 0.8f), FMath::RoundToInt(Look.Cull));
		}
		HISM->RegisterComponent();
		HISM->AddInstances(Entry.Value, false, false);
		TNArt::ApplyToInstances(HISM, TNTutorialBuildDetail::FloraSlot(Sp.Shape, Sp.Prop));
		FloraComponents.Add(HISM);
		Total += Entry.Value.Num();
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Tutorial] Vegetación: %d plantas, rocas y objetos en %d mallas (%.0f ms)."), Total, FloraComponents.Num(),
		(FPlatformTime::Seconds() - T0) * 1000.0);
}

void ATN_TutorialCourse::BuildSigns()
{
	using namespace TNTutorialBuildDetail;
	for (int32 i = 0; i < NumStations; ++i)
	{
		const FTransform Frame = SignFrame(i);
		auto MakeText = [this, &Frame](const FText& Text, const FVector& Offset, float Size, const FColor& Color)
		{
			UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
			Label->SetupAttachment(SceneRoot);
			Label->SetHorizontalAlignment(EHTA_Center);
			Label->SetVerticalAlignment(EVRTA_TextCenter);
			Label->SetWorldSize(Size);
			Label->SetTextRenderColor(Color);
			Label->SetText(Text);
			Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Label->SetCastShadow(false);
			Label->RegisterComponent();
			Label->SetRelativeLocationAndRotation(Frame.TransformPosition(Offset), Frame.GetRotation());
			SignTexts.Add(Label);
		};
		MakeText(TNTutorialTexts::Title(Station(i).Id), FVector(6.5, 0.0, 170.0), 21.f, FColor(255, 251, 240));
		MakeText(FText::AsNumber(i + 1), FVector(8.0, 0.0, 250.0), 34.f, FColor(255, 203, 61));
	}
}

void ATN_TutorialCourse::SpawnLocalActors()
{
	using namespace TNTutorialBuildDetail;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	// Agua nadable de la laguna (local: el nado lo predicen servidor y cliente). La caja va con el giro del recorrido.
	if (!PoolVolume)
	{
		PoolVolume = World->SpawnActor<ATN_ProcWaterVolume>(ATN_ProcWaterVolume::StaticClass(), GetActorTransform(), Params);
		if (PoolVolume)
		{
			const double Bottom = Dims::PoolFloorZ - 150.0;
			const double W = CorridorHalfWidth(0.5 * (Dims::PoolX0 + Dims::PoolX1)) + 80.0;
			const FVector Center(0.5 * (Dims::PoolX0 + Dims::PoolX1), 0.0, 0.5 * (Dims::PoolWaterZ + Bottom));
			const FVector Extent(0.5 * (Dims::PoolX1 - Dims::PoolX0) + 20.0, W, 0.5 * (Dims::PoolWaterZ - Bottom));
			PoolVolume->AddWaterBox(LocalToWorld(Center), Extent);
		}
	}
	if (!Fauna && World->GetNetMode() != NM_DedicatedServer)
	{
		Fauna = World->SpawnActor<ATN_TutorialFauna>(ATN_TutorialFauna::StaticClass(), GetActorTransform(), Params);
		if (Fauna)
		{
			Fauna->Init(this);
		}
	}
}
