#include "World/TN_ButtonInteractable.h"
#include "Components/ChildActorComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATN_ButtonInteractable::ATN_ButtonInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	PromptText = FText::FromString(TEXT("Activar"));
}

void ATN_ButtonInteractable::BeginPlay()
{
	Super::BeginPlay();

	// ── Defer inicialización dependiente de posición/siblings a next tick ────
	// ChildActorComponents crean sus hijos durante el SCS, pero el orden de
	// inicialización entre hermanos no está garantizado. Un tick de delay asegura
	// que todos los Child Actors del chunk existen y están en su posición final.
	//
	// IMPORTANTE: Usar binding a UObject (no lambda) para que EndPlay→
	// ClearAllTimersForObject(this) cancele el timer si el actor es destruido
	// antes del tick (ej. chunk temporal del ChunkManager::GetOrComputeInSocketTransform).
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ATN_ButtonInteractable::DeferredInit));
}

void ATN_ButtonInteractable::DeferredInit()
{
	// ── Resolver MoveTarget por tag si no está asignado ──────────────────────
	if (!MoveTarget && MoveTargetTag != NAME_None)
	{
		AActor* OwnerActor = GetOwner();

		if (OwnerActor)
		{
			// ── 1. Buscar en los ChildActorComponents del Owner (chunk BP) ──
			// Esta es la forma más fiable: accede directamente a los componentes
			// del chunk y sus child actors, sin depender del árbol de attachment
			// (que puede no estar listo en clientes donde los actores llegan
			// replicados antes de que el ChildActorComponent los reclame).
			TArray<UChildActorComponent*> ChildActorComps;
			OwnerActor->GetComponents<UChildActorComponent>(ChildActorComps);
			for (UChildActorComponent* CAC : ChildActorComps)
			{
				AActor* ChildActor = CAC ? CAC->GetChildActor() : nullptr;
				if (ChildActor && ChildActor != this && ChildActor->ActorHasTag(MoveTargetTag))
				{
					MoveTarget = ChildActor;
					break;
				}
			}

			// ── 2. Fallback: buscar attached actors (targets no-ChildActor) ──
			if (!MoveTarget)
			{
				TArray<AActor*> AttachedActors;
				OwnerActor->GetAttachedActors(AttachedActors, /*bResetArray=*/ true,
					/*bRecursivelyIncludeAttachedActors=*/ true);
				for (AActor* Attached : AttachedActors)
				{
					if (Attached && Attached != this && Attached->ActorHasTag(MoveTargetTag))
					{
						MoveTarget = Attached;
						break;
					}
				}
			}

			// ── 3. Fallback clientes: actores con mismo Owner y tag ──────────
			// En clientes, el child actor replicado puede no estar attached aún
			// al chunk pero SÍ tiene Owner replicado correctamente.
			if (!MoveTarget)
			{
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					AActor* Candidate = *It;
					if (Candidate && Candidate != this
						&& Candidate->GetOwner() == OwnerActor
						&& Candidate->ActorHasTag(MoveTargetTag))
					{
						MoveTarget = Candidate;
						break;
					}
				}
			}
		}

		if (MoveTarget)
		{
			UE_LOG(LogTemp, Log, TEXT("[Button] '%s' encontró MoveTarget por tag '%s' → '%s' (Owner='%s')"),
				*GetName(), *MoveTargetTag.ToString(), *GetNameSafe(MoveTarget), *GetNameSafe(OwnerActor));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — MoveTargetTag='%s' no encontrado en chunk '%s'. "
				"Verifica que el target tenga ActorTag='%s' y sea un ChildActor del mismo BP."),
				*GetName(), *MoveTargetTag.ToString(), *GetNameSafe(OwnerActor), *MoveTargetTag.ToString());
		}
	}

	// ── Convertir waypoints relativos a espacio mundo ────────────────────────
	// Los waypoints se interpretan como offsets relativos al MOVETARGET, no al
	// botón. Esto garantiza que la posición destino es correcta incluso cuando
	// el botón y el target están en posiciones distintas dentro del chunk.
	// Ambos (servidor y cliente) ejecutan esta conversión con el mismo MoveTarget
	// local → los waypoints en mundo coinciden → la interpolación local en Tick
	// produce el mismo resultado visual en ambos lados.
	if (bUseRelativeWaypoints && Waypoints.Num() > 0)
	{
		const FTransform BaseTransform = MoveTarget
			? MoveTarget->GetActorTransform()
			: GetActorTransform(); // Fallback al botón si no hay MoveTarget

		if (!MoveTarget)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — bUseRelativeWaypoints activo pero sin MoveTarget. "
				"Usando transform del botón como base (posición destino puede ser incorrecta)."), *GetName());
		}

		for (FTransform& WP : Waypoints)
		{
			WP = WP * BaseTransform;
		}
		bUseRelativeWaypoints = false;
	}
}

