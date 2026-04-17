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

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	/** Caja de colisión — editar su tamaño en el Viewport del Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SlowZone")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Velocidad máxima horizontal (cm/s) mientras el jugador está dentro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowZone",
		meta = (ClampMin = "50.0"))
	float MaxSlowSpeed = 300.f;

	/**
	 * Efecto "sirope": limita la velocidad vertical en ambas direcciones.
	 * Al subir: velocidad Z máxima = MaxUpwardVelocity (salto rasante).
	 * Al bajar: velocidad Z mínima = -MaxFallVelocity (descenso lento).
	 * Aplica siempre (no requiere flag — el comportamiento de zona lenta ES sirope).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowZone|Syrup",
		meta = (ClampMin = "0.0"))
	float MaxUpwardVelocity = 120.f;

	/** Velocidad máxima de caída (cm/s, valor positivo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowZone|Syrup",
		meta = (ClampMin = "50.0"))
	float MaxFallVelocity = 200.f;

	/** Velocidad de salto dentro de la zona (cm/s). Muy baja para el efecto sirope. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlowZone|Syrup",
		meta = (ClampMin = "50.0"))
	float JumpVelocityInZone = 150.f;

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

	struct FSyrupState { float OrigGravityScale = 1.f; float OrigJumpZVel = 600.f; };
	/** Valores CMC originales por personaje — restaurados al salir de la zona. */
	TMap<TWeakObjectPtr<ATortugaCharacter>, FSyrupState> OriginalCMCState;

	/** Limpia el estado de un personaje que se destruyó dentro de la zona. */
	UFUNCTION()
	void OnCharacterDestroyed(AActor* DestroyedActor);
};
