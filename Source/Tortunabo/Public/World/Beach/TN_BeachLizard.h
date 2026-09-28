#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachLizard.generated.h"

class UStaticMeshComponent;

/** Carácter de un lagarto de la playa (sale de su semilla: el mismo en todas las máquinas). */
enum class ETNBeachLizardTemper : uint8
{
	/** Huidizo: se asusta y se esconde (el de siempre). */
	Shy,
	/** Generoso: al irse deja un premio (un objeto o una concha de puntos). Lomo con motas doradas que brillan. */
	Generous,
	/** Mordedor: se lanza a por ti, te muerde, te zarandea y te deja mareada. Cresta roja de púas. */
	Biter,
};

/**
 * Lagarto enorme de la playa (ETNBeachElement::Lizard): un lagarto de unos 55 cm reales (15 m a escala) que toma el sol
 * (flexiones, cabeceos, lengua, cola que se mece) unos segundos y se va andando a otro rincón de su zona, apartándose de
 * los demás enemigos. Cada uno tiene su carácter (ETNBeachLizardTemper, por semilla), que se ve:
 *
 *  - Huidizo (el de siempre): si una tortuga se acerca, se pone alerta y la mira; si se acerca más, unas veces da un
 *    susto (sacudida, se hincha, saca la lengua y bufa, con un amago que como mucho empuja un poco) y otras sale
 *    corriendo sin más. Huye a la roca, tronco o restos más cercanos que no queden hacia la tortuga y se mete debajo; si
 *    no hay ninguno, se entierra sacudiéndose. Al rato, si no hay nadie cerca, sale y vuelve a tomar el sol.
 *  - Generoso (motas doradas en el lomo que destellan): huye igual, pero la primera vez deja un premio donde estaba: un
 *    objeto del catálogo (pesos de la carrera) o una concha de puntos.
 *  - Mordedor (cresta roja de púas): no huye; se lanza a por la tortuga y, si la alcanza de pie (en bola se libra), la
 *    muerde por el caparazón, la zarandea con ella en la boca y la lanza mareada en bola (~1,5 s). Luego la deja en paz
 *    un rato y vuelve a su sitio.
 *
 * Lo que se le lanza lo marea (ApplyHitStun): se queda tumbado con pajaritos y, si tenía a alguien en la boca, lo suelta.
 * Red: el servidor decide; la tortuga en la boca la coloca cada máquina con las mismas cuentas (BeginHoldTurtle).
 */
UCLASS()
class TORTUNABO_API ATN_BeachLizard : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachLizard();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Carácter que sale de una semilla (el mismo en todas las máquinas). */
	static ETNBeachLizardTemper TemperOfSeed(int32 Seed);

	/** Una semilla, desde Start, con ese carácter (para TN.Beach.Lizard). */
	static int32 FindSeedForTemper(ETNBeachLizardTemper InTemper, int32 Start);

	/** Nombre para el registro y la consola («huidizo», «generoso», «mordedor»). */
	static const TCHAR* TemperName(ETNBeachLizardTemper InTemper);

	ETNBeachLizardTemper GetTemper() const { return Temper; }

	// ── Mareo por lo que se le lanza ──
	virtual void ApplyHitStun(float Seconds, AActor* InstigatorActor) override;
	virtual bool GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const override;
	virtual FVector GetHitStunAnchor() const override;
	virtual float GetHitStunScale() const override;

