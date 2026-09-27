#include "World/Beach/TN_BeachEnemy.h"
#include "World/Beach/TN_BeachEnemySynth.h"
#include "World/Beach/TN_BeachStun.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_Log.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Game/TN_BeachRaceGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Player/TortugaCharacter.h"

namespace TNBeachEnemyDebug
{
	/** TN.Beach.Enemy.Debug: 1 dibuja en el servidor el estado de cada enemigo y sus radios. */
	static int32 DebugDraw = 0;
	static FAutoConsoleVariableRef CVarBeachEnemyDebug(TEXT("TN.Beach.Enemy.Debug"), DebugDraw,
		TEXT("1 = radios y estados de los enemigos de la playa (servidor)."), ECVF_Cheat);

	/** Diferencia con signo entre dos giros comprimidos a 16 bits. */
	inline int32 YawDelta(uint16 A, uint16 B)
	{
		return static_cast<int32>(static_cast<int16>(static_cast<uint16>(A - B)));
	}
}

ATN_BeachEnemy::ATN_BeachEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// Siempre relevantes: con la distancia por defecto (150 m) un cliente destruiría el enemigo (y rehará sus mallas)
	// cada vez que se aleje por la playa. Solo se manda lo que cambia, a 10 Hz como mucho.
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.f);
	SetMinNetUpdateFrequency(2.f);
}

void ATN_BeachEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachEnemy, Mover);
}

void ATN_BeachEnemy::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Antes de las primeras réplicas y de BeginPlay (ApplySpec puede llegar antes que BeginPlay en un cliente).
	bHasScreen = GetNetMode() != NM_DedicatedServer;
	Home = GetActorLocation();
	SimLoc = Home;
	SimYaw = static_cast<float>(GetActorRotation().Yaw);
	if (!bHasRep)
	{
		ShownLoc = Home;
		ShownYaw = SimYaw;
	}
}

void ATN_BeachEnemy::BeginPlay()
{
	if (HasAuthority())
	{
		ServerRng.Initialize(Spec.Seed ^ 0x5EED1234);
		Mover.Location = SimLoc;
		Mover.Yaw = FRotator::CompressAxisToShort(SimYaw);
		Mover.StateTime = static_cast<float>(ServerNow(this));
	}
	Super::BeginPlay();
}

void ATN_BeachEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	IgnoreUntil.Reset();
	Super::EndPlay(EndPlayReason);
}

bool ATN_BeachEnemy::IsDebugDraw()
{
	return TNBeachEnemyDebug::DebugDraw != 0;
}

double ATN_BeachEnemy::ServerNow(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return 0.0;
	}
	if (const AGameStateBase* GS = World->GetGameState())
	{
		return GS->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

void ATN_BeachEnemy::GatherTurtles(const UObject* WorldContext, TArray<ATortugaCharacter*>& Out)
{
	Out.Reset();
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		ATortugaCharacter* Turtle = PS ? Cast<ATortugaCharacter>(PS->GetPawn()) : nullptr;
		if (!IsValid(Turtle) || Turtle->IsDead() || Turtle->IsActorBeingDestroyed())
		{
			continue;
		}
		if (const ATN_CoopPlayerState* Coop = Cast<ATN_CoopPlayerState>(PS))
		{
			if (!Coop->IsAliveAndPlaying() || Coop->bHasFinishedRun)
			{
				continue;
			}
		}
		Out.Add(Turtle);
	}
}

bool ATN_BeachEnemy::IsRaceLive(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ATN_BeachRaceGameState* GS = World ? World->GetGameState<ATN_BeachRaceGameState>() : nullptr;
	// Solo se para en el recuento y en el podio: si la fase se quedara en Waiting por lo que sea, se sigue atacando.
	return !GS || (GS->RacePhase != ETNBeachRacePhase::RoundResults && GS->RacePhase != ETNBeachRacePhase::Champion);
}

bool ATN_BeachEnemy::TraceGround(const UObject* WorldContext, const FVector& Where, float& OutZ, FVector* OutNormal, float Up, float Down)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeachEnemyGround), false);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	if (World->LineTraceSingleByObjectType(Hit, Where + FVector(0.0, 0.0, Up), Where - FVector(0.0, 0.0, Down), Objects, Params))
	{
		OutZ = static_cast<float>(Hit.ImpactPoint.Z);
		if (OutNormal)
		{
			*OutNormal = Hit.ImpactNormal;
		}
		return true;
	}
	return false;
}

