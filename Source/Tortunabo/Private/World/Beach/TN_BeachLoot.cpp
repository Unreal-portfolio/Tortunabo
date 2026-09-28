#include "World/Beach/TN_BeachLoot.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/TN_PickupInteractableBase.h"
#include "Core/TN_InventoryTypes.h"
#include "Core/TN_Log.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/SoftObjectPtr.h"

// ─────────────────────────────────────────────────────────────────────────────
// Reglas (qué se rebusca, pesos de la carrera) y consola
// ─────────────────────────────────────────────────────────────────────────────

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNBeachLootDetail
{
	TAutoConsoleVariable<int32> CVarBeachLoot(TEXT("TN.Beach.Loot"), 1,
		TEXT("Botín de la playa del modo carrera (rebuscables, objetos sueltos y conchas de puntos): 1 = sí; 0 = nada desde la ronda siguiente."));

	/** El mundo con autoridad del mismo proceso (el propio si no es un cliente; en PIE, el del servidor del mismo mapa). */
	UWorld* FindAuthorityWorld(UWorld* InWorld)
	{
		if (!InWorld)
		{
			return nullptr;
		}
		if (InWorld->GetNetMode() != NM_Client)
		{
			return InWorld;
		}
		if (!GEngine)
		{
			return nullptr;
		}
		const FString MapName = UWorld::RemovePIEPrefix(InWorld->GetMapName());
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Candidate && Candidate != InWorld && Candidate->IsGameWorld() && Candidate->GetNetMode() != NM_Client
				&& UWorld::RemovePIEPrefix(Candidate->GetMapName()) == MapName)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorld CmdLootReroll(TEXT("TN.Beach.Loot.Reroll"),
		TEXT("Playa del modo carrera: quita el botín de la ronda (rebuscables, objetos sueltos y conchas) y lo reparte otra vez (servidor)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UWorld* Authority = FindAuthorityWorld(World);
			UTN_BeachLootSubsystem* Loot = Authority ? Authority->GetSubsystem<UTN_BeachLootSubsystem>() : nullptr;
			if (!Loot)
			{
				UE_LOG(LogTortunabo, Warning, TEXT("[Playa] TN.Beach.Loot.Reroll: sin mundo con autoridad (escríbelo en el anfitrión)."));
				return;
			}
			Loot->Reroll();
		}));

	/** Huella de un decorado para rebuscarlo: centro en el suelo, eje del lado largo, radio, semilargo y alto (cm). */
	struct FSearchShape
	{
		ATN_BeachElement* Element = nullptr;
		FVector Center = FVector::ZeroVector;
		FVector Axis = FVector::ForwardVector;
		float Radius = 100.f;
		float HalfLength = 0.f;
		float Height = 150.f;
		float Chance = 0.f;
		double Progress = 0.0;
	};

	/**
	 * La huella real de un decorado: la caja de su malla fija más grande (el cuerpo), con el giro, la inclinación y el
	 * tamaño de ese ejemplar; cápsula en planta a lo largo de su lado largo. La sombrilla, por el montoncito de su pie.
	 */
	bool MeasureElement(ATN_BeachElement& Element, FSearchShape& Out)
	{
		const ETNBeachElement Kind = Element.GetSpec().Element;
		const float Size = FMath::Clamp(Element.GetSpec().SizeScale, 0.5f, 1.6f);
		const FVector Origin = Element.GetActorLocation();
		Out.Element = &Element;
		if (Kind == ETNBeachElement::PlantedUmbrella)
		{
			// La lona está a 35 m de alto: se rebusca en el montón de arena del pie (unos 3 m de radio con tamaño 1).
			Out.Center = Origin;
			Out.Axis = Element.GetActorForwardVector().GetSafeNormal2D();
			Out.Radius = 380.f * Size;
			Out.HalfLength = 0.f;
			Out.Height = 300.f;
			return true;
		}

		TInlineComponentArray<UStaticMeshComponent*> Comps(&Element);
		const UStaticMeshComponent* Body = nullptr;
		double BestRadius = 0.0;
		for (const UStaticMeshComponent* Comp : Comps)
		{
			if (!Comp || Comp->IsA<UInstancedStaticMeshComponent>() || !Comp->GetStaticMesh() || !Comp->IsVisible())
			{
				continue;
			}
			if (Comp->Bounds.SphereRadius > BestRadius)
			{
				BestRadius = Comp->Bounds.SphereRadius;
				Body = Comp;
			}
		}
		if (!Body)
		{
			return false;
		}
		const FBox Box = Body->GetStaticMesh()->GetBoundingBox();
		const FTransform BodyXf = Body->GetComponentTransform();
		const FVector BodyScale = BodyXf.GetScale3D().GetAbs();
		const FVector Extent = Box.GetExtent();
		const double HalfX = Extent.X * BodyScale.X;
		const double HalfY = Extent.Y * BodyScale.Y;
		FVector AxisX = BodyXf.GetUnitAxis(EAxis::X).GetSafeNormal2D();
		FVector AxisY = BodyXf.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
		if (AxisX.IsNearlyZero())
		{
			AxisX = FVector::ForwardVector;
		}
		if (AxisY.IsNearlyZero())
		{
			AxisY = FVector::RightVector;
		}
		const FVector BoxCenter = BodyXf.TransformPosition(Box.GetCenter());
		Out.Center = FVector(BoxCenter.X, BoxCenter.Y, Origin.Z);
		Out.Axis = HalfX >= HalfY ? AxisX : AxisY;
		const double Short = FMath::Min(HalfX, HalfY);
		const double Long = FMath::Max(HalfX, HalfY);
		Out.Radius = static_cast<float>(FMath::Max(120.0, Short));
		Out.HalfLength = static_cast<float>(FMath::Max(0.0, Long - Short));
		Out.Height = static_cast<float>(FMath::Clamp(Extent.Z * 2.0 * BodyScale.Z, 80.0, 1500.0));
		return true;
	}
}

