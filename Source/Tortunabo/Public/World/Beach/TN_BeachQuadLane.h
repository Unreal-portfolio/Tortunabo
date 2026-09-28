#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachQuadLane.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Paso de quads (ETNBeachElement::QuadLane): el quad gigante de siempre (ATN_QuadActor) con aspecto nuevo y sin actor
 * propio. El paso va por el eje X local del actor, centrado en él (el generador lo gira 90°: cruza la playa de lado a
 * lado, TN_BeachLayout.h), con Spec.Extent de largo (0 = el ancho de la playa) y la huella del contrato como semiancho
 * por su Y local (a lo largo del camino). En la arena se ven las rodadas.
 *
 *  - Cada 12-20 s sale un quad enorme (a escala: 56 m de largo, ruedas de 17 m de alto y 7 m de ancho, con su piloto).
 *    Antes, 3,5 s de aviso: temblor de pantalla creciente para quien esté cerca, motor que se acerca y nube de humo y
 *    hojas entre las palmeras del lado por el que va a salir. Cruza a 42 m/s y se mete en las palmeras del otro lado.
 *  - Las ruedas atropellan: derribo con ragdoll y mareo, lanzada dando vueltas (TNBeach::KnockDownTurtle). Entre las
 *    ruedas de un lado y las del otro hay un hueco de 10 m (y 7 m de altura libre bajo el chasis) en el que se sobrevive.
 *  - Red: el servidor solo replica la hora de la próxima pasada y el sentido; cada máquina calcula dónde va el quad con
 *    el reloj del servidor (sin replicar movimiento). Los golpes los decide el servidor.
 */
UCLASS()
class TORTUNABO_API ATN_BeachQuadLane : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachQuadLane();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor (pruebas): la próxima pasada empieza ya con su aviso. */
	void DebugPassNow();

	/** Largo del paso (cm, de un borde de la playa al otro). */
	float GetLaneLength() const { return HalfLength * 2.f; }

protected:
	virtual void BeginPlay() override;
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual float GetVisualRange() const override { return 60000.f; }

	/** Todas las máquinas: una rueda ha pasado por encima de Victim. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRunOver(ATortugaCharacter* Victim);

private:
	/** Reloj del servidor en que empieza la próxima pasada (el quad sale de entre las palmeras) y su sentido (+1 hacia +X). */
	UPROPERTY(Replicated)
	float PassTime = -1.f;

	UPROPERTY(Replicated)
	int8 PassDir = 1;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> QuadRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> QuadBody;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Wheels;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Ruts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RutsMesh;

	float SizeK = 1.f;
	/** Medio largo del paso y medio largo del quad (cm). */
	float HalfLength = 14000.f;
	float QuadHalfLen = 2800.f;

	// Servidor.
	TMap<TWeakObjectPtr<ATortugaCharacter>, double> LastHit;

	// Visual.
	float WheelSpin = 0.f;
	float WheelGround[4] = {};
	float GroundTimer = 0.f;
	float RutsRetry = 1.f;
	int32 RutsTries = 0;
	float ShownPass = -1.f;
	bool bCrashIn = false;
	bool bCrashOut = false;
	TNAmbientFX::FEmitter Smoke;
	TNAmbientFX::FEmitter Leaves;
	TNAmbientFX::FEmitter Dust;

	/** Servidor: programa la próxima pasada dentro de Delay s (más el aviso). */
	void SchedulePass(double Now, float Delay);
	float TravelSeconds() const;
	/** Centro del quad a lo largo del paso (X local) en el instante Now; false fuera de la pasada. */
	bool QuadXAt(double Now, float& OutX) const;
	/** Rueda i (0 delantera izquierda, 1 delantera derecha, 2 trasera izquierda, 3 trasera derecha) en el espacio del paso. */
	FVector WheelLocal(int32 Index, float QuadX) const;
	void BuildQuad();
	bool BuildRuts();
};
