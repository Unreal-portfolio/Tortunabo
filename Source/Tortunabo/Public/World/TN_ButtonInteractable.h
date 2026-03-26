#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_ButtonInteractable.generated.h"

/**
 * Botón interactuable que, al pulsar (Enhanced Input IA_Interact),
 * mueve un actor target a través de una lista de waypoints de forma fluida.
 * Cada interacción avanza al siguiente waypoint (cíclico).
 *
 * Replicación: CurrentWaypointIndex se replica a los clientes; tanto servidor
 * como clientes interpolan el MoveTarget localmente en Tick hacia el waypoint
 * actual. No se usa SetReplicateMovement(true) para evitar conflictos entre
 * la posición replicada y la interpolación local.
 *
 * Waypoints relativos (bUseRelativeWaypoints): los offsets se interpretan
 * relativos al transform del MoveTarget al iniciar (no al botón). Esto
 * garantiza posiciones correctas cuando botón y target están a distancias
 * distintas dentro de un chunk.
 *
 * Dentro de un chunk spawneado en runtime, MoveTarget (eyedropper) no funciona.
 * Usa MoveTargetTag para que el botón busque en BeginPlay un ChildActor con ese
 * tag dentro del chunk BP (Owner). Búsqueda en 3 pasos:
 *   1. Itera los UChildActorComponents del Owner → comprueba tag en cada ChildActor.
 *   2. Fallback: GetAttachedActors del Owner (targets no-ChildActor).
 *   3. Fallback clientes: TActorIterator filtrando por mismo Owner + tag
 *      (cubre el caso donde el attachment aún no está listo por replicación).
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
	 * Si MoveTarget está vacío al iniciar, busca en el Owner (chunk BP)
	 * un actor hijo con este ActorTag. Útil para chunks spawneados en runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FName MoveTargetTag;

	/**
	 * Puntos de destino. Si bUseRelativeWaypoints es false, en espacio mundo.
	 * Si es true, relativos al transform del MoveTarget al iniciar (para chunks).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	TArray<FTransform> Waypoints;

	/**
	 * Si true, los Waypoints se interpretan como offsets relativos al transform
	 * del MoveTarget al iniciar. Se convierten a espacio mundo en BeginPlay.
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
	 * Inicialización diferida un tick:
	 * - Resuelve MoveTarget por tag si es necesario (3 estrategias de búsqueda:
	 *   ChildActorComponents del Owner → attached actors → TActorIterator con mismo Owner).
	 * - Convierte waypoints relativos a espacio mundo (base = MoveTarget).
	 * Usa binding a UObject para que se cancele automáticamente si el actor
	 * es destruido (ej. chunk temporal del ChunkManager).
	 */
	void DeferredInit();
};

