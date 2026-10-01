// Buggy biplaza del Rally Tortuga (Docs/Rally_MVP.md). Port de AHYBuggy (HellYeah) sin carga, fichas ni boost.
#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Rally/TN_RallyVehicle.h"
#include "Vehicles/TN_BuggyMath.h"
#include "TN_Buggy.generated.h"

class APlayerState;
class ATN_BuggyGunnerPawn;
class UAnimInstance;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;
class USpringArmComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTN_BuggyData;
class UTN_BuggyInputSet;
class UTN_BuggyTurretComponent;
struct FInputActionValue;

/**
 * Vehículo Chaos sobre el esqueleto SKM_Offroad (oculto: solo física y huesos de rueda) con la carrocería y los
 * neumáticos low poly de HellYeah. La conductora posee el buggy; la artillera posee un ATN_BuggyGunnerPawn sujeto al
 * asiento trasero y maneja la torreta (UTN_BuggyTurretComponent). Si va sola, la conductora dispara con apuntado
 * automático.
 *
 * Red: el servidor tiene la autoridad (movimiento replicado de Chaos con PredictiveInterpolation). Los clientes solo
 * mandan entradas: conducción por el movimiento de Chaos, y enderezado, reaparición y disparo por RPC validada. Los
 * impactos (coco, charco, mortero, tinta, escudo) los decide el servidor y los replica como estado con hora de fin.
 */
UCLASS()
class TORTUNABO_API ATN_Buggy : public AWheeledVehiclePawn, public ITN_RallyVehicle
{
	GENERATED_BODY()

public:
	/** Slot de SM_BuggyBody que lleva M_PlayerTint y su parámetro de color. */
	static const FName TintSlotName;
	static const FName TintParameterName;

	/** Asiento de la artillera relativo al chasis (Seat_Gunner): allí van la torreta y su tortuga. */
	static const FVector GunnerSeatLocal;
	/** Asiento de la conductora relativo al chasis. */
	static const FVector DriverSeatLocal;

	ATN_Buggy();

	// ── ITN_RallyVehicle ────────────────────────────────────────────────────────
	virtual bool SeatController(AController* InController, ETNRallySeat Seat) override;
	virtual void UnseatController(AController* InController) override;
	virtual AController* GetSeatController(ETNRallySeat Seat) const override;
	virtual bool HasFreeSeat(ETNRallySeat Seat) const override;
	virtual void RallyTeleport(const FTransform& Where, float LockSeconds, float GhostSeconds) override;
	virtual void SetEngineLocked(bool bLocked) override;
	virtual float GetForwardSpeedCms() const override;
	virtual bool IsFlipped() const override;
	virtual int32 GetRallyTeamIndex() const override { return TeamIndex; }
	virtual void SetRallyTeamIndex(int32 Index) override;
	virtual void GiveSpecialAmmo(ETNRallyAmmo Ammo, int32 Charges) override;
	virtual ETNRallyAmmo GetSpecialAmmo() const override;
	virtual void SetAIDriveInput(float Throttle, float Brake, float Steer, bool bHandbrake) override;
	virtual void AIFire(const FVector& AimWorldDir, bool bSpecial) override;
	virtual bool ConsumeRespawnRequest() override;

	// ── Para el HUD, la carrera y la artillera ──────────────────────────────────

	/** Ajuste del buggy: Data o, sin asset, los valores por defecto de C++. Nunca nulo. */
	const UTN_BuggyData* GetData() const;

	UTN_BuggyTurretComponent* GetTurret() const { return Turret; }
	ATN_BuggyGunnerPawn* GetGunnerPawn() const { return GunnerPawn; }
	UStaticMeshComponent* GetBody() const { return Body; }
	UChaosWheeledVehicleMovementComponent* GetWheeledMovement() const;

	/** Segundos de tinta en pantalla que quedan (0 = limpia). Vale en cualquier máquina. */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	float GetInkSecondsLeft() const;

	/** Si el escudo de la burbuja está activo. */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	bool IsShielded() const;

	/** Si el buggy pisa un charco de alga (agarre y velocidad máxima reducidos). */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	bool IsInPuddle() const { return bInPuddle; }

