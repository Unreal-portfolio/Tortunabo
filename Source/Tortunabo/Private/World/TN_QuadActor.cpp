#include "World/TN_QuadActor.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
// ATN_QuadActor
// ─────────────────────────────────────────────────────────────────────────────

ATN_QuadActor::ATN_QuadActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;  // Siempre visible para todos los clientes
	SetReplicateMovement(true);

	QuadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("QuadMesh"));
	SetRootComponent(QuadMesh);
	// El cuerpo no tiene colisión con pawns — solo las ruedas matan
	QuadMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	QuadMesh->SetIsReplicated(false);

	// ── Rueda izquierda ──────────────────────────────────────────────────────────
	WheelLeft = CreateDefaultSubobject<UCapsuleComponent>(TEXT("WheelLeft"));
	WheelLeft->SetupAttachment(QuadMesh);
	// Rotar 90° para que la cápsula quede horizontal (orientada en la dirección de avance)
	// Roll=90 orienta el eje de la cápsula a lo largo del eje Y (axle del quad),
	// dando forma de disco/cilindro visto desde el frente.
	WheelLeft->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	WheelLeft->SetRelativeLocation(FVector(0.f, -WheelLateralOffset, 0.f));
	WheelLeft->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WheelLeft->SetCollisionResponseToAllChannels(ECR_Ignore);
	WheelLeft->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// ── Rueda derecha ────────────────────────────────────────────────────────────
	WheelRight = CreateDefaultSubobject<UCapsuleComponent>(TEXT("WheelRight"));
	WheelRight->SetupAttachment(QuadMesh);
	WheelRight->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	WheelRight->SetRelativeLocation(FVector(0.f, WheelLateralOffset, 0.f));
	WheelRight->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WheelRight->SetCollisionResponseToAllChannels(ECR_Ignore);
	WheelRight->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATN_QuadActor::BeginPlay()
{
	Super::BeginPlay();

	// Actualizar tamaño de cápsulas con los valores UPROPERTY (que pueden haberse
	// cambiado en el Blueprint hijo antes de BeginPlay)
	WheelLeft->SetCapsuleSize(WheelCapsuleRadius, WheelCapsuleHalfHeight);
	WheelRight->SetCapsuleSize(WheelCapsuleRadius, WheelCapsuleHalfHeight);
	WheelLeft->SetRelativeLocation(FVector(0.f, -WheelLateralOffset, 0.f));
	WheelRight->SetRelativeLocation(FVector(0.f, WheelLateralOffset, 0.f));

	// Solo el servidor mueve el quad y aplica kills
	SetActorTickEnabled(HasAuthority());

	if (HasAuthority())
	{
		WheelLeft->OnComponentBeginOverlap.AddDynamic(this, &ATN_QuadActor::OnWheelOverlap);
		WheelRight->OnComponentBeginOverlap.AddDynamic(this, &ATN_QuadActor::OnWheelOverlap);
	}
}

void ATN_QuadActor::InitializeTravel(const FVector& InEndLocation, float InSpeed)
{
	EndLocation  = InEndLocation;
	TravelSpeed  = FMath::Max(InSpeed, 100.f);
	bTraveling   = true;

	// Orientar el quad hacia el destino al spawnear
	const FVector Dir = (EndLocation - GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}
}

void ATN_QuadActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority() || !bTraveling) { return; }

	const FVector CurrentLoc = GetActorLocation();
	const FVector Dir        = (EndLocation - CurrentLoc).GetSafeNormal2D();
	const float   Dist       = FVector::Dist2D(CurrentLoc, EndLocation);
	const float   Step       = TravelSpeed * DeltaTime;

	if (Dist <= Step)
	{
		// Llegó al destino — autodestrucción
		Destroy();
		return;
	}

	SetActorLocation(CurrentLoc + Dir * Step);
}

// ── Overlap ruedas → matar ────────────────────────────────────────────────────

void ATN_QuadActor::OnWheelOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) { return; }

	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!IsValid(Char)) { return; }

	const ATN_CoopPlayerState* PS = Char->GetPlayerState<ATN_CoopPlayerState>();
	if (!PS || !PS->bIsAlive || PS->bIsEliminated) { return; }

	Char->RequestKill(this);
}

// ─────────────────────────────────────────────────────────────────────────────
// ATN_QuadSpawner
// ─────────────────────────────────────────────────────────────────────────────

ATN_QuadSpawner::ATN_QuadSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Solo existe en servidor
}

void ATN_QuadSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Solo el servidor spawnea
	if (!HasAuthority()) { return; }
	if (!QuadClass) { return; }

	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ATN_QuadSpawner::SpawnQuad);
	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle, Delegate, SpawnInterval, /*bLoop=*/true, InitialDelay);
}

void ATN_QuadSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ATN_QuadSpawner::SpawnQuad()
{
	if (!HasAuthority() || !QuadClass || !GetWorld()) { return; }

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATN_QuadActor* Quad = GetWorld()->SpawnActor<ATN_QuadActor>(
		QuadClass, GetActorTransform(), Params);

	if (Quad)
	{
		Quad->InitializeTravel(EndLocation, QuadSpeed);
	}
}
