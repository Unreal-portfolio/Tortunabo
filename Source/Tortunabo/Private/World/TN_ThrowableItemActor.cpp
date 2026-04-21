#include "World/TN_ThrowableItemActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/TortugaCharacter.h"
#include "World/TN_PickupInteractableBase.h"
#include "Engine/World.h"

ATN_ThrowableItemActor::ATN_ThrowableItemActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// bAlwaysRelevant: sin esto, si el lanzador está lejos del cliente, la bola
	// no sería network-relevant → el cliente nunca vería el proyectil en vuelo.
	bAlwaysRelevant = true;

	// Servidor es autoridad: ProjectileMovement corre solo en el servidor y su
	// posición se replica a los clientes. Enfoque idéntico al de TN_PhysicsObjectActor.
	// SetReplicateMovement es true por defecto; lo dejamos así intencionadamente.

	// 20 Hz durante el vuelo: suficiente para un party game (NetworkSmoothing cubre
	// los huecos). Al detenerse el actor entra en DORM_DormantAll → 0 updates.
	SetNetUpdateFrequency(20.f);
	SetMinNetUpdateFrequency(5.f);

	// Mesh ES el root: UStaticMeshComponent es UPrimitiveComponent.
	// ProjectileMovement actualizará directamente la posición del actor.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetIsReplicated(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetNotifyRigidBodyCollision(true);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Mesh);
	ProjectileMovement->bAutoActivate                        = false;
	ProjectileMovement->InitialSpeed                         = 0.0f;
	ProjectileMovement->MaxSpeed                             = 6000.0f;
	ProjectileMovement->ProjectileGravityScale               = 1.0f;
	ProjectileMovement->bShouldBounce                        = true;
	ProjectileMovement->BounceAdditionalIterations           = 1;
	// Cuando la velocidad baja de este umbral (cm/s) tras un rebote,
	// ProjectileMovement llama StopSimulating → OnProjectileStop → spawn pickup.
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 50.f;
}

void ATN_ThrowableItemActor::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(MaxLifeSeconds);

	UE_LOG(LogTemp, Log, TEXT("[TN_Throwable] BeginPlay auth=%d bReady=%d bLaunchApplied=%d inst=%s"),
		HasAuthority() ? 1 : 0, ThrowData.bReady ? 1 : 0, bLaunchApplied ? 1 : 0,
		*GetNameSafe(GetInstigator()));

	// Aplicar propiedades de física desde las Class Defaults del BP.
	ProjectileMovement->Bounciness = Bounciness;
	ProjectileMovement->Friction   = RollingFriction;

	// Visibilidad incondicional: el mesh es visible desde el primer frame en TODAS
	// las máquinas. El mesh correcto llega vía ThrowData (OnRep), pero no podemos
	// esperar a él para mostrar la bola — el BP puede tener el placeholder oculto.
	Mesh->SetHiddenInGame(false);
	Mesh->SetVisibility(true, true);

	// En el servidor Instigator está resuelto desde el spawn — ignorar su colisión
	// antes de que el ProjectileMovement arranque evita auto-impactos.
	if (HasAuthority())
	{
		IgnoreInstigatorCollision();
	}

	// Solo el servidor valida impactos y detenciones.
	if (HasAuthority())
	{
		Mesh->OnComponentHit.AddDynamic(this, &ATN_ThrowableItemActor::OnMeshHit);
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ATN_ThrowableItemActor::OnProjectileStopped);
	}

	ApplyLaunchDataIfReady();
}

void ATN_ThrowableItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// REPNOTIFY_Always: garantiza que OnRep_ThrowData dispara para la replicación
	// inicial aunque el cliente ya tuviera ThrowData en estado default.
	DOREPLIFETIME_CONDITION_NOTIFY(ATN_ThrowableItemActor, ThrowData, COND_None, REPNOTIFY_Always);
}

