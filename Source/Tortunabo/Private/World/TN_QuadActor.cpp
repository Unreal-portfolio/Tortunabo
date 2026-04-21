#include "World/TN_QuadActor.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

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

	// ── Cylinder por defecto para las ruedas ────────────────────────────────────
	// BasicShapes/Cylinder: radio 50 cm, alto 100 cm a escala 1,1,1.
	// La escala se ajusta en BeginPlay con los valores de WheelCapsuleRadius/HalfHeight.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder"));

	// ── Rueda izquierda ──────────────────────────────────────────────────────────
	WheelLeft = CreateDefaultSubobject<UCapsuleComponent>(TEXT("WheelLeft"));
	WheelLeft->SetupAttachment(QuadMesh);
	// Roll=90° orienta el eje de la cápsula a lo largo del eje Y local (axle del quad).
	WheelLeft->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	WheelLeft->SetRelativeLocation(FVector(0.f, -WheelLateralOffset, 0.f));
	WheelLeft->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WheelLeft->SetCollisionResponseToAllChannels(ECR_Ignore);
	WheelLeft->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// Mesh visual izquierdo — hijo de la cápsula, hereda su Roll=90 para alinearse
	WheelLeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelLeftMesh"));
	WheelLeftMesh->SetupAttachment(WheelLeft);
	WheelLeftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WheelLeftMesh->SetIsReplicated(false);
	if (CylinderAsset.Succeeded()) { WheelLeftMesh->SetStaticMesh(CylinderAsset.Object); }

	// ── Rueda derecha ────────────────────────────────────────────────────────────
	WheelRight = CreateDefaultSubobject<UCapsuleComponent>(TEXT("WheelRight"));
	WheelRight->SetupAttachment(QuadMesh);
	WheelRight->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	WheelRight->SetRelativeLocation(FVector(0.f, WheelLateralOffset, 0.f));
	WheelRight->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WheelRight->SetCollisionResponseToAllChannels(ECR_Ignore);
	WheelRight->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// Mesh visual derecho
	WheelRightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRightMesh"));
	WheelRightMesh->SetupAttachment(WheelRight);
	WheelRightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WheelRightMesh->SetIsReplicated(false);
	if (CylinderAsset.Succeeded()) { WheelRightMesh->SetStaticMesh(CylinderAsset.Object); }
}

void ATN_QuadActor::BeginPlay()
{
	Super::BeginPlay();

	// Actualizar tamaño de cápsulas con los valores UPROPERTY (pueden haberse
	// cambiado en el Blueprint hijo antes de BeginPlay)
	WheelLeft->SetCapsuleSize(WheelCapsuleRadius, WheelCapsuleHalfHeight);
	WheelRight->SetCapsuleSize(WheelCapsuleRadius, WheelCapsuleHalfHeight);
	WheelLeft->SetRelativeLocation(FVector(0.f, -WheelLateralOffset, 0.f));
	WheelRight->SetRelativeLocation(FVector(0.f, WheelLateralOffset, 0.f));

	// ── Escalar mesh de ruedas para que coincidan con la cápsula ─────────────
	// Cylinder BasicShapes a escala 1,1,1: radio 50 cm, semialtura 50 cm.
	// La rueda hereda Roll=90 de la cápsula → su eje Z local queda alineado con
	// el eje Y del quad (axle lateral). Radio → escala X/Y; ancho → escala Z.
	// Al ser hijos de la cápsula que a su vez es hijo de QuadMesh, la escala
	// mundial se multiplica por la del quad → las ruedas escalan con el vehículo.
	if (WheelLeftMesh && WheelRightMesh)
	{
		const float RadiusScale    = WheelCapsuleRadius     / 50.f;
		const float HalfWidthScale = WheelCapsuleHalfHeight / 50.f;
		const FVector WheelScale(RadiusScale, RadiusScale, HalfWidthScale);
		WheelLeftMesh->SetRelativeScale3D(WheelScale);
		WheelRightMesh->SetRelativeScale3D(WheelScale);
	}

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
		// EndOffsetLocal está en local space — transformarlo al world space del
		// spawner para que el quad viaje respecto a su posición de spawn.
		const FVector WorldEnd = GetActorTransform().TransformPosition(EndOffsetLocal);
		Quad->InitializeTravel(WorldEnd, QuadSpeed);
	}
}
