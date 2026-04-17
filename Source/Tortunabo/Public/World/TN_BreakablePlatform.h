#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_BreakablePlatform.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;
class UNiagaraSystem;
class APawn;

/**
 * Plataforma que se rompe tras aguantar el peso de un jugador durante TimeToBreak segundos.
 *
 * Autoridad: el servidor controla el timer y el estado bBroken.
 * Clientes:  OnRep_bBroken oculta la plataforma y desactiva la colisión.
 * Respawn:   si RespawnTime > 0, la plataforma reaparece automáticamente.
 *
 * Todos los efectos de audio/VFX se configuran aquí como UPROPERTY — sin Blueprints.
 * Uso: crear BP hijo, asignar StaticMesh y los assets de audio/VFX.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_BreakablePlatform : public AActor
{
	GENERATED_BODY()

public:
	ATN_BreakablePlatform();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Platform")
	TObjectPtr<UStaticMeshComponent> PlatformMesh;

	/** Volumen de detección encima del mesh — jugadores que estén encima activan el timer. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Platform")
	TObjectPtr<UBoxComponent> StandTrigger;

	/**
	 * Mínimo de jugadores encima para que empiece el timer de rotura.
	 * 1 = cualquier jugador la rompe (plataforma individual, #21).
	 * 2+ = necesita X jugadores simultáneamente (puente cooperativo, #20).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Platform", meta = (ClampMin = "1", ClampMax = "4"))
	int32 PlayerThreshold = 1;

	/** Segundos que aguanta el peso antes de romperse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Platform", meta = (ClampMin = "0.1"))
	float TimeToBreak = 2.0f;

	/**
	 * Segundos hasta que la plataforma reaparece.
	 * 0 = no reaparece.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Platform", meta = (ClampMin = "0.0"))
	float RespawnTime = 6.0f;

	// ── Audio ─────────────────────────────────────────────────────────────────

	/** Sonido de vibración (jugadores encima, a mitad del timer). */
	UPROPERTY(EditDefaultsOnly, Category = "Platform|Audio")
	TObjectPtr<USoundBase> ShakeSound;

	/** Sonido al romperse. */
	UPROPERTY(EditDefaultsOnly, Category = "Platform|Audio")
	TObjectPtr<USoundBase> BreakSound;

	/** Sonido al reaparecer. */
	UPROPERTY(EditDefaultsOnly, Category = "Platform|Audio")
	TObjectPtr<USoundBase> RespawnSound;

	// ── VFX ───────────────────────────────────────────────────────────────────

	/** VFX de vibración. */
	UPROPERTY(EditDefaultsOnly, Category = "Platform|VFX")
	TObjectPtr<UNiagaraSystem> ShakeVFX;

	/** VFX de rotura. */
	UPROPERTY(EditDefaultsOnly, Category = "Platform|VFX")
	TObjectPtr<UNiagaraSystem> BreakVFX;

private:
	UPROPERTY(ReplicatedUsing = OnRep_bBroken)
	bool bBroken = false;

	UFUNCTION()
	void OnRep_bBroken();

	UFUNCTION()
	void OnStandTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                                bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnStandTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                              UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void ApplyBrokenState();
	void BreakPlatform();
	void RespawnPlatform();

	/**
	 * Vibración antes del colapso — Reliable porque es gameplay-crítico:
	 * señaliza visualmente al jugador que la plataforma está a punto de ceder.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShake();

	/** Pawns actualmente encima de la plataforma. */
	TSet<TWeakObjectPtr<APawn>> PawnsOnPlatform;

	FTimerHandle ShakeTimerHandle;
	FTimerHandle BreakTimerHandle;
	FTimerHandle RespawnTimerHandle;
};
