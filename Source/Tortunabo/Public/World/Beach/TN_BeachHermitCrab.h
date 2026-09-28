#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachHermitCrab.generated.h"

class UStaticMeshComponent;
class UTN_BeachCritterSynthComponent;

/** Una muestra (cada 1/30 s) del camino de una rodada del ermitaño: igual en todas las máquinas. */
struct FTNHermitRollSample
{
	/** A lo largo de la calle y de lado (cm). */
	float S = 0.f;
	float Lat = 0.f;
	/** Cota de la base de la bola (cm, mundo). */
	float Z = 0.f;
	/** Velocidad a lo largo de la calle (cm/s) y vueltas que lleva (radianes). */
	float Speed = 0.f;
	float Spin = 0.f;
	/** Fuerza del bote que acaba de caer en esta muestra (cm/s; 0 = ninguno). */
	float Land = 0.f;
};

/**
 * Cangrejo ermitaño bola (ETNBeachElement::HermitCrab): un ermitaño con una caracola de ~2,2 m (4 cm reales a escala)
 * que espera asomado en lo alto de su calle y, cuando una tortuga entra en ella, se mete dentro y rueda cuesta abajo
 * como una bola de bolos.
 *
 *  - Calle: el eje X local del actor, centrada en él, con Spec.Extent de largo (0 = 40 m). Lo alto es el extremo de -X
 *    (el reparto la orienta cuesta abajo hacia +X); solo si el de +X queda 60 cm o más por encima, rueda al revés.
 *  - Espera asomado (ojos, antenas y pinza que saluda). Con una tortuga atacable en la calle (entre 2,5 m por delante de
 *    él y 2 m antes del final, a menos de 5,2 m del eje) se mete en la concha (0,55 s) y rueda: acelera (3,8 m/s² más la
 *    cuesta) hasta 15 m/s, bota con el relieve y da botes sueltos, y culebrea un poco. En los últimos 7 m frena y se para
 *    al final de la calle.
 *  - Derriba con ragdoll (TNBeach::KnockDownTurtle) a quien pilla, a todas las que estén en fila: la bola no se para.
 *  - Al final asoma, se sacude (2,1 s en total, dando la vuelta) y vuelve andando a lo alto; allí se da la vuelta y
 *    vuelve a esperar.
 *  - Mareado por un golpe (IsHitStunned): si rodaba, se para en seco; asoma mareado y, al pasársele, vuelve andando.
 *
 * Red: el servidor decide cuándo rueda y a quién da; la rodada es determinista (el camino se calcula en cada máquina con
 * el mismo perfil del suelo, la misma semilla y la hora del estado con el reloj del servidor) y los demás estados son
 * función de su hora, su punto de partida (Mover.Location) y su meta (Mover.Aim). Nada se replica mientras rueda.
 */
UCLASS()
class TORTUNABO_API ATN_BeachHermitCrab : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachHermitCrab();

	virtual bool GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const override;
	virtual FVector GetHitStunAnchor() const override;
	virtual float GetHitStunScale() const override;