float TNBeachLoot::SearchChance(ETNBeachElement Element)
{
	switch (Element)
	{
		// Lo grande: siempre.
		case ETNBeachElement::SandCastleHuge:
		case ETNBeachElement::SandCastleSmall:
		case ETNBeachElement::ShipSailWreck:
		case ETNBeachElement::BeachChair:
		case ETNBeachElement::PlantedUmbrella:
		case ETNBeachElement::FishingNet:
		case ETNBeachElement::RockCluster:
		case ETNBeachElement::OldPlanks:
		case ETNBeachElement::MossyLog:
		case ETNBeachElement::ToyBucket:
		case ETNBeachElement::BeachTowel:
		case ETNBeachElement::Sandbags:
		case ETNBeachElement::AmmoCrate:
		case ETNBeachElement::CamoNet:
			return 1.f;
		// Lo mediano: casi siempre.
		case ETNBeachElement::Rock:
		case ETNBeachElement::Driftwood:
		case ETNBeachElement::Bottle:
		case ETNBeachElement::PlasticCup:
		case ETNBeachElement::Buoy:
		case ETNBeachElement::Coconut:
		case ETNBeachElement::WatermelonRind:
		case ETNBeachElement::RedBra:
		case ETNBeachElement::FlipFlop:
		case ETNBeachElement::BeachBall:
		case ETNBeachElement::Frisbee:
		case ETNBeachElement::SunscreenBottle:
		case ETNBeachElement::Jerrycan:
		case ETNBeachElement::TankTrap:
		case ETNBeachElement::ToySoldiers:
		case ETNBeachElement::MilitaryHelmet:
		case ETNBeachElement::Clam:
			return 0.85f;
		// Lo pequeño que aún se puede revolver: a menudo.
		case ETNBeachElement::SodaCan:
		case ETNBeachElement::JuiceBox:
		case ETNBeachElement::Sunglasses:
		case ETNBeachElement::RubberDuck:
		case ETNBeachElement::Lollipop:
		case ETNBeachElement::DecorShell:
		case ETNBeachElement::Starfish:
		case ETNBeachElement::SixPackRings:
		case ETNBeachElement::RopePiece:
		case ETNBeachElement::Cuttlebone:
			return 0.6f;
		// Nunca: lo diminuto o fino (chapas, cáscaras, palitos, pluma, pajita), la medusa (pica) y los caminos (pasarela y
		// caminito de palos). Las trampas y los enemigos no son decorado.
		default:
			return 0.f;
	}
}

