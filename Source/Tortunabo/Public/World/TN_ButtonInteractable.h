#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_ButtonInteractable.generated.h"

/** Delegate disparado en el servidor cuando bIsActivated cambia. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnButtonActivationChanged, ATN_ButtonInteractable* /*Button*/, bool /*bActivated*/);

/**
 * Modo de comportamiento del botón respecto al offset aplicado al target.
 * DoUndo:       toggle clásico — activado aplica ActivatedOffset, desactivado revierte.
 * CyclicStates: cada pulsación avanza a la siguiente entrada de CyclicStateTransforms.
 *               Al llegar al final vuelve a 0 (ciclo). El índice está replicado.
 */
UENUM(BlueprintType)
enum class EButtonOffsetMode : uint8
{
	DoUndo       UMETA(DisplayName = "Do / Undo"),
	CyclicStates UMETA(DisplayName = "Cyclic States")
};

/**
 * Botón interactuable tipo toggle: al pulsar aplica un offset de transform
 * al target, al volver a pulsar lo devuelve a su posición original.
 *
 * Diseñado para funcionar dentro de chunks (BP actors spawneados en runtime):
 * el offset es relativo, así que no importa dónde se coloque el chunk en el mundo.
 *
 * Replicación: bIsActivated (bool) se replica; tanto servidor como clientes
 * interpolan el target localmente en Tick hacia la posición destino.
 *
 * Resolución del target por MoveTargetTag dentro del chunk BP padre:
 *   1. Busca un USceneComponent por nombre en el chunk BP padre.
 *   2. Itera UChildActorComponents del padre → comprueba ActorTag.
 *   3. Fallback: GetAttachedActors del padre.
 *   4. Fallback clientes: TActorIterator filtrando por mismo padre + tag.
 *
 * El chunk BP padre se resuelve via GetParentActor() > GetOwner() > GetAttachParentActor().
 */
UCLASS()
class TORTUNABO_API ATN_ButtonInteractable : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_ButtonInteractable();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Interact(APawn* Interactor) override;

	/** Delegate servidor-only: disparado cuando bIsActivated cambia. ButtonGroupManager lo escucha. */
	FOnButtonActivationChanged OnActivationChanged;

	/** Estado de activación actual. Público para que ButtonGroupManager pueda consultarlo. */
	bool IsActivated() const { return bIsActivated; }

	/** Índice actual del ciclo (solo relevante en modo CyclicStates). */
	UFUNCTION(BlueprintPure, Category = "Button")
	int32 GetCurrentStateIndex() const { return CurrentStateIndex; }

