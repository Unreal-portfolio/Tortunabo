#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerVolume.h"
#include "TN_SlowZoneVolume.generated.h"

class ATortugaCharacter;

/**
 * Volumen que reduce la velocidad máxima de movimiento de los jugadores
 * que entren en él. Al salir, se restaura la velocidad original.
 *
 * Solo ejecuta lógica en el servidor (HasAuthority). El CMC replica
 * MaxWalkSpeed automáticamente a todos los clientes.
 *
 * Uso en Editor: colocar en el nivel, ajustar SlowMultiplier por instancia.
 */
UCLASS()
class TORTUNABO_API ATN_SlowZoneVolume : public ATriggerVolume
{
	GENERATED_BODY()

public:
	ATN_SlowZoneVolume();

protected:
	/** Factor de reducción de velocidad (0.5 = mitad de velocidad). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "SlowZone",
		meta = (ClampMin = "0.05", ClampMax = "0.99"))
	float SlowMultiplier = 0.5f;

private:
	UFUNCTION()
	void OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	/** MaxWalkSpeed original de cada jugador dentro del volumen. */
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> OriginalSpeeds;
};
