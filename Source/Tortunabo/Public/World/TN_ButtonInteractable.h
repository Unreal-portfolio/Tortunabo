#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_ButtonInteractable.generated.h"

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
	 * Offset de transform que se SUMA a la posición original del target
	 * cuando el botón está activado. Al desactivar, vuelve al original.
	 * Configurar en el BP del chunk (ej. mover 300 cm en Z → Location Z=300).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FTransform ActivatedOffset;

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
