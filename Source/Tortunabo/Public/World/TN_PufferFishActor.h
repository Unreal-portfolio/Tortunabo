#pragma once

#include "CoreMinimal.h"
#include "World/TN_ThrowableItemActor.h"
#include "TN_PufferFishActor.generated.h"

UENUM(BlueprintType)
enum class ETN_PufferState : uint8
{
	Flying     UMETA(DisplayName = "Flying"),
	Inflating  UMETA(DisplayName = "Inflating"),
	Deflated   UMETA(DisplayName = "Deflated")
};

/**
 * Pez Globo: throwable que, tras un delay aleatorio, se infla empujando
 * a todos los personajes cercanos. Luego se desinfla y se convierte en pickup.
 * Hereda toda la lógica de lanzamiento/rebote/pickup de ATN_ThrowableItemActor.
 */
UCLASS()
class TORTUNABO_API ATN_PufferFishActor : public ATN_ThrowableItemActor
{
	GENERATED_BODY()

public:
	ATN_PufferFishActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Delay mínimo antes de inflar (s). Bajo para que infle mientras aún vuela. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PufferFish", meta = (ClampMin = "0.1"))
	float InflateDelayMin = 0.3f;

	/** Delay máximo antes de inflar (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PufferFish", meta = (ClampMin = "0.1"))
	float InflateDelayMax = 1.0f;

	/** Factor de escala al inflarse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PufferFish", meta = (ClampMin = "1.0"))
	float InflateScale = 5.0f;

	/** Radio de empuje al inflarse (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PufferFish", meta = (ClampMin = "50.0"))
	float InflateRadius = 400.f;

	/** Fuerza base de empuje (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PufferFish", meta = (ClampMin = "0.0"))
	float InflatePushForce = 1500.f;

	/** Duración del estado inflado (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PufferFish", meta = (ClampMin = "0.1"))
	float InflateDuration = 1.5f;

	/** Velocidad mínima del empuje para causar knockdown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PufferFish", meta = (ClampMin = "0.0"))
	float MinKnockdownForce = 800.f;

	/** Duración del knockdown al empujar (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PufferFish|Knockback", meta = (ClampMin = "0.1"))
	float PufferKnockdownDuration = 2.0f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_PufferState)
	ETN_PufferState PufferState = ETN_PufferState::Flying;

	/** Escala original DESPUÉS de aplicar el ThrowData (capturada en ApplyLaunchDataIfReady). */
	FVector OriginalScale = FVector::OneVector;

	FTimerHandle InflateDelayTimerHandle;
	FTimerHandle DeflatTimerHandle;

	/** Server: llamado tras el delay aleatorio. */
	void Inflate();

	/** Server: llamado tras InflateDuration. */
	void Deflate();

	UFUNCTION()
	void OnRep_PufferState();

	/**
	 * Reemplaza al OnProjectileStopped del padre.
	 * Solo spawnea pickup si el ciclo inflate/deflate ya completó (PufferState == Deflated).
	 * Así, StopSimulating durante la inflación no destruye el pez prematuramente.
	 */
	UFUNCTION()
	void OnPufferProjectileStopped(const FHitResult& ImpactResult);

protected:
	/**
	 * Override: captura OriginalScale DESPUÉS de que el padre aplique la escala
	 * real del DataTable (ThrowData.MeshScale). Sin esto, OriginalScale sería (1,1,1)
	 * del BP default en vez de la escala configurada, rompiendo el efecto inflate.
	 */
	virtual void ApplyLaunchDataIfReady() override;

	/** Aplica visual de inflado/desinflado. */
	void ApplyPufferVisual();
};

