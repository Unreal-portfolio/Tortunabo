#include "Player/TN_TurtleSurface.h"
#include "Player/TortugaCharacter.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapTerrain.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

namespace TNTurtleSurface
{
	/**
	 * Superficie de cada bioma del mapa procedural (en el orden de ETNProcBiome): arena, tierra, roca, madera y agua.
	 * Selva: tierra y hojarasca; playa y desierto: arena; volcán: roca con ceniza y grava; agua con isletas: arena
	 * húmeda; acantilados: roca; manglar: fango; zona humana: tierra pisada y empedrado.
	 */
	constexpr float BiomeSurface[8][Num] = {
		{ 0.15f, 0.85f, 0.f, 0.f, 0.f },
		{ 1.f, 0.f, 0.f, 0.f, 0.f },
		{ 1.f, 0.f, 0.f, 0.f, 0.f },
		{ 0.45f, 0.f, 0.55f, 0.f, 0.f },
		{ 0.8f, 0.2f, 0.f, 0.f, 0.f },
		{ 0.2f, 0.f, 0.8f, 0.f, 0.f },
		{ 0.f, 0.7f, 0.f, 0.f, 0.3f },
		{ 0.f, 0.5f, 0.5f, 0.f, 0.f } };
	static_assert(static_cast<int32>(ETNProcBiome::Count) == 8, "BiomeSurface tiene una fila por bioma, en el orden de ETNProcBiome");
	static_assert(TNProcMap::NumBiomes == 8, "BiomeSurface tiene una fila por bioma");

	/** Roca de las pendientes (el terreno se pinta de roca en los taludes): algo de arena suelta encima. */
	constexpr float SlopeRock[Num] = { 0.15f, 0.f, 0.85f, 0.f, 0.f };

	inline float SurfaceSmoothStep(float X)
	{
		const float C = FMath::Clamp(X, 0.f, 1.f);
		return C * C * (3.f - 2.f * C);
	}

	template <int32 N>
	bool ContainsAnyWord(const FString& Text, const TCHAR* const (&Words)[N])
	{
		for (const TCHAR* Word : Words)
		{
			if (Text.Contains(Word, ESearchCase::CaseSensitive)) { return true; }
		}
		return false;
	}

	/**
	 * Superficie por nombres (ya en minúsculas: componente, clase y nombre del actor, malla y material). Por orden: agua,
	 * arena (el castillo de arena del lobby es arena aunque diga «castle»), madera, tierra y roca.
	 */
	uint8 PresetFromNames(const FString& Text)
	{
		static const TCHAR* const WaterWords[] = { TEXT("water"), TEXT("agua"), TEXT("puddle"), TEXT("charco") };
		static const TCHAR* const SandWords[] = { TEXT("sand"), TEXT("arena"), TEXT("beach"), TEXT("playa"), TEXT("dune"), TEXT("duna"),
			TEXT("landscape") };
		static const TCHAR* const WoodWords[] = { TEXT("wood"), TEXT("madera"), TEXT("plank"), TEXT("tablon"), TEXT("tablón"),
			TEXT("bridge"), TEXT("puente"), TEXT("log_"), TEXT("tronco"), TEXT("crate"), TEXT("barrel"), TEXT("barril"), TEXT("dock"),
			TEXT("muelle"), TEXT("pier"), TEXT("boat"), TEXT("barco"), TEXT("raft"), TEXT("balsa"), TEXT("deck"), TEXT("pallet"),
			TEXT("palet"), TEXT("fence"), TEXT("valla"), TEXT("table"), TEXT("mesa"), TEXT("shell"), TEXT("tortuga"), TEXT("turtle") };
		static const TCHAR* const SoilWords[] = { TEXT("grass"), TEXT("hierba"), TEXT("cesped"), TEXT("césped"), TEXT("dirt"),
			TEXT("tierra"), TEXT("mud"), TEXT("barro"), TEXT("fango"), TEXT("leaf"), TEXT("leaves"), TEXT("hoja"), TEXT("moss"),
			TEXT("musgo"), TEXT("soil"), TEXT("jungle"), TEXT("selva"), TEXT("garden"), TEXT("jardin"), TEXT("jardín"), TEXT("turf") };
		static const TCHAR* const RockWords[] = { TEXT("rock"), TEXT("roca"), TEXT("stone"), TEXT("piedra"), TEXT("cliff"),
			TEXT("boulder"), TEXT("pebble"), TEXT("brick"), TEXT("ladrillo"), TEXT("concrete"), TEXT("castle"), TEXT("castillo"),
			TEXT("tower"), TEXT("torre"), TEXT("wall"), TEXT("muro"), TEXT("tile"), TEXT("baldosa"), TEXT("marble") };
		if (ContainsAnyWord(Text, WaterWords)) { return static_cast<uint8>(Water); }
		if (ContainsAnyWord(Text, SandWords)) { return static_cast<uint8>(Sand); }
		if (ContainsAnyWord(Text, WoodWords)) { return static_cast<uint8>(Wood); }
		if (ContainsAnyWord(Text, SoilWords)) { return static_cast<uint8>(Soil); }
		if (ContainsAnyWord(Text, RockWords)) { return static_cast<uint8>(Rock); }
		return PresetUnknown;
	}

