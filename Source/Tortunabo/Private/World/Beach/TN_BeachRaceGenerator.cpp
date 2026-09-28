// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachRaceGenerator — la playa del modo carrera: ciclo de vida, rondas (reparto,
// asientos y elementos), consultas de la salida, la meta y la zambullida, y el aviso de
// «tortuga en el agua». El terreno, el acantilado, el mar y los muros van en
// TN_BeachRaceGenerator_Build.cpp; la salida, la meta, la selva y las huellas, en
// TN_BeachRaceGenerator_Scenery.cpp. La lógica pura, en TN_BeachLayout.h.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachLoot.h"
#include "World/ProcMap/TN_ProcWaterActors.h"
#include "Core/TN_Log.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectGlobals.h"
#include "TN_BeachRaceKit.h"
#include "../ProcMap/TN_ProcMapAmbientFX.h"

namespace TNBeachRace
{
	TAutoConsoleVariable<int32> CVarShowFootprints(TEXT("TN.Beach.ShowFootprints"), 0,
		TEXT("1 = enseña en juego las huellas del reparto de la playa del modo carrera (ATN_BeachRaceGenerator)."));

	/** Segundos sin que nadie reparta antes de repartir solo: sin el GameMode de la carrera y con él (por si no lo hace). */
	constexpr float IdleSecondsAlone = 3.f;
	constexpr float IdleSecondsWithRaceMode = 20.f;
}

ATN_BeachRaceGenerator::ATN_BeachRaceGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// Colocado en el nivel: se replica solo la ronda (semilla y número); cada máquina construye el terreno igual.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(2.f);
	SetCanBeDamaged(false);

	BeachRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BeachRoot"));
	SetRootComponent(BeachRoot);

	// Mallas generadas en código (editor y ejecución) que no se guardan con el nivel: RF_Transient (el segundo parámetro de
	// CreateDefaultSubobject no basta) y sus punteros, Transient.
	auto MakeMesh = [this](const TCHAR* Name, bool bCollision, bool bShadow)
	{
		UProceduralMeshComponent* Comp = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		Comp->SetFlags(RF_Transient);
		Comp->SetupAttachment(BeachRoot);
		// Colisión cocinada al momento: el suelo tiene que estar antes de crear los elementos y de soltar a las tortugas.
		Comp->bUseAsyncCooking = false;
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetCastShadow(bShadow);
		if (bCollision)
		{
			Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		else
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return Comp;
	};
	// Sombra dinámica solo en lo que está en la playa (salida, acantilado, arco de meta, relieve fijo); las banderolas de
	// la ladera, las boyas, el mar y las tarimas, sin sombra (sombras virtuales más baratas). Las copas del bosquecillo
	// de la salida tampoco: con el sol casi cenital dejaban a oscuras la salida y los primeros cien metros de playa.
	CliffMesh = MakeMesh(TEXT("CliffMesh"), true, true);
	SeabedMesh = MakeMesh(TEXT("SeabedMesh"), true, false);
	SeaMesh = MakeMesh(TEXT("SeaMesh"), false, false);
	GroveSolidMesh = MakeMesh(TEXT("GroveSolidMesh"), true, true);
	GroveDecoMesh = MakeMesh(TEXT("GroveDecoMesh"), false, false);
	FinishSolidMesh = MakeMesh(TEXT("FinishSolidMesh"), true, true);
	FinishDecoMesh = MakeMesh(TEXT("FinishDecoMesh"), false, false);
	FloatMesh = MakeMesh(TEXT("FloatMesh"), false, false);
	FootprintMesh = MakeMesh(TEXT("FootprintMesh"), false, false);
	FootprintMesh->SetHiddenInGame(true);
	FeatureMesh = MakeMesh(TEXT("FeatureMesh"), true, true);
	FeatureDecoMesh = MakeMesh(TEXT("FeatureDecoMesh"), false, false);
	PoolMesh = MakeMesh(TEXT("PoolMesh"), false, false);
}

