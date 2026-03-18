#include "World/TN_ThrowableItemActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

ATN_ThrowableItemActor::ATN_ThrowableItemActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetIsReplicated(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Mesh);
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 6000.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
}

void ATN_ThrowableItemActor::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(MaxLifeSeconds);
	ApplyLaunchDataIfReady();
}

void ATN_ThrowableItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATN_ThrowableItemActor, ReplicatedSpawnLocation);
	DOREPLIFETIME(ATN_ThrowableItemActor, ReplicatedLaunchVelocity);
	DOREPLIFETIME(ATN_ThrowableItemActor, bLaunchDataReady);
}

void ATN_ThrowableItemActor::InitializeThrow(const FVector& SpawnLocation, const FVector& InitialVelocity)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedSpawnLocation = SpawnLocation;
	ReplicatedLaunchVelocity = InitialVelocity;
	bLaunchDataReady = true;
	ApplyLaunchDataIfReady();
}

void ATN_ThrowableItemActor::OnRep_ThrowData()
{
	ApplyLaunchDataIfReady();
}

void ATN_ThrowableItemActor::ApplyLaunchDataIfReady()
{
	if (!bLaunchDataReady || bLaunchApplied)
	{
		return;
	}

	SetActorLocation(ReplicatedSpawnLocation);
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = ReplicatedLaunchVelocity;
		ProjectileMovement->Activate(true);
	}

	bLaunchApplied = true;
}