float TNBeachLoot::RaceWeight(FName /*RowName*/, const FTN_InventoryItem& Row)
{
	switch (Row.UseType)
	{
		// Para ti: energía sin fin unos segundos (dejar atrás la tormenta) o la barra llena de golpe.
		case ETN_ItemUseType::SelfStaminaBoost: return 1.5f;
		case ETN_ItemUseType::SelfStaminaFull:  return 1.2f;
		// Para fastidiar: la bola derriba a la que alcanza, la tinta la ciega y la concha trampa atrapa a las de detrás.
		case ETN_ItemUseType::Throwable:        return 1.3f;
		case ETN_ItemUseType::InkThrower:       return 1.3f;
		case ETN_ItemUseType::Conch:            return 1.f;
		// En la playa no protege de nada (las gaviotas de la carrera no la miran): solo la cabezota y el mareo al acabar.
		case ETN_ItemUseType::BigHead:          return 0.3f;
		// En la carrera no se muere: no revive a nadie.
		case ETN_ItemUseType::Totem:            return 0.f;
		default:                                return 1.f;
	}
}

FLinearColor TNBeachLoot::SandDust()
{
	return FLinearColor(0.9f, 0.82f, 0.64f);
}

bool TNBeachLoot::IsClearOfLayout(const ATN_BeachRaceGenerator& Generator, const FVector2D& Local, double Radius, int32 SkipIndex)
{
	// Fuera del agua de las pozas (con algo de orilla).
	for (const TNBeachLayout::FPool& Pool : TNBeachLayout::Pools())
	{
		if (TNBeachLayout::PoolU(Pool, Local) < 1.15)
		{
			return false;
		}
	}
	TNBeachLayout::FItem Probe;
	Probe.Pos = Local;
	Probe.Radius = Radius;
	Probe.Core = Radius;
	const TArray<TNBeachLayout::FItem>& Items = Generator.GetRoundLayout().Items;
	for (int32 j = 0; j < Items.Num(); ++j)
	{
		const TNBeachLayout::FItem& Item = Items[j];
		// Descarte rápido por X (el reparto tiene cientos de elementos y esto se pregunta miles de veces por ronda).
		if (j == SkipIndex || Item.bOverlay
			|| FMath::Abs(Item.Pos.X - Local.X) > Item.HalfLength + FMath::Max(Item.Radius, Item.Core) + Radius)
		{
			continue;
		}
		if (TNBeachLayout::Clearance(Item, Probe) < 0.0)
		{
			return false;
		}
	}
	return true;
}

void TNBeachLoot::SpawnRoundLoot(ATN_BeachRaceGenerator& Generator)
{
	UWorld* World = Generator.GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (UTN_BeachLootSubsystem* Loot = World->GetSubsystem<UTN_BeachLootSubsystem>())
	{
		Loot->SpawnForRound(Generator);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachSearchSpot
// ─────────────────────────────────────────────────────────────────────────────

ATN_BeachSearchSpot::ATN_BeachSearchSpot()
{
	LootChance = TNBeachLoot::SearchLuck;
	// Los pesos son los de la carrera (GetLootWeight): los del cooperativo, fuera.
	LootWeights.Reset();
	// A la escala de la playa (todo 28 veces más grande): chispitas desde más lejos y el anillo de dónde rebuscar, antes y
	// más grande.
	HintDistance = 3500.f;
	MarkerDistance = 1800.f;
	MarkerRadius = 105.f;
}

float ATN_BeachSearchSpot::GetLootWeight(FName RowName, const FTN_InventoryItem& Row) const
{
	return TNBeachLoot::RaceWeight(RowName, Row);
}

// ─────────────────────────────────────────────────────────────────────────────
// UTN_BeachLootSubsystem
// ─────────────────────────────────────────────────────────────────────────────

bool UTN_BeachLootSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTN_BeachLootSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTN_BeachLootSubsystem, STATGROUP_Tickables);
}

