#include "World/TN_PufferFishActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Player/TortugaCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
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
		// El padre enlaza OnProjectileStopped que spawnea pickup + Destroy.
		// Nosotros necesitamos uno propio que solo actúe cuando el ciclo
		// inflate/deflate ya terminó (estado Deflated). De lo contrario,
		// StopSimulating() durante el inflado destruiría el pez antes del deflate.
		if (ProjectileMovement)
		{
			ProjectileMovement->OnProjectileStop.RemoveAll(this);
			ProjectileMovement->OnProjectileStop.AddDynamic(
				this, &ATN_PufferFishActor::OnPufferProjectileStopped);
		}

		// ── Desactivar knockdown por impacto directo (heredado del padre) ────
		// El PufferFish solo knockea mediante la explosión de inflación.
		// Removemos el OnComponentHit del padre para que los impactos directos
		// solo reboten sin noquear.
		if (Mesh)
		{
			Mesh->OnComponentHit.RemoveAll(this);
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

	// El padre acaba de llamar Mesh->SetRelativeScale3D(ThrowData.MeshScale).
	// Capturamos la escala resultante como "original" para inflate/deflate.
	if (Mesh)
	{
		OriginalScale = Mesh->GetRelativeScale3D();
		UE_LOG(LogTemp, Log, TEXT("[PufferFish] OriginalScale capturada: (%.2f,%.2f,%.2f)"),
			OriginalScale.X, OriginalScale.Y, OriginalScale.Z);

		// Si ThrowData llegó DESPUÉS de OnRep_PufferState (orden de replicación
		// no garantizado), reaplicar el visual con la escala correcta.
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

	// Solo spawnear pickup si el ciclo inflate/deflate ya completó.
	// Flying / Inflating → no hacer nada; Deflate() se encarga.
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
	ApplyPufferVisual();

	// ── NO detenemos la bola ─────────────────────────────────────────────────
	// El pez se infla mientras sigue volando/rodando. Si el proyectil se detiene
	// durante el inflado, OnPufferProjectileStopped lo ignora (PufferState != Deflated)
	// y es Deflate() quien finalmente spawnea el pickup.

	// ── Empujar a todos los personajes en rango (incluido el lanzador) ────
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TN_PufferInflate), false, this);

	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(InflateRadius),
		Params);

	for (const FOverlapResult& Result : Overlaps)
	{
		ACharacter* HitCharacter = Cast<ACharacter>(Result.GetActor());
		if (!HitCharacter) { continue; }

		const float Dist = FVector::Dist(GetActorLocation(), HitCharacter->GetActorLocation());
		const float NormalizedDist = FMath::Clamp(Dist / InflateRadius, 0.f, 1.f);
		const float ForceMagnitude = InflatePushForce * (1.f - NormalizedDist);

		FVector PushDir = (HitCharacter->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		PushDir.Z = FMath::Max(PushDir.Z, 0.7f);
		PushDir.Normalize();

		HitCharacter->LaunchCharacter(PushDir * ForceMagnitude, true, true);

		UE_LOG(LogTemp, Log, TEXT("[PufferFish] Pushed %s force=%.0f dist=%.0f"),
			*GetNameSafe(HitCharacter), ForceMagnitude, Dist);

		if (ForceMagnitude >= MinKnockdownForce)
		{
			if (ATortugaCharacter* Tortuga = Cast<ATortugaCharacter>(HitCharacter))
			{
				Tortuga->ApplyKnockdown(PufferKnockdownDuration);
			}
		}
	}

	// Programar desinflado
	GetWorldTimerManager().SetTimer(
		DeflatTimerHandle, this, &ATN_PufferFishActor::Deflate, InflateDuration, false);
}

// ── Deflate ───────────────────────────────────────────────────────────────────

void ATN_PufferFishActor::Deflate()
{
	if (!HasAuthority()) { return; }

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
		// OriginalScale ya tiene la escala real del DataTable (capturada en ApplyLaunchDataIfReady)
		Mesh->SetRelativeScale3D(OriginalScale * InflateScale);
		break;

	case ETN_PufferState::Deflated:
		Mesh->SetRelativeScale3D(OriginalScale);
		break;
	}
}