protected:
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual float GetBodyRadius() const override;
	virtual float GetActiveRange() const override;
	virtual float GetVisualRange() const override { return 36000.f; }

	/** Todas las máquinas: la bola ha derribado a Victim en Where (HitCount: cuántas lleva en esta rodada). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStrike(ATortugaCharacter* Victim, FVector_NetQuantize Where, uint8 HitCount);

private:
	/** Centro de la bola (absoluto): la caracola y el cangrejo cuelgan de aquí. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> BodyRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShellMesh;

	/** Boca de la caracola: el cangrejo entero (se mete y se saca escalándolo hacia dentro). */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CrabRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Antennae;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Eyes;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BigClaw;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SmallClaw;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Legs;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Shadow;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachCritterSynthComponent> Sound;

	/** Tamaño propio (SizeScale acotado), radio de la bola y medidas de la calle ya escaladas. */
	float SizeK = 1.f;
	float BallRadius = 112.f;
	float LaneLength = 4000.f;
	float LaneHalfWidth = 520.f;

	// Calle: el servidor la decide una vez (lo alto y lo bajo) y la manda en Mover.
	bool bLaneReady = false;
	FVector LaneTop = FVector::ZeroVector;
	FVector LaneBottom = FVector::ZeroVector;

	// Perfil del suelo a lo largo de la calle (cada máquina, una vez): tres líneas (izquierda, eje y derecha).
	TArray<float> Profile;
	int32 ProfileCount = 0;
	FVector ProfileTop = FVector::ZeroVector;
	FVector ProfileDir = FVector::ForwardVector;
	bool bProfileReady = false;

	// Rodada (cada máquina): el camino de la rodada con número Mover.Serial.
	TArray<FTNHermitRollSample> RollPath;
	uint8 PathSerial = 0;
	bool bPathValid = false;
	FVector PathTop = FVector::ZeroVector;
	FVector PathDir = FVector::ForwardVector;
	float PathDuration = 0.f;

	// Servidor.
	float TriggerTimer = 0.f;
	FVector PrevBallCenter = FVector::ZeroVector;
	bool bPrevBallValid = false;
	uint8 HitsThisRoll = 0;

	// Visual.
	float VisualClock = 0.f;
	float CrabOut = 1.f;
	float PrevAge = 0.f;
	uint8 PrevSerial = 0;
	int32 LastLandIndex = -1;
	float TapTimer = 0.f;
	float LegPhase = 0.f;
	TNAmbientFX::FEmitter Dust;
	TNAmbientFX::FEmitter Grains;

	/** Servidor: decide lo alto y lo bajo de la calle y se pone a esperar. */
	void EnsureLane();
	/**
	 * Lo alto y lo bajo de la calle en esta máquina: en el servidor, EnsureLane; en los clientes, el extremo de la calle
	 * más cercano a lo alto que dice Mover. false si aún no se sabe.
	 */
	bool ResolveLane();
	/** Dirección (plana) de lo alto a lo bajo de la calle. */
	FVector LaneDir() const;
	/** Perfil del suelo de la calle que baja de Top hacia Dir (se hace una vez por calle). */
	void EnsureProfile(const FVector& Top, const FVector& Dir);
	/** Suelo del perfil a S cm de lo alto y Lat de lado (fuera del perfil, el borde). */
	float ProfileGround(float S, float Lat) const;
	/** Camino de la rodada de lo alto (Top) a lo bajo (Bottom) con la semilla Seed. */
	void BuildPath(const FVector& Top, const FVector& Bottom, uint32 Seed);
	/** Camino de la rodada actual (lo calcula la primera vez que hace falta). */
	bool EnsurePath();
	/** Base de la bola, vueltas, velocidad e índice de muestra a Age segundos de empezar a rodar. */
	FVector SamplePath(float Age, float& OutSpin, float& OutSpeed, int32& OutIndex) const;
	/** Dónde va andando a Age segundos (de Mover.Location a Mover.Aim), sobre el suelo. */
	FVector WalkPosition(float Age, float& OutSpeed) const;
	/** Suelo bajo un punto: el perfil si cae en la calle; si no, el del generador. */
	float GroundAt(const FVector& Where) const;
	/** Servidor: hay una tortuga atacable dentro de la calle. */
	bool AnyTurtleInLane() const;
	/** Servidor: derriba a quien toque la bola entre From y To (centros). */
	void CheckRollHits(const FVector& From, const FVector& To, float Speed);
	/** Servidor: pasa a mareado donde está, mirando a Yaw. */
	void EnterDizzy(const FVector& Where, float Yaw);

	void BuildCrab();
	void PoseCrab(uint8 State, float Age, float DeltaSeconds, bool bStunned, float MoveSpeed);
};
