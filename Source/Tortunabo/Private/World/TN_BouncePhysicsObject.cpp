#include "World/TN_BouncePhysicsObject.h"
#include "Components/StaticMeshComponent.h"
#include "Player/TortugaCharacter.h"

ATN_BouncePhysicsObject::ATN_BouncePhysicsObject()
{
	// Un balón rueda en los 3 ejes — el padre bloquea los tres por defecto.
	bLockRotationX = false;
	bLockRotationY = false;
	bLockRotationZ = false;
}

void ATN_BouncePhysicsObject::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && Mesh)
	{
		Mesh->OnComponentHit.AddDynamic(this, &ATN_BouncePhysicsObject::OnKickHit);
	}
}

void ATN_BouncePhysicsObject::OnKickHit(UPrimitiveComponent* /*HitComp*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/, const FHitResult& /*Hit*/)
{
	ATortugaCharacter* Tortuga = Cast<ATortugaCharacter>(OtherActor);
	if (!Tortuga || !Mesh) { return; }

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (const float* Last = LastKickTimeByPlayer.Find(Tortuga))
	{
		if (Now - *Last < PerPlayerCooldown) { return; }
	}
	LastKickTimeByPlayer.Add(Tortuga, Now);

	// Dirección de pateo: del jugador hacia la bola (la pelota sale hacia delante
	// respecto al punto de contacto, no hacia atrás contra el jugador).
	FVector HorizontalDir = GetActorLocation() - Tortuga->GetActorLocation();
	HorizontalDir.Z = 0.f;
	HorizontalDir = HorizontalDir.GetSafeNormal();

	const FVector DeltaVelocity =
		HorizontalDir * KickHorizontalBoost +
		FVector(0.f, 0.f, KickVerticalBoost);

	// bVelChange=true → el impulso se aplica como cambio directo de velocidad
	// ignorando la masa, así el "feel" del kick no depende del Mass Scale del BP.
	Mesh->AddImpulse(DeltaVelocity, NAME_None, /*bVelChange=*/true);
}
