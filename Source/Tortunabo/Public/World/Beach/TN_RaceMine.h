#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "World/Beach/TN_RaceItemActor.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_RaceMine.generated.h"

class ATortugaCharacter;
class UPointLightComponent;
class UStaticMeshComponent;
class UTN_BeachMineSynthComponent;

/**
 * Mina de arena lanzable (Docs/Modo_Carrera.md, «Objetos de carrera»; el «bob-omb» de esta carrera). Sale de la tortuga
 * hacia donde mira su cámara, vuela con gravedad, rebota una vez en la arena y se queda quieta. Se arma sola a los 0,9 s
 * (pitido y luz roja que late cada vez más deprisa) y, armada, salta cuando se acerca una tortuga (la de quien la lanzó,
 * solo pasados 1,5 s) o un enemigo: mecha de 0,35 s con pitidos rápidos y explosión. Si nadie se acerca, explota sola a
 * los 10 s. La explosión aturde en bola a las tortugas de alrededor (lanzadas hacia fuera y hacia arriba) y marea a los
 * enemigos cercanos. Como mucho hay 12 a la vez.
 *
 * Red: lo decide todo el servidor (ServerTick) con una simulación propia, sin física del motor; el sitio va por el Track de
 * la base (suavizado en los clientes) y el estado por Phase (0 volando, 1 quieta sin armar, 2 armada, 3 en mecha, 4
 * explotada) con la hora del servidor a la que empezó (PhaseStartTime): cada máquina anima con eso el latido de la luz y los
 * pitidos. La explosión, con sus sonidos y efectos, llega además por un multicast no fiable; quien lo pierde la ve igualmente
 * al replicarse Phase. Todo lo visual solo existe en máquinas con pantalla.
 */
UCLASS()
class TORTUNABO_API ATN_RaceMine : public ATN_RaceItemActor
{
	GENERATED_BODY()

public:
	ATN_RaceMine();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Servidor: Turtle lanza una mina hacia Direction (unitaria, ya con el cabeceo del lanzamiento). Sale a 120 cm delante y
	 * 60 cm arriba de la tortuga. false si no se crea (sin autoridad o ya hay 12 en el mundo).
	 */
	static bool ServerThrow(ATortugaCharacter* Turtle, const FVector& Direction);

protected:
	virtual float GetTrackFrequency() const override { return 30.f; }
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildVisuals() override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnFinished() override;

	/** Efectos y sonidos de la explosión en todas las máquinas con pantalla (además del OnRep de Phase). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastExplode(FVector_NetQuantize10 Where);

	/** 0 volando, 1 quieta y sin armar, 2 armada, 3 en mecha, 4 explotada. */
	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	uint8 Phase = 0;

	/** Hora del servidor a la que empezó la fase actual. */
	UPROPERTY(Replicated)
	float PhaseStartTime = 0.f;

private:
	UFUNCTION()
	void OnRep_Phase();

	// ── Servidor ──

	/** Cambia de fase (hora incluida) y la aplica también en esta máquina (el servidor no recibe OnRep). */
	void SetPhase(uint8 NewPhase);

	/** Un paso de la simulación en el aire y por el suelo. */
	void StepFlight(float DeltaSeconds);

	/** Se acaba el vuelo: quieta en Where, apoyada en la arena. */
	void Settle(const FVector& Where);

	/** ¿Hay alguien al alcance de la armada? OutWho: la tortuga que la dispara (null si es un enemigo o el tiempo). */
	bool ScanForTrigger(ATortugaCharacter*& OutWho) const;

	/** Empieza la mecha. */
	void StartFuse(ATortugaCharacter* Who);

	/** Explota: aturde y marea a los de alrededor, avisa a todos y acaba. */
	void Explode();

	/** Hacia atrás en la carrera (contrario al mar del generador), en horizontal. */
	FVector GetBackDirection() const;

	// ── Visual ──

	void UpdateVisuals(float DeltaSeconds);

	/** Fogonazo, partículas, cráter y todo lo que sigue vivo tras acabar. */
	void TickFX(float DeltaSeconds);

	/** Crea los emisores de partículas la primera vez que hacen falta. */
	void EnsureFX();

	void PlayExplosionFX(const FVector& At);
	void PlayLandFX();

	/** Esconde el cuerpo de la mina (queda el cráter y lo que quede de la explosión). */
	void HideBody();

	/** La voz de la mina (se crea la primera vez; null sin pantalla). */
	UTN_BeachMineSynthComponent* GetMineVoice();

	// Estado del servidor (la simulación).
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	int32 Bounces = 0;
	bool bSliding = false;
	float PhaseClock = 0.f;
	float ScanClock = 0.f;
	TWeakObjectPtr<ATortugaCharacter> TriggerTurtle;

	// Visual.
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MineMesh;

	/** Halo rojo translúcido: el «piloto» que late. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HaloMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CraterMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FlashMesh;

	/** Luz roja del piloto (solo cerca de la cámara) y, un instante, destello anaranjado de la explosión. */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachMineSynthComponent> Voice;

	/** Distancia (cm) del centro de la malla a su base y giro con el que da vueltas en el aire. */
	double MeshRestHalf = 14.0;
	FRotator TumbleRot = FRotator::ZeroRotator;
	float RestBlend = 0.f;
	float LedOffset = 0.f;
	int32 BeepsPlayed = 1000;
	float FlashAge = 10.f;
	float CraterAge = -1.f;
	bool bBoomPlayed = false;
	bool bFXReady = false;
	bool bFlashActive = false;
	bool bLedOn = false;

	TNAmbientFX::FEmitter Fire;
	TNAmbientFX::FEmitter Smoke;
	TNAmbientFX::FEmitter Dust;
	TNAmbientFX::FEmitter Clods;
	TNAmbientFX::FEmitter Sparks;
	FTNTrapPopText BoomText;
};
