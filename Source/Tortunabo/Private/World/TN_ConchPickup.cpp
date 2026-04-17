#include "World/TN_ConchPickup.h"
#include "Player/TortugaCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

ATN_ConchPickup::ATN_ConchPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	ConchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConchMesh"));
	SetRootComponent(ConchMesh);
	ConchMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ConchMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ConchMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ConchMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ConchMesh->SetIsReplicated(false);

	OverlapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OverlapSphere"));
	OverlapSphere->SetupAttachment(ConchMesh);
	OverlapSphere->InitSphereRadius(TrapRadius);
	OverlapSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OverlapSphere->SetCollisionObjectType(ECC_WorldDynamic);
	OverlapSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	OverlapSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	OverlapSphere->SetGenerateOverlapEvents(true);
}

void ATN_ConchPickup::BeginPlay()
{
	Super::BeginPlay();

	// La esfera de detección se ajusta al radio configurado
	OverlapSphere->SetSphereRadius(TrapRadius);

	if (HasAuthority())
	{
		OverlapSphere->OnComponentBeginOverlap.AddDynamic(this, &ATN_ConchPickup::OnSphereBeginOverlap);
	}
}

void ATN_ConchPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TrapTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ATN_ConchPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ConchPickup, bIsPlacedTrap);
}

// ── PlaceAsTrap ────────────────────────────────────────────────────────────────

void ATN_ConchPickup::PlaceAsTrap(const FVector& WorldLocation)
{
	if (!HasAuthority()) { return; }

	SetActorLocation(WorldLocation);
	bIsPlacedTrap = true;
	bTrapUsed = false;

	// OnRep_IsPlacedTrap dispara los efectos en clientes.
	// En listen-server OnRep no dispara, así que los ejecutamos directamente.
	PlayPlaceEffects();
}

// ── Overlap ────────────────────────────────────────────────────────────────────

void ATN_ConchPickup::OnSphereBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority()) { return; }

	ATortugaCharacter* Character = Cast<ATortugaCharacter>(OtherActor);
	if (!Character) { return; }

	if (!bIsPlacedTrap)
	{
		// ── Modo ítem recogible ────────────────────────────────────────────────
		// La concha se destruye; la integración con el inventario la gestiona
		// el sistema de pickup externo (TN_PickupInteractableBase / GameMode).
		Destroy();
		return;
	}

	// ── Modo trampa ───────────────────────────────────────────────────────────
	if (bTrapUsed) { return; }
	bTrapUsed = true;

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp) { return; }

	// Inmovilizar en el servidor
	MoveComp->DisableMovement();

	// Notificar VFX en todos los clientes
	MulticastOnTrapped(Character);

	// Restaurar movimiento tras TrapDurationSeconds
	TWeakObjectPtr<ATortugaCharacter> WeakChar(Character);
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &ATN_ConchPickup::RestoreMovement, WeakChar);
	GetWorldTimerManager().SetTimer(TrapTimerHandle, Del, TrapDurationSeconds, false);
}

// ── RestoreMovement ────────────────────────────────────────────────────────────

void ATN_ConchPickup::RestoreMovement(TWeakObjectPtr<ATortugaCharacter> WeakCharacter)
{
	if (!WeakCharacter.IsValid()) { return; }

	UCharacterMovementComponent* MoveComp = WeakCharacter->GetCharacterMovement();
	if (MoveComp && MoveComp->MovementMode == MOVE_None)
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}
}

// ── Multicast ──────────────────────────────────────────────────────────────────

void ATN_ConchPickup::MulticastOnTrapped_Implementation(APawn* Victim)
{
	const FVector Loc = Victim ? Victim->GetActorLocation() : GetActorLocation();
	if (TrapSound) { UGameplayStatics::SpawnSoundAtLocation(this, TrapSound, Loc); }
	if (TrapVFX)   { UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, TrapVFX, Loc); }
}

// ── OnRep ──────────────────────────────────────────────────────────────────────

void ATN_ConchPickup::OnRep_IsPlacedTrap()
{
	if (bIsPlacedTrap) { PlayPlaceEffects(); }
}

// ── Efectos de colocación ─────────────────────────────────────────────────────

void ATN_ConchPickup::PlayPlaceEffects()
{
	if (PlaceSound) { UGameplayStatics::SpawnSoundAtLocation(this, PlaceSound, GetActorLocation()); }
	if (PlaceVFX)   { UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PlaceVFX, GetActorLocation()); }
}
