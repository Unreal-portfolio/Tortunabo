#include "World/TN_CrabSpawnZone.h"
#include "World/TN_CrabActor.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/BoxComponent.h"

ATN_CrabSpawnZone::ATN_CrabSpawnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Solo lógica de servidor

	ProximityVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ProximityVolume"));
	SetRootComponent(ProximityVolume);
	ProximityVolume->SetBoxExtent(FVector(600.f, 600.f, 200.f));
	ProximityVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProximityVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ProximityVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATN_CrabSpawnZone::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) { return; }

	ProximityVolume->OnComponentBeginOverlap.AddDynamic(
		this, &ATN_CrabSpawnZone::OnProximityBeginOverlap);
}

void ATN_CrabSpawnZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ATN_CrabSpawnZone::OnProximityBeginOverlap(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) { return; }
	if (bOneShot && bAlreadySpawned) { return; }

	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!Char) { return; }

	const ATN_CoopPlayerState* PS = Char->GetPlayerState<ATN_CoopPlayerState>();
	if (!PS || !PS->bIsAlive || PS->bIsEliminated) { return; }

	SpawnCrab();
}

void ATN_CrabSpawnZone::SpawnCrab()
{
	if (!CrabClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrabSpawnZone] '%s': CrabClass no asignada."), *GetName());
		return;
	}

	const FVector SpawnLoc = GetActorTransform().TransformPosition(SpawnOffset);
	const FRotator SpawnRot = GetActorRotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ATN_CrabActor* Crab = GetWorld()->SpawnActor<ATN_CrabActor>(CrabClass, SpawnLoc, SpawnRot, Params);

	if (Crab)
	{
		bAlreadySpawned = true;
		UE_LOG(LogTemp, Log, TEXT("[CrabSpawnZone] '%s': cangrejo spawneado."), *GetName());

		// Desactivar detección en one-shot para no volver a disparar
		if (bOneShot)
		{
			ProximityVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}