void ATN_ThrowableItemActor::InitializeThrow(const FVector& SpawnLocation, const FVector& InitialVelocity)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TN_Throwable] SERVER InitializeThrow origin=(%.0f,%.0f,%.0f) v=(%.0f,%.0f,%.0f) mesh=%s"),
		SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z,
		InitialVelocity.X, InitialVelocity.Y, InitialVelocity.Z,
		*GetNameSafe(SourceItem.EquippedMesh));

	const FVector SafeScale = SourceItem.EquippedMeshScale.IsNearlyZero()
		? FVector::OneVector
		: SourceItem.EquippedMeshScale;

	ThrowData.SpawnLocation  = SpawnLocation;
	ThrowData.LaunchVelocity = InitialVelocity;
	ThrowData.MeshScale      = SafeScale;
	ThrowData.EquippedMesh   = SourceItem.EquippedMesh;  // Fix JIP: replicate mesh with the struct
	ThrowData.bReady         = true;

	// ── Aplicar localmente en el servidor ─────────────────────────────────────
	ApplyLaunchDataIfReady();

	// Forzar replicación inmediata: el primer bunch que el cliente recibe ya
	// lleva ThrowData con bReady=true + la posición actual del actor.
	FlushNetDormancy();
	ForceNetUpdate();
}

void ATN_ThrowableItemActor::SetSourceItem(const FTN_InventoryItem& Item)
{
	SourceItem = Item;
}

void ATN_ThrowableItemActor::OnRep_ThrowData()
{
	UE_LOG(LogTemp, Log, TEXT("[TN_Throwable] OnRep_ThrowData bReady=%d v=(%.0f,%.0f,%.0f) mesh=%s"),
		ThrowData.bReady ? 1 : 0,
		ThrowData.LaunchVelocity.X, ThrowData.LaunchVelocity.Y, ThrowData.LaunchVelocity.Z,
		*GetNameSafe(ThrowData.EquippedMesh));
	ApplyLaunchDataIfReady();
}

void ATN_ThrowableItemActor::IgnoreInstigatorCollision()
{
	if (!Mesh) { return; }
	if (APawn* ThrowInstigator = GetInstigator())
	{
		Mesh->IgnoreActorWhenMoving(ThrowInstigator, true);
	}
}


void ATN_ThrowableItemActor::ApplyLaunchDataIfReady()
{
	if (!ThrowData.bReady || bLaunchApplied)
	{
		return;
	}

	// Mesh y visibilidad: todos (servidor + clientes + JIP late-joiners).
	if (ThrowData.EquippedMesh)
	{
		Mesh->SetStaticMesh(ThrowData.EquippedMesh);
	}
	if (!ThrowData.MeshScale.IsNearlyZero())
	{
		Mesh->SetRelativeScale3D(ThrowData.MeshScale);
	}
	Mesh->SetHiddenInGame(false);
	Mesh->SetVisibility(true, true);

	bLaunchApplied = true;

	// Física: solo en el servidor. El servidor corre el ProjectileMovement y
	// replica la posición resultante a los clientes vía bReplicateMovement=true.
	// Los clientes son observadores pasivos — no necesitan simular localmente.
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[TN_Throwable] ApplyLaunchDataIfReady client — mesh=%s (posición replicada por servidor)"),
			*GetNameSafe(ThrowData.EquippedMesh));
		return;
	}

	IgnoreInstigatorCollision();
	SetActorLocation(ThrowData.SpawnLocation);
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = ThrowData.LaunchVelocity;
		ProjectileMovement->Activate(true);
	}

	UE_LOG(LogTemp, Log, TEXT("[TN_Throwable] ApplyLaunchDataIfReady SERVER origin=(%.0f,%.0f,%.0f) v=(%.0f,%.0f,%.0f) mesh=%s pmActive=%d"),
		ThrowData.SpawnLocation.X, ThrowData.SpawnLocation.Y, ThrowData.SpawnLocation.Z,
		ThrowData.LaunchVelocity.X, ThrowData.LaunchVelocity.Y, ThrowData.LaunchVelocity.Z,
		*GetNameSafe(ThrowData.EquippedMesh),
		ProjectileMovement && ProjectileMovement->IsActive() ? 1 : 0);
}

// ── Colisión y ciclo de vida ──────────────────────────────────────────────────

