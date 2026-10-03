#include "World/Beach/TN_BeachLoot.h"
#include "Core/TN_GameplayPreload.h"
#include "World/Beach/TN_BeachDecorField.h"
#include "TN_BeachDecorKit.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/TN_PickupInteractableBase.h"
#include "World/Beach/TN_RaceItemBox.h"
#include "Core/TN_InventoryTypes.h"
#include "Core/TN_Log.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
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

	/** Distancia (cm) en planta de P al borde de la huella de un punto rebuscable (negativa dentro). */
	double RimDistance(const TNBeachLoot::FSearchPoint& Point, const FVector& P)
	{
		const FVector2D Center(Point.Center.X, Point.Center.Y);
		const FVector2D Axis = FVector2D(Point.Axis.X, Point.Axis.Y).GetSafeNormal();
		double T = 0.0;
		const double Dist = TNProcMap::DistPointSegment(FVector2D(P.X, P.Y), Center - Axis * Point.HalfLength, Center + Axis * Point.HalfLength, T);
		return Dist - Point.Radius;
	}

	/**
	 * Variante del montículo según lo que se rebusca (0 liso, 1 chapa, 2 palito de helado, 3 trozo de concha asomando):
	 * junto a la basura y lo del día anterior, chapas y palitos; junto a lo que trae el mar y los castillos, conchas.
	 */
	int32 MoundVariantFor(ETNBeachElement Element, float Roll)
	{
		switch (Element)
		{
			case ETNBeachElement::BeachChair:
			case ETNBeachElement::PlantedUmbrella:
			case ETNBeachElement::BeachTowel:
			case ETNBeachElement::PlasticCup:
			case ETNBeachElement::Bottle:
			case ETNBeachElement::SodaCan:
			case ETNBeachElement::JuiceBox:
			case ETNBeachElement::Lollipop:
			case ETNBeachElement::WatermelonRind:
			case ETNBeachElement::FlipFlop:
			case ETNBeachElement::BeachBall:
			case ETNBeachElement::Frisbee:
			case ETNBeachElement::SunscreenBottle:
			case ETNBeachElement::ToyBucket:
			case ETNBeachElement::Sunglasses:
			case ETNBeachElement::RubberDuck:
			case ETNBeachElement::RedBra:
			case ETNBeachElement::SixPackRings:
				return Roll < 0.4f ? 1 : (Roll < 0.75f ? 2 : 0);
			case ETNBeachElement::Sandbags:
			case ETNBeachElement::AmmoCrate:
			case ETNBeachElement::TankTrap:
			case ETNBeachElement::MilitaryHelmet:
			case ETNBeachElement::CamoNet:
			case ETNBeachElement::Jerrycan:
			case ETNBeachElement::ToySoldiers:
				return Roll < 0.35f ? 1 : 0;
			default:
				return Roll < 0.5f ? 3 : 0;
		}
	}

	/**
	 * Montículo de un punto rebuscable: en la arena junto a su borde, del lado por el que se llega (hacia la salida, ±50°;
	 * si ahí hay otra pieza, se prueba a los lados, detrás y en las diagonales: TNBeachLoot::PickMoundSide), algo metido bajo
	 * la pieza, como escarbado de debajo. Devuelve false, sin montículo, si ningún lado está libre: un rebuscable sin
	 * montículo a la vista no existe (#254). OutAnchor dice dónde queda el montículo y qué ocupa su base: ahí se pone el actor
	 * interactivo del punto.
	 */
	bool MakeMound(const ATN_BeachRaceGenerator& Gen, const TNBeachLoot::FSearchPoint& Point, ETNBeachElement Element, FRandomStream& Rng,
		FTNBeachSearchMound& OutMound, TNBeachLoot::FSearchAnchor& OutAnchor)
	{
		const FTransform GenXf = Gen.GetActorTransform();
		// Todo el azar de este montículo, junto y antes de buscarle sitio: que lo haya o no, no cambia el de los demás.
		const float Size = Rng.FRandRange(0.8f, 1.25f);
		const double BaseAngle = Rng.FRandRange(-50.f, 50.f);
		const uint8 Yaw = static_cast<uint8>(Rng.RandRange(0, 255));
		const float VariantRoll = Rng.FRand();
		const double Reach = TNBeachDecorKit::SearchMoundRadius * Size;
		const FVector AxisW = FVector(Point.Axis.X, Point.Axis.Y, 0.0).GetSafeNormal();
		// Dónde quedaría el montículo por el lado Side: en el punto del borde de la huella en esa dirección (el de más allá de
		// la cápsula) y algo por fuera.
		const auto SiteOf = [&](int32 Side)
		{
			// Hacia la salida (-X del generador), girado.
			const FVector LocalDir = TNBeachProp::YawQ(BaseAngle + TNBeachLoot::MoundSideTurns[Side]).RotateVector(FVector(-1.0, 0.0, 0.0));
			const FVector Dir = GenXf.TransformVectorNoScale(LocalDir).GetSafeNormal2D();
			const double Along = FVector::DotProduct(AxisW, Dir) >= 0.0 ? 1.0 : -1.0;
			const FVector Rim = Point.Center + AxisW * (Point.HalfLength * Along) + Dir * Point.Radius;
			return Rim + Dir * (Reach * 0.3);
		};
		const int32 Side = TNBeachLoot::PickMoundSide([&](int32 Candidate)
		{
			const FVector LocalAt = GenXf.InverseTransformPosition(SiteOf(Candidate));
			return TNBeachLoot::IsClearOfLayout(Gen, FVector2D(LocalAt.X, LocalAt.Y), Reach * 0.5, Point.Item);
		});
		if (Side == INDEX_NONE)
		{
			return false;
		}
		const FVector At = SiteOf(Side);
		const FVector Ground(At.X, At.Y, Gen.GetGroundHeightAt(At));
		const FVector Local = GenXf.InverseTransformPosition(Ground);
		OutMound = FTNBeachSearchMound();
		OutMound.X = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt32(Local.X * 0.5), 0, 65535));
		OutMound.Y = static_cast<int16>(FMath::Clamp(FMath::RoundToInt32(Local.Y), -32767, 32767));
		OutMound.Z = static_cast<int16>(FMath::Clamp(FMath::RoundToInt32(Local.Z), -32767, 32767));
		OutMound.Yaw = Yaw;
		const int32 Variant = MoundVariantFor(Element, VariantRoll);
		const int32 SizeStep = FMath::Clamp(FMath::RoundToInt32((Size - 0.8f) / 0.45f * 63.f), 0, 63);
		OutMound.Look = static_cast<uint8>((Variant & 3) | (SizeStep << 2));
		// La base con los terrones que asoman: 1,1 veces el radio del montículo, por su tamaño.
		OutAnchor.Ground = Ground;
		OutAnchor.Radius = static_cast<float>(TNBeachDecorKit::SearchMoundRadius * 1.1 * Size);
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