void UTN_BeachLootSubsystem::Deinitialize()
{
	// El mundo se va: se lleva todo lo repartido.
	SpawnedSpots.Reset();
	SpawnedItems.Reset();
	SpawnedShells.Reset();
	SpotDiscs.Reset();
	CachedGenerator.Reset();
	Super::Deinitialize();
}

void UTN_BeachLootSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || World->GetNetMode() == NM_Client)
	{
		return;
	}
	ATN_BeachRaceGenerator* Gen = CachedGenerator.Get();
	if (!Gen)
	{
		// Sin playa en este mapa (lobby, mapa procedural): se mira una vez por segundo.
		FindClock -= DeltaTime;
		if (FindClock > 0.f)
		{
			return;
		}
		FindClock = 1.f;
		Gen = ATN_BeachRaceGenerator::Find(World);
		if (!Gen)
		{
			return;
		}
		CachedGenerator = Gen;
	}
	if (Gen->GetRoundNumber() == LootRound)
	{
		return;
	}
	if (!Gen->IsRoundReady())
	{
		// Ronda quitada (o aún sin repartir): fuera el botín de la anterior; el de la nueva, cuando esté.
		if (LootRound != 0)
		{
			ClearLoot();
			LootRound = 0;
		}
		return;
	}
	SpawnForRound(*Gen);
}

void UTN_BeachLootSubsystem::Reroll()
{
	ATN_BeachRaceGenerator* Gen = CachedGenerator.Get();
	if (!Gen)
	{
		Gen = ATN_BeachRaceGenerator::Find(GetWorld());
		CachedGenerator = Gen;
	}
	if (!Gen || !Gen->IsRoundReady())
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Playa] TN.Beach.Loot.Reroll: no hay ronda repartida."));
		return;
	}
	// Otra tirada: la misma ronda con otra sal.
	++RerollSalt;
	SpawnForRound(*Gen);
}

void UTN_BeachLootSubsystem::ClearLoot()
{
	// Los rebuscables, al irse, se llevan lo que salió de ellos y nadie recogió (ATN_ProcSearchSpot::EndPlay).
	for (TArray<TWeakObjectPtr<AActor>>* List : { &SpawnedSpots, &SpawnedItems, &SpawnedShells })
	{
		for (const TWeakObjectPtr<AActor>& Ptr : *List)
		{
			if (AActor* Actor = Ptr.Get())
			{
				Actor->Destroy();
			}
		}
		List->Reset();
	}
	SpotDiscs.Reset();
}

void UTN_BeachLootSubsystem::SpawnForRound(ATN_BeachRaceGenerator& Gen)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	CachedGenerator = &Gen;
	ClearLoot();
	LootRound = Gen.GetRoundNumber();
	if (TNBeachLootDetail::CVarBeachLoot.GetValueOnGameThread() == 0)
	{
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] botín de la ronda %d: ninguno (TN.Beach.Loot 0)."), LootRound);
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	const int32 Seed = Gen.GetRoundSeed();
	FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(Seed), GetTypeHash(0x10075EEDu + static_cast<uint32>(RerollSalt)))));
	int32 Candidates = 0;
	const int32 NumSpots = SpawnSearchSpots(Gen, Rng, Candidates);
	const UDataTable* Catalog = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TNBeachLoot::CatalogPath())).LoadSynchronous();
	const int32 NumItems = Catalog ? SpawnLooseItems(Gen, Rng, *Catalog) : 0;
	if (!Catalog)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Playa] Sin catálogo de objetos (%s): no hay objetos sueltos."), TNBeachLoot::CatalogPath());
	}
	const FString Shells = SpawnRoundShells(Gen, Seed + RerollSalt * 7919);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] botín de la ronda %d: %d decorados para rebuscar (de %d candidatos) y %d objetos sueltos · %.0f ms."),
		LootRound, NumSpots, Candidates, NumItems, (FPlatformTime::Seconds() - T0) * 1000.0);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] conchas de la ronda %d: %s"), LootRound, *Shells);
}

