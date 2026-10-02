#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachEnemy.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "TN_BeachToyTank.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UTN_BeachCritterSynthComponent;

/** Una bolita de espuma en vuelo (igual en todas las máquinas: sale con la hora del servidor y la misma parábola). */
struct FTNTankShot
{
	bool bAlive = false;
	/** Todavía puede dar (también tras botar en el suelo; tras dar a alguien, ya no). */
	bool bArmed = false;
	uint8 Id = 0;
	int32 Bounces = 0;
	/** Tramo actual de la parábola: sale de P0 con V0 a la hora T0 (reloj del servidor). */
	FVector P0 = FVector::ZeroVector;
	FVector V0 = FVector::ZeroVector;
	double T0 = 0.0;
	double Born = 0.0;
	/** Boca del cañón, suelo de salida y de llegada y distancia en planta hasta donde apuntaba (suelo sin trazas). */
	FVector Origin = FVector::ZeroVector;
	float GroundFrom = 0.f;
	float GroundTo = 0.f;
	float AimDist = 1000.f;
	/** Dónde está ahora y dónde estaba el paso anterior. */
	FVector Pos = FVector::ZeroVector;
	FVector LastPos = FVector::ZeroVector;
	float TrailTimer = 0.f;
};

/**
 * Tanque de juguete teledirigido (ETNBeachElement::ToyTank), el motivo militar de Tortunavy: un tanque verde de
 * plástico de 4,5 m (16 cm reales a escala) con orugas, escarapelas de Tortunavy, antena de látigo y banderita.
 *
 *  - Patrulla adelante y atrás su tramo (el eje X local del actor, centrado en él, con Spec.Extent de largo; 0 = 24 m) a
 *    2,6 m/s, y en cada punta gira sobre sí mismo.
 *  - Con una tortuga atacable a menos de 25 m, se para y gira la torreta hacia ella (110°/s); cuando apunta, dispara
 *    bolitas de espuma naranjas (una cada 4 s; la primera a los 0,7 s) con una parábola visible (estela de humo), un
 *    «¡pomp!» y retroceso del cañón y del casco. Cada bolita que da empuja y marea un poco: bola aturdida 0,8 s
 *    (TNBeach::StunTurtle) a 5,2 m/s, como el golpe directo de las minas. Tras el primer bote ya no hace nada.
 *  - Mareado por un golpe (IsHitStunned): se para, echa humo, tose y la antena da vueltas como una hélice.
 *  - Sólido: la tortuga no lo atraviesa (caja tipo Pawn, como el cangrejo gigante).
 *
 * Red: el casco se mueve con Mover (10 Hz, los clientes interpolan); la torreta de cada máquina gira hacia Mover.Aim
 * (la tortuga que apunta). Cada disparo va por multicast no fiable con su hora del servidor, así cada máquina ve la
 * bolita donde está la del servidor; los golpes los decide el servidor.
 */
UCLASS()
class TORTUNABO_API ATN_BeachToyTank : public ATN_BeachEnemy
{
	GENERATED_BODY()

public:
	ATN_BeachToyTank();

	virtual bool GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const override;
	virtual FVector GetHitStunAnchor() const override;
	virtual float GetHitStunScale() const override;

protected:
	virtual void ApplySpec() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void VisualTick(float DeltaSeconds) override;
	virtual void OnMoverStateChanged(uint8 OldState) override;
	virtual float GetBodyRadius() const override;
	virtual float GetActiveRange() const override;

	/** Todas las máquinas: disparo ShotId desde Origin con Velocity a la hora ServerTime; LandZ es el suelo donde apunta. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFire(uint8 ShotId, FVector_NetQuantize Origin, FVector_NetQuantize10 Velocity, float ServerTime, float LandZ);

	/** Todas las máquinas: la bolita ShotId ha dado a Victim en Where. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFoamHit(uint8 ShotId, FVector_NetQuantize Where, ATortugaCharacter* Victim);

private:
	/** Caja sólida (en todas las máquinas), enganchada a la raíz animada. */
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> Block;