void TNBeachLoot::ClearSearchAround(ATN_BeachRaceGenerator& Generator, const FVector& WorldCenter, float Radius)
{
	UWorld* World = Generator.GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (UTN_BeachLootSubsystem* Loot = World->GetSubsystem<UTN_BeachLootSubsystem>())
	{
		Loot->ClearSearchAround(WorldCenter, Radius);
	}
}

float TNBeachLoot::SearchSpotScale(ETNProcDifficulty Difficulty)
{
	switch (Difficulty)
	{
		case ETNProcDifficulty::Easy: return 1.6f;
		case ETNProcDifficulty::Hard: return 1.4f;
		default:                      return 1.f;
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
	// A la escala de la playa (todo 28 veces más grande): chispitas desde más lejos. El anillo es el de siempre, fijo y
	// centrado en el montículo de arena de su punto (GetMarkerAnchor); como el actor solo existe a menos de
	// ProxySpawnDistance de alguna tortuga, se ve desde que aparece.
	HintDistance = 3500.f;
	// Solo existe cerca de alguna tortuga (el registro lo crea y lo quita): a un cliente le basta con tenerlo cerca.
	SetNetCullDistanceSquared(FMath::Square(TNBeachLoot::ProxyNetRelevance));
}

void ATN_BeachSearchSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATN_BeachSearchSpot, MoundIndex, COND_InitialOnly);
}

