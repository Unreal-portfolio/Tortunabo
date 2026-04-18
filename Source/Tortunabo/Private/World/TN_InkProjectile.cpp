#include "World/TN_InkProjectile.h"
#include "Player/TortugaCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

ATN_InkProjectile::ATN_InkProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(ProjectileRadius);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionSphere->SetNotifyRigidBodyCollision(true);  // genera Hit events
	CollisionSphere->SetGenerateOverlapEvents(false);

	InkMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InkMesh"));
	InkMesh->SetupAttachment(CollisionSphere);
	InkMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InkMesh->SetIsReplicated(false);

	// Fallback a la esfera básica del engine si el BP no asigna mesh.
	// Sin esto, el proyectil era invisible y no se podía apuntar: el jugador
	// no veía la tinta volar, solo el efecto de pantalla al impactar.
	// El BP de InkProjectile puede sobrescribirlo con un mesh custom + material.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackSphere(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (FallbackSphere.Succeeded())
	{
		InkMesh->SetStaticMesh(FallbackSphere.Object);
		// Escalar la esfera del engine (50cm radio nativo) al ProjectileRadius por defecto.
		const float DefaultScale = (ProjectileRadius * 2.f) / 100.f;
		InkMesh->SetRelativeScale3D(FVector(DefaultScale));
	}
}

void ATN_InkProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Actualizar el radio de la esfera con el valor configurado (puede diferir del default del ctor)
	CollisionSphere->SetSphereRadius(ProjectileRadius);

	// Reescalar el mesh fallback al radio real configurado (el BP podría haber cambiado ProjectileRadius).
	// Si el BP asignó un mesh custom no queremos pisarlo — detectamos por nombre del engine sphere.
	if (InkMesh && InkMesh->GetStaticMesh() &&
	    InkMesh->GetStaticMesh()->GetFName() == TEXT("Sphere"))
	{
		const float Scale = (ProjectileRadius * 2.f) / 100.f;
		InkMesh->SetRelativeScale3D(FVector(Scale));
	}

	if (HasAuthority())
	{
		CollisionSphere->OnComponentHit.AddDynamic(this, &ATN_InkProjectile::OnSphereHit);

		// Auto-destroy tras LifetimeSeconds
		SetLifeSpan(LifetimeSeconds);
	}

	// El Tick solo importa en el servidor (mueve el actor)
	SetActorTickEnabled(HasAuthority());
}

void ATN_InkProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// ── Tick ────────────────────────────────────────────────────────────────────────

void ATN_InkProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || bHasHit) { return; }

	// Gravedad manual → trayectoria parabólica. Con GravityAcceleration=0 queda recto.
	LaunchVelocity.Z -= GravityAcceleration * DeltaTime;

	const FVector NewLocation = GetActorLocation() + LaunchVelocity * DeltaTime;
	SetActorLocation(NewLocation, true);  // bSweep=true → genera hit events al barrer

	// Rotar el actor para que el mesh apunte en la dirección del vuelo
	if (!LaunchVelocity.IsNearlyZero())
	{
		SetActorRotation(LaunchVelocity.Rotation());
	}
}

// ── Spawn helper ───────────────────────────────────────────────────────────────

ATN_InkProjectile* ATN_InkProjectile::Spawn(const UObject* WorldContextObject,
	TSubclassOf<ATN_InkProjectile> Class,
	FVector Origin,
	FVector Direction,
	float Speed)
{
	if (!WorldContextObject || !Class) { return nullptr; }

	UWorld* World = WorldContextObject->GetWorld();
	if (!World) { return nullptr; }

	// Only the server can spawn replicated projectiles
	if (World->GetNetMode() == NM_Client) { return nullptr; }

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATN_InkProjectile* Projectile = World->SpawnActor<ATN_InkProjectile>(Class, Origin,
		Direction.Rotation(), Params);

	if (Projectile)
	{
		Projectile->LaunchVelocity = Direction.GetSafeNormal() * Speed;
	}

	return Projectile;
}

// ── Hit ────────────────────────────────────────────────────────────────────────

void ATN_InkProjectile::OnSphereHit(UPrimitiveComponent* /*HitComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, FVector /*NormalImpulse*/,
	const FHitResult& /*Hit*/)
{
	if (!HasAuthority() || bHasHit) { return; }

	bHasHit = true;

	ATortugaCharacter* HitCharacter = Cast<ATortugaCharacter>(OtherActor);
	if (HitCharacter)
	{
		MulticastApplyInkEffect(HitCharacter, InkDurationSeconds);
	}

	// Defer destruction to let the reliable multicast flush before the actor is torn down
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetLifeSpan(1.0f);
}

// ── Multicast ──────────────────────────────────────────────────────────────────

void ATN_InkProjectile::MulticastApplyInkEffect_Implementation(ATortugaCharacter* Target, float Duration)
{
	if (!Target) { return; }

	// ApplyInkEffect comprueba internamente IsLocallyControlled — seguro llamar en todos
	Target->ApplyInkEffect(Duration);
}