protected:
	virtual void ApplySpec() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnMoverStateChanged(uint8 OldState) override;
	virtual float GetBodyRadius() const override;
	virtual float GetActiveRange() const override { return 4500.f * SizeK; }

	/** Todas las máquinas: el susto empuja un poco a Victim (el dueño lo aplica también para no corregir). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShove(ATortugaCharacter* Victim, FVector_NetQuantize10 Push);

	/** Todas las máquinas: el mordedor ha cerrado la boca en Where (con tortuga o no). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBite(FVector_NetQuantize Where, bool bCaught);

	/** Todas las máquinas: el generoso ha dejado su premio en Where. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPrize(FVector_NetQuantize Where);

private:
	/** La tortuga que tiene en la boca el mordedor (replicado: cada máquina la coloca en su boca). */
	UPROPERTY(Replicated)
	TObjectPtr<ATortugaCharacter> HeldVictim;

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

	/** Pivotes (medidas de la fauna, antes de escalar) y medidas del cuerpo. */
	FVector BodyPivot = FVector::ZeroVector;
	FVector HeadPivot = FVector::ZeroVector;
	FVector TailPivot = FVector::ZeroVector;
	FVector TonguePivot = FVector::ZeroVector;
	FVector LegPivots[4];
	float RigHeight = 10.f;
	float BodyHalfLen = 10.f;
	float BodyHalfWidth = 4.4f;
	float HeadSize = 4.3f;
	float SizeK = 1.f;
	ETNBeachLizardTemper Temper = ETNBeachLizardTemper::Shy;

	// Servidor.
	TWeakObjectPtr<ATortugaCharacter> Threat;
	TWeakObjectPtr<ATortugaCharacter> LungeTarget;
	FVector FleeGoal = FVector::ZeroVector;
	bool bToRock = false;
	float StateLeft = 0.f;
	float CalmTime = 0.f;
	float GroundTimer = 0.f;
	float GroundZ = 0.f;
	bool bShoved = false;
	bool bPrizeDropped = false;
	/** El premio del generoso (se quita con él si nadie lo ha cogido). */
	TWeakObjectPtr<AActor> Prize;

	// Visual.
	float Clock = 0.f;
	float Gait = 0.f;
	float Moving = 0.f;
	float Sink = 0.f;
	float TongueTimer = 2.f;
	float TongueAge = 10.f;
	float SkitterTimer = 0.f;
	float GlintTimer = 0.f;
	FVector LastShown = FVector::ZeroVector;
	TNAmbientFX::FEmitter Dust;
	TNAmbientFX::FEmitter Glints;

	void BuildLizard();
	/** Servidor: la tortuga más cercana (viva, sin aturdir) y su distancia plana. */
	ATortugaCharacter* NearestTurtle(float& OutDist) const;
	/** Servidor: busca dónde esconderse lejos de From (roca, tronco...); false si no hay nada a mano. */
	bool FindHideSpot(const FVector& From, FVector& OutSpot) const;
	/** Servidor: avanza hacia Goal mirando hacia donde va. true al llegar. */
	bool RunToward(const FVector& Goal, float MoveSpeed, float DeltaSeconds, float TurnRate);
	void StartFlee(const FVector& From);
	/** Servidor: la tortuga Near le ha asustado de cerca: según su carácter, susto, huida con premio o mordisco. */
	void ReactClose(ATortugaCharacter* Near, float Dist);
	/** Servidor: el mordedor se lanza a por Victim. */
	void StartLunge(ATortugaCharacter* Victim);
	/** Servidor: se le puede morder (de pie, sin que la lleve nadie ni esté en bola). */
	bool CanBite(const ATortugaCharacter* Turtle) const;
	/** Servidor: la tiene en la boca y empieza a zarandearla. */
	void StartBite(ATortugaCharacter* Victim);
	/** Servidor: suelta a la de la boca (bToss: la lanza mareada; si no, la deja caer con un mareo corto). */
	void ReleaseVictim(bool bToss);
	/** Servidor: deja el premio del generoso donde está. */
	void DropPrize();

	/** Giro de la cabeza zarandeando (Age: segundos desde el mordisco), el mismo en todas las máquinas. */
	FRotator ShakeHeadRotation(float Age) const;
	/** Punta del hocico en el mundo con el lagarto en Base mirando a Yaw y la cabeza girada HeadRot (cuerpo sin girar). */
	FVector MouthAt(const FVector& Base, float Yaw, const FRotator& HeadRot) const;
	/** Todas las máquinas: coloca en la boca a la tortuga mordida (o la suelta) según el estado replicado. */
	void TickHold();

	void PoseLizard(float DeltaSeconds);
};