int32 UTN_BeachLootSubsystem::SpawnSearchSpots(ATN_BeachRaceGenerator& Gen, FRandomStream& Rng, int32& OutCandidates)
{
	using namespace TNBeachLootDetail;
	UWorld* World = GetWorld();
	OutCandidates = 0;
	if (!World)
	{
		return 0;
	}
	const FTransform GenXf = Gen.GetActorTransform();
	TArray<FSearchShape> Candidates;
	for (ATN_BeachElement* Element : Gen.GetRoundElements())
	{
		if (!IsValid(Element))
		{
			continue;
		}
		const float Chance = TNBeachLoot::SearchChance(Element->GetSpec().Element);
		FSearchShape Shape;
		if (Chance <= 0.f || !MeasureElement(*Element, Shape))
		{
			continue;
		}
		Shape.Chance = Chance;
		Shape.Progress = TNBeachLayout::ProgressOfX(GenXf.InverseTransformPosition(Shape.Center).X);
		Candidates.Add(Shape);
	}
	OutCandidates = Candidates.Num();

	// En orden al azar (con la semilla): ningún decorado tiene preferencia por salir antes en el reparto.
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		Candidates.Swap(i, Rng.RandRange(0, i));
	}
	int32 PerSection[TNBeachLoot::Sections] = {};
	TArray<const FSearchShape*> Taken;
	for (const FSearchShape& Shape : Candidates)
	{
		if (Taken.Num() >= TNBeachLoot::MaxSearchSpots)
		{
			break;
		}
		if (Rng.FRand() >= Shape.Chance)
		{
			continue;
		}
		const int32 Section = FMath::Clamp(static_cast<int32>(Shape.Progress * TNBeachLoot::Sections), 0, TNBeachLoot::Sections - 1);
		if (PerSection[Section] >= TNBeachLoot::MaxSearchSpotsPerSection)
		{
			continue;
		}
		// Uno por corrillo: lejos de los demás rebuscables (de centro a centro y de borde a borde).
		const double Reach = Shape.Radius + Shape.HalfLength;
		bool bCrowded = false;
		for (const FSearchShape* Other : Taken)
		{
			const double Need = FMath::Max(TNBeachLoot::MinSearchSpacing, Reach + Other->Radius + Other->HalfLength + TNBeachLoot::MinSearchRimGap);
			if (FVector::DistSquared2D(Shape.Center, Other->Center) < FMath::Square(Need))
			{
				bCrowded = true;
				break;
			}
		}
		if (bCrowded)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = Shape.Element;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Shape.Axis.Y, Shape.Axis.X));
		ATN_BeachSearchSpot* Spot = World->SpawnActor<ATN_BeachSearchSpot>(ATN_BeachSearchSpot::StaticClass(),
			FTransform(FRotator(0.0, Yaw, 0.0), Shape.Center), Params);
		if (!Spot)
		{
			continue;
		}
		Spot->SetupSpot(Shape.Radius, Shape.HalfLength, Shape.Height, TNBeachLoot::SandDust());
		SpawnedSpots.Add(Spot);
		SpotDiscs.Add(FVector4(Shape.Center.X, Shape.Center.Y, Shape.Center.Z, Reach));
		Taken.Add(&Shape);
		++PerSection[Section];
	}
	return Taken.Num();
}

