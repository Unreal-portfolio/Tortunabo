#include "World/Beach/TN_BeachStorm.h"
#include "World/Beach/TN_BeachCameraShake.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/Beach/TN_BeachEnemySynth.h"
#include "World/Beach/TN_BeachStun.h"
#include "World/ProcMap/TN_PathStormFX.h"
#include "World/ProcMap/TN_StormCough.h"
#include "TN_BeachEnemyKit.h"
#include "TN_BeachEnemyMeshes.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TN_Log.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"
#include "Player/TortugaCharacter.h"

namespace TNBeachStormTuning
{
	/** Trastos volando a la vez (alrededor de la cámara) y bañistas a lo ancho. */
	constexpr int32 NumDebris = 18;
	constexpr int32 NumBathers = 8;
	/** El frente se ve y se simula si la cámara está a menos de esto (cm, a lo largo de la carrera). */
	constexpr float VisibleRange = 45000.f;
	/** Cada cuánto mira el servidor quién está dentro (s). */
	constexpr float CheckInterval = 0.2f;
	/** Segundos dentro para la peor tos (la tormenta de la playa no mata). */
	constexpr float WorstCoughSeconds = 12.f;
	/** Altura de la cadera de un bañista y media separación de las piernas (cm reales). */
	constexpr double HipHeight = 96.0;
	constexpr double HipHalfWidth = 10.0;

	inline FLinearColor SandVeil()
	{
		return FLinearColor(0.78f, 0.66f, 0.48f, 1.f);
	}

	inline FLinearColor SandFog()
	{
		return FLinearColor(0.62f, 0.5f, 0.34f, 1.f);
	}
}

ATN_BeachStorm::ATN_BeachStorm()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(2.f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	InsidePostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("InsidePostProcess"));
	InsidePostProcess->SetupAttachment(Root);
	InsidePostProcess->bUnbound = true;
	InsidePostProcess->bEnabled = false;
}

void ATN_BeachStorm::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachStorm, StartOffset);
	DOREPLIFETIME(ATN_BeachStorm, FrontSpeed);
	DOREPLIFETIME(ATN_BeachStorm, FrontAccel);
	DOREPLIFETIME(ATN_BeachStorm, StartServerTime);
	DOREPLIFETIME(ATN_BeachStorm, Grace);
	DOREPLIFETIME(ATN_BeachStorm, FrozenFront);
	DOREPLIFETIME(ATN_BeachStorm, bActive);
	DOREPLIFETIME(ATN_BeachStorm, bShown);
}

void ATN_BeachStorm::BeginPlay()
{
	Super::BeginPlay();
	Rng.GenerateNewSeed();
	FrontGroundZ = static_cast<float>(GetActorLocation().Z);
}

void ATN_BeachStorm::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreFog();
	Super::EndPlay(EndPlayReason);
}

ATN_BeachStorm* ATN_BeachStorm::FindStorm(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ATN_BeachStorm> It(World);
	return It ? *It : nullptr;
}

void ATN_BeachStorm::StartStorm()
{
	StartStormAt(0.f, DefaultSpeed, DefaultGrace);
}

void ATN_BeachStorm::StartStormAt(float InStartOffset, float Speed, float GraceSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	StartOffset = InStartOffset;
	FrontSpeed = FMath::Max(0.f, Speed);
	FrontAccel = FMath::Max(0.f, SpeedRampPerMinute) / 60.f;
	Grace = FMath::Max(0.f, GraceSeconds);
	StartServerTime = static_cast<float>(ATN_BeachEnemy::ServerNow(this));
	FrozenFront = StartOffset;
	bActive = true;
	bShown = true;
	LastHit.Reset();
	CheckTimer = 0.f;
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] Tormenta de bañistas: frente a %.0f cm del actor, %.0f cm/s (+%.0f por minuto), gracia %.1f s."),
		StartOffset, FrontSpeed, SpeedRampPerMinute, Grace);
}

void ATN_BeachStorm::StopStorm()
{
	if (!HasAuthority())
	{
		return;
	}
	FrozenFront = GetFrontDistance();
	bActive = false;
	LastHit.Reset();
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] Tormenta de bañistas parada en %.0f cm."), FrozenFront);
}

