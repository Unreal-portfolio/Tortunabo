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

	// Defer un tick para que todos los ChildActors del chunk existan y estén posicionados.
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ATN_ButtonInteractable::DeferredInit));
}

// ─────────────────────────────────────────────────────────────────────────────
// ResolveParentChunk
// ─────────────────────────────────────────────────────────────────────────────

AActor* ATN_ButtonInteractable::ResolveParentChunk() const
{
	if (AActor* Parent = GetParentActor())
	{
		return Parent;
	}
	if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor;
	}
	if (AActor* AttachParent = GetAttachParentActor())
	{
		return AttachParent;
	}
	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Target helpers
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

FTransform ATN_ButtonInteractable::GetGoalTransform() const
{
	return bIsActivated ? ActivatedTransform : OriginalTransform;
}

// ─────────────────────────────────────────────────────────────────────────────
// DeferredInit — resuelve target + captura transform original + calcula activado
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ButtonInteractable::DeferredInit()
{
	// ── Resolver target por tag/nombre si no está asignado ───────────────────
	if (!MoveTarget && !ResolvedMoveComponent.IsValid() && MoveTargetTag != NAME_None)
	{
		AActor* ParentChunk = ResolveParentChunk();

		if (ParentChunk)
		{
			const FString TagStr = MoveTargetTag.ToString();

			// 1. Buscar COMPONENTE por nombre en el chunk BP padre
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

			// 2. Buscar ChildActor por ActorTag
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

			// 3. Fallback: attached actors
			if (!MoveTarget && !ResolvedMoveComponent.IsValid())
			{
				TArray<AActor*> AttachedActors;
				ParentChunk->GetAttachedActors(AttachedActors, true, true);
				for (AActor* Attached : AttachedActors)
				{
					if (Attached && Attached != this && Attached->ActorHasTag(MoveTargetTag))
					{
						MoveTarget = Attached;
						break;
					}
				}
			}

			// 4. Fallback clientes: TActorIterator con mismo padre
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

	// ── Capturar transform original y calcular transform activado ────────────
	if (HasValidTarget())
	{
		OriginalTransform = GetTargetTransform();

		// ActivatedTransform = OriginalTransform + ActivatedOffset (suma simple)
		// NO usar multiplicación de FTransform (ActivatedOffset * Original) porque
		// eso escala el offset por la Scale3D del original → valores absurdos
		// cuando el target hereda escala del chunk.
		ActivatedTransform = OriginalTransform;
		ActivatedTransform.SetLocation(OriginalTransform.GetLocation() + ActivatedOffset.GetLocation());
		ActivatedTransform.SetRotation(ActivatedOffset.GetRotation() * OriginalTransform.GetRotation());
		// Mantener la escala original (el offset no debería cambiar la escala)

		bInitialized = true;

		UE_LOG(LogTemp, Log, TEXT("[Button] '%s' — Original: %s | Offset: %s | Activado: %s"),
			*GetName(),
			*OriginalTransform.GetLocation().ToString(),
			*ActivatedOffset.GetLocation().ToString(),
			*ActivatedTransform.GetLocation().ToString());

		// Si el botón ya estaba activado (por replicación antes de init), empezar a mover
		if (bIsActivated)
		{
			bIsMoving = true;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Replication
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ButtonInteractable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ButtonInteractable, bIsActivated);
}

void ATN_ButtonInteractable::OnRep_IsActivated()
{
	if (bInitialized)
	{
		bIsMoving = true;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Interact — toggle
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ButtonInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}

	if (!HasValidTarget())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — Sin target válido."), *GetName());
		return;
	}

	if (!bInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — Aún no inicializado."), *GetName());
		return;
	}

	// Despertar de dormancy para que bIsActivated replique a clientes.
	FlushNetDormancy();

	// Toggle
	bIsActivated = !bIsActivated;
	bIsMoving = true;

	ForceNetUpdate();

	MulticastPlayButtonFeedback();

	// Cooldown via padre (TN_DirectInteractableBase)
	Super::Interact(Interactor);

	UE_LOG(LogTemp, Log, TEXT("[Button] '%s' → %s (destino: %s)"),
		*GetName(),
		bIsActivated ? TEXT("ACTIVADO") : TEXT("DESACTIVADO"),
		*GetGoalTransform().GetLocation().ToString());
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — interpola el target hacia el destino actual
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ButtonInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsMoving || !bInitialized || !HasValidTarget())
	{
		return;
	}

	const FTransform Goal = GetGoalTransform();

	InterpTargetToward(Goal, DeltaTime);

	// Verificar si llegó al destino
	const FTransform CurrentT = GetTargetTransform();
	const float DistSq = FVector::DistSquared(CurrentT.GetLocation(), Goal.GetLocation());
	const float AngleDiff = (CurrentT.GetRotation().Rotator() - Goal.GetRotation().Rotator())
		.GetNormalized().GetManhattanDistance(FRotator::ZeroRotator);

	if (DistSq < 4.f && AngleDiff < 1.f) // 2cm y 1°
	{
		SetTargetTransform(Goal);
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
