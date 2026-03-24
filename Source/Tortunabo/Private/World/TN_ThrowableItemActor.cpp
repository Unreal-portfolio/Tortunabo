#include "World/TN_ThrowableItemActor.h"
#include "Components/SphereComponent.h"
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
	SetReplicateMovement(true); // Server runs physics; clients get replicated position

	// ── Optimización de red: alta frecuencia solo mientras vuela ───────────
	SetNetUpdateFrequency(30.f);    // 30 Hz durante vuelo (proyectil rápido)
	SetMinNetUpdateFrequency(15.f); // mínimo 15 Hz

	// Sphere collision como root: garantiza que ProjectileMovement siempre tenga
	// geometría de colisión válida para sweeps, independientemente de si el mesh
	// tiene collision model o no. Esto previene que el proyectil caiga al infinito.
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(20.f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	SetRootComponent(CollisionSphere);

	// Mesh visual como hijo: su colisión se desactiva para que no interfiera
	// con los sweeps del ProjectileMovement.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(CollisionSphere);
	Mesh->SetIsReplicated(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionSphere);
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

	// Aplicar propiedades de física desde las Class Defaults del BP.
	ProjectileMovement->Bounciness = Bounciness;
	ProjectileMovement->Friction   = RollingFriction;

	// Ignorar colisión con el lanzador para que la bola no se auto-destruya
	// al salir de la mano del personaje.
	if (APawn* ThrowInstigator = GetInstigator())
	{
		CollisionSphere->IgnoreActorWhenMoving(ThrowInstigator, true);
	}

	// Solo el servidor valida impactos y detenciones.
	if (HasAuthority())
	{
		CollisionSphere->OnComponentHit.AddDynamic(this, &ATN_ThrowableItemActor::OnMeshHit);
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ATN_ThrowableItemActor::OnProjectileStopped);
	}

	ApplyLaunchDataIfReady();
}

void ATN_ThrowableItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ThrowableItemActor, ThrowData);
}

void ATN_ThrowableItemActor::InitializeThrow(const FVector& SpawnLocation, const FVector& InitialVelocity)
{
	if (!HasAuthority())
	{
		return;
	}

	const FVector SafeScale = SourceItem.EquippedMeshScale.IsNearlyZero()
		? FVector::OneVector
		: SourceItem.EquippedMeshScale;

	ThrowData.SpawnLocation  = SpawnLocation;
	ThrowData.LaunchVelocity = InitialVelocity;
	ThrowData.MeshScale      = SafeScale;
	ThrowData.EquippedMesh   = SourceItem.EquippedMesh;  // Fix JIP: replicate mesh with the struct
	ThrowData.bReady         = true;
	ApplyLaunchDataIfReady();
}

void ATN_ThrowableItemActor::SetSourceItem(const FTN_InventoryItem& Item)
{
	SourceItem = Item;
}

void ATN_ThrowableItemActor::OnRep_ThrowData()
{
	ApplyLaunchDataIfReady();
}

void ATN_ThrowableItemActor::ApplyLaunchDataIfReady()
{
	if (!ThrowData.bReady || bLaunchApplied)
	{
		return;
	}

	// Apply the replicated mesh first so JIP clients see the correct asset instead of the placeholder.
	if (ThrowData.EquippedMesh)
	{
		Mesh->SetStaticMesh(ThrowData.EquippedMesh);
	}

	if (!ThrowData.MeshScale.IsNearlyZero())
	{
		Mesh->SetRelativeScale3D(ThrowData.MeshScale);
	}

	if (HasAuthority())
	{
		// El servidor posiciona el actor y activa el movimiento.
		SetActorLocation(ThrowData.SpawnLocation);

		if (ProjectileMovement)
		{
			ProjectileMovement->Velocity = ThrowData.LaunchVelocity;
			ProjectileMovement->Activate(true);
		}
	}
	// En clientes NO llamamos SetActorLocation: con SetReplicateMovement(true)
	// el servidor ya replica la posición. Llamarlo aquí snapearía la bola de vuelta
	// al punto de lanzamiento cuando OnRep_ThrowData llega tarde.

	bLaunchApplied = true;
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
