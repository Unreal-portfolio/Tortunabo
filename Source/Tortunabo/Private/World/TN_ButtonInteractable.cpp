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

// ─────────────────────────────────────────────────────────────────────────────
// ResolveParentChunk — busca el actor BP padre del chunk
// ─────────────────────────────────────────────────────────────────────────────

AActor* ATN_ButtonInteractable::ResolveParentChunk() const
{
	// 1. GetParentActor — para actores creados por UChildActorComponent.
	//    En UE 5.x, esta es la forma canónica de obtener el actor dueño del CAC.
	if (AActor* Parent = GetParentActor())
	{
		return Parent;
	}

	// 2. GetOwner — fallback clásico (el Owner se replica).
	if (AActor* Owner = GetOwner())
	{
		return Owner;
	}

	// 3. GetAttachParentActor — para actores attached por medios no-ChildActor.
	if (AActor* AttachParent = GetAttachParentActor())
	{
		return AttachParent;
	}

	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Target helpers — abstraen si el target es un actor o un componente
// ─────────────────────────────────────────────────────────────────────────────

bool ATN_ButtonInteractable::HasValidTarget() const
{
	return MoveTarget != nullptr || ResolvedMoveComponent.IsValid();
}

FTransform ATN_ButtonInteractable::GetTargetTransform() const
{
	if (MoveTarget)
	{
		return MoveTarget->GetActorTransform();
	}
	if (ResolvedMoveComponent.IsValid())
	{
		return ResolvedMoveComponent->GetComponentTransform();
	}
	return FTransform::Identity;
}

void ATN_ButtonInteractable::SetTargetTransform(const FTransform& T)
{
	if (MoveTarget)
	{
		MoveTarget->SetActorTransform(T);
	}
	else if (ResolvedMoveComponent.IsValid())
	{
		ResolvedMoveComponent->SetWorldTransform(T);
	}
}

void ATN_ButtonInteractable::InterpTargetToward(const FTransform& Goal, float DeltaTime)
{
	const FTransform Current = GetTargetTransform();

	const FVector NewLoc = FMath::VInterpConstantTo(
		Current.GetLocation(), Goal.GetLocation(), DeltaTime, MoveSpeed);
	const FRotator NewRot = FMath::RInterpConstantTo(
		Current.GetRotation().Rotator(), Goal.GetRotation().Rotator(), DeltaTime, RotateSpeed);

	if (MoveTarget)
	{
		MoveTarget->SetActorLocationAndRotation(NewLoc, NewRot);
	}
	else if (ResolvedMoveComponent.IsValid())
	{
		ResolvedMoveComponent->SetWorldLocationAndRotation(NewLoc, NewRot);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// DeferredInit — resuelve target + convierte waypoints
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ButtonInteractable::DeferredInit()
{
	// ── Resolver target por tag/nombre si no está asignado ───────────────────
	if (!MoveTarget && !ResolvedMoveComponent.IsValid() && MoveTargetTag != NAME_None)
	{
		AActor* ParentChunk = ResolveParentChunk();

		if (ParentChunk)
		{
			// ── 1. Buscar un COMPONENTE por nombre en el chunk BP padre ─────
			// Permite mover directamente un StaticMesh/SceneComponent del BP
			// sin necesidad de un ChildActor separado. Ej: "wall1" es un
			// StaticMeshComponent en el BP del chunk.
			const FString TagStr = MoveTargetTag.ToString();
			for (UActorComponent* Comp : ParentChunk->GetComponents())
			{
				USceneComponent* SC = Cast<USceneComponent>(Comp);
				if (SC && SC->GetName() == TagStr)
				{
					ResolvedMoveComponent = SC;
					UE_LOG(LogTemp, Log, TEXT("[Button] '%s' encontró componente '%s' en chunk '%s'."),
						*GetName(), *TagStr, *GetNameSafe(ParentChunk));
					break;
				}
			}

			// ── 2. Buscar ChildActor por ActorTag ───────────────────────────
			if (!ResolvedMoveComponent.IsValid() && !MoveTarget)
			{
				TArray<UChildActorComponent*> ChildActorComps;
				ParentChunk->GetComponents<UChildActorComponent>(ChildActorComps);
				for (UChildActorComponent* CAC : ChildActorComps)
				{
					AActor* ChildActor = CAC ? CAC->GetChildActor() : nullptr;
					if (ChildActor && ChildActor != this && ChildActor->ActorHasTag(MoveTargetTag))
					{
						MoveTarget = ChildActor;
						break;
					}
				}
			}

			// ── 3. Fallback: buscar attached actors ─────────────────────────
			if (!MoveTarget && !ResolvedMoveComponent.IsValid())
			{
				TArray<AActor*> AttachedActors;
				ParentChunk->GetAttachedActors(AttachedActors, /*bResetArray=*/ true,
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

			// ── 4. Fallback clientes: TActorIterator con mismo padre ────────
			if (!MoveTarget && !ResolvedMoveComponent.IsValid())
			{
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					AActor* Candidate = *It;
					if (Candidate && Candidate != this
						&& (Candidate->GetOwner() == ParentChunk || Candidate->GetParentActor() == ParentChunk)
						&& Candidate->ActorHasTag(MoveTargetTag))
					{
						MoveTarget = Candidate;
						break;
					}
				}
			}
		}

		if (HasValidTarget())
		{
			const FString FoundName = MoveTarget
				? GetNameSafe(MoveTarget)
				: (ResolvedMoveComponent.IsValid() ? ResolvedMoveComponent->GetName() : TEXT("?"));
			UE_LOG(LogTemp, Log, TEXT("[Button] '%s' encontró target '%s' por tag '%s' (chunk='%s')."),
				*GetName(), *FoundName, *MoveTargetTag.ToString(), *GetNameSafe(ResolveParentChunk()));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — MoveTargetTag='%s' no encontrado. "
				"Chunk padre='%s'. Verifica que exista un componente o ChildActor con ese nombre/tag en el BP."),
				*GetName(), *MoveTargetTag.ToString(), *GetNameSafe(ResolveParentChunk()));
		}
	}

	// ── Convertir waypoints relativos a espacio mundo ────────────────────────
	// Los waypoints se interpretan como offsets relativos al TARGET, no al
	// botón. Esto garantiza que la posición destino es correcta incluso cuando
	// el botón y el target están en posiciones distintas dentro del chunk.
	// Ambos (servidor y cliente) ejecutan esta conversión con el mismo target
	// local → los waypoints en mundo coinciden → la interpolación local en Tick
	// produce el mismo resultado visual en ambos lados.
	if (bUseRelativeWaypoints && Waypoints.Num() > 0)
	{
		FTransform BaseTransform;
		if (HasValidTarget())
		{
			BaseTransform = GetTargetTransform();
		}
		else
		{
			BaseTransform = GetActorTransform(); // Fallback al botón
			UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — bUseRelativeWaypoints activo pero sin target. "
				"Usando transform del botón como base (posición destino puede ser incorrecta)."), *GetName());
		}

		for (FTransform& WP : Waypoints)
		{
			WP = WP * BaseTransform;
		}
		bUseRelativeWaypoints = false;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Replication
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Interact
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ButtonInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}

	if (!HasValidTarget() || Waypoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — Sin target válido o sin Waypoints."), *GetName());
		return;
	}

	// Despertar de dormancy para que CurrentWaypointIndex replique a clientes.
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

// ─────────────────────────────────────────────────────────────────────────────
// Tick — interpola el target hacia el waypoint actual
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ButtonInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Tanto servidor como clientes interpolan el target hacia el waypoint actual.
	// El servidor avanza CurrentWaypointIndex (replicado); los clientes reciben
	// OnRep_CurrentWaypointIndex que activa bIsMoving para interpolar localmente.
	if (!bIsMoving || !HasValidTarget() || Waypoints.Num() == 0)
	{
		return;
	}

	if (!Waypoints.IsValidIndex(CurrentWaypointIndex))
	{
		bIsMoving = false;
		return;
	}

	const FTransform& GoalTransform = Waypoints[CurrentWaypointIndex];

	InterpTargetToward(GoalTransform, DeltaTime);

	// Verificar si llegó al destino
	const FTransform CurrentT = GetTargetTransform();
	const float DistSq = FVector::DistSquared(CurrentT.GetLocation(), GoalTransform.GetLocation());
	const float AngleDiff = (CurrentT.GetRotation().Rotator() - GoalTransform.GetRotation().Rotator())
		.GetNormalized().GetManhattanDistance(FRotator::ZeroRotator);

	if (DistSq < 4.f && AngleDiff < 1.f) // 2cm y 1°
	{
		SetTargetTransform(GoalTransform);
		bIsMoving = false;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Multicast feedback
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ButtonInteractable::MulticastPlayButtonFeedback_Implementation()
{
	// Override en Blueprint para añadir sonido/VFX/animación del botón.
}