void ATN_BeachRaceGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachRaceGenerator, RoundNet);
}

ATN_BeachRaceGenerator* ATN_BeachRaceGenerator::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) { return nullptr; }
	TActorIterator<ATN_BeachRaceGenerator> It(World);
	return It ? *It : nullptr;
}

void ATN_BeachRaceGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildAll();
}

void ATN_BeachRaceGenerator::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	// Al cargar el nivel (editor) o al duplicarlo para jugar, las mallas transitorias llegan vacías: se rehacen.
	if (!IsTemplate() && GetWorld()) { BuildAll(); }
}

void ATN_BeachRaceGenerator::BeginPlay()
{
	Super::BeginPlay();
	BuildAll();
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) { return; }
	SpawnWaterVolume();
	if (FootprintMesh) { FootprintMesh->SetHiddenInGame(!ShouldShowFootprints()); }
	if (GetNetMode() != NM_DedicatedServer) { StartLiving(); }
	IdleTime = 0.f;
	SetActorTickEnabled(true);
}

void ATN_BeachRaceGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLiving();
	// Al cambiar de nivel o salir, el mundo se lleva todo; solo si se destruye el generador se quita lo suyo.
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		if (HasAuthority()) { DestroyRoundElements(); }
		if (ATN_ProcWaterVolume* Water = WaterVolume.Get()) { Water->Destroy(); }
	}
	RoundElements.Reset();
	WaterVolume.Reset();
	Finishers.Reset();
	WetTurtles.Reset();
	Super::EndPlay(EndPlayReason);
}

void ATN_BeachRaceGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	TickAutoGenerate(Dt);
	TickTurtles(Dt);
	if (bEggsAnimating) { bEggsAnimating = UpdateStartEggs(); }
	if (bLiving)
	{
		TNAmbientFX::TickOwner(this, Dt);
		// Las boyas de meta se mecen (solo arriba y abajo: la línea entera está a 1,2 km del origen).
		FloatClock += Dt;
		if (FloatMesh) { FloatMesh->SetRelativeLocation(FVector(0.0, 0.0, 35.0 * FMath::Sin(FloatClock * 0.9f))); }
		if (FootprintMesh)
		{
			const bool bShow = ShouldShowFootprints();
			if (bShow && FootprintMesh->GetNumSections() == 0 && Layout.Items.Num() > 0) { BuildFootprints(); }
			FootprintMesh->SetHiddenInGame(!bShow);
		}
	}
}

bool ATN_BeachRaceGenerator::ShouldShowFootprints() const
{
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld()) { return TNBeachRace::CVarShowFootprints.GetValueOnGameThread() != 0; }
	return bShowFootprints;
}

// ─────────────────────────────────────────────────────────────────────────────
// Rondas
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::GenerateRound(int32 InSeed)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority()) { return; }
	const double T0 = FPlatformTime::Seconds();
	BuildAll();
	DestroyRoundElements();
	TNBeachLayout::FRoundLayout NewLayout;
	TNBeachLayout::GenerateRound(InSeed, NewLayout);
	const double T1 = FPlatformTime::Seconds();
	ApplyLayoutLocal(NewLayout);
	const double T2 = FPlatformTime::Seconds();
	RoundNet.Seed = InSeed;
	RoundNet.Round += 1;
	RoundNet.bCleared = false;
	// Huevos cerrados otra vez y en la salida: las tortugas de la ronda nueva aparecen dentro (el sprint final los lleva a
	// su línea después, con SetStartEggsAtSprint).
	RoundNet.bStartOpen = false;
	RoundNet.StartOpenTime = 0.f;
	RoundNet.bSprintEggs = false;
	ApplyStartEggs(false);
	AppliedRound = RoundNet.Round;
	const FString Missing = SpawnRoundElements();
	TNBeachLoot::SpawnRoundLoot(*this);
	Finishers.Reset();
	WetTurtles.Reset();
	bRoundReady = true;
	IdleTime = 0.f;
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda %d: %s · %d elementos creados%s · reparto %.0f ms, asientos %.0f ms, total %.0f ms."), RoundNet.Round,
		*Layout.Summary(), RoundElements.Num(), Missing.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (sin clase todavía: %s)"), *Missing),
		(T1 - T0) * 1000.0, (T2 - T1) * 1000.0, (FPlatformTime::Seconds() - T0) * 1000.0);
	if (!Layout.bPassageOk)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Playa] ronda %d: el paso libre de %.0f m no llega de la salida al borde."), RoundNet.Round, TNBeachLayout::MinPassage / 100.0);
	}
	OnRoundLayoutReady.Broadcast(this);
}

