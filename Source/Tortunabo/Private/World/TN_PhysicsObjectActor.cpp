#include "World/TN_PhysicsObjectActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "NiagaraSystem.h"

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
	GetWorldTimerManager().ClearTimer(CrushCheckTimer);
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

		if (bEnableCrushDetection && CrushCheckInterval > 0.f)
		{
			GetWorldTimerManager().SetTimer(
				CrushCheckTimer, this,
				&ATN_PhysicsObjectActor::CheckForCrush,
				CrushCheckInterval, /*bLoop=*/true);
		}
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

// ─────────────────────────────────────────────────────────────────────────────
// Crush detection — destruye el actor si queda atrapado entre geometría estática
// ─────────────────────────────────────────────────────────────────────────────

void ATN_PhysicsObjectActor::CheckForCrush()
{
	if (!Mesh || !HasAuthority()) { return; }

	// Lanzamos 3 pares de rays opuestos desde el centro del actor. Si un par
	// (por ejemplo +X y -X) golpea geometría estática dentro del bounds, ese eje
	// está "bloqueado". Con 2 o más ejes bloqueados el actor no tiene salida →
	// lo consideramos aplastado y lo destruimos con un poof.
	const FVector Origin = GetActorLocation();
	const float Radius = Mesh->Bounds.SphereRadius;
	const float CheckDist = Radius * 1.15f;

	static const FVector Dirs[3] = {
		FVector(1.f, 0.f, 0.f),
		FVector(0.f, 1.f, 0.f),
		FVector(0.f, 0.f, 1.f)
	};

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PhysicsObjectCrush), /*bTraceComplex=*/false, this);
	int32 BlockedAxes = 0;

	UWorld* World = GetWorld();
	if (!World) { return; }

	for (const FVector& Dir : Dirs)
	{
		FHitResult HitPos, HitNeg;
		const bool bPos = World->LineTraceSingleByChannel(HitPos, Origin, Origin + Dir * CheckDist, ECC_WorldStatic, Params);
		const bool bNeg = World->LineTraceSingleByChannel(HitNeg, Origin, Origin - Dir * CheckDist, ECC_WorldStatic, Params);
		if (bPos && bNeg) { ++BlockedAxes; }
	}

	if (BlockedAxes >= 2)
	{
		MulticastCrushPoof();
		// LifeSpan corto da tiempo al Multicast a replicarse antes del Destroy.
		SetLifeSpan(0.2f);
		GetWorldTimerManager().ClearTimer(CrushCheckTimer);
		GetWorldTimerManager().ClearTimer(DormancyCheckTimer);
	}
}

void ATN_PhysicsObjectActor::MulticastCrushPoof_Implementation()
{
	const FVector Loc = GetActorLocation();
	if (CrushPoofVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, CrushPoofVFX, Loc);
	}
	if (CrushPoofSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, CrushPoofSound, Loc);
	}
	// Ocultar mesh inmediatamente para que el poof sea el único visual.
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}