float ATN_BeachStorm::GetFrontDistance() const
{
	if (!bActive)
	{
		return FrozenFront;
	}
	const double T = FMath::Max(0.0, ATN_BeachEnemy::ServerNow(this) - static_cast<double>(StartServerTime) - static_cast<double>(Grace));
	return StartOffset + static_cast<float>(FrontSpeed * T + 0.5 * FrontAccel * T * T);
}

FVector ATN_BeachStorm::GetFrontLocation() const
{
	return GetActorTransform().TransformPositionNoScale(FVector(GetFrontDistance(), 0.0, 0.0));
}

bool ATN_BeachStorm::IsLocationInside(const FVector& WorldLocation) const
{
	if (!bActive)
	{
		return false;
	}
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	return Local.X < GetFrontDistance() - InsideMargin && FMath::Abs(Local.Y) < HalfWidth;
}

void ATN_BeachStorm::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && bActive)
	{
		CheckTimer += DeltaSeconds;
		if (CheckTimer >= TNBeachStormTuning::CheckInterval)
		{
			CheckTimer = 0.f;
			ServerCheck();
		}
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickFX(DeltaSeconds);
		TickCough(DeltaSeconds);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Servidor
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachStorm::ServerCheck()
{
	TArray<ATortugaCharacter*> Turtles;
	ATN_BeachEnemy::GatherTurtles(this, Turtles);
	const double WorldNow = GetWorld()->GetTimeSeconds();
	const FTransform Xf = GetActorTransform();
	for (ATortugaCharacter* Turtle : Turtles)
	{
		if (!IsLocationInside(Turtle->GetActorLocation()) || TNBeach::IsTurtleStunned(Turtle))
		{
			continue;
		}
		if (const double* Last = LastHit.Find(Turtle))
		{
			if (WorldNow - *Last < HitInterval)
			{
				continue;
			}
		}
		LastHit.Add(Turtle, WorldNow);
		// Revolcón: la bola sale lanzada hacia delante (hacia el mar), algo hacia arriba y de lado al azar.
		const FVector LocalLaunch(PushForward * Rng.FRandRange(0.85f, 1.15f), PushSide * Rng.FRandRange(-1.f, 1.f), 0.0);
		const FVector Launch = Xf.TransformVectorNoScale(LocalLaunch) + FVector(0.0, 0.0, PushUp * Rng.FRandRange(0.8f, 1.2f));
		TNBeach::StunTurtle(Turtle, StunSeconds, Launch);
		MulticastTumble(Turtle);
	}
	for (auto It = LastHit.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ATN_BeachStorm::MulticastTumble_Implementation(ATortugaCharacter* Victim)
{
	if (GetNetMode() == NM_DedicatedServer || !Victim)
	{
		return;
	}
	const FVector At = Victim->GetActorLocation();
	if (Voice)
	{
		Voice->SetWorldLocation(At);
		Voice->Play(ETNBeachSfx::Swoop, 0.7f, 1.2f);
	}
	UTN_BeachCameraShake::Kick(this, At, 0.6f, 300.f, 2000.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Efectos
// ─────────────────────────────────────────────────────────────────────────────

float ATN_BeachStorm::Rand01()
{
	FxRng ^= FxRng << 13;
	FxRng ^= FxRng >> 17;
	FxRng ^= FxRng << 5;
	return static_cast<float>(FxRng & 0xFFFFFF) / 16777215.f;
}

void ATN_BeachStorm::SetupFX()
{
	bFXReady = true;
	using TNProcMesh::FTNProcMeshBuffers;
	using TNBeachMeshes::EStormItem;

	// Velo del frente: las láminas de la tormenta del camino, color arena y tan anchas como la playa.
	FTNProcMeshBuffers VeilBuffers;
	TNStormFX::BuildVeil(VeilBuffers, TNBeachStormTuning::SandVeil(), HalfWidth * 2.0 + 8000.0, 6500.0, 3, 23u);
	UMaterialInterface* VeilMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcStormVeil.M_ProcStormVeil"), nullptr, LOAD_NoWarn);
	if (!VeilMat)
	{
		VeilMat = TNBeachKit::SoftMaterial();
	}
	UStaticMesh* VeilMesh = TNProcRuntimeMesh::MakeStaticMesh(this, VeilBuffers, VeilMat, false, 0.f, 1.f, -2.f);
	Veil = TNBeachKit::AddPart(this, Root, VeilMesh, FVector::ZeroVector, false);
	if (Veil)
	{
		Veil->SetAbsolute(true, true, true);
		Veil->SetVisibility(false);
	}

	// Trastos que vuelan: sombrillas, cubos, sillas, toallas, flotadores, palas, chanclas y pelotas.
	const int32 NumItems = static_cast<int32>(EStormItem::Count);
	for (int32 i = 0; i < TNBeachStormTuning::NumDebris; ++i)
	{
		FDebris& D = Debris.AddDefaulted_GetRef();
		D.Item = i % NumItems;
		const int32 Variant = (i * 3 + 1) % 4;
		const EStormItem Item = static_cast<EStormItem>(D.Item);
		UStaticMesh* Mesh = TNBeachKit::CachedMesh(FString::Printf(TEXT("Beach.Storm.%d.%d"), D.Item, Variant),
			[Item, Variant](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildStormItem(M, Item, Variant); });
		const bool bBig = Item == EStormItem::Umbrella || Item == EStormItem::Chair || Item == EStormItem::Towel;
		UStaticMeshComponent* Comp = TNBeachKit::AddPart(this, Root, Mesh, FVector::ZeroVector, bBig);
		if (Comp)
		{
			Comp->SetAbsolute(true, true, true);
			Comp->SetVisibility(false);
		}
		DebrisComps.Add(Comp);
	}

	// Bañistas: caderas y dos piernas cada uno, repartidos a lo ancho.
	const double S = TNBeach::Scale;
	const double HipZ = TNBeachStormTuning::HipHeight * S;
	for (int32 b = 0; b < TNBeachStormTuning::NumBathers; ++b)
	{
		FBather& B = Bathers.AddDefaulted_GetRef();
		const float Spread = static_cast<float>(b) / static_cast<float>(FMath::Max(1, TNBeachStormTuning::NumBathers - 1));
		B.SlotY = FMath::Lerp(-0.85f, 0.85f, Spread) * HalfWidth + 1500.f * (TNBeachKit::Hash01(b * 13u + 1u) - 0.5f);
		B.Depth = 2600.f + 2600.f * TNBeachKit::Hash01(b * 29u + 7u);
		B.Rate = 0.45f + 0.15f * TNBeachKit::Hash01(b * 31u + 3u);
		B.Phase = TNBeachKit::Hash01(b * 37u + 11u);
		B.Scale = 0.9f + 0.2f * TNBeachKit::Hash01(b * 41u + 5u);
		const TNBeachMeshes::FBatherLook Look = TNBeachMeshes::BatherPalette(b);
		UStaticMesh* HipsMesh = TNBeachKit::CachedMesh(FString::Printf(TEXT("Beach.Bather.%d.Hips"), b % 8), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildBatherHips(M, Look); });
		UStaticMesh* LegMesh = TNBeachKit::CachedMesh(FString::Printf(TEXT("Beach.Bather.%d.Leg"), b % 8), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildBatherLeg(M, Look); });
		USceneComponent* BatherRoot = NewObject<USceneComponent>(this, NAME_None, RF_Transient);
		BatherRoot->SetupAttachment(Root);
		BatherRoot->SetAbsolute(true, true, true);
		BatherRoot->RegisterComponent();
		BatherRoots.Add(BatherRoot);
		BatherParts.Add(TNBeachKit::AddPart(this, BatherRoot, HipsMesh, FVector(0.0, 0.0, HipZ), true));
		BatherParts.Add(TNBeachKit::AddPart(this, BatherRoot, LegMesh, FVector(0.0, -TNBeachStormTuning::HipHalfWidth * S, HipZ), true));
		BatherParts.Add(TNBeachKit::AddPart(this, BatherRoot, LegMesh, FVector(0.0, TNBeachStormTuning::HipHalfWidth * S, HipZ), true));
		BatherRoot->SetVisibility(false, true);
	}

	// Arena y polvo en el frente y, dentro, alrededor de la cámara.
	using TNAmbientFX::EShape;
	auto MakeEmitter = [this](TNAmbientFX::FEmitter& E, EShape Shape, const FLinearColor& Color, bool bSoft, float Alpha, int32 MaxCount, float BaseRate,
		float BaseSpeed, float Fall, float Rise, float LifeMin, float LifeMax, float SizeStart, float SizeEnd, float Radius, float Height, uint32 EmitterSeed)
	{
		TNAmbientFX::FEmitterDesc D = TNBeachKit::MakeDesc(Shape, Color, bSoft, Alpha, MaxCount, BaseRate, BaseSpeed, Fall, LifeMin, LifeMax, SizeStart, SizeEnd);
		D.Buoyancy = Rise;
		D.SpawnRadius = Radius;
		D.SpawnHeight = Height;
		D.Spread = 0.55f;
		D.Drag = 0.35f;
		D.Direction = FVector::ForwardVector;
		D.WakeDistance = 1.0e7f;
		TNBeachKit::InitEmitter(E, this, D, EmitterSeed);
	};
	MakeEmitter(FrontSand, EShape::Streak, FLinearColor(0.86f, 0.7f, 0.46f), true, 0.8f, 200, 160.f, 2400.f, -60.f, 0.f, 0.6f, 1.3f, 60.f, 40.f, 3000.f, 2400.f, 61u);
	MakeEmitter(FrontDust, EShape::Puff, FLinearColor(0.8f, 0.68f, 0.5f), true, 0.45f, 70, 26.f, 700.f, 0.f, 30.f, 2.f, 4.f, 400.f, 900.f, 3200.f, 2500.f, 62u);
	MakeEmitter(FrontClouds, EShape::Puff, FLinearColor(0.72f, 0.6f, 0.44f), true, 0.7f, 70, 22.f, 420.f, 0.f, 35.f, 3.f, 5.5f, 700.f, 1500.f, 3500.f, 900.f, 63u);
	MakeEmitter(ViewSand, EShape::Streak, FLinearColor(0.86f, 0.72f, 0.5f), true, 0.8f, 160, 140.f, 2000.f, -60.f, 0.f, 0.5f, 1.1f, 55.f, 40.f, 1500.f, 900.f, 64u);
	MakeEmitter(ViewDust, EShape::Puff, FLinearColor(0.8f, 0.68f, 0.5f), true, 0.4f, 50, 18.f, 500.f, 0.f, 20.f, 2.f, 3.5f, 300.f, 700.f, 1500.f, 900.f, 65u);

	Voice = UTN_BeachEnemySynthComponent::AttachTo(this, Root, 6000.f, 22000.f);

	// Niebla del nivel (la primera); si no hay, una propia apagada.
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		if (UExponentialHeightFogComponent* C = It->GetComponent())
		{
			Fog = C;
			break;
		}
	}
	if (!Fog.IsValid())
	{
		UExponentialHeightFogComponent* Own = NewObject<UExponentialHeightFogComponent>(this, NAME_None, RF_Transient);
		Own->SetupAttachment(Root);
		Own->SetFogDensity(0.f);
		Own->RegisterComponent();
		Fog = Own;
	}
}

void ATN_BeachStorm::TickFX(float DeltaSeconds)
{
	if (!bFXReady)
	{
		SetupFX();
	}
	FXTime += DeltaSeconds;
	UWorld* World = GetWorld();
	const FTransform Xf = GetActorTransform();
	FVector View = GetActorLocation();
	FVector ViewFwd = Xf.GetUnitAxis(EAxis::X);
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		View = PC->PlayerCameraManager->GetCameraLocation();
		ViewFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	}
	const APawn* LocalPawn = PC ? PC->GetPawn() : nullptr;
	const FVector LocalAt = LocalPawn ? LocalPawn->GetActorLocation() : View;
	const FVector ViewLocal = Xf.InverseTransformPositionNoScale(View);

	const float Front = GetFrontDistance();
	const bool bNearFront = bShown && FMath::Abs(static_cast<float>(ViewLocal.X) - Front) < TNBeachStormTuning::VisibleRange;
	FrontBlend = FMath::FInterpConstantTo(FrontBlend, bNearFront ? 1.f : 0.f, DeltaSeconds, 0.5f);
	const bool bLocalInside = IsLocationInside(LocalAt);
	InsideBlend = FMath::FInterpConstantTo(InsideBlend, bLocalInside ? 1.f : 0.f, DeltaSeconds, 0.7f);

	// Suelo del frente donde está la cámara (la playa baja hacia el mar).
	const float NearY = FMath::Clamp(static_cast<float>(ViewLocal.Y), -HalfWidth, HalfWidth);
	FVector FrontPoint = Xf.TransformPositionNoScale(FVector(Front, NearY, 0.0));
	GroundTimer -= DeltaSeconds;
	if (GroundTimer <= 0.f && bShown)
	{
		GroundTimer = 0.5f;
		float Z = FrontGroundZ;
		if (ATN_BeachEnemy::TraceGround(this, FVector(FrontPoint.X, FrontPoint.Y, FMath::Max(FrontPoint.Z, static_cast<double>(FrontGroundZ))), Z, nullptr, 6000.f, 12000.f))
		{
			FrontGroundZ = Z;
		}
	}
	FrontPoint.Z = FrontGroundZ;

	if (Veil)
	{
		const bool bShowVeil = FrontBlend > 0.01f;
		if (Veil->IsVisible() != bShowVeil)
		{
			Veil->SetVisibility(bShowVeil);
		}
		if (bShowVeil)
		{
			FVector At = Xf.TransformPositionNoScale(FVector(Front + 180.f * FMath::Sin(FXTime * 0.55f), 0.0, 0.0));
			At.Z = FrontGroundZ + 120.f * FMath::Sin(FXTime * 0.31f);
			Veil->SetWorldLocationAndRotation(At, FRotator(0.f, static_cast<float>(Xf.Rotator().Yaw), 0.f));
		}
	}

	// Partículas: en el frente donde mira la cámara y, dentro, alrededor de ella.
	const FVector Wind = Xf.TransformVectorNoScale(FVector(1.0, 0.0, 0.12)).GetSafeNormal();
	const FVector Back = Xf.GetUnitAxis(EAxis::X);
	const FVector Flat = FVector(ViewFwd.X, ViewFwd.Y, 0.0).GetSafeNormal();
	auto Run = [&](TNAmbientFX::FEmitter& E, const FVector& Origin, float Scale)
	{
		E.Origin = Origin;
		E.Desc.Direction = Wind;
		E.RateScale = Scale;
		TNBeachKit::TickEmitterIfBusy(E, DeltaSeconds, View);
	};
	Run(FrontSand, FrontPoint - Back * 600.0 - FVector(0.0, 0.0, 300.0), FrontBlend);
	Run(FrontDust, FrontPoint - Back * 600.0 - FVector(0.0, 0.0, 300.0), FrontBlend);
	Run(FrontClouds, FrontPoint - Back * 900.0 - FVector(0.0, 0.0, 300.0), FrontBlend);
	Run(ViewSand, View + Flat * 700.0 - FVector(0.0, 0.0, 350.0), InsideBlend);
	Run(ViewDust, View + Flat * 700.0 - FVector(0.0, 0.0, 350.0), InsideBlend);

	// Trastos y bañistas: solo con el frente a la vista.
	if (FrontBlend > 0.01f)
	{
		TickDebris(DeltaSeconds, Front, ViewLocal);
		TickBathers(Front, ViewLocal);
	}
	else
	{
		for (int32 i = 0; i < DebrisComps.Num(); ++i)
		{
			if (DebrisComps[i] && DebrisComps[i]->IsVisible())
			{
				DebrisComps[i]->SetVisibility(false);
			}
			if (Debris.IsValidIndex(i))
			{
				Debris[i].bLive = false;
			}
		}
		for (USceneComponent* BatherRoot : BatherRoots)
		{
			if (BatherRoot && BatherRoot->IsVisible())
			{
				BatherRoot->SetVisibility(false, true);
			}
		}
	}

	// Viento: fuerte en el frente y dentro. Y un temblor suave si el frente te pisa los talones.
	if (Voice)
	{
		Voice->SetWorldLocation(FrontPoint + FVector(0.0, 0.0, 500.0));
		const float Near = FMath::Clamp(1.f - FMath::Abs(static_cast<float>(ViewLocal.X) - Front) / 30000.f, 0.f, 1.f);
		Voice->SetWind(bShown ? FMath::Max(Near * (bActive ? 0.9f : 0.45f), InsideBlend) : 0.f);
	}
	if (bActive && LocalPawn)
	{
		const float Ahead = static_cast<float>(Xf.InverseTransformPositionNoScale(LocalAt).X) - Front;
		if (bLocalInside)
		{
			UTN_BeachCameraShake::Rumble(this, LocalAt, 0.35f, 100.f, 1000.f);
		}
		else if (Ahead > -InsideMargin && Ahead < 6000.f)
		{
			UTN_BeachCameraShake::Rumble(this, LocalAt, 0.22f * (1.f - Ahead / 6000.f), 100.f, 1000.f);
		}
	}
	ApplyInsideLook();
}

void ATN_BeachStorm::SpawnDebris(FDebris& D, float Front, const FVector& ViewLocal)
{
	using TNBeachMeshes::EStormItem;
	const FTransform Xf = GetActorTransform();
	const EStormItem Item = static_cast<EStormItem>(D.Item);
	const bool bBig = Item == EStormItem::Umbrella || Item == EStormItem::Chair || Item == EStormItem::Towel;
	const FVector Local(Front - FMath::Lerp(500.f, 7000.f, Rand01()),
		FMath::Clamp(static_cast<float>(ViewLocal.Y) + FMath::Lerp(-12000.f, 12000.f, Rand01()), -HalfWidth, HalfWidth), 0.0);
	D.Pos = Xf.TransformPositionNoScale(Local);
	D.Pos.Z = FrontGroundZ + FMath::Lerp(800.f, 6000.f, Rand01());
	D.Vel = Xf.TransformVectorNoScale(FVector(FrontSpeed + FMath::Lerp(200.f, 900.f, Rand01()), FMath::Lerp(-400.f, 400.f, Rand01()), 0.0))
		+ FVector(0.0, 0.0, FMath::Lerp(-150.f, 500.f, Rand01()));
	D.SpinAxis = FVector(Rand01() - 0.5f, Rand01() - 0.5f, Rand01() - 0.5f).GetSafeNormal();
	if (D.SpinAxis.IsNearlyZero())
	{
		D.SpinAxis = FVector::UpVector;
	}
	D.SpinRate = FMath::Lerp(60.f, 220.f, Rand01());
	D.Rot = FQuat(FVector(Rand01() - 0.5f, Rand01() - 0.5f, Rand01() + 0.1f).GetSafeNormal(), Rand01() * 2.f * PI);
	D.Scale = bBig ? FMath::Lerp(0.7f, 0.9f, Rand01()) : FMath::Lerp(0.9f, 1.1f, Rand01());
	D.bLive = true;
}

void ATN_BeachStorm::TickDebris(float DeltaSeconds, float Front, const FVector& ViewLocal)
{
	const FTransform Xf = GetActorTransform();
	const FVector Fwd = Xf.GetUnitAxis(EAxis::X);
	for (int32 i = 0; i < Debris.Num(); ++i)
	{
		FDebris& D = Debris[i];
		UStaticMeshComponent* Comp = DebrisComps.IsValidIndex(i) ? DebrisComps[i].Get() : nullptr;
		if (!Comp)
		{
			continue;
		}
		if (!D.bLive)
		{
			SpawnDebris(D, Front, ViewLocal);
		}
		// Arrastrados por el viento de la tormenta, cayendo poco y dando vueltas; rebotan en la arena.
		const double Along = FVector::DotProduct(D.Vel, Fwd);
		D.Vel += Fwd * ((FrontSpeed + 500.0 - Along) * 0.4 * DeltaSeconds);
		D.Vel.Z -= 250.0 * DeltaSeconds;
		D.Pos += D.Vel * DeltaSeconds;
		D.Rot = (FQuat(D.SpinAxis, FMath::DegreesToRadians(D.SpinRate) * DeltaSeconds) * D.Rot).GetNormalized();
		const double Floor = FrontGroundZ + 200.0 * D.Scale;
		if (D.Pos.Z < Floor)
		{
			D.Pos.Z = Floor;
			D.Vel.Z = FMath::Abs(D.Vel.Z) * 0.45 + 250.0;
			D.SpinRate *= 0.8f;
		}
		const FVector Local = Xf.InverseTransformPositionNoScale(D.Pos);
		if (Local.X > Front + 1500.0 || Local.X < Front - 9000.0 || FMath::Abs(Local.Y - ViewLocal.Y) > 16000.0)
		{
			D.bLive = false;
			continue;
		}
		Comp->SetWorldTransform(FTransform(D.Rot, D.Pos, FVector(D.Scale)));
		if (!Comp->IsVisible())
		{
			Comp->SetVisibility(true);
		}
	}
}

void ATN_BeachStorm::TickBathers(float Front, const FVector& ViewLocal)
{
	const FTransform Xf = GetActorTransform();
	const float Yaw = static_cast<float>(Xf.Rotator().Yaw);
	const double S = TNBeach::Scale;
	const double HipZ = TNBeachStormTuning::HipHeight * S;
	int32 Nearest = INDEX_NONE;
	double NearestSq = FMath::Square(15000.0);
	for (int32 b = 0; b < Bathers.Num(); ++b)
	{
		FBather& B = Bathers[b];
		USceneComponent* BatherRoot = BatherRoots.IsValidIndex(b) ? BatherRoots[b].Get() : nullptr;
		if (!BatherRoot)
		{
			continue;
		}
		const FVector Local(Front - B.Depth, B.SlotY, 0.0);
		const bool bShow = FMath::Abs(Local.Y - ViewLocal.Y) < 30000.0;
		if (BatherRoot->IsVisible() != bShow)
		{
			BatherRoot->SetVisibility(bShow, true);
		}
		if (!bShow)
		{
			continue;
		}
		FVector At = Xf.TransformPositionNoScale(Local);
		At.Z = FrontGroundZ;
		// Andar pesado: las piernas se balancean a contratiempo y la cadera sube y baja; parada, quieta.
		const float Cycle = bActive ? FXTime * B.Rate + B.Phase : B.Phase;
		const float Swing = FMath::Sin(Cycle * 2.f * PI);
		const float Bob = FMath::Abs(FMath::Cos(Cycle * 2.f * PI)) * 0.04f;
		BatherRoot->SetWorldTransform(FTransform(FRotator(0.f, Yaw, 3.f * Swing), At, FVector(B.Scale)));
		const int32 First = b * 3;
		if (BatherParts.IsValidIndex(First + 2))
		{
			const FVector Hip(0.0, 0.0, HipZ * (1.0 + Bob));
			TNBeachKit::Pose(BatherParts[First], Hip, FRotator(0.f, 4.f * Swing, 0.f));
			TNBeachKit::Pose(BatherParts[First + 1], Hip + FVector(0.0, -TNBeachStormTuning::HipHalfWidth * S, 0.0), FRotator(24.f * Swing, 0.f, 0.f));
			TNBeachKit::Pose(BatherParts[First + 2], Hip + FVector(0.0, TNBeachStormTuning::HipHalfWidth * S, 0.0), FRotator(-24.f * Swing, 0.f, 0.f));
		}
		// Un pisotón en cada paso (dos por ciclo).
		const float Step = FMath::FloorToFloat(Cycle * 2.f);
		if (bActive && Step != B.LastStep)
		{
			B.LastStep = Step;
			const double DistSq = FVector::DistSquared2D(Local, ViewLocal);
			if (DistSq < FMath::Square(20000.0))
			{
				const float Side = FMath::Fmod(Step, 2.f) < 0.5f ? -1.f : 1.f;
				const FVector Foot = Xf.TransformPositionNoScale(Local + FVector(1500.0, Side * TNBeachStormTuning::HipHalfWidth * S, 0.0));
				TNBeachKit::BurstAt(FrontDust, FVector(Foot.X, Foot.Y, FrontGroundZ + 200.0), FVector::UpVector, 3);
			}
			if (DistSq < NearestSq)
			{
				NearestSq = DistSq;
				Nearest = b;
			}
		}
	}
	// Solo el pisotón más cercano suena y hace temblar.
	if (Nearest != INDEX_NONE)
	{
		const FBather& B = Bathers[Nearest];
		FVector At = Xf.TransformPositionNoScale(FVector(Front - B.Depth, B.SlotY, 0.0));
		At.Z = FrontGroundZ;
		if (Voice)
		{
			Voice->Play(ETNBeachSfx::Stomp, 0.9f + 0.2f * Rand01(), 1.1f);
		}
		UTN_BeachCameraShake::Kick(this, At, 0.28f, 3000.f, 12000.f);
	}
}

void ATN_BeachStorm::TickCough(float DeltaSeconds)
{
	// Tos de las tortugas: cada máquina con audio decide quién está dentro con el frente replicado, sin RPC.
	if (!FApp::CanEverRenderAudio())
	{
		return;
	}
	CoughTimer += DeltaSeconds;
	if (CoughTimer < 0.1f)
	{
		return;
	}
	const float Step = CoughTimer;
	CoughTimer = 0.f;
	TArray<ATortugaCharacter*> Turtles;
	ATN_BeachEnemy::GatherTurtles(this, Turtles);
	for (ATortugaCharacter* Turtle : Turtles)
	{
		UTN_StormCoughComponent* Cough = bActive ? UTN_StormCoughComponent::FindOrAddTo(Turtle) : Turtle->FindComponentByClass<UTN_StormCoughComponent>();
		if (!Cough)
		{
			continue;
		}
		const bool bInside = IsLocationInside(Turtle->GetActorLocation());
		float& Seconds = CoughInside.FindOrAdd(Turtle);
		Seconds = bInside ? Seconds + Step : 0.f;
		Cough->SetStormExposure(bInside, Seconds / TNBeachStormTuning::WorstCoughSeconds);
	}
	for (auto It = CoughInside.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ATN_BeachStorm::ApplyInsideLook()
{
	const float B = InsideBlend;
	// Niebla de arena: se cierra dentro y vuelve exactamente a su estado al salir.
	if (UExponentialHeightFogComponent* F = Fog.Get())
	{
		if (!bFogCached)
		{
			bFogCached = true;
			FogDensity0 = F->FogDensity;
			FogFalloff0 = F->FogHeightFalloff;
			FogStart0 = F->StartDistance;
			FogOpacity0 = F->FogMaxOpacity;
			FogColor0 = F->FogInscatteringLuminance;
		}
		if (B > 0.001f)
		{
			F->SetFogDensity(FMath::Lerp(FogDensity0, 0.3f, B));
			F->SetFogHeightFalloff(FMath::Lerp(FogFalloff0, 0.002f, B));
			F->SetStartDistance(FMath::Lerp(FogStart0, 0.f, B));
			F->SetFogMaxOpacity(FMath::Lerp(FogOpacity0, 1.f, B));
			F->SetFogInscatteringColor(FMath::Lerp(FogColor0, TNBeachStormTuning::SandFog(), B));
			bFogApplied = true;
		}
		else
		{
			RestoreFog();
		}
	}
	// Tinte cálido, menos color y viñeta.
	InsidePostProcess->bEnabled = B > 0.001f;
	InsidePostProcess->BlendWeight = B;
	FPostProcessSettings& Settings = InsidePostProcess->Settings;
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(0.65f, 0.65f, 0.65f, 1.f);
	Settings.bOverride_ColorGain = true;
	Settings.ColorGain = FVector4(1.1f, 0.97f, 0.8f, 1.f);
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.8f;
}

void ATN_BeachStorm::RestoreFog()
{
	if (!bFogApplied)
	{
		return;
	}
	bFogApplied = false;
	if (UExponentialHeightFogComponent* F = Fog.Get())
	{
		F->SetFogDensity(FogDensity0);
		F->SetFogHeightFalloff(FogFalloff0);
		F->SetStartDistance(FogStart0);
		F->SetFogMaxOpacity(FogOpacity0);
		F->SetFogInscatteringColor(FogColor0);
	}
}