void ATN_BeachRaceGenerator::ClearRound()
{
	if (!HasAuthority()) { return; }
	DestroyRoundElements();
	ApplyLayoutLocal(TNBeachLayout::FRoundLayout());
	RoundNet.Round += 1;
	RoundNet.bCleared = true;
	RoundNet.bStartOpen = false;
	RoundNet.bSprintEggs = false;
	ApplyStartEggs(false);
	AppliedRound = RoundNet.Round;
	bRoundReady = false;
	Finishers.Reset();
	ForceNetUpdate();
}

bool ATN_BeachRaceGenerator::IsRoundReady() const
{
	return bBuilt && RoundNet.Round > 0 && !RoundNet.bCleared && AppliedRound == RoundNet.Round && (!HasAuthority() || bRoundReady);
}

void ATN_BeachRaceGenerator::OnRep_RoundNet()
{
	BuildAll();
	if (RoundNet.Round != AppliedRound)
	{
		const double T0 = FPlatformTime::Seconds();
		TNBeachLayout::FRoundLayout NewLayout;
		if (RoundNet.Round > 0 && !RoundNet.bCleared) { TNBeachLayout::GenerateRound(RoundNet.Seed, NewLayout); }
		ApplyLayoutLocal(NewLayout);
		AppliedRound = RoundNet.Round;
		WetTurtles.Reset();
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda %d recibida: %d asientos en la arena · %.0f ms."), RoundNet.Round, Layout.Stamps.Num(),
			(FPlatformTime::Seconds() - T0) * 1000.0);
		if (RoundNet.Round > 0 && !RoundNet.bCleared) { OnRoundLayoutReady.Broadcast(this); }
	}
	// Huevos (en su línea, la salida o la del sprint): se rompen con el salto si llega a tiempo (el servidor lanzó hace
	// poco); si no, ya rotos.
	if (RoundNet.bStartOpen != bEggsOpenLocal || IsStartEggLineStale())
	{
		const UWorld* World = GetWorld();
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		const double Since = GS ? GS->GetServerWorldTimeSeconds() - static_cast<double>(RoundNet.StartOpenTime) : 0.0;
		ApplyStartEggs(RoundNet.bStartOpen && Since < 1.5);
	}
}