ETNSearchMarkerAnchor ATN_BeachSearchSpot::GetMarkerAnchor(FVector& OutGround, float& OutFootRadius) const
{
	if (MoundIndex == INDEX_NONE)
	{
		return ETNSearchMarkerAnchor::Footprint;
	}
	// El montículo sale de lo replicado del registro (mismo sitio y tamaño en todas las máquinas). Si el registro o ese
	// montículo aún no han llegado aquí, sin anillo todavía (no sale uno en la huella para saltar luego).
	if (!MoundRegistry.IsValid())
	{
		MoundRegistry = ATN_BeachSearchRegistry::Find(this);
	}
	const ATN_BeachSearchRegistry* Registry = MoundRegistry.Get();
	return Registry && Registry->GetMoundFoot(MoundIndex, OutGround, OutFootRadius)
		? ETNSearchMarkerAnchor::Point : ETNSearchMarkerAnchor::Pending;
}

// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachSearchRegistry
// ─────────────────────────────────────────────────────────────────────────────

ATN_BeachSearchRegistry::ATN_BeachSearchRegistry()
{
	// El tick solo mueve los montículos cercanos (máquinas con pantalla); se enciende al tenerlos.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RegistryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RegistryRoot"));
	RegistryRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(RegistryRoot);
	bReplicates = true;
	SetReplicateMovement(false);
	// Pocos bytes y hacen falta en toda la playa (TN.Beach.Perf en cada cliente): siempre relevante y dormido salvo al
	// cambiar (una vez por ronda y una por rebuscable usado).
	bAlwaysRelevant = true;
	NetDormancy = DORM_DormantAll;
	SetNetUpdateFrequency(2.f);
	SetMinNetUpdateFrequency(1.f);
	SetCanBeDamaged(false);
}

void ATN_BeachSearchRegistry::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachSearchRegistry, SearchNet);
}

ATN_BeachSearchRegistry* ATN_BeachSearchRegistry::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ATN_BeachSearchRegistry> It(World);
	return It ? *It : nullptr;
}

void ATN_BeachSearchRegistry::WakeForChange()
{
	// Despierto antes del cambio (el patrón de los rebuscables) y dormido otra vez unos segundos después del último.
	if (NetDormancy != DORM_Awake)
	{
		SetNetDormancy(DORM_Awake);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SleepTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			SetNetDormancy(DORM_DormantAll);
		}), 3.f, false);
	}
}

void ATN_BeachSearchRegistry::ServerReset(int32 Round, int32 Salt, const TArray<FTNBeachSearchMound>& InMounds)
{
	if (!HasAuthority())
	{
		return;
	}
	WakeForChange();
	SearchNet.Round = Round;
	SearchNet.Salt = Salt;
	SearchNet.Count = InMounds.Num();
	SearchNet.UsedBits.Init(0u, (SearchNet.Count + 31) / 32);
	SearchNet.Mounds = InMounds;
	ForceNetUpdate();
	// El anfitrión no recibe OnRep: sus montículos, aquí.
	RefreshMounds();
}

void ATN_BeachSearchRegistry::ServerMarkUsed(int32 Index)
{
	if (!HasAuthority() || Index < 0 || Index >= SearchNet.Count || IsUsed(Index))
	{
		return;
	}
	WakeForChange();
	SearchNet.UsedBits[Index / 32] |= 1u << (Index % 32);
	ForceNetUpdate();
	RefreshMounds();
}

bool ATN_BeachSearchRegistry::IsUsed(int32 Index) const
{
	const int32 Word = Index / 32;
	return Index >= 0 && SearchNet.UsedBits.IsValidIndex(Word) && (SearchNet.UsedBits[Word] & (1u << (Index % 32))) != 0u;
}

int32 ATN_BeachSearchRegistry::NumUsed() const
{
	int32 Count = 0;
	for (const uint32 Word : SearchNet.UsedBits)
	{
		Count += FMath::CountBits(Word);
	}
	return Count;
}

float ATN_BeachSearchSpot::GetLootWeight(FName RowName, const FTN_InventoryItem& Row) const
{
	return TNBeachLoot::RaceWeight(RowName, Row);
}

