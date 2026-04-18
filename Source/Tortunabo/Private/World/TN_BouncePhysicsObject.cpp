#include "World/TN_BouncePhysicsObject.h"
#include "Components/StaticMeshComponent.h"
#include "Player/TortugaCharacter.h"

ATN_BouncePhysicsObject::ATN_BouncePhysicsObject()
{
	// Una bola rueda en los 3 ejes — el padre bloquea los tres por defecto.
	bLockRotationX = false;
	bLockRotationY = false;
	bLockRotationZ = false;
}

void ATN_BouncePhysicsObject::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && Mesh)
	{
		Mesh->OnComponentHit.AddDynamic(this, &ATN_BouncePhysicsObject::OnBounceHit);
	}
}

void ATN_BouncePhysicsObject::OnBounceHit(UPrimitiveComponent* /*HitComp*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/, const FHitResult& /*Hit*/)
{
	ATortugaCharacter* Tortuga = Cast<ATortugaCharacter>(OtherActor);
	if (!Tortuga) { return; }

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (const float* Last = LastBounceTimeByPlayer.Find(Tortuga))
	{
		if (Now - *Last < PerPlayerCooldown) { return; }
	}
	LastBounceTimeByPlayer.Add(Tortuga, Now);

	FVector HorizontalDir = Tortuga->GetActorLocation() - GetActorLocation();
	HorizontalDir.Z = 0.f;
	HorizontalDir = HorizontalDir.GetSafeNormal();

	const FVector Impulse =
		HorizontalDir * PlayerBounceHorizontal +
		FVector(0.f, 0.f, PlayerBounceImpulseZ);

	// LaunchCharacter setea PendingLaunchVelocity en el CMC: respeta autoridad
	// y predicción de cliente — no hay que replicar manualmente.
	Tortuga->LaunchCharacter(Impulse, /*XYOverride=*/false, /*ZOverride=*/true);
}