	/** Si el motor está cortado (semáforo, salida anticipada, reaparición o fin). */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	bool IsEngineLocked() const;

	/** Si el PlayerController local ocupa este buggy (conductora o artillera): para la tinta del HUD. */
	bool IsOccupiedByLocalPlayer() const;

	/** Hora del servidor (vale en cualquier máquina). */
	double GetServerNow() const;

	// ── Impactos (solo servidor) ────────────────────────────────────────────────

	/** Coco: impulso lateral y bamboleo de la dirección. HitDir es la dirección del proyectil. */
	void ApplyCocoHit(const FVector& HitDir);
	/** Mortero: impulso vertical sin vuelco forzado. */
	void ApplyMortarBlast();
	/** Tinta en la pantalla de las dos ocupantes. */
	void ApplyInk();
	/** Escudo de la burbuja. */
	void GrantShield();
	/** Si hay escudo, lo gasta y devuelve true (el impacto o el charco no llegan). */
	bool TryConsumeShield();
	/** El charco de alga avisa cada vez que comprueba que el buggy está dentro. */
	void NotePuddleContact();
	/** Impulso de velocidad (cm/s) al chasis en el servidor, con ForceNetUpdate. */
	void ApplyVelocityImpulse(const FVector& DeltaVelocity);

	/** Pide el enderezado (lo revalida el servidor) y lleva la cuenta del botón mantenido para reaparecer. */
	void HandleSelfRightInput(bool bPressed);

	/** Disparo de la conductora sola (servidor): apuntado automático hacia delante o hacia atrás. */
	void DriverFireAuto(bool bSpecial, bool bBackward);

	/** Conductora local sin artillera: pide al servidor un disparo con apuntado automático (entrada y TN.Rally.LocalFire). */
	void RequestDriverFire(bool bSpecial, bool bBackward);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSelfRight();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestRespawn();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Buggy")
	TObjectPtr<UTN_BuggyData> Data;

	// Rutas de los assets (copiados de HellYeah con sus rutas /Game). El constructor pone los de por defecto.
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Assets")
	TSoftObjectPtr<USkeletalMesh> ChassisMeshAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Assets")
	TSoftClassPtr<UAnimInstance> ChassisAnimClass;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Assets")
	TSoftObjectPtr<UStaticMesh> BodyMeshAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Assets")
	TSoftObjectPtr<UStaticMesh> TireMeshAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Assets")
	TSoftObjectPtr<USkeletalMesh> TurtleMeshAsset;

