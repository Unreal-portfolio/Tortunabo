#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "TN_BeachSpadeRamp.generated.h"

class ACharacter;
class APawn;
class UBoxComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTN_BeachTrapSynthComponent;

/**
 * Pala de playa de juguete gigante (~11 m con SizeScale = 1: hoja de 4 m y 3 de ancho, mango de 84 cm de ancho por el que
 * se anda), a lo largo de X (X local = sentido de la carrera). Dos variantes según la semilla:
 *
 * - Balancín (semilla par): la pala sobre una piedra redonda (el fulcro, ~1,5 m), con la hoja apoyada en la arena (-X) y
 *   el mango en alto (+X, ~3,3 m). Se sube andando por la hoja y el mango. Saltar en el último 20 % del mango es un
 *   trampolín (TipUp hacia arriba y TipForward hacia +X). Si una tortuga cae de un salto sobre la mitad del mango
 *   (velocidad de caída > FlipImpactSpeed), la pala da la vuelta: el mango golpea la arena y la hoja sube de golpe
 *   lanzando a quien esté en ella (CatapultUp y CatapultForward, más lejos que el trampolín); a los ~2 s vuelve sola.
 * - Puente-trampolín (semilla impar): la pala apoyada entre dos alturas, del montículo de arena (-X, el mango) a lo alto
 *   de una roca (+X), con la hoja asomando por fuera de la roca como un trampolín de piscina. Se sube por el mango (una
 *   pasarela) y saltar en la punta de la hoja lanza hacia delante.
 *
 * Red: el trampolín lo aplican a la vez el servidor y el cliente dueño dentro del mismo movimiento (el salto cambia el
 * modo de movimiento y el lanzamiento se aplica en ese mismo paso), como la medusa del lobby; el resto ve el efecto por un
 * multicast no fiable. El balancín lo decide el servidor (FlipAt, hora del servidor replicada): lanza a las víctimas y el
 * cliente de cada víctima aplica el mismo lanzamiento al recibir el aviso (la corrección del servidor queda pequeña).
 * La pala es una base móvil (cajas con nombre estable) que cada máquina gira desde FlipAt.
 */
UCLASS()
class TORTUNABO_API ATN_BeachSpadeRamp : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachSpadeRamp();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool IsSeesaw() const { return bSeesaw; }

	/** Trampolín de la punta: velocidad vertical (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Pala", meta = (ClampMin = "0.0"))
	float TipUp = 1250.f;

	/** Trampolín de la punta: velocidad hacia +X (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Pala", meta = (ClampMin = "0.0"))
	float TipForward = 700.f;

	/** Balancín: velocidad vertical de quien sale lanzado desde la hoja (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Pala", meta = (ClampMin = "0.0"))
	float CatapultUp = 1500.f;

	/** Balancín: velocidad hacia +X de quien sale lanzado (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Pala", meta = (ClampMin = "0.0"))
	float CatapultForward = 1050.f;

	/** Velocidad de caída (cm/s) al aterrizar en el mango que da la vuelta a la pala. */
	UPROPERTY(EditAnywhere, Category = "Pala", meta = (ClampMin = "0.0"))
	float FlipImpactSpeed = 380.f;

protected:
	virtual void BeginPlay() override;
	virtual void ApplySpec() override;

	UFUNCTION()
	void OnRep_Flip();

	/** Saltos de las tortugas cerca que simula esta máquina (servidor: todas; cliente: la suya). */
	UFUNCTION()
	void OnRiderModeChanged(ACharacter* Rider, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	/** Muelle del trampolín en todas las máquinas (el cliente dueño ya lo ha oído al predecir). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTipFX(APawn* Jumper);

	/** Balancín: las víctimas (su cliente aplica también el lanzamiento y la inmunidad a la caída). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastCatapult(const TArray<APawn*>& Victims);

	/** Hora del servidor de la última vuelta del balancín (< 0 = ninguna). */
	UPROPERTY(ReplicatedUsing = OnRep_Flip)
	float FlipAt = -1.f;

	/** Piedra del fulcro, o montículo y roca del puente. */
	UPROPERTY(VisibleAnywhere, Category = "Pala")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "Pala")
	TObjectPtr<UProceduralMeshComponent> BaseCollision;

	/** Punto de apoyo (en el balancín, gira). */
	UPROPERTY(VisibleAnywhere, Category = "Pala")
	TObjectPtr<USceneComponent> SpadePivot;

	/** Espacio de la pala: origen en la punta de la hoja, X hacia el mango, Z = 0 en la cara de abajo. */
	UPROPERTY(VisibleAnywhere, Category = "Pala")
	TObjectPtr<USceneComponent> SpadeFrame;

	UPROPERTY(VisibleAnywhere, Category = "Pala")
	TObjectPtr<UStaticMeshComponent> SpadeMesh;

	UPROPERTY(VisibleAnywhere, Category = "Pala")
	TObjectPtr<UBoxComponent> BladeBox;

	UPROPERTY(VisibleAnywhere, Category = "Pala")
	TObjectPtr<UBoxComponent> ShaftBox;

	UPROPERTY(VisibleAnywhere, Category = "Pala")
	TObjectPtr<UBoxComponent> GripBox;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachTrapSynthComponent> Voice;

private:
	struct FRiderTrack
	{
		double LastTipTime = -10.0;
		double SpadeX = -1.0;
		float LastVz = 0.f;
		bool bOn = false;
		bool bBound = false;
	};

	/** Quién está en la pala, en la punta y cayendo en el mango; pide la vuelta (servidor). */
	void TrackRiders(double ServerTime);

	/** Servidor: vuelta del balancín con las víctimas de la hoja. */
	void FlipSeesaw();

	/** Ángulo (grados, cabeceo del fulcro) T segundos después de la vuelta. */
	double SeesawAngle(double T) const;

	/** El balancín está quieto en reposo (se puede usar el trampolín y dar otra vuelta). */
	bool IsSeesawIdle(double ServerTime) const;

	bool IsOnSpade(const ACharacter* Character) const;
	bool IsInTipZone(double SpadeX) const;

	void PlayTipFX(const FVector& WorldAt);
	void Unbind(ACharacter* Rider, FRiderTrack& Track);

	bool bSeesaw = true;
	double SpadeLength = 1120.0;
	double BladeLength = 400.0;
	double FulcrumSpadeX = 490.0;
	double RestDeg = 17.0;
	double FlippedDeg = 15.0;
	bool bSlamPending = false;

	TMap<TWeakObjectPtr<ACharacter>, FRiderTrack> Tracks;
	FTNTrapClock Clock;
	FTNTrapBurst Dust;
};
