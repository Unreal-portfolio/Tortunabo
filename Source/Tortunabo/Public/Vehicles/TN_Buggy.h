// Buggy biplaza del Rally Tortuga (Docs/Rally_MVP.md). Port de AHYBuggy (HellYeah) sin carga ni fichas, con turbo (#294).
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
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class USkeletalMesh;
class USkeletalMeshComponent;
class USpringArmComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTN_BuggyData;
class UTN_BuggyHealthComponent;
class UTN_BuggyEngineAudioComponent;
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
	virtual bool ConsumeFellOutOfWorld() override;
	virtual bool ConsumeDestroyed() override;

	/** Solo servidor: el buggy ha reventado; la carrera lo recoge con ConsumeDestroyed y lo hace reaparecer. */
	void NotifyDestroyed();

	/** Fantasma tras reaparecer o girar (hora del servidor): no recibe impactos ni daño. Válido en el servidor. */
	bool IsRespawnProtected() const;
	virtual void SetWeaponsLocked(bool bLocked) override;
	/** Freno de carrera replicado: la conductora local y el servidor cortan el acelerador y frenan a fondo. */
	virtual void SetRaceBrakeHeld(bool bHeld) override;

	// ── Para el HUD, la carrera y la artillera ──────────────────────────────────

	/** Ajuste del buggy: Data o, sin asset, los valores por defecto de C++. Nunca nulo. */
	const UTN_BuggyData* GetData() const;

	UTN_BuggyTurretComponent* GetTurret() const { return Turret; }
	UTN_BuggyHealthComponent* GetHealthComponent() const { return HealthComponent; }
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

	/** Si la carrera tiene la torreta bloqueada (calentamiento, semáforo, resultados o equipo retirado). */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	bool AreWeaponsLocked() const { return bWeaponsLockedByRace; }

	/** Si la plaza de artillera está ocupada (estado replicado; vale en cualquier máquina). */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	bool HasGunner() const { return bGunnerSeated; }

	/** Si la carrera tiene el freno puesto (parrilla durante el semáforo): sin acelerador ni turbo. */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	bool IsRaceBrakeHeld() const { return bRaceBrakeHeld; }

	/** Carga del turbo en [0, 1] (la decide el servidor; vale en cualquier máquina). */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	float GetBoost01() const { return BoostCharge01; }

	/** Si el turbo empuja ahora: la conductora local lo predice con su botón; el resto lee el estado del servidor. */
	UFUNCTION(BlueprintPure, Category = "Rally|Buggy")
	bool IsBoosting() const;

	/** Sacudida (0..1) para la cámara de la conductora local; en otras máquinas no hace nada. Para impactos y disparos. */
	UFUNCTION(BlueprintCallable, Category = "Rally|Buggy")
	void AddCameraTrauma(float Amount);

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
	/** Bajo el KillZ no se destruye (AActor lo haría): se queda quieto y pide a la carrera volver a la pista. */
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

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

	/** Llama del turbo en el escape: se crea en cada máquina con pantalla mientras el turbo empuja. Vacío = sin efecto. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Turbo")
	TObjectPtr<UNiagaraSystem> BoostEffect;

	/** Sonido del turbo (en bucle mientras empuja), en cada máquina con pantalla. Vacío = sin sonido. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Turbo")
	TObjectPtr<USoundBase> BoostSound;

	/** Golpe de arranque del turbo (una vez, al empezar a empujar). Vacío = sin sonido. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Turbo")
	TObjectPtr<USoundBase> BoostStartSound;

	/** Escape relativo a la carrocería (cm): de ahí salen la llama y el sonido. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Turbo")
	FVector BoostEffectOffset = FVector(-190.f, 0.f, 70.f);

private:
	// ── Física ─────────────────────────────────────────────────────────────────
	/** Fricción, par, frenos de carrera, golpes, charco, antivuelco, estabilidad, turbo y dirección de cada fotograma. */
	void TickDrivePhysics();
	void ApplyWheelFriction();
	void ApplyEngineTorque();
	void ApplySteeringAssist();
	void ApplyBumpKicks();
	void ApplyPuddleSpeedCap();
	/** Antivuelco (TNBuggy::AntiRollAccel) en cada máquina que simula el chasis. */
	void ApplyAntiRoll();
	void HoldLockedInPlace();
	void UpdateSelfRight(float DeltaSeconds);
	void DoSelfRight();
	void UpdateCamera(float DeltaSeconds);
	void UpdateServerTimers();

	// ── Estabilidad y turbo (TN_Buggy_Drive.cpp) ───────────────────────────────
	/** Recalcula bAirborne (ninguna rueda en contacto) en cada máquina. */
	void UpdateAirborne();
	/** Control de estabilidad sin freno de mano (TNBuggy::StabilityYawAccel), en cada máquina que simula el chasis. */
	void ApplyStability();
	/** Gasto y recarga de la barra del turbo (solo servidor). */
	void UpdateBoost(float DeltaSeconds);
	/** Empuje del turbo hacia la punta aumentada, en cada máquina que simula el chasis. */
	void ApplyBoostPush();
	/** Llama y sonido del turbo según IsBoosting (solo en máquinas con pantalla). */
	void RefreshBoostEffects();
	void SetBoostHeld(bool bHeld);
	/** Freno de carrera en el servidor y en la conductora local: acelerador a 0 y freno a fondo cada fotograma. */
	void ApplyRaceBrake();
	/** Al soltar el freno de carrera, quita el freno que puso (servidor y conductora local). */
	void ReleaseRaceBrake();

	UFUNCTION()
	void OnRep_RaceBrake();

	// ── Asientos y tortugas visuales ────────────────────────────────────────────
	void SetDriverSeat(AController* NewDriver);
	void SetGunnerSeat(AController* NewGunner);
	ATN_BuggyGunnerPawn* SpawnGunnerPawn();
	void DestroyGunnerPawn();
	void RefreshSeatVisuals(bool bForce);
	void ApplySeatLook(int32 SeatIndex, bool bForce);
	void FitTurtle(USkeletalMeshComponent* Turtle) const;
	/** Tortuga de la artillera caída hacia atrás mientras está noqueada (cosmético, en cada máquina con pantalla). */
	void UpdateGunnerKnockPose(float DeltaSeconds);

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
	void OnFireCocoReleased(const FInputActionValue& Value);
	void OnFireSpecial(const FInputActionValue& Value);
	void OnFireBackPressed(const FInputActionValue& Value);
	void OnFireBackReleased(const FInputActionValue& Value);
	void OnBoostPressed(const FInputActionValue& Value);
	void OnBoostReleased(const FInputActionValue& Value);
	void OnCycleAmmo(const FInputActionValue& Value);
	void SetHandbrakeHeld(bool bHeld);

	UFUNCTION(Server, Reliable)
	void ServerSetHandbrake(bool bHeld);

	UFUNCTION(Server, Reliable)
	void ServerSetBoostHeld(bool bHeld);

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

	/** Vida del buggy: sus efectos (humo, explosión, choque) se asignan en el Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTN_BuggyHealthComponent> HealthComponent;

	/** Motor en tres capas por RPM y derrape en bucle: local y cosmético (los sonidos, en el componente). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTN_BuggyEngineAudioComponent> EngineAudio;

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

	/** Torreta bloqueada por la carrera; fuera del Rally (sin carrera) dispara siempre. */
	UPROPERTY(Replicated)
	bool bWeaponsLockedByRace = false;

	/** Freno de carrera (parrilla durante el semáforo); lo pone y lo quita el servidor. */
	UPROPERTY(ReplicatedUsing = OnRep_RaceBrake)
	bool bRaceBrakeHeld = false;

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

	/** Carga del turbo [0, 1]; la gasta y la recarga el servidor. */
	UPROPERTY(Replicated)
	float BoostCharge01 = 0.f;

	/** Turbo empujando según el servidor. La conductora no lo recibe: lo predice con su botón (IsBoosting). */
	UPROPERTY(Replicated)
	bool bBoostActive = false;

	// ── Estado local o de servidor ─────────────────────────────────────────────
	UPROPERTY(Transient)
	TObjectPtr<AController> DriverController;

	UPROPERTY(Transient)
	TObjectPtr<AController> GunnerController;

	float GhostEndServerTime = 0.f;
	float PuddleUntilServerTime = 0.f;
	bool bRespawnRequested = false;
	bool bFellOutOfWorld = false;
	bool bDestroyedPending = false;
	bool bSelfRightRequested = false;

	bool bHandbrakeFrictionApplied = false;
	bool bWheelFrictionApplied = false;
	float AppliedGripMultiplier = 1.f;
	bool bEngineTorqueLockedApplied = false;
	bool bBoostTorqueApplied = false;
	/** Botón del turbo: el de la conductora local y, en el servidor, el último que pidió por RPC. */
	bool bBoostHeld = false;
	bool bAirborne = false;
	bool bBoostEffectsOn = false;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BoostEffectComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BoostSoundComponent;

	/** Cámara dinámica de la conductora (#298) y sacudida pedida desde fuera para el siguiente fotograma. */
	TNBuggy::FDriverCameraState CameraState;
	float PendingCameraTrauma = 0.f;
	bool bAimBackward = false;
	double LastDriverFireRequest = -1000.0;
	/** Con una especial seleccionada, la conductora sola dispara una vez por pulsación (como la artillera). */
	bool bDriverFireLatched = false;
	/** Caída de la tortuga de la artillera noqueada: 0 sentada, 1 tumbada hacia atrás. */
	float GunnerKnockLean01 = 0.f;

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