	/** Altura a la que se escala la tortuga sentada (cm, de la caja de la malla). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Assets")
	float SeatedTurtleHeightCm = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Buggy")
	TSubclassOf<ATN_BuggyGunnerPawn> GunnerPawnClass;

private:
	// ── Física ─────────────────────────────────────────────────────────────────
	void ApplyWheelFriction();
	void ApplyEngineTorque();
	void ApplySteeringAssist();
	void ApplyBumpKicks();
	void ApplyPuddleSpeedCap();
	void HoldLockedInPlace();
	void UpdateSelfRight(float DeltaSeconds);
	void DoSelfRight();
	void UpdateCamera(float DeltaSeconds);
	void UpdateServerTimers();

	// ── Asientos y tortugas visuales ────────────────────────────────────────────
	void SetDriverSeat(AController* NewDriver);
	void SetGunnerSeat(AController* NewGunner);
	ATN_BuggyGunnerPawn* SpawnGunnerPawn();
	void DestroyGunnerPawn();
	void RefreshSeatVisuals(bool bForce);
	void ApplySeatLook(int32 SeatIndex, bool bForce);
	void FitTurtle(USkeletalMeshComponent* Turtle) const;

	UFUNCTION()
	void OnRep_Seats();

	UFUNCTION()
	void OnRep_TeamIndex();
	void ApplyTint();

	UFUNCTION()
	void OnRep_Ghost();
	void ApplyGhost();

	// ── Input de la conductora ─────────────────────────────────────────────────
	UTN_BuggyInputSet* GetInputSet();
	void OnThrottle(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnHandbrakePressed(const FInputActionValue& Value);
	void OnHandbrakeReleased(const FInputActionValue& Value);
	void OnSelfRightPressed(const FInputActionValue& Value);
	void OnSelfRightReleased(const FInputActionValue& Value);
	void OnFireCoco(const FInputActionValue& Value);
	void OnFireSpecial(const FInputActionValue& Value);
	void OnFireBackPressed(const FInputActionValue& Value);
	void OnFireBackReleased(const FInputActionValue& Value);
	void SetHandbrakeHeld(bool bHeld);

	UFUNCTION(Server, Reliable)
	void ServerSetHandbrake(bool bHeld);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerDriverFire(bool bSpecial, bool bBackward);

	// ── Componentes ────────────────────────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Tires;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTN_BuggyTurretComponent> Turret;

	/** Cañón de la torreta (cilindro básico): gira con el apuntado. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TurretBarrel;

	/** Tortugas visuales: 0 = conductora, 1 = artillera. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<USkeletalMeshComponent>> SeatTurtles;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> SeatHelmets;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BuggyInputSet> InputSet;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TintMaterial;

	/** Materiales originales de cada tortuga (UTN_CosmeticLook::ApplyLook parte de ellos). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> DriverTurtleDefaults;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> GunnerTurtleDefaults;

	// ── Estado replicado ───────────────────────────────────────────────────────
	UPROPERTY(ReplicatedUsing = OnRep_TeamIndex)
	int32 TeamIndex = INDEX_NONE;

	/** Freno de mano de la conductora, para que todas las máquinas simulen la misma fricción. */
	UPROPERTY(Replicated)
	bool bHandbrakeHeld = false;

	UPROPERTY(ReplicatedUsing = OnRep_Seats)
	bool bDriverSeated = false;

	UPROPERTY(ReplicatedUsing = OnRep_Seats)
	bool bGunnerSeated = false;

	/** PlayerState de cada ocupante (null para la IA): su aspecto viste a la tortuga del asiento. */
	UPROPERTY(ReplicatedUsing = OnRep_Seats)
	TObjectPtr<APlayerState> DriverPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_Seats)
	TObjectPtr<APlayerState> GunnerPlayerState;

	UPROPERTY(Replicated)
	TObjectPtr<ATN_BuggyGunnerPawn> GunnerPawn;

	UPROPERTY(Replicated)
	bool bEngineLockedByRace = false;

	/** Horas del servidor en que acaban los efectos (0 = sin efecto). */
	UPROPERTY(Replicated)
	float LockEndServerTime = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Ghost)
	bool bGhost = false;

	UPROPERTY(Replicated)
	float ShieldEndServerTime = 0.f;

	UPROPERTY(Replicated)
	float InkEndServerTime = 0.f;

	UPROPERTY(Replicated)
	float WobbleEndServerTime = 0.f;

	UPROPERTY(Replicated)
	bool bInPuddle = false;

	// ── Estado local o de servidor ─────────────────────────────────────────────
	UPROPERTY(Transient)
	TObjectPtr<AController> DriverController;

	UPROPERTY(Transient)
	TObjectPtr<AController> GunnerController;

	float GhostEndServerTime = 0.f;
	float PuddleUntilServerTime = 0.f;
	bool bRespawnRequested = false;
	bool bSelfRightRequested = false;

	bool bHandbrakeFrictionApplied = false;
	bool bWheelFrictionApplied = false;
	float AppliedGripMultiplier = 1.f;
	bool bEngineTorqueLockedApplied = false;
	bool bAimBackward = false;
	double LastDriverFireRequest = -1000.0;

	float SteerRequest = 0.f;
	float FlippedSeconds = 0.f;
	TNBuggy::FHold RespawnHold;
	bool bSelfRightHeld = false;
	float SeatLookCheckAccumulator = 0.f;

	/** Aspecto aplicado a cada tortuga (para no reaplicarlo cada vez). */
	TArray<FString> AppliedLookKeys;

	TArray<FVector> PrevContactPoint;
	TArray<bool> bPrevWheelContact;
	TArray<TNBuggy::FWheelTrack> BumpTracks;
};
