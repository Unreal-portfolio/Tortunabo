#include "World/TN_PufferFishActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/TortugaCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ATN_PufferFishActor::ATN_PufferFishActor()
{
	// Hereda toda la config del padre (replicación, projectile movement, etc.)
}

void ATN_PufferFishActor::BeginPlay()
{
	Super::BeginPlay();
	// Nota: OriginalScale se captura en ApplyLaunchDataIfReady() override,
	// DESPUÉS de que el padre aplique la escala real del DataTable.

	if (HasAuthority())
	{
		// ── Reemplazar el binding de OnProjectileStop del padre ───────────────
		if (ProjectileMovement)
		{
			ProjectileMovement->OnProjectileStop.RemoveAll(this);
			ProjectileMovement->OnProjectileStop.AddDynamic(
				this, &ATN_PufferFishActor::OnPufferProjectileStopped);
		}

		// ── Desactivar knockdown por impacto directo (heredado del padre) ────
		if (CollisionSphere)
		{
			CollisionSphere->OnComponentHit.RemoveAll(this);
		}

		const float Delay = FMath::FRandRange(InflateDelayMin, InflateDelayMax);
		GetWorldTimerManager().SetTimer(
			InflateDelayTimerHandle, this, &ATN_PufferFishActor::Inflate, Delay, false);

		UE_LOG(LogTemp, Log, TEXT("[PufferFish] '%s' — se inflará en %.1fs"), *GetName(), Delay);
	}
}

void ATN_PufferFishActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_PufferFishActor, PufferState);
}

// ── Override: capturar OriginalScale tras aplicar escala real del DataTable ──

void ATN_PufferFishActor::ApplyLaunchDataIfReady()
{
	Super::ApplyLaunchDataIfReady();

	if (Mesh)
	{
		OriginalScale = Mesh->GetRelativeScale3D();
		UE_LOG(LogTemp, Log, TEXT("[PufferFish] OriginalScale capturada: (%.2f,%.2f,%.2f)"),
			OriginalScale.X, OriginalScale.Y, OriginalScale.Z);

		if (PufferState != ETN_PufferState::Flying)
		{
			ApplyPufferVisual();
		}
	}
}

// ── OnProjectileStop personalizado ───────────────────────────────────────────

void ATN_PufferFishActor::OnPufferProjectileStopped(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || bPickupSpawned) { return; }

	if (PufferState != ETN_PufferState::Deflated) { return; }

	FVector StopLocation = GetActorLocation();
	if (ImpactResult.IsValidBlockingHit())
	{
		StopLocation.X = ImpactResult.ImpactPoint.X;
		StopLocation.Y = ImpactResult.ImpactPoint.Y;
		StopLocation.Z = ImpactResult.ImpactPoint.Z;
	}

	SpawnPickupAtLocation(StopLocation);
	Destroy();
}

// ── Inflate ───────────────────────────────────────────────────────────────────

void ATN_PufferFishActor::Inflate()
{
	if (!HasAuthority()) { return; }

	PufferState = ETN_PufferState::Inflating;

	// ── Desactivar colisión ANTES de escalar ─────────────────────
	// Sin esto, al escalar 5x la geometría se solapa con las cápsulas
	// de personajes cercanos y el motor de físicas puede depenetrarlos de forma
	// errática. El empuje de bomba lo manejamos nosotros en BombExplosionTick.
	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	ApplyPufferVisual();

	// ── Bomba: empujar con ventana multi-frame ────────────────────────────
	// Tickeamos a ~60fps durante BombWindowSeconds (0.15s).
	// Garantiza que personajes que entren en rango durante la expansión
	// también sean empujados como por una bomba.
	BombElapsedTime = 0.f;
	BombHitCharacters.Reset();

	// Primer tick inmediato (no esperar al próximo frame)
	BombExplosionTick();

	// Timer repetitivo para el resto de la ventana
	GetWorldTimerManager().SetTimer(
		BombTickTimerHandle, this, &ATN_PufferFishActor::BombExplosionTick,
		BombTickInterval, true);

	// Programar desinflado
	GetWorldTimerManager().SetTimer(
		DeflatTimerHandle, this, &ATN_PufferFishActor::Deflate, InflateDuration, false);
}

// ── BombExplosionTick ─────────────────────────────────────────────────────────