int32 UTN_BeachLootSubsystem::SpawnLooseItems(ATN_BeachRaceGenerator& Gen, FRandomStream& Rng, const UDataTable& Catalog)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	const FTransform GenXf = Gen.GetActorTransform();
	TArray<FVector2D> Placed;

	// Libre: a 3 m de lo que ocupa cada elemento del reparto (también de los pasos de quads), a 2,5 m del borde de los
	// rebuscables y a 6 m de otro objeto suelto.
	auto IsFree = [&](const FVector2D& P)
	{
		if (!TNBeachLoot::IsClearOfLayout(Gen, P, 300.0))
		{
			return false;
		}
		const FVector OnBeach = GenXf.TransformPosition(FVector(P.X, P.Y, 0.0));
		for (const FVector4& Disc : SpotDiscs)
		{
			if (FVector2D::DistSquared(FVector2D(OnBeach.X, OnBeach.Y), FVector2D(Disc.X, Disc.Y)) < FMath::Square(Disc.W + 250.0))
			{
				return false;
			}
		}
		for (const FVector2D& Other : Placed)
		{
			if (FVector2D::DistSquared(P, Other) < FMath::Square(600.0))
			{
				return false;
			}
		}
		return true;
	};

	// Un objeto del catálogo (pesos de la carrera) en la arena de P, con su pickup de siempre.
	auto SpawnAt = [&](const FVector2D& P)
	{
		FTN_InventoryItem Picked;
		if (!ATN_ProcSearchSpot::PickCatalogItem(&Catalog,
			[](FName RowName, const FTN_InventoryItem& Row) { return TNBeachLoot::RaceWeight(RowName, Row); }, Picked))
		{
			return false;
		}
		const FVector Flat = GenXf.TransformPosition(FVector(P.X, P.Y, 0.0));
		const FVector Where(Flat.X, Flat.Y, Gen.GetGroundHeightAt(Flat) + 3.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATN_PickupInteractableBase* Pickup = World->SpawnActor<ATN_PickupInteractableBase>(Picked.PickupActorClass, Where,
			FRotator(0.0, Rng.FRandRange(0.f, 360.f), 0.0), Params);
		if (!Pickup)
		{
			return false;
		}
		Pickup->InitializeFromInventoryItem(Picked);
		SpawnedItems.Add(Pickup);
		Placed.Add(P);
		return true;
	};

	const double X0 = TNBeachLoot::LooseStartX;
	const double X1 = TNBeachLayout::ItemsEndX;
	const double HalfW = TNBeachLayout::HalfWidth - TNBeachLayout::SideMargin - 700.0;
	int32 Count = 0;

	// Sueltos: uno por tramo igual del recorrido, más hacia el centro que hacia la selva (suma de dos al azar).
	const int32 Singles = Rng.RandRange(TNBeachLoot::MinLooseItems, TNBeachLoot::MaxLooseItems);
	const double Band = (X1 - X0) / FMath::Max(1, Singles);
	for (int32 i = 0; i < Singles; ++i)
	{
		for (int32 Attempt = 0; Attempt < 24; ++Attempt)
		{
			const FVector2D P(X0 + Band * (i + Rng.FRandRange(0.1f, 0.9f)), (Rng.FRand() + Rng.FRand() - 1.0) * HalfW);
			if (IsFree(P) && SpawnAt(P))
			{
				++Count;
				break;
			}
		}
	}

	// Filas de lado a lado de la playa (como las cajas de objetos de las carreras de karts): todas pasan por una.
	for (int32 r = 0; r < TNBeachLoot::ItemRows; ++r)
	{
		const double T = FMath::Clamp((r + 0.5) / TNBeachLoot::ItemRows + Rng.FRandRange(-0.06f, 0.06f), 0.05, 0.95);
		const double RowX = FMath::Lerp(X0 + 3000.0, X1 - 3000.0, T);
		for (double Y = -HalfW; Y <= HalfW + 1.0; Y += TNBeachLoot::ItemRowStep)
		{
			// Si su sitio está ocupado, un poco más adelante o atrás.
			for (int32 Attempt = 0; Attempt < 5; ++Attempt)
			{
				const double Shift = Attempt == 0 ? 0.0 : Rng.FRandRange(-900.f, 900.f);
				const FVector2D P(RowX + Shift, Y + (Attempt == 0 ? 0.0 : Rng.FRandRange(-300.f, 300.f)));
				if (IsFree(P) && SpawnAt(P))
				{
					++Count;
					break;
				}
			}
		}
	}
	return Count;
}