float ATN_BeachEnemy::LocalViewDistance(const UObject* WorldContext, const FVector& Where)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return 1.0e9f;
	}
	float Best = 1.0e9f;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
		{
			continue;
		}
		Best = FMath::Min(Best, static_cast<float>(FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Where)));
	}
	return Best;
}

void ATN_BeachEnemy::OnRep_Mover()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const FVector NewLoc = Mover.Location;
	if (!bHasRep)
	{
		// Primera réplica: se coloca sin más y sin efectos de cambio de estado (quien entra a mitad no oye golpes viejos).
		bHasRep = true;
		ShownLoc = NewLoc;
		ShownYaw = static_cast<float>(FRotator::DecompressAxisFromShort(Mover.Yaw));
		RepVelocity = FVector::ZeroVector;
		LastRepLoc = NewLoc;
		LastRepTime = Now;
		LastSerial = Mover.Serial;
		LastState = Mover.State;
		return;
	}
	if (!NewLoc.Equals(LastRepLoc, 0.5))
	{
		const double Dt = Now - LastRepTime;
		if (FVector::DistSquared(NewLoc, LastRepLoc) > FMath::Square(4000.0))
		{
			// Salto (reaparición, teletransporte): sin interpolar.
			ShownLoc = NewLoc;
			RepVelocity = FVector::ZeroVector;
		}
		else if (Dt > 0.02)
		{
			RepVelocity = ((NewLoc - LastRepLoc) / Dt).GetClampedToMaxSize(4500.0);
		}
		LastRepLoc = NewLoc;
		LastRepTime = Now;
	}
	if (Mover.Serial != LastSerial)
	{
		const uint8 OldState = LastState;
		LastSerial = Mover.Serial;
		LastState = Mover.State;
		OnMoverStateChanged(OldState);
	}
}

void ATN_BeachEnemy::ServerMoveTo(const FVector& Location, float YawDeg)
{
	SimLoc = Location;
	SimYaw = static_cast<float>(FRotator::NormalizeAxis(YawDeg));
	if (FVector::DistSquared(FVector(Mover.Location), Location) > 4.0)
	{
		Mover.Location = Location;
	}
	const uint16 NewYaw = FRotator::CompressAxisToShort(SimYaw);
	// Medio grado de umbral (65536 / 720): el giro no manda nada mientras no cambie de verdad.
	if (FMath::Abs(TNBeachEnemyDebug::YawDelta(NewYaw, Mover.Yaw)) > 91)
	{
		Mover.Yaw = NewYaw;
	}
}

void ATN_BeachEnemy::ServerSetState(uint8 NewState, const FVector& Aim)
{
	const uint8 OldState = Mover.State;
	Mover.State = NewState;
	Mover.Aim = Aim;
	++Mover.Serial;
	Mover.StateTime = static_cast<float>(ServerNow(this));
	LastSerial = Mover.Serial;
	LastState = NewState;
	ForceNetUpdate();
	OnMoverStateChanged(OldState);
}

void ATN_BeachEnemy::ServerSetAim(const FVector& Aim)
{
	Mover.Aim = Aim;
}

float ATN_BeachEnemy::GetStateAge() const
{
	return FMath::Max(0.f, static_cast<float>(ServerNow(this) - static_cast<double>(Mover.StateTime)));
}

USceneComponent* ATN_BeachEnemy::MakeRig()
{
	if (Rig)
	{
		return Rig;
	}
	Rig = NewObject<USceneComponent>(this, TEXT("Rig"));
	Rig->SetMobility(EComponentMobility::Movable);
	Rig->SetupAttachment(GetRootComponent());
	Rig->SetAbsolute(true, true, false);
	Rig->RegisterComponent();
	Rig->SetWorldLocationAndRotation(ShownLoc, FRotator(0.f, ShownYaw, 0.f));
	return Rig;
}

UTN_BeachEnemySynthComponent* ATN_BeachEnemy::GetVoice(USceneComponent* Parent, float InnerRadius, float Falloff)
{
	if (!Voice && !bVoiceTried && bHasScreen)
	{
		bVoiceTried = true;
		Voice = UTN_BeachEnemySynthComponent::AttachTo(this, Parent, InnerRadius, Falloff);
	}
	return Voice;
}

