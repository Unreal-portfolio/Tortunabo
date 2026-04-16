#include "World/TN_ScorePickup.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATN_ScorePickup::ATN_ScorePickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	SetRootComponent(PickupMesh);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetIsReplicated(false);

	CollectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollectSphere"));
	CollectSphere->SetupAttachment(PickupMesh);
	CollectSphere->SetSphereRadius(60.f);
	CollectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATN_ScorePickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		CollectSphere->OnComponentBeginOverlap.AddDynamic(this, &ATN_ScorePickup::OnSphereOverlap);
	}
}

void ATN_ScorePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ScorePickup, bActive);
}

// ── Recogida ───────────────────────────────────────────────────────────────────

void ATN_ScorePickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bActive) { return; }

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->GetController()) { return; }

	// Solo jugadores vivos
	ATN_CoopPlayerState* PS = Pawn->GetPlayerState<ATN_CoopPlayerState>();
	if (!PS || !PS->bIsAlive || PS->bIsEliminated) { return; }

	// Sumar puntos
	PS->RaceScore += ScoreValue;
	UE_LOG(LogTemp, Log, TEXT("[ScorePickup] %s recogió %d puntos → total %d"),
		*GetNameSafe(Pawn), ScoreValue, PS->RaceScore);

	// Desactivar
	bActive = false;
	ApplyActiveState(false);
	MulticastPickedUp(Pawn);

	if (bRespawn)
	{
		FTimerDelegate RespawnDelegate;
		RespawnDelegate.BindUObject(this, &ATN_ScorePickup::Respawn);
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, RespawnSeconds, false);
	}
	else
	{
		Destroy();
	}
}

void ATN_ScorePickup::MulticastPickedUp_Implementation(APawn* Collector)
{
	OnPickedUp(Collector);
}

void ATN_ScorePickup::Respawn()
{
	bActive = true;
	ApplyActiveState(true);
}

void ATN_ScorePickup::ApplyActiveState(bool bNowActive)
{
	SetActorHiddenInGame(!bNowActive);
	SetActorEnableCollision(bNowActive);
}

void ATN_ScorePickup::OnRep_bActive()
{
	ApplyActiveState(bActive);
}