void ATN_ThrowableItemActor::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                        UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                                        const FHitResult& Hit)
{
	if (!HasAuthority() || bPickupSpawned)
	{
		return;
	}

	ATortugaCharacter* HitPlayer = Cast<ATortugaCharacter>(OtherActor);

	// Superficies → el ProjectileMovement gestiona el rebote; nada que hacer aquí.
	if (!HitPlayer)
	{
		return;
	}

	// Auto-impacto → ignorar (IgnoreActorWhenMoving ya debería prevenirlo).
	if (GetInstigator() == HitPlayer)
	{
		return;
	}

	// Golpe a otro jugador → knockdown solo si la bola va lo bastante rápido.
	// La bola rebota naturalmente via ProjectileMovement (bShouldBounce=true).
	// Pickup se genera cuando la bola se detiene completamente (OnProjectileStopped).
	if (!AlreadyHitPlayers.Contains(HitPlayer))
	{
		const float CurrentSpeed = ProjectileMovement ? ProjectileMovement->Velocity.Size() : 0.f;

		if (CurrentSpeed >= MinKnockdownSpeed)
		{
			AlreadyHitPlayers.Add(HitPlayer);
			HitPlayer->ApplyKnockdown(KnockbackDuration);
			UE_LOG(LogTemp, Log, TEXT("[ThrowableItem] Hit %s at %.0f cm/s → KNOCKDOWN (threshold=%.0f)"),
				*GetNameSafe(HitPlayer), CurrentSpeed, MinKnockdownSpeed);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[ThrowableItem] Hit %s at %.0f cm/s → too slow, bounce only (threshold=%.0f)"),
				*GetNameSafe(HitPlayer), CurrentSpeed, MinKnockdownSpeed);
		}
	}
}

void ATN_ThrowableItemActor::OnProjectileStopped(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || bPickupSpawned)
	{
		return;
	}

	// ImpactResult.ImpactPoint (FVector_NetQuantize) no es asignable a FVector
	// en un ternario (C2446/C2737). Usamos if/else con XYZ explícitos.
	// ImpactPoint es el contacto exacto con la superficie → fuente primaria.
	// GetActorLocation() como fallback si el hit no es válido.
	FVector StopLocation = GetActorLocation();
	if (ImpactResult.IsValidBlockingHit())
	{
		StopLocation.X = ImpactResult.ImpactPoint.X;
		StopLocation.Y = ImpactResult.ImpactPoint.Y;
		StopLocation.Z = ImpactResult.ImpactPoint.Z;
	}

	UE_LOG(LogTemp, Log, TEXT("[ThrowableItem] OnProjectileStopped — loc=(%.0f,%.0f,%.0f) validHit=%d"),
		StopLocation.X, StopLocation.Y, StopLocation.Z, ImpactResult.IsValidBlockingHit() ? 1 : 0);

	SpawnPickupAtLocation(StopLocation);
	// Go dormant once stopped — the pickup actor handles the rest.
	// Dormancy stops net updates so clients don't keep receiving position packets for a static ball.
	SetNetDormancy(DORM_DormantAll);
	Destroy();
}

void ATN_ThrowableItemActor::LifeSpanExpired()
{
	if (HasAuthority() && !bPickupSpawned)
	{
		SpawnPickupAtLocation(GetActorLocation());
	}

	Super::LifeSpanExpired();
}

void ATN_ThrowableItemActor::SpawnPickupAtLocation(const FVector& Location)
{
	if (!HasAuthority() || !GetWorld() || bPickupSpawned)
	{
		return;
	}
	bPickupSpawned = true;

	if (!SourceItem.IsValid() || !SourceItem.PickupActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableItemActor] Sin PickupActorClass en SourceItem — el ítem se pierde."));
		return;
	}

	// Floor trace: asegurar que el pickup quede apoyado en el suelo
	FVector SpawnLocation = Location;
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TN_PickupFloorTrace), false, this);
		const FVector TraceStart = Location + FVector(0.f, 0.f, 80.f);
		const FVector TraceEnd   = Location - FVector(0.f, 0.f, 600.f);

		// Primero intentar WorldStatic (suelos estáticos)
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params);

		// Si no impacta, intentar Visibility (captura más tipos de geometría)
		if (!bHit)
		{
			bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
		}

		if (bHit)
		{
			SpawnLocation = Hit.ImpactPoint + FVector(0.f, 0.f, 5.f);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ThrowableItem] SpawnPickup: no floor found below %.1f,%.1f,%.1f — spawning at ball position."),
				Location.X, Location.Y, Location.Z);
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ATN_PickupInteractableBase* Pickup = GetWorld()->SpawnActor<ATN_PickupInteractableBase>(
	        SourceItem.PickupActorClass, SpawnLocation, FRotator::ZeroRotator, Params))
	{
		Pickup->InitializeFromInventoryItem(SourceItem);
	}
}