void ATN_BeachRaceGenerator::ApplyLayoutLocal(const TNBeachLayout::FRoundLayout& NewLayout)
{
	const TArray<TNBeachLayout::FStamp> OldStamps = Layout.Stamps;
	Layout = NewLayout;
	if (bBuilt && TilesX > 0 && TilesY > 0)
	{
		// Solo las teselas que tocan un asiento nuevo o uno de la ronda anterior (para quitarlo), con dos filas de margen para
		// que las normales de los bordes cuadren.
		const int32 Quads = TNBeachRaceKit::TerrainTileQuads;
		const double Margin = 700.0;
		TArray<int32> Touched;
		for (int32 Ty = 0; Ty < TilesY; ++Ty)
		{
			for (int32 Tx = 0; Tx < TilesX; ++Tx)
			{
				const int32 I0 = Tx * Quads;
				const int32 I1 = FMath::Min(I0 + Quads, GridXs.Num() - 1);
				const int32 J0 = Ty * Quads;
				const int32 J1 = FMath::Min(J0 + Quads, GridYs.Num() - 1);
				bool bTouch = false;
				const TArray<TNBeachLayout::FStamp>* Lists[2] = { &OldStamps, &Layout.Stamps };
				for (int32 l = 0; l < 2 && !bTouch; ++l)
				{
					for (const TNBeachLayout::FStamp& Stamp : *Lists[l])
					{
						const double R = Stamp.Radius + Stamp.Blend + Margin;
						bTouch |= FMath::Max(Stamp.A.X, Stamp.B.X) + R >= GridXs[I0] && FMath::Min(Stamp.A.X, Stamp.B.X) - R <= GridXs[I1]
							&& FMath::Max(Stamp.A.Y, Stamp.B.Y) + R >= GridYs[J0] && FMath::Min(Stamp.A.Y, Stamp.B.Y) - R <= GridYs[J1];
					}
				}
				if (bTouch) { Touched.Add(Ty * TilesX + Tx); }
			}
		}
		// Casi toda la playa se toca en cada ronda: las alturas, en paralelo; la subida y la colisión, en este hilo.
		BuildTerrainTiles(Touched, Layout.Stamps);
	}
	BuildFootprints();
}

FString ATN_BeachRaceGenerator::SpawnRoundElements()
{
	UWorld* World = GetWorld();
	if (!World) { return FString(); }
	const bool bEditorPreview = !World->IsGameWorld();
	const FTransform Xf = GetActorTransform();
	// Las clases que faltan (otro agente aún no las ha escrito) se cuentan una vez por clase, sin crear nada.
	TMap<FString, int32> MissingByClass;
	TArray<int8> HasClass;
	HasClass.Init(-1, static_cast<int32>(ETNBeachElement::Count));
	for (const TNBeachLayout::FItem& Item : Layout.Items)
	{
		const int32 Kind = static_cast<int32>(Item.Element);
		const TCHAR* ClassName = TNBeach::ClassNameOf(Item.Element);
		if (HasClass[Kind] < 0)
		{
			const UClass* Class = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Tortunabo.%s"), ClassName));
			HasClass[Kind] = static_cast<int8>(Class && Class->IsChildOf(ATN_BeachElement::StaticClass()) ? 1 : 0);
		}
		if (HasClass[Kind] == 0)
		{
			++MissingByClass.FindOrAdd(ClassName);
			continue;
		}
		// A la cota de su asiento (la arena natural de su centro, con las pozas y las trincheras).
		const FVector Local(Item.Pos.X, Item.Pos.Y, TNBeachLayout::PlacementZ(Item));
		const FTransform ElementXf = FTransform(FRotator(0.0, Item.Yaw, 0.0), Local) * Xf;
		ATN_BeachElement* Element = ATN_BeachElement::SpawnElement(World, ElementXf, Item.Spec);
		if (!Element) { continue; }
		// La playa mide 1,2 km: todo se ve desde lejos (la distancia de corte por defecto es de 150 m).
		Element->bAlwaysRelevant = true;
		if (bEditorPreview)
		{
			// Ronda de prueba del editor: no se guarda con el nivel y se construye ya (en el editor no hay BeginPlay).
			Element->SetFlags(RF_Transient);
			if (UFunction* Build = Element->FindFunction(TEXT("OnRep_Spec"))) { Element->ProcessEvent(Build, nullptr); }
		}
		RoundElements.Add(Element);
	}
	FString Missing;
	for (const TPair<FString, int32>& Entry : MissingByClass)
	{
		Missing += FString::Printf(TEXT("%s%s x%d"), Missing.IsEmpty() ? TEXT("") : TEXT(", "), *Entry.Key, Entry.Value);
	}
	return Missing;
}

void ATN_BeachRaceGenerator::DestroyRoundElements()
{
	for (ATN_BeachElement* Element : RoundElements)
	{
		if (IsValid(Element)) { Element->Destroy(); }
	}
	RoundElements.Reset();
}

