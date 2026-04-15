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

	/** Velocidad máxima horizontal (cm/s) mientras el jugador está dentro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowZone",
		meta = (ClampMin = "50.0"))
	float MaxSlowSpeed = 300.f;

	/**
	 * Si true, reduce la gravedad del jugador mientras está en la zona.
	 * Produce efecto "vuelo lento" / flotación — limita el movimiento en Z.
	 * Configurable desde el Blueprint hijo.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowZone|Gravity")
	bool bSlowFall = false;

	/**
	 * Escala de gravedad aplicada cuando bSlowFall=true.
	 * 0.0 = sin gravedad, 1.0 = gravedad normal. Default: 0.3 (caída lenta).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowZone|Gravity",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bSlowFall"))
	float GravityScaleInZone = 0.3f;

private:
	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Personajes actualmente dentro de la zona — para evitar aplicar el cap dos veces. */
	TSet<TWeakObjectPtr<ATortugaCharacter>> CharactersInZone;

	/** Gravedad original por personaje — para restaurar al salir cuando bSlowFall=true. */
	TMap<TWeakObjectPtr<ATortugaCharacter>, float> OriginalGravityScales;
};
