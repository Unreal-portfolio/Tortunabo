#include "World/TN_BouncePhysicsObject.h"
#include "Components/StaticMeshComponent.h"
#include "Player/TortugaCharacter.h"

ATN_BouncePhysicsObject::ATN_BouncePhysicsObject()
{
	// Un balón rueda en los 3 ejes — el padre bloquea los tres por defecto.
	bLockRotationX = false;
	bLockRotationY = false;
	bLockRotationZ = false;

	// Crush detection está pensado para cajas empujables que se atascan entre
	// paredes; una bola que rebota libremente nunca debería destruirse sola, así
	// que desactivamos la detección (también ahorra 1 timer + 6 raycasts/2s).
	bEnableCrushDetection = false;

	// CLAVE: la bola SÍ usa físicas reales (rebota, rueda, recibe impulsos al
	// patearla). El padre por defecto pasó a kinematic-push para el cubo —
	// aquí lo apagamos para mantener Chaos behavior.
	bUseKinematicPush = false;

	// Umbral de velocidad para dormir más alto que el default del padre (15 cm/s).
	// Una bola con inercia rueda largo rato a <30 cm/s sin llegar a reposo real;
	// si no subimos el umbral, nunca entra dormancy y sigue enviando packets.
	SleepVelocityThreshold = 30.f;
}

void ATN_BouncePhysicsObject::BeginPlay()
{
	Super::BeginPlay();

	// Damping en BodyInstance — tras Super::BeginPlay() el BodyInstance ya
	// existe (Super configuró DOF locks). Aplicar damping aquí hace que la
	// bola decelere como sobre césped en lugar de rodar indefinidamente.
	if (Mesh)
	{
		Mesh->BodyInstance.LinearDamping  = LinearDamping;
		Mesh->BodyInstance.AngularDamping = AngularDamping;
		Mesh->BodyInstance.UpdateDampingProperties();
	}

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

	// Empujar el snapshot al cliente inmediatamente para que el feel del kick
	// no se perciba desincronizado (sin esto, a 30Hz el cliente podría ver la
	// bola moverse ~33ms después del impacto).
	FlushNetDormancy();
	ForceNetUpdate();
}
