#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "World/Beach/TN_BeachElement.h"
#include "TN_BeachEnemy.generated.h"

class ATortugaCharacter;
class USceneComponent;
class UTN_BeachEnemySynthComponent;

/**
 * Movimiento replicado barato de un enemigo de la playa: dónde está (el suelo bajo el cuerpo), hacia dónde mira, su
 * estado, cuándo empezó (reloj del servidor) y un punto de interés del estado (dónde cae la pinza, la roca a la que
 * corre...). Se replica con la frecuencia de red del actor (~10 Hz) y solo lo que cambia; los clientes interpolan.
 */
USTRUCT()
struct FTNBeachMoverRep
{
	GENERATED_BODY()

	/** Suelo bajo el centro del cuerpo (cm, cuantizado a 1 cm). */
	UPROPERTY()
	FVector_NetQuantize Location = FVector_NetQuantize(0.0, 0.0, 0.0);

	/** Punto de interés del estado actual. */
	UPROPERTY()
	FVector_NetQuantize Aim = FVector_NetQuantize(0.0, 0.0, 0.0);

	/** Hacia dónde mira (grados comprimidos a 16 bits). */
	UPROPERTY()
	uint16 Yaw = 0;

	/** Estado propio de cada enemigo (su enum). */
	UPROPERTY()
	uint8 State = 0;

	/** Sube en cada cambio de estado (aunque se repita el mismo). */
	UPROPERTY()
	uint8 Serial = 0;

	/** Reloj del servidor (GetServerWorldTimeSeconds) al empezar el estado. */
	UPROPERTY()
	float StateTime = 0.f;
};

/**
 * Base de los enemigos de la playa (cangrejo, erizo, lagarto, paso de quads y zona de gaviotas). No cambia el contrato
 * de ATN_BeachElement: lo amplía con lo que comparten.
 *
 *  - Red con servidor escucha: el servidor decide objetivos, golpes y aturdimientos (ServerTick) y escribe Mover; los
 *    clientes interpolan la posición y el giro (con una pizca de extrapolación) y animan a partir del estado. Los
 *    momentos puntuales (golpes) van por multicast no fiable. Siempre relevantes: si no, un cliente destruiría y
 *    reconstruiría sus mallas al alejarse más de 150 m por la playa.
 *  - Visual (VisualTick) solo en máquinas con pantalla; la raíz animada (Rig) se coloca en todas (su colisión cuenta
 *    también en un servidor dedicado).
 *  - Utilidades: tortugas vivas, suelo bajo un punto, reloj del servidor, carrera en marcha, aturdir (TNBeach::StunTurtle),
 *    temblor de cámara y una voz sintetizada (UTN_BeachEnemySynthComponent).
 *
 * Consola: TN.Beach.Enemy.Debug 1 dibuja radios y estados en el servidor.
 */
