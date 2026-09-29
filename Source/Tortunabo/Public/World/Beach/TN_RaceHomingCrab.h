#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_RaceItemActor.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_RaceHomingCrab.generated.h"

class ATortugaCharacter;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Cangrejo teledirigido (Docs/Modo_Carrera.md, «Objetos de carrera»): la «concha roja» de la playa. Un cangrejito de juguete
 * rojo (~1,1 m de ancho, con una antenita de mando a distancia que lleva una bolita que parpadea) sale corriendo hacia la
 * tortuga que va por delante de quien lo lanza y, si la alcanza, la derriba.
 *
 *  - Objetivo (al lanzarlo, ServerLaunch): la tortuga más cercana por delante (TNRaceItems::FindTurtleTarget); si no hay
 *    ninguna, el enemigo de la playa más cercano por delante (a menos de 80 m, que se pueda marear); si no hay nada,
 *    ServerLaunch devuelve false y el objeto se queda. Como mucho hay 10 en el mundo.
 *  - Carrera: nace 2,5 m por delante de quien lo lanza, en el suelo, a 9 m/s y llega a 16 m/s en 0,8 s. Gira hacia el objetivo
 *    hasta 420°/s (su radio de giro a toda velocidad, ~2,2 m, es menor que el del golpe, 2,6 m: no puede dar vueltas alrededor
 *    de la tortuga sin alcanzarla) y va dando saltitos de 0,45 s y 1,2 m de alto encadenados. No choca con nada: es un
 *    proyectil. Vive 12 s como mucho.
 *  - Golpe: a menos de 2,6 m en planta y en vertical. A una tortuga la derriba con ragdoll (TNBeach::KnockDownTurtle); si es
 *    invulnerable (protector solar, pelícano) o no se le puede dar ahora (ya aturdida o derribada), rebota con un «bonk».
 *    A un enemigo lo marea 4 s (ATN_BeachEnemy::ApplyHitStun). Si el objetivo se acaba (muerta, llegó a la meta, se la
 *    traga el gusano, el enemigo desaparece), sigue recto hasta que caduca.
 *
 * Red: el servidor mueve el actor y lo replica con Track (ATN_RaceItemActor: siempre relevante, 20 Hz y suavizado en los
 * clientes) sin la altura del salto: los saltitos salen de una fórmula del reloj del servidor (la misma en todas las
 * máquinas), así que se ven suaves sin replicarlos. El golpe llega a todos por sus vías de siempre (derribo, mareo) y el
 * «bonk» y la nube, por un multicast no fiable.
 */
UCLASS(NotBlueprintable)
class TORTUNABO_API ATN_RaceHomingCrab : public ATN_RaceItemActor
{
	GENERATED_BODY()

public:
	/**
	 * Servidor: lanza un cangrejo teledirigido desde Turtle. false (sin crear nada) si no hay a quién perseguir o ya hay
	 * demasiados cangrejos en el mundo; true si lo ha creado (el uso gasta el objeto).
	 */
	static bool ServerLaunch(ATortugaCharacter* Turtle);

protected:
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildVisuals() override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnFinished() override;

	/** Todas las máquinas: el cangrejo ha dado en Where (o ha rebotado sin hacer daño): «bonk» y nube. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHit(FVector_NetQuantize10 Where, bool bBounced);

	/** Todas las máquinas: se le ha acabado la cuerda en Where sin dar a nadie: una nube pequeña. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFizzle(FVector_NetQuantize10 Where);

private:
	// ── Servidor ─────────────────────────────────────────────────────────────

	/** El punto al que corre ahora (una tortuga: su sitio; un enemigo: el punto más cercano de su cápsula). false si ya no hay objetivo. */
	bool ResolveChasePoint(float DeltaSeconds, FVector& OutPoint);

	/** Da al objetivo (derriba a la tortuga o marea al enemigo), avisa a todas las máquinas y acaba. */
	void ApplyHit(const FVector& Where);

	/** Se acaba sin dar a nadie (caduca o acaba la carrera). */
	void Fizzle();

	/** A quién persigue (una tortuga o un enemigo). Débil: puede desaparecer. */
	TWeakObjectPtr<AActor> Chased;

	/** Ya no hay objetivo: sigue recto hasta caducar. */
	bool bChaseLost = false;

	/** Rumbo (grados alrededor de Z). */
	float HeadingYaw = 0.f;

	/** Cuenta atrás para comprobar que la tortuga perseguida sigue en carrera. */
	float RacerClock = 0.f;

	// ── Visual (máquinas con pantalla) ───────────────────────────────────────

	/** Sube y baja con cada saltito (la altura sale del reloj del servidor). */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Hopper;

	/** Escala del cangrejo (las mallas están a escala 28: cangrejo de 5 m). */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Scaler;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Eyes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Legs;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BigArm;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BigHand;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BigFinger;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SmallClaw;

	/** Antena del mando y su bolita (encendida y apagada: una de las dos se ve). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Antenna;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BallOn;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BallOff;

	/** Sombra redonda en la arena. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Shadow;

	/** Polvo de arena que levanta al correr y al aterrizar de cada saltito. */
	TNAmbientFX::FEmitter Dust;

	float VisualClock = 0.f;
	float GaitPhase = 0.f;
	float ScuttleClock = 0.f;
	float PrevHopFraction = 0.f;
	float ShownYaw = 0.f;
	float LeanRoll = 0.f;
	bool bBornSoundPlayed = false;
	bool bBallLit = true;
};
