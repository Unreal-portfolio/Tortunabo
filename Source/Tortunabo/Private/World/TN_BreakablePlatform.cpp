#include "World/TN_BreakablePlatform.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATN_BreakablePlatform::ATN_BreakablePlatform()
{
	PrimaryActorTick.bCanEverTick = false;

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	SetRootComponent(PlatformMesh);
	PlatformMesh->SetCollisionProfileName(TEXT("BlockAll"));
	PlatformMesh->Mobility = EComponentMobility::Movable;

	// Trigger delgado encima del mesh — detecta si alguien está sobre la plataforma.
	// Ajustar extensión en el BP hijo para que coincida con el mesh.
	StandTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("StandTrigger"));
	StandTrigger->SetupAttachment(PlatformMesh);
	StandTrigger->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	StandTrigger->SetBoxExtent(FVector(90.f, 90.f, 10.f));
	StandTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StandTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	StandTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	bReplicates = true;
	bAlwaysRelevant = true;
}

void ATN_BreakablePlatform::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BreakablePlatform, bBroken);
}

// ─────────────────────────────────────────────────────────────────────────────
// Overlap — solo el servidor gestiona el timer de rotura
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::OnStandTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bBroken || !Cast<APawn>(OtherActor))
	{
		return;
	}

	++PawnsOnPlatform;

	if (PawnsOnPlatform == 1 && !GetWorldTimerManager().IsTimerActive(BreakTimerHandle))
	{
		// Vibración a mitad del timer — handle separado para no sobrescribir el de rotura
		const float ShakeAt = TimeToBreak * 0.5f;
		FTimerDelegate ShakeDelegate;
		ShakeDelegate.BindUObject(this, &ATN_BreakablePlatform::MulticastShake);
		GetWorldTimerManager().SetTimer(ShakeTimerHandle, ShakeDelegate, ShakeAt, false);

		// Rotura al final del timer completo
		FTimerDelegate BreakDelegate;
		BreakDelegate.BindUObject(this, &ATN_BreakablePlatform::BreakPlatform);
		GetWorldTimerManager().SetTimer(BreakTimerHandle, BreakDelegate, TimeToBreak, false);
	}
}

void ATN_BreakablePlatform::OnStandTriggerEndOverlap(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority() || !Cast<APawn>(OtherActor))
	{
		return;
	}

	PawnsOnPlatform = FMath::Max(0, PawnsOnPlatform - 1);

	// Si nadie queda encima, cancelar el timer (la plataforma "aguanta")
	if (PawnsOnPlatform == 0)
	{
		GetWorldTimerManager().ClearTimer(ShakeTimerHandle);
		GetWorldTimerManager().ClearTimer(BreakTimerHandle);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Rotura y respawn — solo servidor
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::BreakPlatform()
{
	bBroken = true;
	ApplyBrokenState();         // Servidor aplica inmediatamente
	OnPlatformBreak();          // VFX en servidor / listen-server

	if (RespawnTime > 0.f)
	{
		FTimerDelegate RespawnDelegate;
		RespawnDelegate.BindUObject(this, &ATN_BreakablePlatform::RespawnPlatform);
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, RespawnTime, false);
	}
}

void ATN_BreakablePlatform::RespawnPlatform()
{
	bBroken = false;
	PawnsOnPlatform = 0;

	PlatformMesh->SetVisibility(true);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StandTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	OnPlatformRespawn();
}

// ─────────────────────────────────────────────────────────────────────────────
// Estado replicado — clientes aplican vía OnRep
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::OnRep_bBroken()
{
	if (bBroken)
	{
		ApplyBrokenState();
		OnPlatformBreak();
	}
	else
	{
		PlatformMesh->SetVisibility(true);
		PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		StandTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		OnPlatformRespawn();
	}
}

void ATN_BreakablePlatform::ApplyBrokenState()
{
	PlatformMesh->SetVisibility(false);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StandTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ─────────────────────────────────────────────────────────────────────────────
// VFX de vibración (multicast cosmético)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::MulticastShake_Implementation()
{
	OnPlatformShake();
}

// ─────────────────────────────────────────────────────────────────────────────
// Registrar overlaps en BeginPlay (no en constructor — componentes aún no listos)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		StandTrigger->OnComponentBeginOverlap.AddDynamic(
			this, &ATN_BreakablePlatform::OnStandTriggerBeginOverlap);
		StandTrigger->OnComponentEndOverlap.AddDynamic(
			this, &ATN_BreakablePlatform::OnStandTriggerEndOverlap);
	}
}

void ATN_BreakablePlatform::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ShakeTimerHandle);
	GetWorldTimerManager().ClearTimer(BreakTimerHandle);
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	Super::EndPlay(EndPlayReason);
}
