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
 *  - Patrulla su zona (paseos y ratos quieto agitando la pinza). Si una tortuga entra en su radio de visión y está
 *    dentro de su correa, la persigue de lado (más rápido que andando, más lento que esprintando).
 *  - Mazazo: se para, levanta la pinza, que tiembla ~0,6 s, y en la arena aparece la sombra de dónde va a caer (donde
 *    estará la tortuga); la pinza cae de golpe y a quien pille dentro la deja en bola aturdida (TNBeach::StunTurtle).
 *    Luego se le queda la pinza clavada un momento y vuelve a por la siguiente (a la golpeada la ignora un rato).
 *  - Si la tortuga se aleja de su zona, vuelve a casa. Los objetos del jugador (concha, bola...) lo aturden y ciegan
 *    como al cangrejo de siempre (ITN_EnemyTargetInterface): aturdido, se queda quieto con los ojos dando vueltas.
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

protected:
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnMoverStateChanged(uint8 OldState) override;

	/** Todas las máquinas: la pinza ha caído en Where (bHit: ha pillado a alguien). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSlam(FVector_NetQuantize Where, bool bHit);

private:
	/** Reloj del servidor en que acaba el aturdimiento y la ceguera por objetos (replicados una vez). */
	UPROPERTY(Replicated)
	float StunEndTime = 0.f;

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

	/** Tamaño propio (SizeScale acotado) y medidas de la zona ya escaladas. */
	float SizeK = 1.f;
	float PatrolRadius = 1300.f;
	float DetectRadius = 2200.f;
	float LeashRadius = 3800.f;
	float Reach = 700.f;

	// Servidor.
	TWeakObjectPtr<ATortugaCharacter> Target;
	FVector PatrolGoal = FVector::ZeroVector;
	float StateLeft = 0.f;
	float AttackCooldown = 0.f;
	float GroundTimer = 0.f;
	float GroundZ = 0.f;

	// Visual.
	float Gait = 0.f;
	float VisualClock = 0.f;
	FVector LastShown = FVector::ZeroVector;
	float Moving = 0.f;
	float SkitterTimer = 0.f;
	float ClackTimer = 0.f;
	TNAmbientFX::FEmitter Foam;
	TNAmbientFX::FEmitter SandPuff;
	TNAmbientFX::FEmitter SandGrains;

	void BuildCrab();
	FVector PickPatrolGoal();
	/** Servidor: siguiente posición hacia Goal (con la correa y el suelo) y la dirección de avance. */
	FVector StepToward(const FVector& Goal, float Speed, float DeltaSeconds, FVector& OutDir);
	/** Servidor: anda hacia Goal, de lado o de frente. */
	void ServerWalk(const FVector& Goal, float Speed, float DeltaSeconds, bool bSideways);
	void StartWindUp(ATortugaCharacter* Victim);
	void ResolveSlam();
	void PoseCrab(float DeltaSeconds);
};
