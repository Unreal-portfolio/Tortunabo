#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "TN_BeachBarbedWire.generated.h"

class ACharacter;
class APawn;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTN_BeachTrapSynthComponent;

/**
 * Alambre de espino enrollado (concertina) tendido a lo largo del eje X local, como todos los alargados del reparto
 * (TN_BeachLayout.h: el generador lo gira ~90° para que cruce la playa): va de X = -Largo/2 a +Largo/2 (Largo =
 * Spec.Extent o, si es 0, DefaultLength) y ocupa ±CoilRadius·0,85 en Y, dentro del semigrosor de la huella del contrato
 * (±250·SizeScale). Rollos de ~66·SizeScale cm de radio (52-84) con pinchos, un hilo tenso por encima y estacas de madera
 * cada ~7 m.
 *
 * Tocarlo aturde un poco (StunSeconds, TNBeach::StunTurtle) y empuja hacia atrás, hacia el lado (±Y) del que venía la
 * tortuga (PushSpeed y PushUp): chispa, «¡ay!» sintetizado y un «¡AY!» que sale flotando. Bloquea el paso (colisión
 * sólida) y el sensor está unos centímetros por fuera: se toca antes de chocar. Se puede saltar por encima si se llega
 * (rollo de ~1,3 m) o rodear.
 *
 * Red: lo decide el servidor (sensor geométrico en su Tick, un aturdimiento por tortuga cada StunSeconds + HitCooldown);
 * los efectos van por un multicast no fiable a todas las máquinas.
 */
UCLASS()
class TORTUNABO_API ATN_BeachBarbedWire : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachBarbedWire();

	virtual void Tick(float DeltaSeconds) override;

	/** Largo del alambre (cm). */
	float GetWireLength() const;

	/** Segundos de aturdimiento al tocarlo. */
	UPROPERTY(EditAnywhere, Category = "Alambre", meta = (ClampMin = "0.2", ClampMax = "5.0"))
	float StunSeconds = 1.2f;

	/** Velocidad horizontal del empujón hacia atrás (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Alambre", meta = (ClampMin = "0.0"))
	float PushSpeed = 750.f;

	/** Velocidad vertical del empujón (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Alambre", meta = (ClampMin = "0.0"))
	float PushUp = 420.f;

	/** Segundos de gracia tras el aturdimiento antes de poder volver a pincharse. */
	UPROPERTY(EditAnywhere, Category = "Alambre", meta = (ClampMin = "0.0"))
	float HitCooldown = 1.5f;

	/** Largo (cm) si Spec.Extent es 0. */
	UPROPERTY(EditAnywhere, Category = "Alambre", meta = (ClampMin = "300.0"))
	float DefaultLength = 2400.f;

protected:
	virtual void BeginPlay() override;
	virtual void ApplySpec() override;

	/** Chispa, «¡ay!» y texto en todas las máquinas. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastZapFX(FVector_NetQuantize WorldAt, APawn* Victim);

	UPROPERTY(VisibleAnywhere, Category = "Alambre")
	TObjectPtr<UStaticMeshComponent> WireMesh;

	/** Rollos (cajas convexas por tramos): bloquean a personajes y caparazones; la cámara los atraviesa. */
	UPROPERTY(VisibleAnywhere, Category = "Alambre")
	TObjectPtr<UProceduralMeshComponent> WireCollision;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachTrapSynthComponent> Voice;

private:
	/** Servidor: quién toca el alambre. */
	void CheckTouches();

	void PlayZapFX(const FVector& WorldAt, const APawn* Victim);

	double HalfLength = 1200.0;
	double CoilRadius = 66.0;
	TMap<TWeakObjectPtr<ACharacter>, double> LastHit;
	FTNTrapBurst Sparks;
	FTNTrapBurst Bits;
	FTNTrapPopText Ouch;
};
