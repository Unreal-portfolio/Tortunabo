#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_SlowZoneVolume.generated.h"

class UBoxComponent;
class ATortugaCharacter;

/**
 * Zona que limita la velocidad máxima de cualquier TortugaCharacter que entre.
 *
 * Lógica 100% local en cada máquina — no necesita replicación.
 * Cada cliente limita su propia velocidad; el servidor hace lo mismo,
 * por lo que el CMC predice y valida con el mismo MaxWalkSpeed en todos lados.
 *
 * Uso: colocar BP_SlowZoneVolume en el nivel y ajustar MaxSlowSpeed y el
 * tamaño del BoxComponent desde el Editor.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_SlowZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	ATN_SlowZoneVolume();

protected:
	/** Caja de colisión — editar su tamaño en el Viewport del Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SlowZone")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Velocidad máxima (cm/s) mientras el jugador está dentro. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SlowZone",
		meta = (ClampMin = "50.0"))
	float MaxSlowSpeed = 300.f;

private:
	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Velocidad original de cada personaje dentro de la zona (por máquina). */
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> OriginalSpeeds;
};
