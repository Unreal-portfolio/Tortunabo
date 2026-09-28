#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_BeachStunComponent.generated.h"

class ACharacter;
class AActor;

/**
 * Estado de aturdida del modo carrera (TNBeach::StunTurtle, TN_BeachStun.h). No viene en la tortuga: el servidor lo
 * añade en ejecución la primera vez que la aturde y se replica solo (componente dinámico replicado), así que el
 * cooperativo no lo lleva nunca.
 *
 * Servidor: mete a la tortuga en su caparazón como bola (UTN_ShellComponent: cuerpo físico, salida bloqueada), la
 * lanza con la velocidad pedida, la suelta de lo que lleve y, al acabar, desbloquea la salida y la deja salir en cuanto
 * la bola se para. Todas las máquinas, a partir de bStunned: la bola tiembla (solo lo visual: la malla se agita sobre la
 * caja física, después de que la caja la coloque) y los pajaritos del mareo (UTN_DizzyBirdsComponent) dan vueltas.
 *
 * En el servidor guarda además la reserva de quién la mueve (patada de la tormenta, red de seguridad) y la gracia de la
 * tormenta (TN_BeachStun.h); no se replican.
 */
UCLASS(ClassGroup = (Custom))
class TORTUNABO_API UTN_BeachStunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_BeachStunComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** El componente de la tortuga, si ya lo tiene (en cualquier máquina). */
	static UTN_BeachStunComponent* FindOn(const AActor* Turtle);

	/** Servidor: el de la tortuga, creándolo y registrándolo (replicado) si aún no lo tiene. */
	static UTN_BeachStunComponent* FindOrAddOn(ACharacter* Turtle);

	/** Servidor: aturde Seconds (o alarga hasta el mayor de los dos finales) y lanza la bola con Launch. */
	void StartStun(float Seconds, const FVector& Launch);

	/** Servidor: acaba el aturdimiento ya. Con bExitShellNow sale del caparazón en el acto (si no, al pararse la bola). */
	void EndStun(bool bExitShellNow);

	UFUNCTION(BlueprintPure, Category = "Beach|Stun")
	bool IsStunned() const { return bStunned; }

	/** Segundos que le quedan (servidor exacto; clientes, con el reloj de servidor del GameState). */
	UFUNCTION(BlueprintPure, Category = "Beach|Stun")
	float GetSecondsLeft() const;

	// Solo servidor (sin replicar): el árbitro de quién mueve a la tortuga (TNBeach::ClaimTurtle, GrantStormGrace).
	/** Reserva vigente: quién (TNBeach::ETNBeachMover como número) y hasta cuándo (hora del mundo del servidor). */
	uint8 ClaimMover = 0;
	double ClaimUntil = 0.0;
	/** Hasta cuándo (hora del mundo del servidor) no la patea la tormenta. */
	double StormGraceUntil = 0.0;

protected:
	/** Amplitud (cm) del temblor de la bola. */
	UPROPERTY(EditAnywhere, Category = "Beach|Stun", meta = (ClampMin = "0.0"))
	float TrembleAmplitude = 2.5f;

	/** Giro máximo (grados) del temblor de la bola. */
	UPROPERTY(EditAnywhere, Category = "Beach|Stun", meta = (ClampMin = "0.0"))
	float TrembleDegrees = 4.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Stunned)
	bool bStunned = false;

	/** Momento de servidor (GetTimeSeconds) en que se acaba. */
	UPROPERTY(Replicated)
	float StunEndServerTime = 0.f;

	UFUNCTION()
	void OnRep_Stunned();

	/** Pájaros del mareo y tick del temblor en esta máquina. */
	void ApplyLocalVisuals(bool bOn);

	/** Agita la malla sobre la caja física del caparazón (después de que la caja la coloque en este fotograma). */
	void TickTremble(float DeltaTime);

	/** Caja física a la que se ha atado el tick (para correr después de ella). */
	TWeakObjectPtr<AActor> TrembleAfter;
	float TrembleTime = 0.f;
};
