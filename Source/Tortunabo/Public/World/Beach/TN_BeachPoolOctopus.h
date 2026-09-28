#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachPoolOctopus.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UTN_BeachCritterSynthComponent;

/**
 * Pulpo de poza (ETNBeachElement::PoolOctopus): un pulpo de ~1,5 m de cuerpo (5,5 cm reales a escala) y brazos de
 * 3,7 m que vive dentro de una poza de la playa (el reparto lo pone en su centro; si lo ponen fuera de una poza, hace su
 * propio charco de 7 m de radio para poder probarlo en cualquier sitio).
 *
 *  - Bajo el agua se ve su silueta oscura y sus burbujas; pasea despacio por el centro de la poza.
 *  - Si una tortuga nada en su poza, se fija en ella (0,5 s de burbujas y puntas de tentáculo que asoman) y va a por
 *    ella bajo el agua a 5,2 m/s (el doble que nadando ella). A 3 m, saca la cabeza y tres brazos, la agarra y la sube
 *    (0,8 s colgando, con la sujeción de ATN_BeachEnemy::BeginHoldTurtle) y la lanza en bola (TNBeach::StunTurtle)
 *    fuera de la poza, hacia atrás (hacia la salida), con un chorro de tinta. Después se hunde y vuelve al centro.
 *  - Mareado por un golpe (IsHitStunned): suelta a la que tenga (cae al agua), flota panza arriba y, al pasársele,
 *    vuelve a hundirse.
 *
 * Red: el servidor decide a quién va y cuándo agarra y lanza; la posición del pulpo va en Mover (bajo el agua, a 8 Hz,
 * los clientes interpolan) y la tortuga agarrada, en Grabbed. Cada máquina la sujeta y la coloca igual con la hora del
 * estado (reloj del servidor) y el punto donde la cogió (Mover.Aim). El lanzamiento es el aturdimiento en bola de
 * siempre (física replicada del caparazón); la tinta y los chapoteos van por multicast no fiable.
 */
UCLASS()
class TORTUNABO_API ATN_BeachPoolOctopus : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachPoolOctopus();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const override;
	virtual FVector GetHitStunAnchor() const override;
	virtual float GetHitStunScale() const override;

protected:
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnMoverStateChanged(uint8 OldState) override;
	virtual float GetActiveRange() const override;
	virtual float GetVisualRange() const override { return 24000.f; }

	/** Todas las máquinas: ha lanzado a Victim con Launch (tinta, chapoteo, sonido y texto). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastThrow(ATortugaCharacter* Victim, FVector_NetQuantize10 Launch);

private:
	/** La tortuga que tiene agarrada (o que acaba de lanzar), para sujetarla y animar los brazos en cada máquina. */
	UPROPERTY(Replicated)
	TObjectPtr<ATortugaCharacter> Grabbed;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> BodyRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Tramos de los ocho brazos (instancias en coordenadas de mundo). */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Arms;

	/** Silueta oscura a ras del agua. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Silhouette;

	/** Charco propio (solo fuera de una poza). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Puddle;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachCritterSynthComponent> Sound;

	/** Tamaño propio (SizeScale acotado). */
	float SizeK = 1.f;

	// Poza (cada máquina, una vez): la del generador que contiene al actor o, si no hay, el charco propio.
	bool bPoolResolved = false;
	bool bHasPool = false;
	int32 PoolIndex = INDEX_NONE;
	FTransform GenXf = FTransform::Identity;
	/** Centro (a ras del agua), cota del agua, radio para pasear y profundidad del cuerpo al acechar. */
	FVector PoolHome = FVector::ZeroVector;
	float WaterZ = 0.f;
	float PoolRadius = 700.f;
	float LurkDepth = 110.f;
	/** Hacia dónde lanza: hacia la salida (contra el mar del generador). */
	FVector BackDir = -FVector::ForwardVector;

	// Servidor.
	bool bPlaced = false;
	TWeakObjectPtr<ATortugaCharacter> Target;
	FVector DriftGoal = FVector::ZeroVector;
	float ScanTimer = 0.f;
	float DriftTimer = 0.f;
	float MoveSpeedNow = 0.f;

	// Visual.
	float VisualClock = 0.f;
	float ShownDepth = 110.f;
	float GrabBlend = 0.f;
	float BubbleTimer = 0.f;
	TArray<FTransform> ArmXf;
	TNAmbientFX::FEmitter Bubbles;
	TNAmbientFX::FEmitter Ink;
	TNAmbientFX::FEmitter Splash;

	/** La poza en la que vive (todas las máquinas). */
	void ResolvePool();
	/** true si Point (mundo) está dentro de su poza hasta el radio normalizado UMax (1 = la orilla). */
	bool IsInPool(const FVector& Point, float UMax) const;
	/** Servidor: la tortuga está nadando en su poza y se le puede dar. */
	bool IsSwimmer(const ATortugaCharacter* Turtle) const;
	/** Servidor: la nadadora de su poza más cercana (o null). */
	ATortugaCharacter* FindSwimmer() const;
	/** Servidor: nada hacia Goal a Speed sin salirse del agua honda. */
	void SwimToward(const FVector& Goal, float Speed, float DeltaSeconds);
	/** Agua que hay sobre el fondo en Where (cm). */
	float BedDepthAt(const FVector& Where) const;
	/** Profundidad bajo el agua del anillo de los brazos en cada estado, en Where. */
	float DepthFor(uint8 State, const FVector& Where) const;
	/** Cabeceo y alabeo del cuerpo en cada estado (tumbado al acechar, estirado al nadar, derecho al agarrar). */
	FRotator BodyTilt(uint8 State) const;
	/** Dónde lleva a la agarrada Age segundos después de cogerla (todas las máquinas, las mismas cuentas). */
	FVector GripAt(float Age) const;
	/** Velocidad del lanzamiento fuera de la poza desde From, hacia atrás. */
	FVector ThrowLaunch(const FVector& From, float& OutFlight) const;
	/** Todas las máquinas: sujeta a Grabbed mientras está agarrada y la suelta al acabar. */
	void UpdateHold();

	void BuildOctopus();
	void PoseArms(uint8 State, float Age, const FVector& BodyAt, float Yaw, float DeltaSeconds);
};