bool ATN_BeachSearchSpot::PickLoot(FTN_InventoryItem& OutItem, const APawn* Searcher) const
{
	// Objetos de siempre y de carrera, con el peso según el puesto de quien rebusca; si algo falla, el sorteo de siempre.
	if (TNRaceItems::RollLoot(Searcher, GetRaceLootSource(), GetLootTable(), OutItem))
	{
		return true;
	}
	return Super::PickLoot(OutItem, Searcher);
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
	SpawnedItems.Reset();
	SpawnedShells.Reset();
	SearchPoints.Reset();
	SearchAnchors.Reset();
	SearchProxies.Reset();
	SearchUsed.Reset();
	SpotDiscs.Reset();
	CachedGenerator.Reset();
	Registry.Reset();
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
	// Los rebuscables de la ronda: su actor, solo cerca de alguna tortuga.
	ProxyClock -= DeltaTime;
	if (ProxyClock <= 0.f)
	{
		ProxyClock = TNBeachLoot::ProxyCheckSeconds;
		TickSearchProxies();
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
	for (const TWeakObjectPtr<ATN_BeachSearchSpot>& Ptr : SearchProxies)
	{
		if (ATN_BeachSearchSpot* Spot = Ptr.Get())
		{
			Spot->Destroy();
		}
	}
	SearchProxies.Reset();
	SearchPoints.Reset();
	SearchAnchors.Reset();
	SearchUsed.Reset();
	for (TArray<TWeakObjectPtr<AActor>>* List : { &SpawnedItems, &SpawnedShells })
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
	if (ATN_BeachSearchRegistry* Reg = Registry.Get())
	{
		Reg->ServerReset(0, 0, TArray<FTNBeachSearchMound>());
	}
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
	int32 NoMoundSite = 0;
	const int32 NumSpots = BuildSearchRegistry(Gen, Rng, Candidates, NoMoundSite);
	const UDataTable* Catalog = TNPreload::ItemCatalog();
	const int32 NumItems = Catalog ? SpawnLooseItems(Gen, Rng, *Catalog) : 0;
	if (!Catalog)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Playa] Sin catálogo de objetos (%s): no hay objetos sueltos."), TNBeachLoot::CatalogPath());
	}
	const FString Shells = SpawnRoundShells(Gen, Seed + RerollSalt * 7919);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] botín de la ronda %d: %d decorados para rebuscar (de %d candidatos, %d sin sitio libre para su montículo; su actor aparece solo cerca de una tortuga, en su montículo) y %d objetos sueltos · %.0f ms."),
		LootRound, NumSpots, Candidates, NoMoundSite, NumItems, (FPlatformTime::Seconds() - T0) * 1000.0);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] conchas de la ronda %d: %s"), LootRound, *Shells);
}

