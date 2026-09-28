#pragma once

#include "CoreMinimal.h"
#include "Core/ITN_EnemyTargetInterface.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachGiantCrab.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Cangrejo gigante de la playa (ETNBeachElement::GiantCrab): cangrejo de unos 5 m de ancho (una cría de 20 cm a escala)
 * con una pinza enorme a la derecha. Anda de lado, con ojos en pedúnculos que se mueven y espuma en la boca.
 *
 *  - Se mueve como un cangrejo: de lado, con las patas en dos grupos que se alternan (dan el paso a la vez las patas
 *    1 y 3 de un lado con la 2 y la 4 del otro), el cuerpo que se balancea con cada paso y se inclina hacia donde va,
 *    acelera y frena (no arranca ni se para en seco) y gira poco a poco. Rodea lo grande del reparto mirando por delante
 *    (SteerAroundObstacles) y, si aun así se queda atascado, busca por dónde salir un momento.
 *  - Patrulla todo el rato su propio recorrido (ida y vuelta de lado, un óvalo o entre dos rocas), con paradas cortas
 *    chasqueando la pinza en los extremos. Ve de frente (cono de 140°) y oye alrededor: si una tortuga que está dentro
 *    de su correa entra en el cono o en el radio de oído (mayor si corre, menor si va agachada, en bola o quieta), se da
 *    la vuelta y la persigue de lado (más rápido que andando, más lento que esprintando). Si la pierde, vuelve a su
 *    recorrido por el punto más cercano.
 *  - Mazazo (de cerca): se para, levanta la pinza, que tiembla ~0,6 s, y en la arena aparece la sombra de dónde va a
 *    caer (donde estará la tortuga); la pinza cae de golpe y a quien pille dentro la deja en bola aturdida.
 *  - Embestida (a media distancia): se agacha clavando las patas, sale disparado de lado hacia la tortuga, más rápido
 *    que ella esprintando, y frena derrapando con surcos y arena por delante. A quien arrolla la derriba (ragdoll)
 *    lanzada en su sentido; si se estampa contra algo grande, se queda mareado.
 *  - Mareo (lo que se le lanza, ApplyHitStun, o la concha trampa por ITN_EnemyTargetInterface): quieto con los ojos
 *    dando vueltas y los pajaritos encima. La tinta lo ciega (vuelve a su recorrido).
 *
 * Reutiliza la decisión de persecución del cangrejo de siempre (TNCrabLogic::DecideChaseTransition, con tests).
 * Huella del contrato: 25 m de radio; el cuerpo patrulla dentro de la mitad.
 */
UCLASS()
class TORTUNABO_API ATN_BeachGiantCrab : public ATN_BeachEnemy, public ITN_EnemyTargetInterface
{
	GENERATED_BODY()

public:
	ATN_BeachGiantCrab();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── ITN_EnemyTargetInterface ──
	virtual void ApplyStun(float Duration) override;
	virtual void ApplyBlind(float Duration) override;
	virtual bool IsStunned() const override;
	virtual bool IsBlinded() const override;

	// ── Mareo por lo que se le lanza ──
	virtual void ApplyHitStun(float Seconds, AActor* InstigatorActor) override;
	virtual bool GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const override;
	virtual FVector GetHitStunAnchor() const override;
	virtual float GetHitStunScale() const override;

protected:
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnMoverStateChanged(uint8 OldState) override;
	virtual float GetBodyRadius() const override;
	virtual float GetActiveRange() const override { return LeashRadius + DetectRadius + 1500.f; }

	/** Todas las máquinas: la pinza ha caído en Where (bHit: ha pillado a alguien). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSlam(FVector_NetQuantize Where, bool bHit);

	/** Todas las máquinas: la embestida ha arrollado a una tortuga en Where. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRam(FVector_NetQuantize Where);

	/** Todas las máquinas: se ha estampado contra algo grande en Where. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCrash(FVector_NetQuantize Where);

private:
	/** Reloj del servidor en que acaba la ceguera por la tinta (replicado una vez). */
	UPROPERTY(Replicated)
	float BlindEndTime = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Scaler;

	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> BodyBlock;

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

	/** Sombra de dónde va a caer la pinza. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ClawShadow;

	/** Surcos de la embestida en la arena (se reutilizan en rueda; se crean al hacer falta). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Furrows;

	/** Tamaño propio (SizeScale acotado) y medidas de la zona ya escaladas. */
	float SizeK = 1.f;
	float PatrolRadius = 1300.f;
	float DetectRadius = 2200.f;
	float HearRadius = 1000.f;
	float LeashRadius = 3800.f;
	float Reach = 700.f;

	// Servidor.
	TWeakObjectPtr<ATortugaCharacter> Target;
	FVector PatrolGoal = FVector::ZeroVector;
	float StateLeft = 0.f;
	float AttackCooldown = 0.f;
	float GroundTimer = 0.f;
	float GroundZ = 0.f;

