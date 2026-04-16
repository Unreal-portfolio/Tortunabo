#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_DeathZoneVolume.generated.h"

class UBoxComponent;
class APlayerController;
class ATN_RunGameMode;

/**
 * Zona de muerte por countdown.
 *
 * Refactorizado de ATriggerVolume a AActor + UBoxComponent para poder
 * ser colocado dentro de Blueprint Actors (chunks spawneados en runtime).
 * ATriggerVolume hereda de ABrush y NO puede vivir dentro de un BP hijo.
 *
 * Patrón: idéntico a TN_SlowZoneVolume.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_DeathZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	ATN_DeathZoneVolume();

	/**
	 * Reinicia el countdown de un jugador dentro de la zona.
	 * Llamar tras RecoverFromKnockdown si el jugador sigue dentro del volumen,
	 * para evitar que muera instantáneamente al ser revivido.
	 */
	void ResetPlayerTimer(APlayerController* PC);

	/**
	 * Comprueba geométricamente si el pawn del jugador está dentro del TriggerBox.
	 * Si lo está y no hay countdown activo, inicia uno nuevo.
	 * Llamar desde RunGameMode cuando la inmunidad post-revive expira,
	 * porque SetActorEnableCollision(true) no siempre dispara OnBoxBeginOverlap
	 * de forma fiable cuando el pawn ya está geométricamente dentro del volumen.
	 */
	void ForceCheckPlayer(APlayerController* PC);

protected:
	/** Caja de colisión — editar su tamaño en el Viewport del Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathZone")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathZone", meta = (ClampMin = "0.1"))
	float SecondsInsideToDie = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathZone")
	bool bDestroyOnlyDuringRun = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathZone", meta = (ClampMin = "0.05"))
	float CountdownTickInterval = 0.1f;

	/** Called on the server when a player's death countdown starts in this zone. Override in subclasses. */
	virtual void OnPlayerEnteredZone(APawn* Pawn) {}

	/** Called on the server when a player dies in this zone. Override in subclasses. */
	virtual void OnPlayerDiedInZone(APlayerController* PC) {}

private:
	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void HandlePlayerDeath(APlayerController* PlayerController);
	/** Single shared timer that ticks ALL players inside the zone at once. */
	void TickAllCountdowns();
	ATN_RunGameMode* ResolveRunGameMode() const;

	/** Single shared timer handle — replaces N per-player timers. */
	FTimerHandle SharedCountdownTimerHandle;

	TMap<TWeakObjectPtr<APlayerController>, float> PendingDeathRemaining;
};