	void PresetWeights(uint8 Preset, float OutWeights[Num])
	{
		for (int32 s = 0; s < Num; ++s) { OutWeights[s] = 0.f; }
		OutWeights[Preset < Num ? Preset : Rock] = 1.f;
	}

	uint8 ClassifyByNames(const UPrimitiveComponent* Comp, FNameCache* Cache)
	{
		if (!Comp) { return PresetUnknown; }
		const TObjectKey<UPrimitiveComponent> Key(Comp);
		if (Cache)
		{
			if (const uint8* Found = Cache->Find(Key))
			{
				return *Found;
			}
			if (Cache->Num() > 256) { Cache->Reset(); }
		}

		FString Names = Comp->GetName();
		if (const AActor* CompOwner = Comp->GetOwner())
		{
			Names += TEXT(" ");
			Names += CompOwner->GetClass()->GetName();
			Names += TEXT(" ");
			Names += CompOwner->GetName();
		}
		if (const UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Comp))
		{
			if (const UStaticMesh* MeshAsset = MeshComp->GetStaticMesh())
			{
				Names += TEXT(" ");
				Names += MeshAsset->GetName();
			}
		}
		if (const UMaterialInterface* Material = Comp->GetMaterial(0))
		{
			Names += TEXT(" ");
			Names += Material->GetName();
		}
		Names.ToLowerInline();
		const uint8 Preset = PresetFromNames(Names);
		if (Cache) { Cache->Add(Key, Preset); }
		return Preset;
	}

	ATN_ProcMapGenerator* FindGenerator(UWorld* World)
	{
		ATN_ProcMapGenerator* Found = nullptr;
		if (World)
		{
			for (TActorIterator<ATN_ProcMapGenerator> It(World); It; ++It)
			{
				Found = *It;
				break;
			}
		}
		return Found;
	}

	void Resolve(const FHitResult* Hit, const FVector& FootLocation, const ATN_ProcMapGenerator* Generator,
		FNameCache* Cache, float OutWeights[Num], FContact* OutContact)
	{
		float W[Num];
		PresetWeights(PresetUnknown, W);
		FContact Contact;

		const bool bHit = Hit != nullptr && Hit->bBlockingHit;
		const UPrimitiveComponent* HitComp = bHit ? Hit->GetComponent() : nullptr;
		const AActor* HitActor = bHit ? Hit->GetActor() : nullptr;
		const bool bMap = Generator && Generator->IsMapReady() && Generator->GetLayout().bValid;
		if (bMap && (!bHit || HitActor == Generator))
		{
			const FVector Point = bHit ? FVector(Hit->ImpactPoint) : FootLocation;
			const FTransform MapXf = Generator->GetActorTransform();
			const double TerrainZ = static_cast<double>(Generator->GetTerrainHeightAt(Point));
			if (HitComp && HitComp->IsA<UStaticMeshComponent>())
			{
				// Piedras y restos sueltos del decorado del mapa: por sus nombres (roca si no dicen nada).
				PresetWeights(ClassifyByNames(HitComp, Cache), W);
			}
			else if (HitComp && FMath::Abs(Point.Z - TerrainZ) > 35.0)
			{
				// Estructura (puentes, torres, adarves): la sección 1 son los tablones; el resto, piedra o hierro pintado.
				// Sin índice de cara (el suelo del movimiento es un barrido de la cápsula) no se sabe: mitad y mitad.
				if (Hit->FaceIndex == INDEX_NONE)
				{
					for (int32 s = 0; s < Num; ++s) { W[s] = 0.f; }
					W[Wood] = 0.5f;
					W[Rock] = 0.5f;
				}
				else
				{
					int32 Section = 0;
					HitComp->GetMaterialFromCollisionFaceIndex(Hit->FaceIndex, Section);
					PresetWeights(static_cast<uint8>(Section == 1 ? Wood : Rock), W);
				}
			}
			else
			{
				// Terreno: la mezcla de biomas del sitio y, en las pendientes, roca (el terreno se pinta de roca en los taludes).
				const FVector MapPoint = MapXf.InverseTransformPosition(Point);
				double BiomeW[TNProcMap::NumBiomes];
				Generator->GetLayout().BiomeWeightsAt(FVector2D(MapPoint.X, MapPoint.Y), BiomeW);
				float Sum = 0.f;
				int32 BestBiome = 0;
				for (int32 s = 0; s < Num; ++s) { W[s] = 0.f; }
				for (int32 b = 0; b < TNProcMap::NumBiomes; ++b)
				{
					const float BW = static_cast<float>(BiomeW[b]);
					Sum += BW;
					if (BiomeW[b] > BiomeW[BestBiome]) { BestBiome = b; }
					for (int32 s = 0; s < Num; ++s) { W[s] += BW * BiomeSurface[b][s]; }
				}
				if (Sum <= 1e-3f)
				{
					PresetWeights(static_cast<uint8>(Sand), W);
					Sum = 1.f;
				}
				else
				{
					Contact.Biome = BestBiome;
				}
				Contact.bProcTerrain = true;
				if (bHit)
				{
					const float Steep = SurfaceSmoothStep((0.9f - static_cast<float>(Hit->ImpactNormal.Z)) / 0.2f) * 0.8f;
					for (int32 s = 0; s < Num; ++s) { W[s] = FMath::Lerp(W[s], SlopeRock[s] * Sum, Steep); }
				}

				// Agua poco profunda: pisando terreno que queda por debajo del nivel del agua del mapa (mar, lagunas y río
				// comparten nivel). Solo en el terreno: un puente o una estructura no chapotean.
				const double FootMapZ = MapXf.InverseTransformPosition(FootLocation).Z;
				const double GroundMapZ = MapXf.InverseTransformPosition(FVector(Point.X, Point.Y, TerrainZ)).Z;
				const float Depth = static_cast<float>((TNProcMap::SeaLevel - FootMapZ) * MapXf.GetScale3D().Z);
				if (Depth > 2.f && GroundMapZ < TNProcMap::SeaLevel)
				{
					const float Wet = FMath::Clamp(0.35f + Depth / 20.f, 0.f, 1.f);
					for (int32 s = 0; s < Num; ++s)
					{
						const float WaterW = s == Water ? Sum : 0.f;
						W[s] = FMath::Lerp(W[s], WaterW, Wet);
					}
				}
			}
		}
		else if (bHit)
		{
			// Encima de otra tortuga: su caparazón suena hueco y resbala, como un tablón.
			const uint8 Preset = Cast<ATortugaCharacter>(HitActor) ? static_cast<uint8>(Wood) : ClassifyByNames(HitComp, Cache);
			PresetWeights(Preset, W);
		}

		// Normalizados (sin datos, roca).
		float Total = 0.f;
		for (int32 s = 0; s < Num; ++s)
		{
			W[s] = FMath::Max(0.f, W[s]);
			Total += W[s];
		}
		if (Total <= 1e-4f)
		{
			PresetWeights(PresetUnknown, W);
			Total = 1.f;
		}
		for (int32 s = 0; s < Num; ++s)
		{
			OutWeights[s] = W[s] / Total;
		}
		if (OutContact) { *OutContact = Contact; }
	}

	bool Probe(UWorld* World, const AActor* Ignore, const FVector& From, const FVector& FootLocation,
		const ATN_ProcMapGenerator* Generator, FNameCache* Cache, float OutWeights[Num], FContact* OutContact)
	{
		FHitResult Hit;
		bool bHit = false;
		if (World)
		{
			// Compleja y con el índice de la cara: en las mallas del mapa procedural, la sección dice si es madera o piedra.
			FCollisionQueryParams Query(SCENE_QUERY_STAT(TNTurtleSurface), true, Ignore);
			Query.bReturnFaceIndex = true;
			bHit = World->LineTraceSingleByChannel(Hit, From, FootLocation - FVector(0.0, 0.0, 60.0), ECC_Visibility, Query);
		}
		Resolve(bHit ? &Hit : nullptr, FootLocation, Generator, Cache, OutWeights, OutContact);
		return bHit;
	}
}
