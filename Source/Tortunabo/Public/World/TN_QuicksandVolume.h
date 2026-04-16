#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_QuicksandVolume.generated.h"

class UBoxComponent;
class ATortugaCharacter;

/**
 * Zona de arena movediza: ralentiza al jugador drásticamente y lo mata
 * tras un tiempo configurable si no escapa.
 *
 * Ralentización: local en todas las máquinas (igual que TN_SlowZoneVolume),
 * no necesita replicación — CMC predice correctamente al usar el mismo speed cap.
 *
 * Hundimiento y muerte: servidor autoritario.
 * Un timer de servidor procesa todos los jugadores activos cada SandTickInterval:
 *   - Aplica impulso descendente gradual (efecto visual de hundimiento)
 *   - Acumula tiempo y mata al jugador al llegar a SinkDuration
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

	/** Velocidad máxima dentro de la arena (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quicksand",
		meta = (ClampMin = "50.0", ClampMax = "400.0"))
	float SlowSpeed = 150.f;

	/** Segundos dentro de la arena antes de morir. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quicksand",
		meta = (ClampMin = "1.0"))
	float SinkDuration = 4.f;

	/** Fuerza hacia abajo por tick (cm/s) — produce el efecto visual de hundimiento. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quicksand")
	float SinkForce = 60.f;

	/** Intervalo del tick de servidor (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quicksand",
		meta = (ClampMin = "0.05"))
	float SandTickInterval = 0.15f;

private:
	struct FSandState
	{
		float TimeInSand       = 0.f;
		// CMC originals — cacheados en el momento de entrar, restaurados al salir
		float OrigGravityScale = 1.f;
		float OrigJumpZVel     = 600.f;
	};

	// Activos en servidor (muerte) + en todas las máquinas (CMC cache)
	TMap<TWeakObjectPtr<ATortugaCharacter>, FSandState> ActiveChars;
	FTimerHandle SandTickHandle;

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnCharacterDestroyed(AActor* DestroyedActor);

	void ServerSandTick();
	void StartSandTick();
	void StopSandTick();
};