	/** Casco: inclinación por el suelo y el retroceso (cuelga de la raíz animada). */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> HullRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Hull;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Wheels;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> TurretPivot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Turret;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> BarrelPivot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Barrel;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> AntennaPivot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Antenna;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Flag;

	/** Bolitas en vuelo (instancias en coordenadas de mundo). */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> FoamBalls;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachCritterSynthComponent> Sound;

	/** Tamaño propio (SizeScale acotado) y tramo de patrulla (media longitud ya descontado el casco). */
	float SizeK = 1.f;
	float PatrolHalf = 1000.f;
	FVector PatrolAxis = FVector::ForwardVector;
	FVector PatrolCenter = FVector::ZeroVector;

	/** Bolitas: ocho a la vez como mucho (el hueco es Id % 8). */
	static constexpr int32 MaxShots = 8;
	FTNTankShot Shots[MaxShots];

	// Servidor.
	bool bPlaced = false;
	float PatrolSign = 1.f;
	float DriveSpeed = 0.f;
	float TurnGoalYaw = 0.f;
	float ServerTurretYaw = 0.f;
	float ReloadLeft = 0.f;
	float LoseTimer = 0.f;
	float ScanTimer = 0.f;
	float GroundTimer = 0.f;
	float GroundZ = 0.f;
	uint8 NextShotId = 0;
	TWeakObjectPtr<ATortugaCharacter> Target;

	// Visual.
	float VisualClock = 0.f;
	float TurretYawShown = 0.f;
	float BarrelPitchShown = 0.f;
	float RecoilAge = 10.f;
	float WheelSpinL = 0.f;
	float WheelSpinR = 0.f;
	float LastYawShown = 0.f;
	FVector LastLocShown = FVector::ZeroVector;
	FVector2D AntennaLean = FVector2D::ZeroVector;
	FVector2D AntennaLeanVel = FVector2D::ZeroVector;
	float AntennaSpin = 0.f;
	float SpeedShown = 0.f;
	float TiltPitch = 0.f;
	float TiltRoll = 0.f;
	float TiltTimer = 0.f;
	float SputterTimer = 0.f;
	float ServoLevel = 0.f;
	bool bWasStunned = false;
	TArray<FTransform> WheelXf;
	TArray<FTransform> FoamXf;
	TNAmbientFX::FEmitter Muzzle;
	TNAmbientFX::FEmitter Trail;
	TNAmbientFX::FEmitter Smoke;
	TNAmbientFX::FEmitter Dust;

	/** Servidor: arma el tramo de patrulla y se pone a patrullar. */
	void EnsurePatrol();
	/** Servidor: la tortuga atacable más cercana a menos de Radius (o null). */
	ATortugaCharacter* ScanTarget(float Radius) const;
	/** Servidor: avanza el casco a lo largo del tramo hacia Goal (con frenada al llegar), mirando a Yaw. */
	void DriveToward(const FVector& Goal, float MaxSpeed, float DeltaSeconds, bool bBrake);
	/** Servidor: dispara a Victim desde la torreta. */
	void Fire(ATortugaCharacter* Victim);
	/** Ángulo (grados) para dar a una distancia en planta Dx con desnivel Dz a la velocidad de la bolita. */
	float SolveElevation(float Dx, float Dz) const;
	/** Boca del cañón en el mundo para un giro de torreta y un cabeceo (desde el casco en Base mirando a HullYaw). */
	FVector MuzzleAt(const FVector& Base, float HullYaw, float TurretYaw, float Pitch) const;
	/** Avanza las bolitas hasta Now (rebotes y fin); en el servidor, además, a quién dan. */
	void AdvanceShots(double Now, bool bServer);
	/** La bolita da a Victim en Where: rebota hacia atrás y deja de dar. */
	void BounceOffTurtle(FTNTankShot& Shot, const FVector& Where, double Now);

	void BuildTank();
	void PoseTank(float DeltaSeconds, bool bStunned);
};