int32 UTN_BeachLootSubsystem::BuildSearchRegistry(ATN_BeachRaceGenerator& Gen, FRandomStream& Rng, int32& OutCandidates, int32& OutNoMoundSite)
{
	OutCandidates = 0;
	OutNoMoundSite = 0;
	SearchPoints.Reset();
	SearchAnchors.Reset();
	SearchProxies.Reset();
	SearchUsed.Reset();
	const ATN_BeachDecorField* Field = Gen.GetDecorField();
	const TArray<TNBeachLayout::FItem>& Items = Gen.GetRoundLayout().Items;
	const FTransform GenXf = Gen.GetActorTransform();
	TArray<TNBeachLoot::FSearchPoint> Candidates;
	TArray<float> Chances;
	if (Field)
	{
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			// El decorado de la ronda es local e instanciado: la huella sale de su malla en el campo (la misma en todas las
			// máquinas), sin actor.
			const float Chance = TNBeachLoot::SearchChance(Items[i].Element);
			FTNBeachDecorShape Shape;
			if (Chance <= 0.f || !Field->GetSearchShape(i, Shape))
			{
				continue;
			}
			TNBeachLoot::FSearchPoint& Point = Candidates.AddDefaulted_GetRef();
			Point.Item = i;
			Point.Center = Shape.Center;
			Point.Axis = Shape.Axis;
			Point.Radius = Shape.Radius;
			Point.HalfLength = Shape.HalfLength;
			Point.Height = Shape.Height;
			Point.Progress = TNBeachLayout::ProgressOfX(GenXf.InverseTransformPosition(Shape.Center).X);
			Chances.Add(Chance);
		}
	}
	OutCandidates = Candidates.Num();

	// En orden al azar (con la semilla): ningún decorado tiene preferencia por salir antes en el reparto.
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		const int32 j = Rng.RandRange(0, i);
		Candidates.Swap(i, j);
		Chances.Swap(i, j);
	}
	// Más o menos según la dificultad (las ayudas del perfil).
	const float Scale = TNBeachLoot::SearchSpotScale(Gen.GetRoundDifficulty());
	const int32 MaxSpots = FMath::RoundToInt32(TNBeachLoot::MaxSearchSpots * Scale);
	const int32 MaxPerSection = FMath::RoundToInt32(TNBeachLoot::MaxSearchSpotsPerSection * Scale);
	int32 PerSection[TNBeachLoot::Sections] = {};
	// El montículo de arena de cada punto (lo que se ve en todas las máquinas y donde se rebusca), con su propio azar: no
	// cambia el resto. Se hace al elegir el punto: sin sitio libre para él, el punto no se crea.
	TArray<FTNBeachSearchMound> Mounds;
	FRandomStream MoundRng(static_cast<int32>(HashCombine(GetTypeHash(Gen.GetRoundSeed()), GetTypeHash(0x40D5u + static_cast<uint32>(RerollSalt)))));
	for (int32 c = 0; c < Candidates.Num(); ++c)
	{
		const TNBeachLoot::FSearchPoint& Shape = Candidates[c];
		if (SearchPoints.Num() >= MaxSpots)
		{
			break;
		}
		if (Rng.FRand() >= Chances[c])
		{
			continue;
		}
		const int32 Section = FMath::Clamp(static_cast<int32>(Shape.Progress * TNBeachLoot::Sections), 0, TNBeachLoot::Sections - 1);
		if (PerSection[Section] >= MaxPerSection)
		{
			continue;
		}
		// Uno por corrillo: lejos de los demás rebuscables (de centro a centro y de borde a borde).
		const double Reach = Shape.Radius + Shape.HalfLength;
		bool bCrowded = false;
		for (const TNBeachLoot::FSearchPoint& Other : SearchPoints)
		{
			const double Need = FMath::Max(TNBeachLoot::MinSearchSpacing, Reach + Other.Radius + Other.HalfLength + TNBeachLoot::MinSearchRimGap);
			if (FVector::DistSquared2D(Shape.Center, Other.Center) < FMath::Square(Need))
			{
				bCrowded = true;
				break;
			}
		}
		if (bCrowded)
		{
			continue;
		}
		const ETNBeachElement Element = Items.IsValidIndex(Shape.Item) ? Items[Shape.Item].Element : ETNBeachElement::Rock;
		FTNBeachSearchMound Mound;
		TNBeachLoot::FSearchAnchor Anchor;
		if (!TNBeachLootDetail::MakeMound(Gen, Shape, Element, MoundRng, Mound, Anchor))
		{
			++OutNoMoundSite;
			continue;
		}
		SearchPoints.Add(Shape);
		SearchAnchors.Add(Anchor);
		Mounds.Add(Mound);
		SpotDiscs.Add(FVector4(Shape.Center.X, Shape.Center.Y, Shape.Center.Z, Reach));
		++PerSection[Section];
	}
	SearchProxies.SetNum(SearchPoints.Num());
	SearchUsed.Init(false, SearchPoints.Num());
	if (ATN_BeachSearchRegistry* Reg = EnsureRegistry())
	{
		Reg->ServerReset(LootRound, RerollSalt, Mounds);
	}
	// Los que ya tengan una tortuga cerca (la salida está lejos del reparto, pero TN.Beach.Loot.Reroll se usa en medio).
	TickSearchProxies();
	return SearchPoints.Num();
}

ATN_BeachSearchRegistry* UTN_BeachLootSubsystem::EnsureRegistry()
{
	if (ATN_BeachSearchRegistry* Existing = Registry.Get())
	{
		return Existing;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	ATN_BeachSearchRegistry* Found = ATN_BeachSearchRegistry::Find(World);
	if (!Found)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Found = World->SpawnActor<ATN_BeachSearchRegistry>(ATN_BeachSearchRegistry::StaticClass(), FTransform::Identity, Params);
	}
	Registry = Found;
	return Found;
}

void UTN_BeachLootSubsystem::MarkSearchUsed(int32 Index)
{
	if (!SearchUsed.IsValidIndex(Index) || SearchUsed[Index])
	{
		return;
	}
	SearchUsed[Index] = true;
	if (ATN_BeachSearchRegistry* Reg = Registry.Get())
	{
		Reg->ServerMarkUsed(Index);
	}
}

int32 UTN_BeachLootSubsystem::NumSearchProxies() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ATN_BeachSearchSpot>& Ptr : SearchProxies)
	{
		Count += Ptr.IsValid() ? 1 : 0;
	}
	return Count;
}