protected:
	/**
	 * Actor que se moverá al interactuar.
	 * En niveles estáticos: asignar por eyedropper.
	 * En chunks: dejar vacío y usar MoveTargetTag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	TObjectPtr<AActor> MoveTarget;

	/**
	 * Actores adicionales que se mueven junto con MoveTarget al activar el botón.
	 * Todos reciben el mismo ActivatedOffset relativo a su posición original.
	 * Útil para puzzles que mueven múltiples plataformas o puertas a la vez.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	TArray<TObjectPtr<AActor>> AdditionalMoveTargets;

	/**
	 * Nombre/tag para resolver el target automáticamente en BeginPlay.
	 * Busca PRIMERO un componente por nombre en el chunk BP padre (ej. "wall1"),
	 * luego busca un ChildActor con este ActorTag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FName MoveTargetTag;

	/**
	 * Tag del ATN_ButtonGroupManager al que este botón debe auto-registrarse.
	 * Útil cuando el botón está en un chunk y el manager está en el nivel:
	 * el botón busca un manager con este ActorTag y llama RegisterButton(this).
	 * Dejar en NAME_None si el manager referencia directamente este botón por eyedropper.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FName ManagerTag = NAME_None;

	/**
	 * Modo de offset aplicado al target.
	 * DoUndo: toggle clásico usando ActivatedOffset.
	 * CyclicStates: cada pulsación pasa al siguiente FTransform de CyclicStateTransforms.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	EButtonOffsetMode OffsetMode = EButtonOffsetMode::DoUndo;

	/**
	 * Offset de transform que se SUMA a la posición original del target
	 * cuando el botón está activado. Al desactivar, vuelve al original.
	 * Configurar en el BP del chunk (ej. mover 300 cm en Z → Location Z=300).
	 * Solo se usa en modo DoUndo.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button",
		meta = (EditCondition = "OffsetMode == EButtonOffsetMode::DoUndo"))
	FTransform ActivatedOffset;

	/**
	 * Estados cíclicos: cada pulsación avanza al siguiente offset de este array.
	 * El offset se aplica como suma respecto al transform original del target.
	 * Ej: 4 entradas de rotación Yaw=0/90/180/270 → rotación cíclica.
	 * Solo se usa en modo CyclicStates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button",
		meta = (EditCondition = "OffsetMode == EButtonOffsetMode::CyclicStates"))
	TArray<FTransform> CyclicStateTransforms;

	/** Velocidad de movimiento (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button", meta = (ClampMin = "1.0"))
	float MoveSpeed = 200.f;

	/** Velocidad de rotación (grados/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button", meta = (ClampMin = "1.0"))
	float RotateSpeed = 180.f;

	/** Estado del botón: false=posición original, true=posición con offset. Replicado. */
	UPROPERTY(ReplicatedUsing = OnRep_IsActivated, BlueprintReadOnly, Category = "Button")
	bool bIsActivated = false;

	/**
	 * Número de pulsaciones necesarias para activar/desactivar el botón.
	 * 1 = comportamiento clásico toggle inmediato.
	 * 2 = primera pulsación da feedback "medio pulsado", la segunda activa.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button", meta = (ClampMin = "1", ClampMax = "10"))
	int32 PressesRequired = 1;

	/** Pulsaciones acumuladas en el ciclo actual. Replicado para feedback en clientes. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPresses, BlueprintReadOnly, Category = "Button")
	int32 CurrentPresses = 0;

	/** Índice del estado cíclico actual (solo modo CyclicStates). Replicado. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentStateIndex, BlueprintReadOnly, Category = "Button")
	int32 CurrentStateIndex = 0;

	/** Multicast cosmético: feedback al pulsar (override en BP para sonido/VFX). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayButtonFeedback();

	/**
	 * Multicast cosmético: feedback de pulsación parcial (override en BP).
	 * Se llama cuando CurrentPresses > 0 pero aún no llega a PressesRequired.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHalfPressedFeedback();

	UFUNCTION()
	void OnRep_IsActivated();

	UFUNCTION()
	void OnRep_CurrentPresses();

	UFUNCTION()
	void OnRep_CurrentStateIndex();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** true mientras el target se está moviendo hacia su destino. */
	bool bIsMoving = false;

	/** Transform original del target, capturado en DeferredInit. */
	FTransform OriginalTransform;

	/** Transform activado (original + offset), calculado en DeferredInit. */
	FTransform ActivatedTransform;

	/** Transforms originales de AdditionalMoveTargets — índice sincronizado con el array. */
	TArray<FTransform> AdditionalOriginalTransforms;

	/** Transforms activados de AdditionalMoveTargets — índice sincronizado con el array. */
	TArray<FTransform> AdditionalActivatedTransforms;

	/** Transforms finales absolutos por estado cíclico (Original + CyclicStateTransforms[i]). */
	TArray<FTransform> CyclicResolvedTransforms;

	/** Transforms finales de AdditionalMoveTargets por estado cíclico (externa: targets, interna: estados). */
	TArray<TArray<FTransform>> CyclicAdditionalResolvedTransforms;

	/** true cuando DeferredInit ha ejecutado y los transforms están listos. */
	bool bInitialized = false;

	/**
	 * Componente resuelto como target (cuando el target es un componente del chunk BP,
	 * no un actor separado). Usado en Tick cuando MoveTarget es null.
	 */
	TWeakObjectPtr<USceneComponent> ResolvedMoveComponent;

	/**
	 * Inicialización diferida un tick:
	 * - Resuelve MoveTarget/ResolvedMoveComponent por tag/nombre si es necesario.
	 * - Captura el transform original del target y calcula el transform activado.
	 */
	void DeferredInit();

	/** Resuelve el actor chunk BP padre. */
	AActor* ResolveParentChunk() const;

	/** Calcula el goal transform actual según bIsActivated. */
	FTransform GetGoalTransform() const;

	// ── Helpers para leer/escribir el transform del target ────────────────────
	FTransform GetTargetTransform() const;
	void SetTargetTransform(const FTransform& T);
	void InterpTargetToward(const FTransform& Goal, float DeltaTime);
	bool HasValidTarget() const;
};