	/** Marcha: velocidad en planta (cm/s), velocidad de giro (grados/s) y hacia qué lado anda de lado (+1 derecha). */
	FVector2D MoveVel = FVector2D::ZeroVector;
	float YawRate = 0.f;
	float SideSign = 0.f;

	/** Atascos: desde dónde se mide, cuánto lleva midiendo, cuántas veces seguidas y la salida en curso. */
	FVector StuckFrom = FVector::ZeroVector;
	float StuckTimer = 0.f;
	int32 StuckCount = 0;
	FVector2D EscapeDir = FVector2D::ZeroVector;
	float EscapeLeft = 0.f;

	/** Embestida: hacia dónde va y cuándo puede volver a embestir. */
	FVector ChargeDir = FVector::ForwardVector;
	float ChargeCooldownLeft = 0.f;

	/** Recorrido de patrulla (puntos en el mundo), dónde va y en qué sentido, y en qué puntos se para. */
	TArray<FVector> Route;
	TArray<uint8> RouteStops;
	int32 RouteIndex = 0;
	int32 RouteStep = 1;
	bool bRouteLoops = false;
	bool bRouteBuilt = false;

	// Visual.
	float Gait = 0.f;
	float VisualClock = 0.f;
	FVector LastShown = FVector::ZeroVector;
	/** Velocidad que se ve (suavizada) en el mundo y en el marco del cangrejo, y cuánto anda (0-1,5). */
	FVector ShownVel = FVector::ZeroVector;
	FVector LocalVel = FVector::ZeroVector;
	float Moving = 0.f;
	float LeanRoll = 0.f;
	float LeanPitch = 0.f;
	float SkitterTimer = 0.f;
	float ClackTimer = 0.f;
	float FurrowTimer = 0.f;
	int32 NextFurrow = 0;
	TArray<float> FurrowAge;
	TNAmbientFX::FEmitter Foam;
	TNAmbientFX::FEmitter SandPuff;
	TNAmbientFX::FEmitter SandGrains;
	TNAmbientFX::FEmitter DragSand;

	void BuildCrab();
	/** Servidor: arma el recorrido (ida y vuelta, óvalo o entre dos rocas) fuera de lo grande del reparto. */
	void BuildRoute();
	/** Servidor: pasa al siguiente punto del recorrido (en la ida y vuelta, da la vuelta en los extremos). */
	void AdvanceRoute();
	/** Servidor: el punto del recorrido más cercano a donde está (para volver tras perder a la tortuga). */
	int32 NearestRoutePoint() const;
	/** Servidor: la tortuga que ve (cono delantero) u oye (alrededor), dentro de su correa; la más cercana. */
	ATortugaCharacter* Perceive() const;

	/**
	 * Servidor: acelera o frena hacia Goal a MaxSpeed como mucho (con bArrive, frena para llegar parado), rodeando lo
	 * grande del reparto y saliendo de los atascos. Devuelve la posición siguiente (suelo incluido) sin moverlo.
	 */
	FVector Drive(const FVector& Goal, float MaxSpeed, float DeltaSeconds, bool bArrive, float Accel, float Decel);
	/**
	 * Servidor: la posición siguiente con MoveVel (sin meterse en nada, dentro de la correa, sobre la arena); MoveVel se
	 * queda con lo que de verdad avanza (si algo lo para, se para). bOutBlocked: algo grande lo ha frenado de golpe.
	 */
	FVector Integrate(float DeltaSeconds, bool* bOutBlocked = nullptr);
	/** Servidor: el giro de este paso hacia WantYaw (acelera y frena el giro, sin pasarse). */
	float UpdateFacing(float WantYaw, float DeltaSeconds, float MaxRate);
	/** Servidor: hacia dónde mirar para andar de lado en el rumbo Heading (el costado que menos haya que girar, con margen). */
	float SidewaysYaw(float Heading);
	/** Servidor: anda hacia Goal, de lado o de frente. */
	void ServerWalk(const FVector& Goal, float Speed, float DeltaSeconds, bool bSideways, bool bArrive = true);
	/** Servidor: frena hasta pararse (Decel cm/s²) girando hacia WantYaw (MaxTurnRate 0: sin girar). */
	void ServerBrake(float DeltaSeconds, float Decel, float WantYaw, float MaxTurnRate);

	void StartWindUp(ATortugaCharacter* Victim);
	void ResolveSlam();
	/** Servidor: true si puede embestir hacia To (de lado, sin nada grande en medio). */
	bool IsChargeLaneClear(const FVector& To);
	void StartChargePrep(ATortugaCharacter* Victim);
	/** Servidor: derriba a quien arrolle la embestida. */
	void ChargeHits();
	/** Servidor: sale de la embestida hacia la persecución o de vuelta a su recorrido. */
	void EndCharge();

	void PoseCrab(float DeltaSeconds);
	/** Visual: surcos y arena de la embestida y del derrape. */
	void TickDrag(float DeltaSeconds, bool bNear);
};