void ATN_BeachRaceGenerator::PreviewRound()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	if (World->IsGameWorld())
	{
		if (HasAuthority()) { GenerateRound(bEditorRandomSeed ? FMath::Rand() : EditorSeed); }
		return;
	}
	BuildAll();
	const int32 Seed = bEditorRandomSeed ? FMath::Rand() : EditorSeed;
	DestroyRoundElements();
	TNBeachLayout::FRoundLayout NewLayout;
	TNBeachLayout::GenerateRound(Seed, NewLayout);
	ApplyLayoutLocal(NewLayout);
	const FString Missing = SpawnRoundElements();
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda de prueba del editor: %s · %d elementos creados%s."), *Layout.Summary(), RoundElements.Num(),
		Missing.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (sin clase todavía: %s)"), *Missing));
	OnRoundLayoutReady.Broadcast(this);
}

void ATN_BeachRaceGenerator::ClearPreview()
{
	DestroyRoundElements();
	ApplyLayoutLocal(TNBeachLayout::FRoundLayout());
}

void ATN_BeachRaceGenerator::TickAutoGenerate(float DeltaSeconds)
{
	if (!bAutoGenerateIfIdle || !HasAuthority() || RoundNet.Round > 0) { return; }
	IdleTime += DeltaSeconds;
	if (IdleTime < TNBeachRace::IdleSecondsAlone) { return; }
	const UWorld* World = GetWorld();
	const AGameModeBase* GM = World ? World->GetAuthGameMode() : nullptr;
	const UClass* RaceMode = FindObject<UClass>(nullptr, TEXT("/Script/Tortunabo.TN_BeachRaceGameMode"));
	const bool bRaceMode = GM && RaceMode && GM->IsA(RaceMode);
	if (bRaceMode && IdleTime < TNBeachRace::IdleSecondsWithRaceMode) { return; }
	if (bRaceMode)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Playa] El GameMode de la carrera no ha repartido en %.0f s: reparto una ronda al azar."), IdleTime);
	}
	GenerateRound(FMath::Rand());
}

// ─────────────────────────────────────────────────────────────────────────────
// Meta: tortugas en el agua
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::TickTurtles(float /*DeltaSeconds*/)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS) { return; }
	const bool bServer = HasAuthority();
	const bool bArmed = RoundNet.Round > 0 && !RoundNet.bCleared;
	for (APlayerState* PS : GS->PlayerArray)
	{
		ACharacter* Turtle = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
		if (!Turtle) { continue; }
		const UCapsuleComponent* Capsule = Turtle->GetCapsuleComponent();
		const double HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 50.0;
		const FVector Feet = Turtle->GetActorLocation() - FVector(0.0, 0.0, HalfHeight);
		const bool bWet = IsFinishWater(Feet);
		if (bLiving)
		{
			// Chapuzón al entrar en el agua de meta o en una poza.
			const FVector Local = GetActorTransform().InverseTransformPosition(Feet);
			const int32 PoolIndex = TNBeachLayout::PoolAt(FVector2D(Local.X, Local.Y), 1.0);
			const bool bInPool = PoolIndex != INDEX_NONE && Local.Z <= TNBeachLayout::Pools()[PoolIndex].Water + 30.0;
			const bool bAnyWet = bWet || bInPool;
			bool& bWasWet = WetTurtles.FindOrAdd(Turtle);
			if (bAnyWet && !bWasWet)
			{
				const double Surface = bInPool ? TNBeachLayout::Pools()[PoolIndex].Water : TNBeachLayout::WaterZ;
				Splash(GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, Surface)));
			}
			bWasWet = bAnyWet;
		}
		if (bServer && bArmed && bWet && !Finishers.Contains(Turtle))
		{
			Finishers.Add(Turtle);
			UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda %d: %s ha tocado el agua de meta."), RoundNet.Round, *PS->GetPlayerName());
			OnTurtleReachedWater.Broadcast(Turtle);
			OnTurtleReachedWaterNative.Broadcast(Turtle);
		}
	}
}

