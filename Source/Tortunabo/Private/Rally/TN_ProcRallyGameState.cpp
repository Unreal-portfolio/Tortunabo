#include "Rally/TN_ProcRallyGameState.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Rally/TN_ProcRallyTrack.h"
#include "Rally/TN_RallyLogic.h"
#include "Rally/TN_RallyTrack.h"
#include "TimerManager.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"

namespace TNProcRallyState
{
	/** Cada cuánto vuelve a mirar un cliente si ya tiene el mapa (por si el generador llega después que el estado). */
	constexpr float RetrySeconds = 0.5f;
}

void ATN_ProcRallyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ProcRallyGameState, ProcGeneration);
	DOREPLIFETIME(ATN_ProcRallyGameState, ProcDifficulty);
	DOREPLIFETIME(ATN_ProcRallyGameState, MapSeed);
	DOREPLIFETIME(ATN_ProcRallyGameState, TrackLengthCm);
}

void ATN_ProcRallyGameState::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RetryHandle, this, &ATN_ProcRallyGameState::TryBuildClientTrack, TNProcRallyState::RetrySeconds, true);
		TryBuildClientTrack();
	}
}

void ATN_ProcRallyGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RetryHandle);
	if (ATN_ProcMapGenerator* Generator = BoundGenerator.Get())
	{
		Generator->OnMapGeneratedNative.Remove(MapGeneratedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

ATN_ProcMapGenerator* ATN_ProcRallyGameState::FindGenerator() const
{
	if (ATN_ProcMapGenerator* Bound = BoundGenerator.Get())
	{
		return Bound;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ATN_ProcMapGenerator> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

ATN_RallyTrack* ATN_ProcRallyGameState::BuildTrackFromMap(ATN_ProcMapGenerator& Generator)
{
	if (!FindOrSpawnTrack() || !TNProcRally::BuildTrackFromGenerator(*Track, Generator))
	{
		return nullptr;
	}
	TrackGeneration = Generator.GetBuiltGeneration();
	ProcGeneration = TrackGeneration;
	TrackLengthCm = Track->GetTrackLengthCm();
	ForceNetUpdate();
	OnTrackReady.Broadcast();
	return Track;
}

void ATN_ProcRallyGameState::OnRep_ProcGeneration()
{
	TryBuildClientTrack();
}

void ATN_ProcRallyGameState::HandleMapGenerated(int32 Generation)
{
	TryBuildClientTrack();
}

void ATN_ProcRallyGameState::TryBuildClientTrack()
{
	if (HasAuthority())
	{
		return;
	}
	ATN_ProcMapGenerator* Generator = FindGenerator();
	if (Generator && BoundGenerator.Get() != Generator)
	{
		BoundGenerator = Generator;
		MapGeneratedHandle = Generator->OnMapGeneratedNative.AddUObject(this, &ATN_ProcRallyGameState::HandleMapGenerated);
	}
	const bool bWanted = ProcGeneration > 0 && TrackGeneration != ProcGeneration;
	if (!bWanted || !Generator || !Generator->IsMapReady() || Generator->GetBuiltGeneration() != ProcGeneration)
	{
		return;
	}
	if (!FindOrSpawnTrack() || !TNProcRally::BuildTrackFromGenerator(*Track, *Generator))
	{
		UE_LOG(LogTNRally, Error, TEXT("[ProcRally] Este cliente no ha podido hacer la pista de la generación %d."), ProcGeneration);
		return;
	}
	TrackGeneration = ProcGeneration;
	GetWorldTimerManager().ClearTimer(RetryHandle);
	UE_LOG(LogTNRally, Log, TEXT("[ProcRally] Pista del cliente lista (generación %d, %d puertas, %.2f km)."), TrackGeneration,
		Track->GetGateCount(), Track->GetTrackLengthCm() / 100000.0);
	// El PlayerController local lo ve (GetBuiltTrackGeneration) y avisa al servidor: el semáforo espera a todas.
	OnTrackReady.Broadcast();
}
