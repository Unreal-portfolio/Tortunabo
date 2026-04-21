#include "World/TN_SeagullSpawnZone.h"
#include "World/TN_EnemySeagull.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"

ATN_SeagullSpawnZone::ATN_SeagullSpawnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Solo lógica de servidor, sin replicación propia

	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
	SetRootComponent(SpawnVolume);
	SpawnVolume->SetBoxExtent(FVector(500.f, 500.f, 300.f));
	SpawnVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATN_SeagullSpawnZone::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) { return; }

	if (!SeagullClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SeagullSpawnZone] '%s': SeagullClass no asignada — zona inactiva."),
			*GetName());
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&ATN_SeagullSpawnZone::TrySpawnSeagull,
		SpawnInterval,
		true,       // looping
		InitialDelay
	);
}

void ATN_SeagullSpawnZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	ActiveSeagulls.Empty();
	Super::EndPlay(EndPlayReason);
}

// ── Lógica de spawn ───────────────────────────────────────────────────────────

void ATN_SeagullSpawnZone::TrySpawnSeagull()
{
	PruneActiveSeagulls();

	if (ActiveSeagulls.Num() >= MaxConcurrentSeagulls) { return; }

	TArray<ATortugaCharacter*> Candidates;
	GetPlayersInsideZone(Candidates);
	if (Candidates.Num() == 0) { return; }

	// Elegir objetivo aleatorio
	const int32 Idx = FMath::RandRange(0, Candidates.Num() - 1);
	ATortugaCharacter* Target = Candidates[Idx];

	// Si el jugador está en el borde, la gaviota quedaría (en XY) fuera del volumen.
	// Clampeamos al AABB para que el spawn siempre caiga dentro de la zona dueña.
	const FVector ClampedXY = ClampXYToVolume(Target->GetActorLocation());
	const FVector SpawnLoc = ClampedXY + FVector(0.f, 0.f, 400.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATN_EnemySeagull* Seagull = GetWorld()->SpawnActor<ATN_EnemySeagull>(
		SeagullClass, SpawnLoc, FRotator::ZeroRotator, Params);

	if (Seagull)
	{
		Seagull->InitializeWithTarget(Target);
		ActiveSeagulls.Add(Seagull);

		UE_LOG(LogTemp, Log, TEXT("[SeagullSpawnZone] '%s': gaviota spawneada sobre '%s'."),
			*GetName(), *GetNameSafe(Target));
	}
}

void ATN_SeagullSpawnZone::GetPlayersInsideZone(TArray<ATortugaCharacter*>& OutPlayers) const
{
	const FTransform ZoneTransform  = SpawnVolume->GetComponentTransform();
	const FVector    HalfExtent     = SpawnVolume->GetUnscaledBoxExtent();

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

void ATN_SeagullSpawnZone::PruneActiveSeagulls()
{
	ActiveSeagulls.RemoveAll([](const TWeakObjectPtr<ATN_EnemySeagull>& Ptr)
	{
		return !Ptr.IsValid();
	});
}

FVector ATN_SeagullSpawnZone::ClampXYToVolume(const FVector& WorldLoc) const
{
	if (!SpawnVolume) { return WorldLoc; }

	const FTransform ZoneTransform = SpawnVolume->GetComponentTransform();
	const FVector    HalfExtent    = SpawnVolume->GetUnscaledBoxExtent();

	FVector Local = ZoneTransform.InverseTransformPosition(WorldLoc);
	Local.X = FMath::Clamp(Local.X, -HalfExtent.X, HalfExtent.X);
	Local.Y = FMath::Clamp(Local.Y, -HalfExtent.Y, HalfExtent.Y);
	// No clampeamos Z local para no distorsionar la altura del mundo.
	FVector WorldClamped = ZoneTransform.TransformPosition(Local);
	// Preservar la Z original: TransformPosition con una caja rotada puede
	// desplazar la Z en coordenadas mundo; restauramos la altura del jugador.
	WorldClamped.Z = WorldLoc.Z;
	return WorldClamped;
}