void ATN_BeachRaceGenerator::ResetFinishWater()
{
	if (HasAuthority()) { Finishers.Reset(); }
}

// ─────────────────────────────────────────────────────────────────────────────
// Consultas
// ─────────────────────────────────────────────────────────────────────────────

FTransform ATN_BeachRaceGenerator::GetStartTransform(int32 PlayerIndex) const
{
	const FVector Local = TNBeachLayout::StartSpot(PlayerIndex) + FVector(0.0, 0.0, 110.0);
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(Local));
}

FTransform ATN_BeachRaceGenerator::GetSprintStartTransform(int32 Index) const
{
	const FVector Spot = TNBeachLayout::SprintSpot(Index);
	const double Ground = TNBeachLayout::StampedZ(Layout.Stamps, Spot.X, Spot.Y, TNBeachLayout::SurfaceZ(Spot.X, Spot.Y));
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(FVector(Spot.X, Spot.Y, Ground + 110.0)));
}

int32 ATN_BeachRaceGenerator::ClearElementsAround(const FVector& WorldCenter, float Radius)
{
	if (!HasAuthority()) { return 0; }
	const FTransform& Xf = GetActorTransform();
	const FVector LocalCenter = Xf.InverseTransformPosition(WorldCenter);
	const FVector2D Center(LocalCenter.X, LocalCenter.Y);
	int32 Removed = 0;
	for (int32 i = RoundElements.Num() - 1; i >= 0; --i)
	{
		ATN_BeachElement* Element = RoundElements[i];
		if (!IsValid(Element))
		{
			RoundElements.RemoveAt(i);
			continue;
		}
		// La huella: un disco o, en los alargados (Extent > 0 en el reparto), una cápsula a lo largo de su X.
		const FVector Local = Xf.InverseTransformPosition(Element->GetActorLocation());
		const FVector LocalAxis = Xf.InverseTransformVectorNoScale(Element->GetActorForwardVector());
		const FVector2D Axis = FVector2D(LocalAxis.X, LocalAxis.Y).GetSafeNormal();
		const double Half = 0.5 * FMath::Max(0.0, static_cast<double>(Element->GetSpec().Extent));
		const FVector2D P(Local.X, Local.Y);
		double T = 0.0;
		const double Dist = TNProcMap::DistPointSegment(Center, P - Axis * Half, P + Axis * Half, T);
		if (Dist > static_cast<double>(Radius) + Element->GetFootprintRadius()) { continue; }
		Element->Destroy();
		RoundElements.RemoveAt(i);
		++Removed;
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda %d: %d elementos quitados en %.0f m alrededor de (%.0f, %.0f) m."), RoundNet.Round, Removed, Radius / 100.f,
		Center.X / 100.0, Center.Y / 100.0);
	return Removed;
}

bool ATN_BeachRaceGenerator::IsFinishWater(const FVector& WorldLocation) const
{
	return TNBeachLayout::IsFinishWaterLocal(GetActorTransform().InverseTransformPosition(WorldLocation));
}

bool ATN_BeachRaceGenerator::IsCliffJumpZone(const FVector& WorldLocation) const
{
	return TNBeachLayout::IsCliffJumpZoneLocal(GetActorTransform().InverseTransformPosition(WorldLocation));
}

float ATN_BeachRaceGenerator::GetCliffEdgeDistance(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	return static_cast<float>(Local.X - TNBeachLayout::EdgeX(Local.Y));
}

float ATN_BeachRaceGenerator::GetCourseProgress(const FVector& WorldLocation) const
{
	return static_cast<float>(TNBeachLayout::CourseProgress(GetActorTransform().InverseTransformPosition(WorldLocation)));
}

float ATN_BeachRaceGenerator::GetGroundHeightAt(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const double Z = TNBeachLayout::StampedZ(Layout.Stamps, Local.X, Local.Y, TNBeachLayout::SurfaceZ(Local.X, Local.Y));
	return static_cast<float>(GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, Z)).Z);
}
