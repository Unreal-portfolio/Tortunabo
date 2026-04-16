#include "World/TN_InkProjectile.h"
#include "Player/TortugaCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

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
}

void ATN_InkProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Actualizar el radio de la esfera con el valor configurado (puede diferir del default del ctor)
	CollisionSphere->SetSphereRadius(ProjectileRadius);

	if (HasAuthority())
	{
		CollisionSphere->OnComponentHit.AddDynamic(this, &ATN_InkProjectile::OnSphereHit);

		// Auto-destroy tras LifetimeSeconds
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AActor::Destroy);
		GetWorldTimerManager().SetTimer(LifetimeTimerHandle, Del, LifetimeSeconds, false);
	}

	// El Tick solo importa en el servidor (mueve el actor)
	SetActorTickEnabled(HasAuthority());
}

void ATN_InkProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LifetimeTimerHandle);
	Super::EndPlay(EndPlayReason);
}

// ── Tick ────────────────────────────────────────────────────────────────────────

void ATN_InkProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || bHasHit) { return; }

	// Movimiento lineal: sin gravedad, sin proyectil movement component —
	// simplificado según la spec. Si se quiere gravedad, ajustar LaunchVelocity.Z aquí.
	const FVector NewLocation = GetActorLocation() + LaunchVelocity * DeltaTime;
	SetActorLocation(NewLocation, true);  // bSweep=true → genera hit events al barrer
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

	ATortugaCharacter* HitCharacter = Cast<ATortugaCharacter>(OtherActor);
	if (HitCharacter)
	{
		bHasHit = true;
		MulticastApplyInkEffect(HitCharacter, InkDurationSeconds);
		Destroy();
	}
	else
	{
		// Impacto contra geometría — destruir igualmente
		bHasHit = true;
		Destroy();
	}
}

// ── Multicast ──────────────────────────────────────────────────────────────────

void ATN_InkProjectile::MulticastApplyInkEffect_Implementation(ATortugaCharacter* Target, float Duration)
{
	if (!Target) { return; }

	// Solo la máquina que controla localmente al jugador afectado muestra el efecto
	if (Target->IsLocallyControlled())
	{
		OnInkEffect(Duration);
	}
}
