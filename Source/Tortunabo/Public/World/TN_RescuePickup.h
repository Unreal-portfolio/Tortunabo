#pragma once

#include "CoreMinimal.h"
#include "World/TN_InteractableBase.h"
#include "TN_RescuePickup.generated.h"

/**
 * Pickup de rescate: spawnea cuando un jugador muere.
 * Al interactuar, respawnea al jugador muerto en la ubicación del pickup.
 * Hereda de TN_InteractableBase (mesh + prompt 3D + distancia configurable).
 */
UCLASS()
class TORTUNABO_API ATN_RescuePickup : public ATN_InteractableBase
{
	GENERATED_BODY()

public:
	ATN_RescuePickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract(APawn* Interactor) const override;
	virtual void Interact(APawn* Interactor) override;

	/**
	 * Inicializa el pickup con el PlayerId del jugador muerto.
	 * Solo llamar en el servidor.
	 */
	void SetDeadPlayerId(int32 InPlayerId);

	/** Devuelve el PlayerId del jugador muerto asociado a este pickup. */
	int32 GetDeadPlayerId() const { return DeadPlayerId; }

protected:
	/**
	 * PlayerId (APlayerState::GetPlayerId()) del jugador muerto.
	 * Replicado para que todos los clientes puedan mostrarlo en la UI.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rescue")
	int32 DeadPlayerId = -1;
};

