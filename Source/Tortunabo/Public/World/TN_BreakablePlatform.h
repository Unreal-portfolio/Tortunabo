#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_BreakablePlatform.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

/**
 * Plataforma que se rompe tras aguantar el peso de un jugador durante TimeToBreak segundos.
 *
 * Autoridad: el servidor controla el timer y el estado bBroken.
 * Clientes:  OnRep_bBroken oculta la plataforma y desactiva la colisión.
 * Respawn:   si RespawnTime > 0, la plataforma reaparece automáticamente.
 *
 * Uso: crear BP hijo, asignar StaticMesh, ajustar TimeToBreak y RespawnTime.
 * Implementar OnPlatformShake y OnPlatformBreak en el BP para VFX/audio.
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

	/** Llamado en todas las máquinas cuando el timer de rotura está a punto de expirar. Override en BP para VFX de vibración. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Platform")
	void OnPlatformShake();

	/** Llamado en todas las máquinas cuando la plataforma se rompe. Override en BP para VFX/audio. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Platform")
	void OnPlatformBreak();

	/** Llamado en todas las máquinas cuando la plataforma reaparece. Override en BP para VFX/audio. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Platform")
	void OnPlatformRespawn();

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

	/** Multicast para VFX de vibración (cosmético, no reliable). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShake();

	/** Número de pawns actualmente encima de la plataforma. */
	int32 PawnsOnPlatform = 0;

	FTimerHandle ShakeTimerHandle;
	FTimerHandle BreakTimerHandle;
	FTimerHandle RespawnTimerHandle;
};