void ATN_ButtonInteractable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ButtonInteractable, CurrentWaypointIndex);
}

void ATN_ButtonInteractable::OnRep_CurrentWaypointIndex()
{
	// Cuando el cliente recibe un nuevo waypoint, activar el movimiento local
	bIsMoving = true;
}

void ATN_ButtonInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}

	if (!MoveTarget || Waypoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — Sin MoveTarget o sin Waypoints."), *GetName());
		return;
	}

	// Despertar de dormancy para que CurrentWaypointIndex replique a clientes.
	// ATN_InteractableBase usa DORM_DormantAll → sin esto, los cambios de
	// propiedades nunca llegan a clientes remotos. FlushNetDormancy transiciona
	// a DORM_DormantPartial (despierta para este cambio y futuros).
	FlushNetDormancy();

	// Avanzar al siguiente waypoint (wrap around)
	CurrentWaypointIndex = (CurrentWaypointIndex + 1) % Waypoints.Num();
	bIsMoving = true;

	// Forzar replicación inmediata del nuevo índice
	ForceNetUpdate();

	MulticastPlayButtonFeedback();

	// Actualizar cooldown via padre (TN_DirectInteractableBase)
	Super::Interact(Interactor);

	UE_LOG(LogTemp, Log, TEXT("[Button] '%s' → Waypoint %d/%d"), *GetName(), CurrentWaypointIndex + 1, Waypoints.Num());
}

void ATN_ButtonInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Tanto servidor como clientes interpolan MoveTarget hacia el waypoint actual.
	// El servidor avanza CurrentWaypointIndex (replicado); los clientes reciben
	// OnRep_CurrentWaypointIndex que activa bIsMoving para interpolar localmente.
	if (!bIsMoving || !MoveTarget || Waypoints.Num() == 0)
	{
		return;
	}

	if (!Waypoints.IsValidIndex(CurrentWaypointIndex))
	{
		bIsMoving = false;
		return;
	}

	const FTransform& TargetTransform = Waypoints[CurrentWaypointIndex];
	const FVector CurrentLoc = MoveTarget->GetActorLocation();
	const FRotator CurrentRot = MoveTarget->GetActorRotation();

	// Interpolar posición a velocidad constante
	const FVector NewLoc = FMath::VInterpConstantTo(CurrentLoc, TargetTransform.GetLocation(), DeltaTime, MoveSpeed);

	// Interpolar rotación a velocidad constante
	const FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetTransform.GetRotation().Rotator(), DeltaTime, RotateSpeed);

	MoveTarget->SetActorLocationAndRotation(NewLoc, NewRot);

	// Verificar si llegó al destino
	const float DistSq = FVector::DistSquared(NewLoc, TargetTransform.GetLocation());
	const float AngleDiff = (NewRot - TargetTransform.GetRotation().Rotator()).GetNormalized().GetManhattanDistance(FRotator::ZeroRotator);

	if (DistSq < 4.f && AngleDiff < 1.f) // 2cm y 1°
	{
		MoveTarget->SetActorTransform(TargetTransform);
		bIsMoving = false;
	}
}

void ATN_ButtonInteractable::MulticastPlayButtonFeedback_Implementation()
{
	// Override en Blueprint para añadir sonido/VFX/animación del botón.
}

