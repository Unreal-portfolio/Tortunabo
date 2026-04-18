#include "World/TN_PhysicsObjectActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

ATN_PhysicsObjectActor::ATN_PhysicsObjectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));

	// CCD: evita el tunneling clásico cuando el jugador empuja el objeto
	// contra un muro estático y se mete dentro de la geometría.
	// Coste pequeño para actores puntuales — aceptable aquí.
	Mesh->SetUseCCD(true);

	bReplicates = true;
	SetReplicatingMovement(true);

	// Actualizaciones frecuentes para un objeto físico preciso en todos los clientes.
	SetNetUpdateFrequency(50.f);
	SetMinNetUpdateFrequency(10.f);

	// Empieza dormido — 0 bytes hasta que algo lo golpee.
	NetDormancy = DORM_DormantAll;
}

void ATN_PhysicsObjectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DormancyCheckTimer);
	Super::EndPlay(EndPlayReason);
}

void ATN_PhysicsObjectActor::BeginPlay()
{
	Super::BeginPlay();

	// Locks de rotación — aplicar antes de que el cuerpo físico arranque para
	// que el constraint DOF se construya con los ejes ya fijados. CreateDOFLock
	// fuerza la reconstrucción si el body ya existía (seguridad ante reruns).
	if (Mesh)
	{
		Mesh->BodyInstance.bLockXRotation = bLockRotationX;
		Mesh->BodyInstance.bLockYRotation = bLockRotationY;
		Mesh->BodyInstance.bLockZRotation = bLockRotationZ;
		Mesh->BodyInstance.CreateDOFLock();
	}

	if (HasAuthority())
	{
		// Despertarse inmediatamente para que los clientes reciban la posición inicial.
		FlushNetDormancy();
		Mesh->OnComponentHit.AddDynamic(this, &ATN_PhysicsObjectActor::OnMeshHit);
	}
	else
	{
		// Clientes no simulan — la posición llega replicada desde el servidor.
		Mesh->SetSimulatePhysics(false);
	}
}

void ATN_PhysicsObjectActor::OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Despertar: el servidor empieza a enviar actualizaciones de posición.
	FlushNetDormancy();

	// Iniciar comprobación periódica para volver a dormir cuando pare.
	if (!GetWorldTimerManager().IsTimerActive(DormancyCheckTimer))
	{
		GetWorldTimerManager().SetTimer(
			DormancyCheckTimer, this,
			&ATN_PhysicsObjectActor::TryEnterDormancy,
			DormancyCheckInterval, /*bLoop=*/true);
	}
}

void ATN_PhysicsObjectActor::TryEnterDormancy()
{
	const float SpeedSq = Mesh->GetComponentVelocity().SizeSquared();
	if (SpeedSq > SleepVelocityThreshold * SleepVelocityThreshold)
	{
		return; // Aún en movimiento — seguir comprobando.
	}

	// Detenido: volver a dormir y cancelar el timer.
	SetNetDormancy(DORM_DormantAll);
	GetWorldTimerManager().ClearTimer(DormancyCheckTimer);
}