UCLASS(Abstract)
class TORTUNABO_API ATN_BeachEnemy : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachEnemy();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** TN.Beach.Enemy.Debug: dibujar radios y estados (servidor). */
	static bool IsDebugDraw();

	/** Reloj del servidor en esta máquina (s): el replicado del GameState o, sin él, el del mundo. */
	static double ServerNow(const UObject* WorldContext);

	/** Tortugas de jugadores vivas, en juego y sin haber acabado la carrera (en cualquier máquina). */
	static void GatherTurtles(const UObject* WorldContext, TArray<ATortugaCharacter*>& Out);

	/** true si la carrera está en marcha (con el GameState de la playa en otra fase no se ataca). Sin él, true. */
	static bool IsRaceLive(const UObject* WorldContext);

	/** Suelo (geometría estática) bajo Where: traza de Where + Up a Where - Down. */
	static bool TraceGround(const UObject* WorldContext, const FVector& Where, float& OutZ, FVector* OutNormal = nullptr,
		float Up = 3000.f, float Down = 8000.f);

	/** Distancia (cm) de Where a la cámara local más cercana; enorme si no hay (servidor dedicado). */
	static float LocalViewDistance(const UObject* WorldContext, const FVector& Where);

	virtual void PostInitializeComponents() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Servidor: objetivos, estados, golpes. */
	virtual void ServerTick(float DeltaSeconds) {}

	/** Máquinas con pantalla: animación, efectos y sonido. */
	virtual void VisualTick(float DeltaSeconds) {}

	/** Todas las máquinas: ha cambiado el estado replicado (en el servidor, al cambiarlo). */
	virtual void OnMoverStateChanged(uint8 OldState) {}

	/** Radio (cm) de relevancia visual: más lejos de la cámara local no se anima. */
	virtual float GetVisualRange() const { return 30000.f; }

	// ── Movimiento replicado ──────────────────────────────────────────────

	UPROPERTY(ReplicatedUsing = OnRep_Mover)
	FTNBeachMoverRep Mover;

	UFUNCTION()
	void OnRep_Mover();

	/** Servidor: nueva posición (suelo bajo el cuerpo) y giro de la simulación. */
	void ServerMoveTo(const FVector& Location, float YawDeg);

	/** Servidor: cambia de estado, con su hora y su punto de interés. */
	void ServerSetState(uint8 NewState, const FVector& Aim);
	void ServerSetState(uint8 NewState) { ServerSetState(NewState, FVector(Mover.Aim)); }

	/** Servidor: cambia el punto de interés sin cambiar de estado. */
	void ServerSetAim(const FVector& Aim);

	uint8 GetMoverState() const { return Mover.State; }

	/** Segundos en el estado actual (en clientes, con el reloj del servidor replicado). */
	float GetStateAge() const;

	/** Crea la raíz animada (Rig), enganchada a la del actor y colocada en el mundo en cada fotograma. */
	USceneComponent* MakeRig();

	/** Voz sintetizada enganchada a Parent (la crea la primera vez; null sin audio). */
	UTN_BeachEnemySynthComponent* GetVoice(USceneComponent* Parent, float InnerRadius, float Falloff);

	/** Servidor: aturde (TNBeach::StunTurtle) y apunta la hora para no repetir con la misma. */
	void StunTurtle(ATortugaCharacter* Turtle, float Seconds, const FVector& Launch);

	/** Servidor: ignorar a esta tortuga durante Seconds (tras golpearla). */
	void IgnoreTurtle(ATortugaCharacter* Turtle, float Seconds);
	bool IsIgnored(const ATortugaCharacter* Turtle) const;

	/**
	 * Servidor: la tortuga atacable más cercana a From a menos de MaxDist (plano) que además esté a menos de Leash de
	 * InHome (Leash <= 0: sin correa). Atacable: no aturdida y no ignorada.
	 */
	ATortugaCharacter* FindTarget(const FVector& From, float MaxDist, const FVector& InHome, float Leash) const;

	/** Tortuga atacable (servidor): viva, no aturdida y no ignorada. */
	bool IsTargetable(const ATortugaCharacter* Turtle) const;

	/** Posición y giro simulados (servidor). */
	FVector SimLoc = FVector::ZeroVector;
	float SimYaw = 0.f;

	/** Posición y giro que se ven en esta máquina (en el servidor, los simulados). */
	FVector ShownLoc = FVector::ZeroVector;
	float ShownYaw = 0.f;

	/** Sitio del enemigo (donde lo colocó el generador). */
	FVector Home = FVector::ZeroVector;

	/** Raíz animada: sigue a ShownLoc y ShownYaw. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Rig;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachEnemySynthComponent> Voice;

	/** Azar del servidor (sembrado con Spec.Seed). */
	FRandomStream ServerRng;

	/** Esta máquina dibuja (no es un servidor dedicado). */
	bool bHasScreen = false;

	/** Distancia de ShownLoc a la cámara local (se refresca cada fotograma con pantalla). */
	float ViewDistance = 1.0e9f;

	/** El enemigo usa Mover (los que andan); si no, la base no toca Rig ni ShownLoc. */
	bool bUsesMover = true;

private:
	TMap<TWeakObjectPtr<ATortugaCharacter>, double> IgnoreUntil;

	FVector LastRepLoc = FVector::ZeroVector;
	FVector RepVelocity = FVector::ZeroVector;
	double LastRepTime = -1.0;
	uint8 LastSerial = 0;
	uint8 LastState = 0;
	bool bHasRep = false;
	bool bVoiceTried = false;

	void UpdateShown(float DeltaSeconds);
};