void ATN_PufferFishActor::BombExplosionTick()
{
	BombElapsedTime += BombTickInterval;

	// Ventana expirada → detener el tick
	if (BombElapsedTime > BombWindowSeconds)
	{
		GetWorldTimerManager().ClearTimer(BombTickTimerHandle);
		return;
	}

	if (!GetWorld() || !HasAuthority()) { return; }

	const FVector Origin = GetActorLocation();

	// ── Búsqueda directa por TActorIterator ──────────────────────────────
	// Más fiable que OverlapMultiByObjectType que depende de canales/responses.
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* HitCharacter = *It;
		if (!HitCharacter || HitCharacter->IsPendingKillPending()) { continue; }

		// No re-empujar al mismo personaje
		TWeakObjectPtr<ACharacter> WeakChar(HitCharacter);
		if (BombHitCharacters.Contains(WeakChar)) { continue; }

		const float Dist = FVector::Dist(Origin, HitCharacter->GetActorLocation());
		if (Dist > InflateRadius) { continue; }

		// ── Calcular fuerza de empuje ─────────────────────────────────────
		const float NormalizedDist = FMath::Clamp(Dist / InflateRadius, 0.f, 1.f);
		const float ForceMagnitude = InflatePushForce * (1.f - NormalizedDist);

		FVector PushDir = (HitCharacter->GetActorLocation() - Origin).GetSafeNormal();
		// Componente vertical mínimo para lanzar por los aires
		PushDir.Z = FMath::Max(PushDir.Z, 0.7f);
		PushDir.Normalize();

		// Marcar como empujado ANTES de aplicar efectos
		BombHitCharacters.Add(WeakChar);

		UE_LOG(LogTemp, Log, TEXT("[PufferFish] BOMB pushed %s — force=%.0f dist=%.0f elapsed=%.3fs"),
			*GetNameSafe(HitCharacter), ForceMagnitude, Dist, BombElapsedTime);

		// ── Knockdown PRIMERO (bloquea input + visual) ────────────────────
		if (ForceMagnitude >= MinKnockdownForce)
		{
			if (ATortugaCharacter* Tortuga = Cast<ATortugaCharacter>(HitCharacter))
			{
				Tortuga->ApplyKnockdown(PufferKnockdownDuration);
			}
		}

		// ── Lanzar DESPUÉS ────────────────────────────────────────────────
		// ApplyKnockdown setea MOVE_None, pero LaunchCharacter pone
		// PendingLaunchVelocity que en el siguiente tick fuerza MOVE_Falling
		// via HandlePendingLaunch → el personaje vuela por los aires.
		// Al aterrizar, TortugaCharacter::Tick re-desactiva el movimiento
		// si sigue knocked down.
		HitCharacter->LaunchCharacter(PushDir * ForceMagnitude, true, true);
	}
}

// ── Deflate ───────────────────────────────────────────────────────────────────

void ATN_PufferFishActor::Deflate()
{
	if (!HasAuthority()) { return; }

	// Limpiar timer de bomba si aún estuviera activo
	GetWorldTimerManager().ClearTimer(BombTickTimerHandle);

	PufferState = ETN_PufferState::Deflated;
	ApplyPufferVisual();

	UE_LOG(LogTemp, Log, TEXT("[PufferFish] '%s' — desinflado. Spawneando pickup en (%.0f,%.0f,%.0f)."),
		*GetName(), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);

	SpawnPickupAtLocation(GetActorLocation());
	Destroy();
}

// ── Replicación visual ────────────────────────────────────────────────────────

void ATN_PufferFishActor::OnRep_PufferState()
{
	ApplyPufferVisual();
}

void ATN_PufferFishActor::ApplyPufferVisual()
{
	if (!Mesh) { return; }

	switch (PufferState)
	{
	case ETN_PufferState::Flying:
		Mesh->SetRelativeScale3D(OriginalScale);
		break;

	case ETN_PufferState::Inflating:
		// Desactivar colisión en TODAS las máquinas: evita que el motor
		// de físicas depenetere jugadores por solapamiento con el mesh escalado.
		// El empuje real se maneja por BombExplosionTick (servidor).
		if (CollisionSphere)
		{
			CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetRelativeScale3D(OriginalScale * InflateScale);
		break;

	case ETN_PufferState::Deflated:
		Mesh->SetRelativeScale3D(OriginalScale);
		break;
	}
}
