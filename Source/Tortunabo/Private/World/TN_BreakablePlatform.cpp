#include "World/TN_BreakablePlatform.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

ATN_BreakablePlatform::ATN_BreakablePlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;  // solo tickea durante el shake

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
	// La oscilación se calcula localmente en cada máquina — evitar que el server
	// envíe snapshots de posición durante el shake (introduciría jitter en clientes).
	SetReplicateMovement(false);
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
	if (!HasAuthority() || bBroken) { return; }

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) { return; }

	PawnsOnPlatform.Remove(nullptr);
	PawnsOnPlatform.Add(Pawn);

	if (PawnsOnPlatform.Num() >= PlayerThreshold && !GetWorldTimerManager().IsTimerActive(BreakTimerHandle))
	{
		// La vibración arranca ShakeDuration segundos antes del break
		const float ShakeAt = FMath::Max(0.f, TimeToBreak - ShakeDuration);
		FTimerDelegate ShakeDelegate;
		ShakeDelegate.BindUObject(this, &ATN_BreakablePlatform::MulticastShake);
		GetWorldTimerManager().SetTimer(ShakeTimerHandle, ShakeDelegate, ShakeAt, false);

		FTimerDelegate BreakDelegate;
		BreakDelegate.BindUObject(this, &ATN_BreakablePlatform::BreakPlatform);
		GetWorldTimerManager().SetTimer(BreakTimerHandle, BreakDelegate, TimeToBreak, false);
	}
}

void ATN_BreakablePlatform::OnStandTriggerEndOverlap(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) { return; }

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) { return; }

	PawnsOnPlatform.Remove(Pawn);
	PawnsOnPlatform.Remove(nullptr);

	if (PawnsOnPlatform.Num() < PlayerThreshold)
	{
		GetWorldTimerManager().ClearTimer(ShakeTimerHandle);
		GetWorldTimerManager().ClearTimer(BreakTimerHandle);
		// Si la vibración ya había empezado, cancelarla en todas las máquinas
		if (bIsShaking)
		{
			MulticastStopShake();
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Rotura y respawn — solo servidor
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::BreakPlatform()
{
	bBroken = true;
	StopShakeAndResetPosition();
	ApplyBrokenState();

	if (BreakSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, BreakSound, GetActorLocation());
	}
	if (BreakVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakVFX, GetActorLocation());
	}

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
	PawnsOnPlatform.Empty();

	PlatformMesh->SetVisibility(true);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StandTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	if (RespawnSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, RespawnSound, GetActorLocation());
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Estado replicado — clientes aplican vía OnRep
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::OnRep_bBroken()
{
	if (bBroken)
	{
		StopShakeAndResetPosition();
		ApplyBrokenState();
		if (BreakSound)
		{
			UGameplayStatics::SpawnSoundAtLocation(this, BreakSound, GetActorLocation());
		}
		if (BreakVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakVFX, GetActorLocation());
		}
	}
	else
	{
		PlatformMesh->SetVisibility(true);
		PlatformMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		StandTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		if (RespawnSound)
		{
			UGameplayStatics::SpawnSoundAtLocation(this, RespawnSound, GetActorLocation());
		}
	}
}

void ATN_BreakablePlatform::ApplyBrokenState()
{
	PlatformMesh->SetVisibility(false);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StandTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ─────────────────────────────────────────────────────────────────────────────
// Vibración (Reliable — es señal de gameplay, no cosmético puro)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::MulticastShake_Implementation()
{
	if (ShakeSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, ShakeSound, GetActorLocation());
	}
	if (ShakeVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ShakeVFX, GetActorLocation());
	}

	StartShake();
}

void ATN_BreakablePlatform::MulticastStopShake_Implementation()
{
	StopShakeAndResetPosition();
}

// ─────────────────────────────────────────────────────────────────────────────
// Oscilación visual — ejecutada localmente en cada máquina mientras bIsShaking
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BreakablePlatform::StartShake()
{
	bIsShaking = true;
	ShakeElapsedSeconds = 0.f;
	OriginalRootLocation = GetActorLocation();
	SetActorTickEnabled(true);
}

void ATN_BreakablePlatform::StopShakeAndResetPosition()
{
	if (!bIsShaking) { return; }
	bIsShaking = false;
	SetActorTickEnabled(false);
	// Volver a posición original para evitar deriva si la oscilación terminó fuera de 0
	SetActorLocation(OriginalRootLocation);
}

void ATN_BreakablePlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bIsShaking) { return; }

	ShakeElapsedSeconds += DeltaTime;
	const float OffsetZ = FMath::Sin(ShakeElapsedSeconds * ShakeFrequencyHz * 2.f * PI) * ShakeAmplitude;
	SetActorLocation(OriginalRootLocation + FVector(0.f, 0.f, OffsetZ));
}

// ─────────────────────────────────────────────────────────────────────────────
// Registrar overlaps en BeginPlay
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
