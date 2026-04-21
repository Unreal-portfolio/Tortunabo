#include "World/TN_DroppingSpawnZone.h"
#include "World/TN_SeagullDroppingActor.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"

ATN_DroppingSpawnZone::ATN_DroppingSpawnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
	SetRootComponent(SpawnVolume);
	SpawnVolume->SetBoxExtent(FVector(500.f, 500.f, 300.f));
	SpawnVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATN_DroppingSpawnZone::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) { return; }

	if (!DroppingClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DroppingSpawnZone] '%s': DroppingClass no asignada — zona inactiva."),
			*GetName());
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle, this, &ATN_DroppingSpawnZone::TrySpawnDropping,
		SpawnInterval, /*bLoop=*/true, InitialDelay);
}

void ATN_DroppingSpawnZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle); }
	ActiveDroppings.Empty();
	Super::EndPlay(EndPlayReason);
}

void ATN_DroppingSpawnZone::TrySpawnDropping()
{
	PruneActiveDroppings();
	if (ActiveDroppings.Num() >= MaxConcurrentDrops) { return; }

	TArray<ATortugaCharacter*> Candidates;
	GetPlayersInsideZone(Candidates);
	if (Candidates.Num() == 0) { return; }

	const int32 Idx = FMath::RandRange(0, Candidates.Num() - 1);
	ATortugaCharacter* Target = Candidates[Idx];

	// La caca cae sobre la XY del jugador, pero si éste está en el borde del
	// volumen podría impactar fuera. Clampeamos al AABB para garantizar que el
	// impacto siempre cae dentro de la zona dueña.
	const FVector ClampedGround = ClampXYToVolume(Target->GetActorLocation());
	ATN_SeagullDroppingActor* Dropping = ATN_SeagullDroppingActor::SpawnDroppingAtLocation(
		this, DroppingClass, ClampedGround);

	if (Dropping)
	{
		ActiveDroppings.Add(Dropping);
		UE_LOG(LogTemp, Log, TEXT("[DroppingSpawnZone] '%s': caca spawneada sobre '%s'."),
			*GetName(), *GetNameSafe(Target));
	}
}

void ATN_DroppingSpawnZone::GetPlayersInsideZone(TArray<ATortugaCharacter*>& OutPlayers) const
{
	const FTransform ZoneTransform = SpawnVolume->GetComponentTransform();
	const FVector    HalfExtent    = SpawnVolume->GetUnscaledBoxExtent();

	for (TActorIterator<ATortugaCharacter> It(GetWorld()); It; ++It)
	{
		ATortugaCharacter* C = *It;
		if (!C || !C->GetController()) { continue; }

		const ATN_CoopPlayerState* PS = C->GetPlayerState<ATN_CoopPlayerState>();
		if (!PS || !PS->bIsAlive || PS->bIsEliminated) { continue; }

		const FVector LocalPos = ZoneTransform.InverseTransformPosition(C->GetActorLocation());
		if (FMath::Abs(LocalPos.X) <= HalfExtent.X &&
			FMath::Abs(LocalPos.Y) <= HalfExtent.Y &&
			FMath::Abs(LocalPos.Z) <= HalfExtent.Z)
		{
			OutPlayers.Add(C);
		}
	}
}

void ATN_DroppingSpawnZone::PruneActiveDroppings()
{
	ActiveDroppings.RemoveAll([](const TWeakObjectPtr<ATN_SeagullDroppingActor>& Ptr)
	{
		return !Ptr.IsValid();
	});
}

FVector ATN_DroppingSpawnZone::ClampXYToVolume(const FVector& WorldLoc) const
{
	if (!SpawnVolume) { return WorldLoc; }

	const FTransform ZoneTransform = SpawnVolume->GetComponentTransform();
	const FVector    HalfExtent    = SpawnVolume->GetUnscaledBoxExtent();

	FVector Local = ZoneTransform.InverseTransformPosition(WorldLoc);
	Local.X = FMath::Clamp(Local.X, -HalfExtent.X, HalfExtent.X);
	Local.Y = FMath::Clamp(Local.Y, -HalfExtent.Y, HalfExtent.Y);
	// No clampeamos Z local para no distorsionar la altura del mundo.
	FVector WorldClamped = ZoneTransform.TransformPosition(Local);
	// Preservar la Z original del jugador: una caja rotada trasladaría
	// erróneamente la Z mundo tras TransformPosition.
	WorldClamped.Z = WorldLoc.Z;
	return WorldClamped;
}
