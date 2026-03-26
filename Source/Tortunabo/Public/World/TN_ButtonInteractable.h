#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_ButtonInteractable.generated.h"

/**
 * Botón interactuable que, al pulsar (Enhanced Input IA_Interact),
 * mueve un actor/componente target a través de una lista de waypoints de forma fluida.
 * Cada interacción avanza al siguiente waypoint (cíclico).
 *
 * Replicación: CurrentWaypointIndex se replica a los clientes; tanto servidor
 * como clientes interpolan el target localmente en Tick hacia el waypoint
 * actual. No se usa SetReplicateMovement(true) para evitar conflictos entre
 * la posición replicada y la interpolación local.
 *
 * Waypoints relativos (bUseRelativeWaypoints): los offsets se interpretan
 * relativos al transform del target al iniciar (no al botón). Esto
 * garantiza posiciones correctas cuando botón y target están a distancias
 * distintas dentro de un chunk.
 *
 * Dentro de un chunk spawneado en runtime, MoveTarget (eyedropper) no funciona.
 * Usa MoveTargetTag para resolver el target automáticamente en BeginPlay.
 * Búsqueda en 4 pasos (primero en el chunk BP padre, luego en el mundo):
 *   1. Busca un USceneComponent por nombre (MoveTargetTag) en el chunk BP padre.
 *      Esto permite mover componentes del BP directamente (ej. "wall1").
 *   2. Itera los UChildActorComponents del padre → comprueba ActorTag en cada ChildActor.
 *   3. Fallback: GetAttachedActors del padre (targets attached).
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
	 * Nombre/tag para resolver el target automáticamente en BeginPlay.
	 * Busca PRIMERO un componente por nombre en el chunk BP padre (ej. "wall1"),
	 * luego busca un ChildActor con este ActorTag.
	 * Útil para chunks spawneados en runtime donde el eyedropper no funciona.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FName MoveTargetTag;

	/**
	 * Puntos de destino. Si bUseRelativeWaypoints es false, en espacio mundo.
	 * Si es true, relativos al transform del target al iniciar (para chunks).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	TArray<FTransform> Waypoints;

	/**
	 * Si true, los Waypoints se interpretan como offsets relativos al transform
	 * del target al iniciar. Se convierten a espacio mundo en BeginPlay.
	 * Activar cuando el botón está dentro de un chunk (posición desconocida en design time).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bUseRelativeWaypoints = false;

	/** Velocidad de movimiento (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button", meta = (ClampMin = "1.0"))
	float MoveSpeed = 200.f;

	/** Velocidad de rotación (grados/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button", meta = (ClampMin = "1.0"))
	float RotateSpeed = 180.f;

	/** Índice del waypoint actual al que se dirige. Replicado para sincronizar. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWaypointIndex, BlueprintReadOnly, Category = "Button")
	int32 CurrentWaypointIndex = 0;

	/** Multicast cosmético: feedback al pulsar (override en BP para sonido/VFX). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayButtonFeedback();

	UFUNCTION()
	void OnRep_CurrentWaypointIndex();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** true mientras el target se está moviendo hacia el waypoint actual. */
	bool bIsMoving = false;

	/**
	 * Componente resuelto como target (cuando el target es un componente del chunk BP,
	 * no un actor separado). Usado en Tick cuando MoveTarget es null.
	 */
	TWeakObjectPtr<USceneComponent> ResolvedMoveComponent;

	/**
	 * Inicialización diferida un tick:
	 * - Resuelve MoveTarget/ResolvedMoveComponent por tag/nombre si es necesario.
	 *   Estrategias: componente por nombre en chunk BP → ChildActors por tag →
	 *   attached actors → TActorIterator con mismo padre.
	 * - Convierte waypoints relativos a espacio mundo (base = target).
	 * Usa binding a UObject para que se cancele automáticamente si el actor
	 * es destruido (ej. chunk temporal del ChunkManager).
	 */
	void DeferredInit();

	/** Resuelve el actor chunk BP padre: GetParentActor → GetOwner → GetAttachParentActor. */
	AActor* ResolveParentChunk() const;

	// ── Helpers para leer/escribir el transform del target ────────────────────
	FTransform GetTargetTransform() const;
	void SetTargetTransform(const FTransform& T);
	void InterpTargetToward(const FTransform& Goal, float DeltaTime);
	bool HasValidTarget() const;
};
