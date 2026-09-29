#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "World/Beach/TN_RaceItemActor.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_RaceFrisbee.generated.h"

class ATortugaCharacter;
class UStaticMeshComponent;

/**
 * Disco volador (Docs/Modo_Carrera.md, «Objetos de carrera»; el «bumerán» de esta carrera): un disco de colores de más de un
 * metro que sale hacia delante, dibuja un arco hacia un lado, gira y vuelve a quien lo lanzó, derribando a lo que toca en el
 * camino (tortugas: derribo con ragdoll; enemigos: mareo con pajaritos). Cada víctima recibe un golpe por pasada (ida y
 * vuelta); quien lo lanzó no lo recibe nunca. Acaba cuando pasa a menos de 2 m de quien lo lanzó en la vuelta (lo caza) o a los
 * 6 s. Como mucho hay 6 a la vez.
 *
 * Red: la trayectoria es una fórmula del tiempo que calcula cada máquina (UsesTrack() es false, sin Track): solo se replican
 * Origin, Dir y Curve (una vez, al nacer) y el reloj del servidor de la base; el disco vuelve a la posición ACTUAL de quien lo
 * lanzó, que cada máquina lee de su copia de la tortuga. El servidor calcula la misma posición en ServerTick para los golpes.
 * Los golpes llegan a todos por sus vías de siempre (TNBeach::KnockDownTurtle, ATN_BeachEnemy::ApplyHitStun) y el «¡bonk!»
 * por un multicast no fiable. Todo lo visual solo existe en máquinas con pantalla.
 */
UCLASS()
class TORTUNABO_API ATN_RaceFrisbee : public ATN_RaceItemActor
{
	GENERATED_BODY()

public:
	ATN_RaceFrisbee();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Servidor: Turtle lanza un disco hacia Direction (unitaria; solo cuenta su parte horizontal). false si no se crea (sin
	 * autoridad o ya hay 6 en el mundo).
	 */
	static bool ServerThrow(ATortugaCharacter* Turtle, const FVector& Direction);

protected:
	virtual bool UsesTrack() const override { return false; }
	virtual float GetTrackFrequency() const override { return 10.f; }
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildVisuals() override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnFinished() override;

	/** El disco ha dado a algo en Where: «¡bonk!», chispas y un temblor leve (todas las máquinas con pantalla). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHit(FVector_NetQuantize10 Where);

	/** Dónde sale (cm). Se replica una vez, al nacer. */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 Origin = FVector_NetQuantize10(0.0, 0.0, 0.0);

	/** Hacia delante, unitaria y horizontal. Se replica una vez. */
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal Dir = FVector_NetQuantizeNormal(1.0, 0.0, 0.0);

	/** Lado hacia el que se curva: +1 o -1 (al azar al lanzarlo). Se replica una vez. */
	UPROPERTY(Replicated)
	int8 Curve = 1;

private:
	/**
	 * Dónde está el disco Seconds después de nacer. OutLeg: 0 ida, 1 giro, 2 vuelta; OutBackAlpha: avance de la vuelta (0..1).
	 * Solo depende de lo replicado y de la posición de quien lo lanzó, así que sale igual en todas las máquinas.
	 */
	FVector ComputePosition(double Seconds, int32& OutLeg, double& OutBackAlpha) const;

	/** Coloca el actor donde toca a los Seconds. OutPrev: dónde estaba antes de moverlo. */
	void UpdatePose(double Seconds, FVector& OutPrev, int32& OutLeg, double& OutBackAlpha);

	/** Servidor: golpea a lo que toca el disco al ir de Prev a Cur (una vez por víctima y pasada). */
	void ServerSweepHits(const FVector& Prev, const FVector& Cur);

	/** Partículas y texto que siguen vivos aunque el disco haya acabado. */
	void TickFX(float DeltaSeconds);

	/** Estela y destellos de vuelo (al construir) y, la primera vez que hacen falta, los golpes y la nubecilla. */
	void EnsureFlightFX();
	void EnsureImpactFX();

	// Servidor.
	TSet<TWeakObjectPtr<AActor>> HitThisPass;
	int32 LastLeg = -1;

	// Todas las máquinas.
	double HeadingYaw = 0.0;
	int32 CurrentLeg = 0;

	// Visual.
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> DiscMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShadowMesh;

	float WhirrClock = 0.f;
	bool bImpactFXReady = false;

	TNAmbientFX::FEmitter Trail;
	TNAmbientFX::FEmitter Glitter;
	TNAmbientFX::FEmitter Impact;
	TNAmbientFX::FEmitter Poof;
	FTNTrapPopText BonkText;
};
