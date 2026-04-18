#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_QuicksandVolume.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ATortugaCharacter;

/**
 * Zona de arena movediza: ralentiza al jugador y aumenta la gravedad para
 * dar sensación de "pegado al suelo". **No mata** — el rediseño delega la
 * muerte a un patrón compuesto en el nivel:
 *   1. Este volumen (ralentiza + anula salto + gravedad x2.5)
 *   2. TN_DeathZoneVolume a ras de suelo (kill por countdown estándar)
 *   3. Static mesh sólido debajo (evita caída infinita)
 *
 * Ralentización: local en todas las máquinas (igual que TN_SlowZoneVolume).
 * El CMC predice correctamente al usar el mismo speed cap en server y cliente.
 *
 * Compatible con chunks: sin lógica dependiente de posición en BeginPlay.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_QuicksandVolume : public AActor
{
	GENERATED_BODY()

public:
	ATN_QuicksandVolume();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quicksand")
	TObjectPtr<UBoxComponent> TriggerBox;

	/**
	 * Mesh visual de la arena. Sin colisión bloqueante — el jugador entra al volumen.
	 * Asignar en BP_QuicksandVolume un plane/quad mesh de arena.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quicksand")
	TObjectPtr<UStaticMeshComponent> SandMesh;

	/** Velocidad máxima dentro de la arena (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quicksand",
		meta = (ClampMin = "50.0", ClampMax = "400.0"))
	float SlowSpeed = 150.f;

	/** Multiplicador de gravedad dentro de la arena. Valores altos "pegan" al jugador. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quicksand",
		meta = (ClampMin = "0.1"))
	float GravityScaleOverride = 2.5f;

	/** Altura de salto dentro de la arena — 0 lo desactiva efectivamente. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quicksand",
		meta = (ClampMin = "0.0"))
	float JumpZVelocityOverride = 100.f;

private:
	struct FSandState
	{
		float OrigGravityScale = 1.f;
		float OrigJumpZVel     = 600.f;
	};

	// Cache de valores originales del CMC por personaje — restaurados en EndOverlap
	TMap<TWeakObjectPtr<ATortugaCharacter>, FSandState> ActiveChars;

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnCharacterDestroyed(AActor* DestroyedActor);
};
