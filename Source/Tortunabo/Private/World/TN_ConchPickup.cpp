#include "World/TN_ConchPickup.h"
#include "Player/TortugaCharacter.h"
#include "Core/ITN_EnemyTargetInterface.h"
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

	// Aplicar mesh normal al inicio (ítem sin activar)
	if (ConchMesh && MeshNormal) { ConchMesh->SetStaticMesh(MeshNormal); }

	if (HasAuthority())
	{
		OverlapSphere->OnComponentBeginOverlap.AddDynamic(this, &ATN_ConchPickup::OnSphereBeginOverlap);
	}
}

void ATN_ConchPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TrapTimerHandle);
	GetWorldTimerManager().ClearTimer(RearmTimerHandle);
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
	if (!HasAuthority() || !OtherActor) { return; }

	// ── Modo trampa vs enemigo ─────────────────────────────────────────────────
	// Si está colocada como trap y un enemigo (Cangrejo, Gaviota...) la pisa,
	// lo aturde con la misma duración que aturde a un jugador.
	if (bIsPlacedTrap && !bTrapUsed)
	{
		if (ITN_EnemyTargetInterface* Enemy = Cast<ITN_EnemyTargetInterface>(OtherActor))
		{
			bTrapUsed = true;
			Enemy->ApplyStun(TrapDurationSeconds);
			MulticastOnTrapped(nullptr);

			// Mismo comportamiento que con jugador: tras consumir la trampa, dejar
			// un pickup recogible en la posición — la concha es reciclable.
			if (bDestroyAfterActivation)
			{
				if (UWorld* World = GetWorld())
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride =
						ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
					SpawnParams.Owner = GetOwner();
					World->SpawnActor<ATN_ConchPickup>(
						GetClass(), GetActorLocation(), GetActorRotation(), SpawnParams);
				}
				SetLifeSpan(0.2f);
			}
			else
			{
				// Modo persistente: rearm tras cooldown.
				if (ResetCooldownSeconds > 0.f)
				{
					GetWorldTimerManager().SetTimer(RearmTimerHandle, this,
						&ATN_ConchPickup::RearmTrap, ResetCooldownSeconds, false);
				}
				else
				{
					RearmTrap();
				}
			}
			return;
		}
	}

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
	if (WeakCharacter.IsValid())
	{
		UCharacterMovementComponent* MoveComp = WeakCharacter->GetCharacterMovement();
		if (MoveComp && MoveComp->MovementMode == MOVE_None)
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
	}

	if (bDestroyAfterActivation)
	{
		// Antes de auto-destruirse, dejar un pickup-ítem en la misma ubicación para
		// que la trampa sea recuperable como recurso y el flujo de rescate del item
		// no acabe en "trampa consumida y nada en el suelo".
		if (UWorld* World = GetWorld())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.Owner = GetOwner();
			// GetClass() preserva el BP hijo configurado (mesh/audio/VFX) en lugar
			// de spawnear el ATN_ConchPickup nativo "pelado".
			World->SpawnActor<ATN_ConchPickup>(
				GetClass(), GetActorLocation(), GetActorRotation(), SpawnParams);
		}

		// SetLifeSpan permite que cualquier callback pendiente termine en paz.
		SetLifeSpan(0.2f);
		return;
	}

	// Modo persistente: tras el cooldown, re-armar para que pueda volver a atrapar.
	if (ResetCooldownSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(RearmTimerHandle, this,
			&ATN_ConchPickup::RearmTrap, ResetCooldownSeconds, false);
	}
	else
	{
		RearmTrap();
	}
}

void ATN_ConchPickup::RearmTrap()
{
	if (!HasAuthority()) { return; }
	bTrapUsed = false;

	// Volver al mesh normal cuando la trampa se re-arma (bDestroyAfterActivation=false)
	if (ConchMesh && MeshNormal) { ConchMesh->SetStaticMesh(MeshNormal); }
}

// ── Multicast ──────────────────────────────────────────────────────────────────

void ATN_ConchPickup::MulticastOnTrapped_Implementation(APawn* Victim)
{
	const FVector Loc = Victim ? Victim->GetActorLocation() : GetActorLocation();
	if (TrapSound) { UGameplayStatics::SpawnSoundAtLocation(this, TrapSound, Loc); }
	if (TrapVFX)   { UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, TrapVFX, Loc); }

	// Swap al mesh "activado" en todas las máquinas
	if (ConchMesh && MeshTriggered) { ConchMesh->SetStaticMesh(MeshTriggered); }
}

// ── OnRep ──────────────────────────────────────────────────────────────────────

void ATN_ConchPickup::OnRep_IsPlacedTrap()
{
	if (bIsPlacedTrap)
	{
		// Sincronizar mesh normal en clientes cuando la trampa se coloca
		if (ConchMesh && MeshNormal) { ConchMesh->SetStaticMesh(MeshNormal); }
		PlayPlaceEffects();
	}
}

// ── Efectos de colocación ─────────────────────────────────────────────────────

void ATN_ConchPickup::PlayPlaceEffects()
{
	if (PlaceSound) { UGameplayStatics::SpawnSoundAtLocation(this, PlaceSound, GetActorLocation()); }
	if (PlaceVFX)   { UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PlaceVFX, GetActorLocation()); }
}
