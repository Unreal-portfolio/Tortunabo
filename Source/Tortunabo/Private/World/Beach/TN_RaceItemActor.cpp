#include "World/Beach/TN_RaceItemActor.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/TortugaCharacter.h"

ATN_RaceItemActor::ATN_RaceItemActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	// Pocos y importantes en toda la playa (la carrera mide 800 m): siempre relevantes, con la posición propia (Track) y sin
	// el movimiento replicado del motor.
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(20.f);
	SetMinNetUpdateFrequency(5.f);
	SetCanBeDamaged(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

void ATN_RaceItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_RaceItemActor, OwnerTurtle);
	DOREPLIFETIME(ATN_RaceItemActor, StartServerTime);
	DOREPLIFETIME(ATN_RaceItemActor, Track);
	DOREPLIFETIME(ATN_RaceItemActor, bFinished);
}

void ATN_RaceItemActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Antes de las primeras réplicas y de BeginPlay (OnRep_Track puede llegar antes que BeginPlay en un cliente).
	bHasScreen = GetNetMode() != NM_DedicatedServer;
}

void ATN_RaceItemActor::SetOwnerTurtle(ATortugaCharacter* Turtle)
{
	if (HasAuthority())
	{
		OwnerTurtle = Turtle;
		SetInstigator(Turtle);
	}
}

void ATN_RaceItemActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		StartServerTime = static_cast<float>(ServerNow());
		SetNetUpdateFrequency(GetTrackFrequency());
		if (const ATN_BeachRaceGenerator* Generator = GetGenerator())
		{
			SpawnRound = Generator->GetRoundNumber();
		}
		if (UsesTrack())
		{
			ServerPublishTrack();
		}
	}
	if (bHasScreen)
	{
		BuildVisuals();
	}
}

void ATN_RaceItemActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Sfx = nullptr;
	Super::EndPlay(EndPlayReason);
}

double ATN_RaceItemActor::ServerNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

double ATN_RaceItemActor::GetAge() const
{
	return FMath::Max(0.0, ServerNow() - static_cast<double>(StartServerTime));
}

int32 ATN_RaceItemActor::CountOf(UWorld* World, const UClass* ItemClass)
{
	if (!World || !ItemClass)
	{
		return 0;
	}
	int32 Count = 0;
	for (TActorIterator<ATN_RaceItemActor> It(World); It; ++It)
	{
		const ATN_RaceItemActor* Item = *It;
		if (IsValid(Item) && !Item->bFinished && Item->IsA(ItemClass))
		{
			++Count;
		}
	}
	return Count;
}

void ATN_RaceItemActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		if (!bFinished)
		{
			ServerTick(DeltaSeconds);
			if (UsesTrack())
			{
				ServerPublishTrack();
			}
			// Nada de lo lanzado sobrevive a la ronda: si cambia la ronda del generador, se acaba.
			RoundCheckClock -= DeltaSeconds;
			if (RoundCheckClock <= 0.f)
			{
				RoundCheckClock = 0.5f;
				const ATN_BeachRaceGenerator* Generator = SpawnRound >= 0 ? GetGenerator() : nullptr;
				if (Generator && Generator->GetRoundNumber() != SpawnRound)
				{
					ServerFinish(0.2f);
				}
			}
		}
	}
	else if (UsesTrack())
	{
		SmoothToTrack(DeltaSeconds);
	}
	if (bHasScreen && !bFinished)
	{
		VisualTick(DeltaSeconds);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Posición replicada
// ─────────────────────────────────────────────────────────────────────────────

void ATN_RaceItemActor::ServerPublishTrack()
{
	if (!HasAuthority())
	{
		return;
	}
	const FVector Where = GetActorLocation();
	const FRotator Facing = GetActorRotation();
	Track.Location = FVector_NetQuantize10(Where);
	Track.Yaw = FRotator::CompressAxisToShort(Facing.Yaw);
	Track.Pitch = FRotator::CompressAxisToShort(Facing.Pitch);
}

void ATN_RaceItemActor::OnRep_Track()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FVector NewLoc(Track.Location);
	const FRotator NewRot(FRotator::DecompressAxisFromShort(Track.Pitch), FRotator::DecompressAxisFromShort(Track.Yaw), 0.f);
	if (!bHasTrack)
	{
		// La primera réplica: se coloca sin más.
		bHasTrack = true;
		TrackLoc = NewLoc;
		TrackVel = FVector::ZeroVector;
		TrackRot = NewRot.Quaternion();
		LastTrackTime = Now;
		SetActorLocationAndRotation(NewLoc, NewRot);
		return;
	}
	if (FVector::DistSquared(NewLoc, TrackLoc) > FMath::Square(4000.0))
	{
		// Salto (teletransporte): sin interpolar.
		TrackVel = FVector::ZeroVector;
		SetActorLocation(NewLoc);
	}
	else
	{
		const double Dt = Now - LastTrackTime;
		if (Dt > 0.015)
		{
			TrackVel = ((NewLoc - TrackLoc) / Dt).GetClampedToMaxSize(7000.0);
		}
	}
	TrackLoc = NewLoc;
	TrackRot = NewRot.Quaternion();
	LastTrackTime = Now;
}