void UTN_BeachLootSubsystem::TickSearchProxies()
{
	UWorld* World = GetWorld();
	if (!World || SearchPoints.Num() == 0)
	{
		return;
	}
	// Dónde están las tortugas (las de todos los jugadores; las fantasmas y las que miran no rebuscan).
	TArray<FVector, TInlineAllocator<8>> Turtles;
	if (const AGameStateBase* GS = World->GetGameState())
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			if (const APawn* Pawn = PS ? PS->GetPawn() : nullptr)
			{
				Turtles.Add(Pawn->GetActorLocation());
			}
		}
	}
	for (int32 i = 0; i < SearchPoints.Num(); ++i)
	{
		const TNBeachLoot::FSearchPoint& Point = SearchPoints[i];
		double Nearest = TNumericLimits<double>::Max();
		for (const FVector& Turtle : Turtles)
		{
			Nearest = FMath::Min(Nearest, TNBeachLootDetail::RimDistance(Point, Turtle));
		}
		ATN_BeachSearchSpot* Spot = SearchProxies[i].Get();
		if (Spot)
		{
			// Rebuscado: ya no vuelve a salir (su actor se queda mientras quede en el suelo lo que soltó).
			if (Spot->IsSearched())
			{
				MarkSearchUsed(i);
			}
			if (Nearest > TNBeachLoot::ProxyReleaseDistance && !Spot->IsBeingSearched() && !(Spot->IsSearched() && Spot->HasLootLying()))
			{
				Spot->Destroy();
				SearchProxies[i] = nullptr;
			}
			continue;
		}
		if (SearchUsed[i] || Nearest > TNBeachLoot::ProxySpawnDistance)
		{
			continue;
		}
		// Alguien se acerca: el rebuscable de siempre, pero en el montículo de su punto y con la huella de su base (#254): el
		// aviso, el alcance, las chispitas, el anillo y la tierra salen donde se ve que se rebusca, no por cualquier lado del
		// decorado entero.
		if (!SearchAnchors.IsValidIndex(i))
		{
			continue;
		}
		const TNBeachLoot::FSearchAnchor& Anchor = SearchAnchors[i];
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATN_BeachSearchSpot* NewSpot = World->SpawnActor<ATN_BeachSearchSpot>(ATN_BeachSearchSpot::StaticClass(),
			FTransform(FRotator::ZeroRotator, Anchor.Ground), Params);
		if (!NewSpot)
		{
			continue;
		}
		// Su montículo es el del mismo índice en el registro (BuildSearchRegistry los crea en el mismo orden); antes de
		// SetupSpot, que despierta la réplica: así el índice sale en el primer envío y el anillo se centra en él (#214).
		NewSpot->SetMoundIndex(i);
		NewSpot->SetupSpot(Anchor.Radius, 0.f, TNBeachLoot::MoundSpotHeight, TNBeachLoot::SandDust());
		SearchProxies[i] = NewSpot;
	}
}

void UTN_BeachLootSubsystem::ClearSearchAround(const FVector& WorldCenter, float Radius)
{
	int32 Cleared = 0;
	for (int32 i = 0; i < SearchPoints.Num(); ++i)
	{
		if (SearchUsed[i] || TNBeachLootDetail::RimDistance(SearchPoints[i], WorldCenter) > Radius)
		{
			continue;
		}
		if (ATN_BeachSearchSpot* Spot = SearchProxies[i].Get())
		{
			Spot->Destroy();
			SearchProxies[i] = nullptr;
		}
		MarkSearchUsed(i);
		++Cleared;
	}
	if (Cleared > 0)
	{
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] %d rebuscables quitados con su decorado (%.0f m alrededor)."), Cleared, Radius / 100.f);
	}
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

	// Una caja de objetos (el «?» de las carreras de karts) en la arena de P. Lo que da se sortea al cogerla, según el puesto
	// de quien la coge (ATN_RaceItemBox): los de atrás reciben lo que hace remontar y los de delante, lo defensivo.
	static_cast<void>(Catalog);
	auto SpawnAt = [&](const FVector2D& P)
	{
		const FVector Flat = GenXf.TransformPosition(FVector(P.X, P.Y, 0.0));
		const FVector Where(Flat.X, Flat.Y, Gen.GetGroundHeightAt(Flat) + 3.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATN_RaceItemBox* Box = World->SpawnActor<ATN_RaceItemBox>(ATN_RaceItemBox::StaticClass(), Where,
			FRotator(0.0, Rng.FRandRange(0.f, 360.f), 0.0), Params);
		if (!Box)
		{
			return false;
		}
		SpawnedItems.Add(Box);
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
