#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachLizard.generated.h"

class UStaticMeshComponent;

/**
 * Lagarto enorme de la playa (ETNBeachElement::Lizard): vida del ambiente, no un enemigo de verdad. Un lagarto de unos
 * 55 cm reales (15 m a escala) que toma el sol (flexiones, cabeceos, lengua, cola que se mece) unos segundos y se va
 * andando a otro rincón de su zona, apartándose de los demás enemigos.
 *
 *  - Si una tortuga se acerca, se pone alerta y la mira. Si se acerca más, unas veces da un susto (sacudida, se hincha,
 *    saca la lengua y bufa, con un amago hacia ella que como mucho empuja un poco) y otras sale corriendo sin más.
 *  - Huye a la roca, tronco o restos más cercanos que no queden hacia la tortuga y se mete debajo; si no hay ninguno, se
 *    entierra en la arena sacudiéndose. Al rato, si no hay nadie cerca, vuelve a salir y regresa a tomar el sol.
 *
 * No aturde ni derriba a nadie: el empujón del susto es pequeño (servidor y dueño a la vez, como las piezas del parque del
 * lobby), y nunca a una tortuga derribada, aturdida o en el pico de una gaviota.
 */
UCLASS()
class TORTUNABO_API ATN_BeachLizard : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachLizard();

protected:
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnMoverStateChanged(uint8 OldState) override;
	virtual float GetBodyRadius() const override;
	virtual float GetActiveRange() const override { return 4500.f * SizeK; }

	/** Todas las máquinas: el susto empuja un poco a Victim (el dueño lo aplica también para no corregir). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShove(ATortugaCharacter* Victim, FVector_NetQuantize10 Push);

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Scaler;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Tail;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Tongue;

	/** Patas: delantera izquierda, delantera derecha, trasera izquierda, trasera derecha. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Legs;

	/** Pivotes (medidas de la fauna, antes de escalar). */
	FVector BodyPivot = FVector::ZeroVector;
	FVector HeadPivot = FVector::ZeroVector;
	FVector TailPivot = FVector::ZeroVector;
	FVector TonguePivot = FVector::ZeroVector;
	FVector LegPivots[4];
	float RigHeight = 10.f;
	float SizeK = 1.f;

	// Servidor.
	TWeakObjectPtr<ATortugaCharacter> Threat;
	FVector FleeGoal = FVector::ZeroVector;
	bool bToRock = false;
	float StateLeft = 0.f;
	float CalmTime = 0.f;
	float GroundTimer = 0.f;
	float GroundZ = 0.f;
	bool bShoved = false;

	// Visual.
	float Clock = 0.f;
	float Gait = 0.f;
	float Moving = 0.f;
	float Sink = 0.f;
	float TongueTimer = 2.f;
	float TongueAge = 10.f;
	float SkitterTimer = 0.f;
	FVector LastShown = FVector::ZeroVector;
	TNAmbientFX::FEmitter Dust;

	void BuildLizard();
	/** Servidor: la tortuga más cercana (viva, sin aturdir) y su distancia plana. */
	ATortugaCharacter* NearestTurtle(float& OutDist) const;
	/** Servidor: busca dónde esconderse lejos de From (roca, tronco...); false si no hay nada a mano. */
	bool FindHideSpot(const FVector& From, FVector& OutSpot) const;
	/** Servidor: avanza hacia Goal mirando hacia donde va. true al llegar. */
	bool RunToward(const FVector& Goal, float MoveSpeed, float DeltaSeconds, float TurnRate);
	void StartFlee(const FVector& From);
	void PoseLizard(float DeltaSeconds);
};