void ATN_RaceItemActor::SmoothToTrack(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	if (!World || !bHasTrack)
	{
		return;
	}
	// Un pelo por delante de lo último recibido (con su velocidad estimada), sin pasarse si se corta la réplica.
	const double Ahead = FMath::Clamp(World->GetTimeSeconds() - LastTrackTime, 0.0, 0.2);
	const FVector Target = TrackLoc + TrackVel * Ahead;
	const double K = 1.0 - FMath::Exp(-static_cast<double>(DeltaSeconds) / 0.06);
	const FVector Current = GetActorLocation();
	SetActorLocationAndRotation(Current + (Target - Current) * K, FQuat::Slerp(GetActorQuat(), TrackRot, K).GetNormalized());
}

// ─────────────────────────────────────────────────────────────────────────────
// Final
// ─────────────────────────────────────────────────────────────────────────────

void ATN_RaceItemActor::ServerFinish(float Delay)
{
	if (!HasAuthority() || bFinished)
	{
		return;
	}
	bFinished = true;
	// El servidor no recibe OnRep: lo aplica aquí.
	OnRep_Finished();
	SetLifeSpan(FMath::Max(0.2f, Delay));
	ForceNetUpdate();
}

void ATN_RaceItemActor::OnRep_Finished()
{
	if (bFinished)
	{
		OnFinished();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Utilidades
// ─────────────────────────────────────────────────────────────────────────────

UTN_RaceItemSynthComponent* ATN_RaceItemActor::GetSfx()
{
	if (!bHasScreen)
	{
		return nullptr;
	}
	if (!Sfx)
	{
		Sfx = UTN_RaceItemSynthComponent::AttachTo(this, GetActorLocation());
	}
	return Sfx;
}

void ATN_RaceItemActor::PlaySfx(ETNRaceSound Sound, float Pitch, float Volume)
{
	if (UTN_RaceItemSynthComponent* Synth = GetSfx())
	{
		Synth->Play(Sound, Pitch, Volume);
	}
}

ATN_BeachRaceGenerator* ATN_RaceItemActor::GetGenerator() const
{
	ATN_BeachRaceGenerator* Found = CachedGenerator.Get();
	if (!Found)
	{
		Found = ATN_BeachRaceGenerator::Find(this);
		CachedGenerator = Found;
	}
	return Found;
}

float ATN_RaceItemActor::GroundHeightAt(const FVector& Where, float Fallback) const
{
	if (const ATN_BeachRaceGenerator* Generator = GetGenerator())
	{
		return Generator->GetGroundHeightAt(Where);
	}
	const UWorld* World = GetWorld();
	if (World)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TNRaceItemGround), false, this);
		if (World->LineTraceSingleByChannel(Hit, Where + FVector(0.0, 0.0, 2000.0), Where - FVector(0.0, 0.0, 6000.0), ECC_WorldStatic, Params))
		{
			return static_cast<float>(Hit.ImpactPoint.Z);
		}
	}
	return Fallback;
}
