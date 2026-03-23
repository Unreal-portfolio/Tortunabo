#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_ButtonInteractable.generated.h"

/**
 * Botón interactuable que, al pulsar (Enhanced Input IA_Interact),
 * mueve un actor target a través de una lista de waypoints de forma fluida.
 * Cada interacción avanza al siguiente waypoint (cíclico).
 * El MoveTarget debe tener SetReplicateMovement(true) para que los
 * clientes vean el movimiento sin RPCs adicionales.
 */
UCLASS()
class TORTUNABO_API ATN_ButtonInteractable : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_ButtonInteractable();

	virtual void Tick(float DeltaTime) override;
	virtual void Interact(APawn* Interactor) override;

protected:
	/** Actor que se moverá al interactuar. Debe estar en el nivel. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Button")
	TObjectPtr<AActor> MoveTarget;

	/** Puntos de destino (en espacio mundo). El actor se mueve al siguiente en cada interacción. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	TArray<FTransform> Waypoints;

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
};

