#include "World/TN_SpawnZoneBase.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ATN_SpawnZoneBase::ATN_SpawnZoneBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Solo lógica de servidor, sin replicación propia

	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
	SetRootComponent(SpawnVolume);
	SpawnVolume->SetBoxExtent(FVector(500.f, 500.f, 300.f));
	SpawnVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATN_SpawnZoneBase::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) { return; }

	// El timer llama al TrySpawn virtual → despacha a la subclase (SpawnActor concreto).
	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle, this, &ATN_SpawnZoneBase::TrySpawn,
		SpawnInterval, /*bLoop=*/true, InitialDelay);
}

void ATN_SpawnZoneBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
	ActiveInstances.Empty();
	Super::EndPlay(EndPlayReason);
}

void ATN_SpawnZoneBase::GetLivingPlayersInside(TArray<ATortugaCharacter*>& OutPlayers) const
{
	if (!SpawnVolume) { return; }

	const FTransform ZoneTransform = SpawnVolume->GetComponentTransform();
	const FVector    HalfExtent    = SpawnVolume->GetUnscaledBoxExtent();

	for (TActorIterator<ATortugaCharacter> It(GetWorld()); It; ++It)
	{
		ATortugaCharacter* C = *It;
		if (!C || !C->GetController()) { continue; }

		const ATN_CoopPlayerState* PS = C->GetPlayerState<ATN_CoopPlayerState>();
		if (!PS || !PS->bIsAlive || PS->bIsEliminated) { continue; }

		// Convertir posición del jugador al espacio local de la caja
		const FVector LocalPos = ZoneTransform.InverseTransformPosition(C->GetActorLocation());
		if (FMath::Abs(LocalPos.X) <= HalfExtent.X &&
			FMath::Abs(LocalPos.Y) <= HalfExtent.Y &&
			FMath::Abs(LocalPos.Z) <= HalfExtent.Z)
		{
			OutPlayers.Add(C);
		}
	}
}

FVector ATN_SpawnZoneBase::ClampXYToVolume(const FVector& WorldLoc) const
{
	if (!SpawnVolume) { return WorldLoc; }

	const FTransform ZoneTransform = SpawnVolume->GetComponentTransform();
	const FVector    HalfExtent    = SpawnVolume->GetUnscaledBoxExtent();

	FVector Local = ZoneTransform.InverseTransformPosition(WorldLoc);
	Local.X = FMath::Clamp(Local.X, -HalfExtent.X, HalfExtent.X);
	Local.Y = FMath::Clamp(Local.Y, -HalfExtent.Y, HalfExtent.Y);
	// No clampeamos Z local para no distorsionar la altura del mundo.
	FVector WorldClamped = ZoneTransform.TransformPosition(Local);
	// Preservar la Z original: una caja rotada trasladaría erróneamente la Z mundo
	// tras TransformPosition.
	WorldClamped.Z = WorldLoc.Z;
	return WorldClamped;
}

void ATN_SpawnZoneBase::RegisterActive(AActor* Instance)
{
	if (Instance)
	{
		ActiveInstances.Add(Instance);
	}
}

int32 ATN_SpawnZoneBase::GetActiveCount()
{
	ActiveInstances.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr)
	{
		return !Ptr.IsValid();
	});
	return ActiveInstances.Num();
}