void ATN_BeachEnemy::StunTurtle(ATortugaCharacter* Turtle, float Seconds, const FVector& Launch)
{
	if (!HasAuthority() || !IsValid(Turtle))
	{
		return;
	}
	TNBeach::StunTurtle(Turtle, Seconds, Launch);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] %s aturde a %s %.1f s."), *GetName(), *GetNameSafe(Turtle), Seconds);
}

void ATN_BeachEnemy::IgnoreTurtle(ATortugaCharacter* Turtle, float Seconds)
{
	if (!Turtle || !GetWorld())
	{
		return;
	}
	IgnoreUntil.Add(Turtle, GetWorld()->GetTimeSeconds() + Seconds);
}

bool ATN_BeachEnemy::IsIgnored(const ATortugaCharacter* Turtle) const
{
	const UWorld* World = GetWorld();
	if (!Turtle || !World)
	{
		return false;
	}
	for (const TPair<TWeakObjectPtr<ATortugaCharacter>, double>& Pair : IgnoreUntil)
	{
		if (Pair.Key.Get() == Turtle)
		{
			return World->GetTimeSeconds() < Pair.Value;
		}
	}
	return false;
}

bool ATN_BeachEnemy::IsTargetable(const ATortugaCharacter* Turtle) const
{
	return IsValid(Turtle) && !Turtle->IsDead() && !TNBeach::IsTurtleStunned(Turtle) && !IsIgnored(Turtle);
}

ATortugaCharacter* ATN_BeachEnemy::FindTarget(const FVector& From, float MaxDist, const FVector& InHome, float Leash) const
{
	TArray<ATortugaCharacter*> Turtles;
	GatherTurtles(this, Turtles);
	ATortugaCharacter* Best = nullptr;
	double BestSq = FMath::Square(static_cast<double>(MaxDist));
	for (ATortugaCharacter* Turtle : Turtles)
	{
		if (!IsTargetable(Turtle))
		{
			continue;
		}
		const FVector At = Turtle->GetActorLocation();
		if (Leash > 0.f && FVector::Dist2D(At, InHome) > Leash)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(At, From);
		if (DistSq < BestSq)
		{
			BestSq = DistSq;
			Best = Turtle;
		}
	}
	return Best;
}

void ATN_BeachEnemy::UpdateShown(float DeltaSeconds)
{
	if (HasAuthority())
	{
		ShownLoc = SimLoc;
		ShownYaw = SimYaw;
	}
	else if (bHasRep)
	{
		const UWorld* World = GetWorld();
		const double Now = World ? World->GetTimeSeconds() : 0.0;
		const double Ahead = FMath::Clamp(Now - LastRepTime, 0.0, 0.25);
		const FVector Target = LastRepLoc + RepVelocity * Ahead;
		const float K = 1.f - FMath::Exp(-DeltaSeconds / 0.08f);
		ShownLoc += (Target - ShownLoc) * K;
		const float TargetYaw = static_cast<float>(FRotator::DecompressAxisFromShort(Mover.Yaw));
		const float KYaw = 1.f - FMath::Exp(-DeltaSeconds / 0.1f);
		ShownYaw = FMath::UnwindDegrees(ShownYaw + FMath::FindDeltaAngleDegrees(ShownYaw, TargetYaw) * KYaw);
	}
	if (Rig)
	{
		Rig->SetWorldLocationAndRotation(ShownLoc, FRotator(0.f, ShownYaw, 0.f));
	}
}

void ATN_BeachEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		ServerTick(DeltaSeconds);
	}
	if (bUsesMover)
	{
		UpdateShown(DeltaSeconds);
	}
	if (bHasScreen)
	{
		ViewDistance = LocalViewDistance(this, bUsesMover ? ShownLoc : GetActorLocation());
		VisualTick(DeltaSeconds);
	}
	if (HasAuthority() && IsDebugDraw())
	{
		const FVector At = bUsesMover ? ShownLoc : GetActorLocation();
		DrawDebugString(GetWorld(), At + FVector(0.0, 0.0, 900.0), FString::Printf(TEXT("%s · estado %d · %.1f s"), *GetClass()->GetName(),
			static_cast<int32>(Mover.State), GetStateAge()), nullptr, FColor::White, 0.f, true);
		DrawDebugCircle(GetWorld(), GetActorLocation() + FVector(0.0, 0.0, 30.0), GetFootprintRadius(), 48, FColor::Cyan, false, -1.f, 0, 8.f,
			FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
	}
}
